/**
 * @file lv_digital_accrual_fd_buehler_option.cpp
 * @brief `LvDigitalAccrualFdBuehlerOption` pricing.
 */

#include "lv_digital_accrual_fd_buehler_option.h"
#include "lv_digital_fd_buehler_option.h"
#include "buehler_model.h"
#include "buehler_pure_x_strike.h"
#include <ql/quantlib.hpp>
#include <string>

std::string LvDigitalAccrualFdBuehlerOption::scenarioExportBaseName() const {
    return std::string("training_set_lv_digital_accrual_fd_") +
           (quoteSpace_ == BuehlerOptionPriceSpace::X ? "X" : "S");
}

QuantLib::Real LvDigitalAccrualFdBuehlerOption::pureXStrikeFromSpot(const BuehlerModel& b,
                                                                 const QuantLib::Date& expiry,
                                                                 QuantLib::Real strikeS) {
    return buehlerPureXStrikeFromSpot(b, expiry, strikeS);
}

LvDigitalAccrualFdBuehlerOption::LvDigitalAccrualFdBuehlerOption(OptionContractParams params,
                                                             QuantLib::Size tGridPerYear,
                                                             QuantLib::Size xGrid)
: Option(std::move(params)), tGridPerYear_(tGridPerYear), xGrid_(xGrid) {
    QL_REQUIRE(tGridPerYear_ > 0, "LvDigitalAccrualFdBuehlerOption: tGridPerYear must be positive");
    QL_REQUIRE(xGrid_ > 2, "LvDigitalAccrualFdBuehlerOption: xGrid must be > 2");
}

LvDigitalAccrualFdBuehlerOption::LvDigitalAccrualFdBuehlerOption(OptionContractParams params,
                                                             BuehlerOptionPriceSpace buehlerPriceSpace,
                                                             QuantLib::Size tGridPerYear,
                                                             QuantLib::Size xGrid)
: LvDigitalAccrualFdBuehlerOption(std::move(params), tGridPerYear, xGrid) {
    quoteSpace_ = buehlerPriceSpace;
}

QuantLib::Real LvDigitalAccrualFdBuehlerOption::priceCashDigitalCallInX(
    const BuehlerModel& buehler,
    const QuantLib::Date& expiry,
    QuantLib::Real strikeS) const {
    OptionContractParams leg;
    leg.expiry = expiry;
    leg.strike = strikeS;
    leg.isCall = true;
    const LvDigitalFdBuehlerOption digital(leg, BuehlerOptionPriceSpace::X, tGridPerYear_, xGrid_, false);
    return digital.price(buehler);
}

QuantLib::Real LvDigitalAccrualFdBuehlerOption::price(const BuehlerModel& buehler) const {
    using namespace QuantLib;

    QL_REQUIRE(!buehler.riskFreeTs().empty(),
               "LvDigitalAccrualFdBuehlerOption: empty risk-free curve in BuehlerModel");
    QL_REQUIRE(!buehler.fixedPureLocalVolTs().empty(),
               "LvDigitalAccrualFdBuehlerOption: empty fixed pure-X local vol (run BuehlerModel::calibration)");
    QL_REQUIRE(params_.strikeLow != Null<Real>(), "LvDigitalAccrualFdBuehlerOption: strikeLow must be set");
    QL_REQUIRE(params_.strikeUp != Null<Real>(), "LvDigitalAccrualFdBuehlerOption: strikeUp must be set");
    QL_REQUIRE(params_.strikeLow < params_.strikeUp,
               "LvDigitalAccrualFdBuehlerOption: strikeLow must be strictly below strikeUp");
    QL_REQUIRE(!params_.observationDates.empty(),
               "LvDigitalAccrualFdBuehlerOption: observationDates (accrual expiries) must be non-empty");

    const Real strikeLowS = params_.strikeLow;
    const Real strikeUpS = params_.strikeUp;

    Real sumX = 0.0;
    Real sumS = 0.0;
    Size nLegs = 0;

    for (const Date& expiry : params_.observationDates) {
        QL_REQUIRE(expiry > buehler.today(),
                   "LvDigitalAccrualFdBuehlerOption: each observation date must be after today");
        const Real kxLow = pureXStrikeFromSpot(buehler, expiry, strikeLowS);
        const Real kxUp = pureXStrikeFromSpot(buehler, expiry, strikeUpS);
        QL_REQUIRE(kxLow > 0.0 && kxUp > 0.0,
                   "LvDigitalAccrualFdBuehlerOption: pure-X strikes must be positive");
        QL_REQUIRE(kxLow < kxUp,
                   "LvDigitalAccrualFdBuehlerOption: mapped kx(Low) must be below kx(Up) at " << expiry);

        const Real digitalLowX = priceCashDigitalCallInX(buehler, expiry, strikeLowS);
        const Real digitalUpX = priceCashDigitalCallInX(buehler, expiry, strikeUpS);
        const Real legX = digitalLowX - digitalUpX;
        sumX += legX;

        const Real discount = buehler.riskFreeTs()->discount(expiry);
        sumS += discount * legX;
        ++nLegs;
    }

    QL_REQUIRE(nLegs > 0, "LvDigitalAccrualFdBuehlerOption: no valid observation dates");
    const Real invN = 1.0 / static_cast<Real>(nLegs);

    if (quoteSpace_ == BuehlerOptionPriceSpace::X) {
        return sumX * invN;
    }
    return sumS * invN;
}
