/**
 * @file lookback_mc_buehler_option.cpp
 * @brief Discrete max lookback payoffs on the Buehler model save path.
 */

#include "lookback_mc_buehler_option.h"
#include "buehler_mc_path_pricing.h"
#include "buehler_model.h"
#include "mc_observation_schedule.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

using namespace QuantLib;

Real callPutPayoff(const Real underlying, const Real strike, const bool isCall) {
    if (isCall)
        return std::max(underlying - strike, 0.0);
    return std::max(strike - underlying, 0.0);
}

struct LookbackMcPricingSetup {
    std::vector<Size> obsIdx;
    Size expiryIdx = 0;
    bool isCall = true;
    Real discount = 1.0;
    Real strike = 0.0;
    LookbackMcStrikeStyle strikeStyle = LookbackMcStrikeStyle::Fixed;
};

LookbackMcPricingSetup buildLookbackMcPricingSetup(const BuehlerFixingSavePath& bank,
                                                   const OptionContractParams& params,
                                                   const BuehlerModel& buehler,
                                                   const LookbackMcStrikeStyle strikeStyle) {
    QL_REQUIRE(params.expiry > buehler.today(),
               "LookbackMcBuehlerOption: expiry must be after today");
    QL_REQUIRE(bank.hasFixingDate(params.expiry),
               "LookbackMcBuehlerOption: expiry not on save path");

    const std::vector<Date> fixingDates =
        resolveMcObservationDates(buehler, params, "LookbackMcBuehlerOption");
    QL_REQUIRE(!fixingDates.empty(), "LookbackMcBuehlerOption: empty observation schedule");

    LookbackMcPricingSetup setup;
    setup.obsIdx.reserve(fixingDates.size());
    for (const Date& d : fixingDates) {
        QL_REQUIRE(bank.hasFixingDate(d),
                   "LookbackMcBuehlerOption: observation " << d << " not on save path");
        setup.obsIdx.push_back(bank.fixingIndex(d));
    }
    setup.expiryIdx = bank.fixingIndex(params.expiry);
    setup.isCall = params.isCall;
    setup.discount = buehler.riskFreeTs()->discount(params.expiry);
    setup.strikeStyle = strikeStyle;

    if (strikeStyle == LookbackMcStrikeStyle::Fixed) {
        QL_REQUIRE(params.strike != Null<Real>(),
                   "LookbackMcBuehlerOption: fixed strike requires params.strike");
        setup.strike = params.strike;
    }
    return setup;
}

Real pathMaxSpot(const BuehlerFixingSavePath& bank, const Size pathIndex,
                 const LookbackMcPricingSetup& setup) {
    Real runningMax = 0.0;
    for (const Size obsIdx : setup.obsIdx) {
        const Real spot = bank.sLevel(pathIndex, obsIdx);
        QL_REQUIRE(spot > 0.0, "LookbackMcBuehlerOption: spot level must be positive");
        runningMax = std::max(runningMax, spot);
    }
    QL_REQUIRE(runningMax > 0.0, "LookbackMcBuehlerOption: running max must be positive");
    return runningMax;
}

Real pathPayoff(const BuehlerFixingSavePath& bank, const Size pathIndex,
                const LookbackMcPricingSetup& setup) {
    const Real runningMax = pathMaxSpot(bank, pathIndex, setup);
    const Real terminal = bank.sLevel(pathIndex, setup.expiryIdx);
    QL_REQUIRE(terminal > 0.0, "LookbackMcBuehlerOption: terminal spot must be positive");

    if (setup.strikeStyle == LookbackMcStrikeStyle::Fixed)
        return callPutPayoff(runningMax, setup.strike, setup.isCall);
    return callPutPayoff(runningMax, terminal, setup.isCall);
}

struct LookbackMcBatchSetup {
    std::vector<Size> obsIdx;
    Size expiryIdx = 0;
    bool isCall = true;
    Real discount = 1.0;
    Real strike = 0.0;
};

LookbackMcBatchSetup buildLookbackMcBatchSetup(const BuehlerFixingSavePath& bank,
                                               const OptionContractParams& params,
                                               const BuehlerModel& buehler) {
    QL_REQUIRE(params.expiry > buehler.today(),
               "LookbackMcBuehlerOption: expiry must be after today");
    QL_REQUIRE(params.strike != Null<Real>(),
               "LookbackMcBuehlerOption: priceAllPayoffs requires params.strike");
    QL_REQUIRE(bank.hasFixingDate(params.expiry),
               "LookbackMcBuehlerOption: expiry not on save path");

    const std::vector<Date> fixingDates =
        resolveMcObservationDates(buehler, params, "LookbackMcBuehlerOption");
    QL_REQUIRE(!fixingDates.empty(), "LookbackMcBuehlerOption: empty observation schedule");

    LookbackMcBatchSetup setup;
    setup.obsIdx.reserve(fixingDates.size());
    for (const Date& d : fixingDates) {
        QL_REQUIRE(bank.hasFixingDate(d),
                   "LookbackMcBuehlerOption: observation " << d << " not on save path");
        setup.obsIdx.push_back(bank.fixingIndex(d));
    }
    setup.expiryIdx = bank.fixingIndex(params.expiry);
    setup.isCall = params.isCall;
    setup.discount = buehler.riskFreeTs()->discount(params.expiry);
    setup.strike = params.strike;
    return setup;
}

void pathPayoffsAll(const BuehlerFixingSavePath& bank, const Size pathIndex,
                    const LookbackMcBatchSetup& setup, Real& fixedPayoff, Real& floatingPayoff) {
    Real runningMax = 0.0;
    for (const Size obsIdx : setup.obsIdx) {
        const Real spot = bank.sLevel(pathIndex, obsIdx);
        QL_REQUIRE(spot > 0.0, "LookbackMcBuehlerOption: spot level must be positive");
        runningMax = std::max(runningMax, spot);
    }
    QL_REQUIRE(runningMax > 0.0, "LookbackMcBuehlerOption: running max must be positive");

    const Real terminal = bank.sLevel(pathIndex, setup.expiryIdx);
    QL_REQUIRE(terminal > 0.0, "LookbackMcBuehlerOption: terminal spot must be positive");

    fixedPayoff = callPutPayoff(runningMax, setup.strike, setup.isCall);
    floatingPayoff = callPutPayoff(runningMax, terminal, setup.isCall);
}

} // namespace

std::string LookbackMcBuehlerOption::scenarioExportBaseName() const {
    return std::string("training_set_lookback_mc_max_") + (params_.isCall ? "call" : "put") + "_" +
           (strikeStyle_ == LookbackMcStrikeStyle::Floating ? "float" : "fixed") + "_S";
}

LookbackMcBuehlerOption::LookbackMcBuehlerOption(OptionContractParams params,
                                                 const LookbackMcStrikeStyle strikeStyle)
: Option(std::move(params)), strikeStyle_(strikeStyle) {}

BuehlerMcPathPricingResult LookbackMcBuehlerOption::priceFromSavePath(
    const BuehlerFixingSavePath& savePath,
    const OptionContractParams& params,
    const BuehlerModel& buehler,
    const LookbackMcStrikeStyle strikeStyle) {
    const LookbackMcPricingSetup setup =
        buildLookbackMcPricingSetup(savePath, params, buehler, strikeStyle);
    std::vector<Real> payoffs;
    payoffs.reserve(savePath.numPaths());
    for (Size p = 0; p < savePath.numPaths(); ++p)
        payoffs.push_back(pathPayoff(savePath, p, setup));
    return buehlerMcStatsFromPayoffs(payoffs, setup.discount);
}

LookbackMcTwoPayoffs LookbackMcBuehlerOption::priceAllPayoffsFromSavePath(
    const BuehlerFixingSavePath& savePath,
    const OptionContractParams& params,
    const BuehlerModel& buehler) {
    const LookbackMcBatchSetup setup = buildLookbackMcBatchSetup(savePath, params, buehler);
    const Size nPaths = savePath.numPaths();

    std::vector<Real> payFixed;
    std::vector<Real> payFloating;
    payFixed.reserve(nPaths);
    payFloating.reserve(nPaths);

    for (Size p = 0; p < nPaths; ++p) {
        Real fixedPayoff = 0.0;
        Real floatingPayoff = 0.0;
        pathPayoffsAll(savePath, p, setup, fixedPayoff, floatingPayoff);
        payFixed.push_back(fixedPayoff);
        payFloating.push_back(floatingPayoff);
    }

    LookbackMcTwoPayoffs out;
    out.fixed = buehlerMcStatsFromPayoffs(payFixed, setup.discount);
    out.floating = buehlerMcStatsFromPayoffs(payFloating, setup.discount);
    return out;
}

BuehlerMcPathPricingResult LookbackMcBuehlerOption::priceWithStdError(
    const BuehlerModel& buehler) const {
    QL_REQUIRE(buehler.hasFixingSavePath(),
               "LookbackMcBuehlerOption: call simulateFixingPaths first");
    return priceFromSavePath(buehler.fixingSavePath(), params_, buehler, strikeStyle_);
}

QuantLib::Real LookbackMcBuehlerOption::price(const BuehlerModel& buehler) const {
    return priceWithStdError(buehler).value;
}

LookbackMcTwoPayoffs LookbackMcBuehlerOption::priceAllPayoffs(const BuehlerModel& buehler) const {
    QL_REQUIRE(buehler.hasFixingSavePath(),
               "LookbackMcBuehlerOption: call simulateFixingPaths first");
    return priceAllPayoffsFromSavePath(buehler.fixingSavePath(), params_, buehler);
}
