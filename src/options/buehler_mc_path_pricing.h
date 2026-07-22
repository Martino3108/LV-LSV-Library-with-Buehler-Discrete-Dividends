/**
 * @file buehler_mc_path_pricing.h
 * @brief Shared MC statistics on BuehlerFixingSavePath payoffs.
 */

#ifndef BUEHLER_MC_PATH_PRICING_H
#define BUEHLER_MC_PATH_PRICING_H

#include <ql/quantlib.hpp>
#include <vector>
#include <vector>

struct BuehlerMcPathPricingResult {
    QuantLib::Real value = 0.0;
    QuantLib::Real errorEstimate = QuantLib::Null<QuantLib::Real>();
};

/** @brief Mean (and stderr of the mean) across independent MC sub-bank prices. */
struct McSubbankAccumulator {
    void add(const BuehlerMcPathPricingResult& result);
    BuehlerMcPathPricingResult finish() const;

private:
    std::vector<QuantLib::Real> values_;
    std::vector<QuantLib::Real> withinSubbankStderr_;
};

BuehlerMcPathPricingResult buehlerMcStatsFromPayoffs(const std::vector<QuantLib::Real>& payoffs,
                                                     QuantLib::Real valueScale);

#endif
