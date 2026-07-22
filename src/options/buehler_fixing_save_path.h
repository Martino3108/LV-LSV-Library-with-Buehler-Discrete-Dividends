/**
 * @file buehler_fixing_save_path.h
 * @brief MC path bank in pure X with affine map to S.
 */

#ifndef BUEHLER_FIXING_SAVE_PATH_H
#define BUEHLER_FIXING_SAVE_PATH_H

#include <ql/quantlib.hpp>
#include <vector>

/** @brief Bank paths are in X; map to S with @c sLevel. */
enum class SavePathCoordinate { X, S };

/** @brief Simulated LV paths at the save fixings; flat path-major storage. */
class BuehlerFixingSavePath {
public:
    BuehlerFixingSavePath() = default;
    /** @param xAtFixings flat buffer of size numPaths*numFixings, layout path-major:
     *         X of path @c p at fixing @c i lives at @c p*numFixings+i. */
    BuehlerFixingSavePath(std::vector<QuantLib::Date> fixingDates,
                          QuantLib::Size numPaths,
                          std::vector<QuantLib::Real> xAtFixings,
                          std::vector<QuantLib::Real> dividendCarryAtFixing,
                          std::vector<QuantLib::Real> slopeGAtFixing,
                          QuantLib::Size mcBrownianSteps = 0);

    BuehlerFixingSavePath(const BuehlerFixingSavePath&) = delete;
    BuehlerFixingSavePath& operator=(const BuehlerFixingSavePath&) = delete;
    BuehlerFixingSavePath(BuehlerFixingSavePath&&) = default;
    BuehlerFixingSavePath& operator=(BuehlerFixingSavePath&&) = default;
    ~BuehlerFixingSavePath() = default;

    SavePathCoordinate coordinate() const { return SavePathCoordinate::X; }
    const std::vector<QuantLib::Date>& fixingDates() const { return fixingDates_; }
    QuantLib::Size numPaths() const { return numPaths_; }
    QuantLib::Size numFixings() const { return fixingDates_.size(); }
    QuantLib::Size mcBrownianSteps() const { return mcBrownianSteps_; }

    QuantLib::Size fixingIndex(const QuantLib::Date& date) const;
    bool hasFixingDate(const QuantLib::Date& date) const;

    QuantLib::Real level(QuantLib::Size pathIndex, QuantLib::Size fixingIndex) const;
    QuantLib::Real level(QuantLib::Size pathIndex, const QuantLib::Date& date) const;
    QuantLib::Real xLevel(QuantLib::Size pathIndex, QuantLib::Size fixingIndex) const {
        QL_REQUIRE(pathIndex < numPaths_, "BuehlerFixingSavePath: path index out of range");
        QL_REQUIRE(fixingIndex < numFixings(), "BuehlerFixingSavePath: fixing index out of range");
        return xAtFixings_[pathIndex * numFixings() + fixingIndex];
    }
    /** @brief S = D(t) + G(t)·X with precomputed carry and slope. */
    QuantLib::Real sLevel(QuantLib::Size pathIndex, QuantLib::Size fixingIndex) const {
        return dividendCarryAtFixing_[fixingIndex] +
               slopeGAtFixing_[fixingIndex] * xLevel(pathIndex, fixingIndex);
    }

private:
    std::vector<QuantLib::Date> fixingDates_;
    QuantLib::Size numPaths_ = 0;
    std::vector<QuantLib::Real> xAtFixings_;
    std::vector<QuantLib::Real> dividendCarryAtFixing_;
    std::vector<QuantLib::Real> slopeGAtFixing_;
    QuantLib::Size mcBrownianSteps_ = 0;
};

#endif
