/**
 * @file fd_buehler_x_fdm.h
 * @brief Shared pure-X FD defaults and QuantLib FDM helpers.
 */

#ifndef FD_BUEHLER_X_FDM_H
#define FD_BUEHLER_X_FDM_H

#include <ql/quantlib.hpp>

class BuehlerModel;

constexpr QuantLib::Size kDefaultFdTGridPerYear = 100;
constexpr QuantLib::Size kDefaultFdXGrid = 200;
/** Asset-or-nothing FD refinement (cash/vanilla keep @c kDefaultFdXGrid / base steps). */
constexpr QuantLib::Size kFdAssetDigitalXGridMultiplier = 4;
constexpr QuantLib::Size kFdAssetDigitalTimeStepMultiplier = 2;
/** Time mesh: t_k = T * (k/N)^2 (fixed; not configurable). */

/** @brief @c max(1, ceil(tGridPerYear * T)). */
QuantLib::Size effectiveBuehlerFdTimeSteps(const BuehlerModel& buehler,
                                           const QuantLib::Date& expiry,
                                           QuantLib::Size tGridPerYear);

/** @brief Unit forward X process with @c fixedPureLocalVolTs. */
QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>
makeBuehlerPureXLocalVolProcess(const BuehlerModel& buehler);

/** @brief European FD NPV in X (Crank–Nicolson, w^2 rollback times). */
QuantLib::Real fdVanillaNPVInX(
    const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& process,
    const QuantLib::ext::shared_ptr<QuantLib::StrikedTypePayoff>& payoff,
    const QuantLib::ext::shared_ptr<QuantLib::Exercise>& exercise,
    QuantLib::Size xGrid,
    QuantLib::Size timeSteps,
    QuantLib::Real mesherStrikeX,
    QuantLib::Real xMinConstraint = QuantLib::Null<QuantLib::Real>(),
    QuantLib::Real xMaxConstraint = QuantLib::Null<QuantLib::Real>());

#endif
