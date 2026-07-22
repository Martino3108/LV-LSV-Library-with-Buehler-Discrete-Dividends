/**
 * @file barrier_mc_buehler_option.cpp
 * @brief Discrete barrier payoffs on the Buehler model save path (`simulateFixingPaths`).
 */

#include "barrier_mc_buehler_option.h"
#include "buehler_mc_path_pricing.h"
#include "buehler_model.h"
#include "mc_observation_schedule.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

using namespace QuantLib;

const char* barrierMcTypeLabel(const BarrierMcType type) {
    switch (type) {
      case BarrierMcType::DownOut:
          return "down_out";
      case BarrierMcType::DownIn:
          return "down_in";
      case BarrierMcType::UpOut:
          return "up_out";
      case BarrierMcType::UpIn:
          return "up_in";
    }
    return "unknown";
}

Real callPutPayoff(const Real underlying, const Real strike, const bool isCall) {
    if (isCall)
        return std::max(underlying - strike, 0.0);
    return std::max(strike - underlying, 0.0);
}

bool isDownBarrier(const BarrierMcType type) {
    return type == BarrierMcType::DownOut || type == BarrierMcType::DownIn;
}

bool isKnockOut(const BarrierMcType type) {
    return type == BarrierMcType::DownOut || type == BarrierMcType::UpOut;
}

bool barrierTriggered(const Real spot, const Real barrierLevel, const BarrierMcType type) {
    if (isDownBarrier(type))
        return spot <= barrierLevel;
    return spot >= barrierLevel;
}

Real barrierLevelInS(const OptionContractParams& params, const BarrierMcType type) {
    if (isDownBarrier(type)) {
        QL_REQUIRE(params.barrierDown.has_value(),
                   "BarrierMcBuehlerOption: down barrier requires params.barrierDown");
        return *params.barrierDown;
    }
    QL_REQUIRE(params.barrierUp.has_value(),
               "BarrierMcBuehlerOption: up barrier requires params.barrierUp");
    return *params.barrierUp;
}

struct BarrierMcPricingSetup {
    std::vector<Size> obsIdx;
    Size expiryIdx = 0;
    bool isCall = true;
    Real discount = 1.0;
    Real strike = 0.0;
    Real barrierLevel = 0.0;
    BarrierMcType barrierType = BarrierMcType::DownOut;
};

BarrierMcPricingSetup buildBarrierMcPricingSetup(const BuehlerFixingSavePath& bank,
                                                 const OptionContractParams& params,
                                                 const BuehlerModel& buehler,
                                                 const BarrierMcType barrierType) {
    QL_REQUIRE(params.expiry > buehler.today(), "BarrierMcBuehlerOption: expiry must be after today");
    QL_REQUIRE(params.strike != Null<Real>(), "BarrierMcBuehlerOption: strike must be set");
    QL_REQUIRE(bank.hasFixingDate(params.expiry),
               "BarrierMcBuehlerOption: expiry not on save path");

    const std::vector<Date> fixingDates =
        resolveMcObservationDates(buehler, params, "BarrierMcBuehlerOption");
    BarrierMcPricingSetup setup;
    setup.obsIdx.reserve(fixingDates.size());
    for (const Date& d : fixingDates) {
        QL_REQUIRE(bank.hasFixingDate(d),
                   "BarrierMcBuehlerOption: observation " << d << " not on save path");
        setup.obsIdx.push_back(bank.fixingIndex(d));
    }
    setup.expiryIdx = bank.fixingIndex(params.expiry);
    setup.isCall = params.isCall;
    setup.discount = buehler.riskFreeTs()->discount(params.expiry);
    setup.strike = params.strike;
    setup.barrierLevel = barrierLevelInS(params, barrierType);
    setup.barrierType = barrierType;
    return setup;
}

Real pathPayoff(const BuehlerFixingSavePath& bank, const Size pathIndex,
                const BarrierMcPricingSetup& setup) {
    bool triggered = false;
    for (const Size obsIdx : setup.obsIdx) {
        const Real spot = bank.sLevel(pathIndex, obsIdx);
        QL_REQUIRE(spot > 0.0, "BarrierMcBuehlerOption: spot level must be positive");
        if (barrierTriggered(spot, setup.barrierLevel, setup.barrierType))
            triggered = true;
    }

    const bool knockOut = isKnockOut(setup.barrierType);
    if (knockOut && triggered)
        return 0.0;
    if (!knockOut && !triggered)
        return 0.0;

    const Real terminal = bank.sLevel(pathIndex, setup.expiryIdx);
    QL_REQUIRE(terminal > 0.0, "BarrierMcBuehlerOption: terminal spot must be positive");
    return callPutPayoff(terminal, setup.strike, setup.isCall);
}

struct BarrierMcBatchSetup {
    std::vector<Size> obsIdx;
    Size expiryIdx = 0;
    bool isCall = true;
    Real discount = 1.0;
    Real strike = 0.0;
    Real barrierDown = 0.0;
    Real barrierUp = 0.0;
};

BarrierMcBatchSetup buildBarrierMcBatchSetup(const BuehlerFixingSavePath& bank,
                                             const OptionContractParams& params,
                                             const BuehlerModel& buehler) {
    QL_REQUIRE(params.expiry > buehler.today(), "BarrierMcBuehlerOption: expiry must be after today");
    QL_REQUIRE(params.strike != Null<Real>(), "BarrierMcBuehlerOption: strike must be set");
    QL_REQUIRE(params.barrierDown.has_value(),
               "BarrierMcBuehlerOption: priceAllPayoffs requires params.barrierDown");
    QL_REQUIRE(params.barrierUp.has_value(),
               "BarrierMcBuehlerOption: priceAllPayoffs requires params.barrierUp");
    QL_REQUIRE(bank.hasFixingDate(params.expiry),
               "BarrierMcBuehlerOption: expiry not on save path");

    const std::vector<Date> fixingDates =
        resolveMcObservationDates(buehler, params, "BarrierMcBuehlerOption");
    BarrierMcBatchSetup setup;
    setup.obsIdx.reserve(fixingDates.size());
    for (const Date& d : fixingDates) {
        QL_REQUIRE(bank.hasFixingDate(d),
                   "BarrierMcBuehlerOption: observation " << d << " not on save path");
        setup.obsIdx.push_back(bank.fixingIndex(d));
    }
    setup.expiryIdx = bank.fixingIndex(params.expiry);
    setup.isCall = params.isCall;
    setup.discount = buehler.riskFreeTs()->discount(params.expiry);
    setup.strike = params.strike;
    setup.barrierDown = *params.barrierDown;
    setup.barrierUp = *params.barrierUp;
    return setup;
}

void pathPayoffsAll(const BuehlerFixingSavePath& bank, const Size pathIndex,
                    const BarrierMcBatchSetup& setup, Real& downOut, Real& downIn, Real& upOut,
                    Real& upIn) {
    bool triggeredDown = false;
    bool triggeredUp = false;
    for (const Size obsIdx : setup.obsIdx) {
        const Real spot = bank.sLevel(pathIndex, obsIdx);
        QL_REQUIRE(spot > 0.0, "BarrierMcBuehlerOption: spot level must be positive");
        if (spot <= setup.barrierDown)
            triggeredDown = true;
        if (spot >= setup.barrierUp)
            triggeredUp = true;
    }

    const Real terminal = bank.sLevel(pathIndex, setup.expiryIdx);
    QL_REQUIRE(terminal > 0.0, "BarrierMcBuehlerOption: terminal spot must be positive");
    const Real vanilla = callPutPayoff(terminal, setup.strike, setup.isCall);

    downOut = triggeredDown ? 0.0 : vanilla;
    downIn = triggeredDown ? vanilla : 0.0;
    upOut = triggeredUp ? 0.0 : vanilla;
    upIn = triggeredUp ? vanilla : 0.0;
}

} // namespace

std::string BarrierMcBuehlerOption::scenarioExportBaseName() const {
    return std::string("training_set_barrier_mc_") + barrierMcTypeLabel(barrierType_) + "_" +
           (params_.isCall ? "call" : "put") + "_S";
}

BarrierMcBuehlerOption::BarrierMcBuehlerOption(OptionContractParams params,
                                               const BarrierMcType barrierType)
: Option(std::move(params)), barrierType_(barrierType) {}

BuehlerMcPathPricingResult BarrierMcBuehlerOption::priceFromSavePath(
    const BuehlerFixingSavePath& savePath,
    const OptionContractParams& params,
    const BuehlerModel& buehler,
    const BarrierMcType barrierType) {
    const BarrierMcPricingSetup setup =
        buildBarrierMcPricingSetup(savePath, params, buehler, barrierType);
    std::vector<Real> payoffs;
    payoffs.reserve(savePath.numPaths());
    for (Size p = 0; p < savePath.numPaths(); ++p)
        payoffs.push_back(pathPayoff(savePath, p, setup));
    return buehlerMcStatsFromPayoffs(payoffs, setup.discount);
}

BarrierMcFourPayoffs BarrierMcBuehlerOption::priceAllPayoffsFromSavePath(
    const BuehlerFixingSavePath& savePath,
    const OptionContractParams& params,
    const BuehlerModel& buehler) {
    const BarrierMcBatchSetup setup = buildBarrierMcBatchSetup(savePath, params, buehler);
    const Size nPaths = savePath.numPaths();

    std::vector<Real> payDownOut;
    std::vector<Real> payDownIn;
    std::vector<Real> payUpOut;
    std::vector<Real> payUpIn;
    payDownOut.reserve(nPaths);
    payDownIn.reserve(nPaths);
    payUpOut.reserve(nPaths);
    payUpIn.reserve(nPaths);

    for (Size p = 0; p < nPaths; ++p) {
        Real downOut = 0.0;
        Real downIn = 0.0;
        Real upOut = 0.0;
        Real upIn = 0.0;
        pathPayoffsAll(savePath, p, setup, downOut, downIn, upOut, upIn);
        payDownOut.push_back(downOut);
        payDownIn.push_back(downIn);
        payUpOut.push_back(upOut);
        payUpIn.push_back(upIn);
    }

    BarrierMcFourPayoffs out;
    out.downOut = buehlerMcStatsFromPayoffs(payDownOut, setup.discount);
    out.downIn = buehlerMcStatsFromPayoffs(payDownIn, setup.discount);
    out.upOut = buehlerMcStatsFromPayoffs(payUpOut, setup.discount);
    out.upIn = buehlerMcStatsFromPayoffs(payUpIn, setup.discount);
    return out;
}

BuehlerMcPathPricingResult BarrierMcBuehlerOption::priceWithStdError(
    const BuehlerModel& buehler) const {
    QL_REQUIRE(buehler.hasFixingSavePath(),
               "BarrierMcBuehlerOption: call simulateFixingPaths first");
    return priceFromSavePath(buehler.fixingSavePath(), params_, buehler, barrierType_);
}

QuantLib::Real BarrierMcBuehlerOption::price(const BuehlerModel& buehler) const {
    return priceWithStdError(buehler).value;
}

BarrierMcFourPayoffs BarrierMcBuehlerOption::priceAllPayoffs(const BuehlerModel& buehler) const {
    QL_REQUIRE(buehler.hasFixingSavePath(),
               "BarrierMcBuehlerOption: call simulateFixingPaths first");
    return priceAllPayoffsFromSavePath(buehler.fixingSavePath(), params_, buehler);
}
