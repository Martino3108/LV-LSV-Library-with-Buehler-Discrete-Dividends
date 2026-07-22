/**
 * @file lookback_mc_buehler_option.h
 * @brief Discrete max lookback options on the Buehler MC save path (quoted in S only).
 *
 * Running maximum of @c sLevel on the observation schedule. Set @c params.observationDates
 * explicitly, or leave empty for the @c params.observationFrequency fallback (monthly
 * @c Following grid through @c params.expiry, or every save-path fixing when daily).
 * Fixed strike: @f$\max(0, \max S - K_S)@f$ (or put on @f$K_S - \max S@f$).
 * Floating strike: @f$\max(0, \max S - S_T)@f$ with @f$S_T@f$ at @c params.expiry.
 * Returns @f$P(0,T)\,\mathbb{E}[\mathrm{payoff}_S]@f$.
 */

#ifndef LOOKBACK_MC_BUEHLER_OPTION_H
#define LOOKBACK_MC_BUEHLER_OPTION_H

#include "buehler_fixing_save_path.h"
#include "buehler_mc_path_pricing.h"
#include "option.h"
#include <string>

class BuehlerModel;

enum class LookbackMcStrikeStyle { Fixed, Floating };

struct LookbackMcTwoPayoffs {
    BuehlerMcPathPricingResult fixed;
    BuehlerMcPathPricingResult floating;
};

/** @brief Max lookback from @c buehler.fixingSavePath() after @c simulateFixingPaths. */
class LookbackMcBuehlerOption : public Option {
public:
    explicit LookbackMcBuehlerOption(OptionContractParams params,
                                     LookbackMcStrikeStyle strikeStyle = LookbackMcStrikeStyle::Fixed);

    QuantLib::Real price(const BuehlerModel& buehler) const override;
    BuehlerMcPathPricingResult priceWithStdError(const BuehlerModel& buehler) const;
    LookbackMcTwoPayoffs priceAllPayoffs(const BuehlerModel& buehler) const;
    BuehlerOptionPriceSpace quotedPriceSpace() const override { return BuehlerOptionPriceSpace::S; }
    std::string scenarioExportBaseName() const override;

    static BuehlerMcPathPricingResult priceFromSavePath(const BuehlerFixingSavePath& savePath,
                                                        const OptionContractParams& params,
                                                        const BuehlerModel& buehler,
                                                        LookbackMcStrikeStyle strikeStyle);

    static LookbackMcTwoPayoffs priceAllPayoffsFromSavePath(const BuehlerFixingSavePath& savePath,
                                                            const OptionContractParams& params,
                                                            const BuehlerModel& buehler);

    LookbackMcStrikeStyle strikeStyle() const { return strikeStyle_; }

private:
    LookbackMcStrikeStyle strikeStyle_;
};

#endif
