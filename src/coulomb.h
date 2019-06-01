#pragma once

#include <string>
#include <limits>
#include <cmath>
#include <Eigen/Core>

namespace CoulombGalore {

typedef Eigen::Vector3d Point; //!< typedef for 3d vector

constexpr double infty = std::numeric_limits<double>::infinity(); //!< Numerical infinity

/**
 * Base class for truncation schemes
 */
class SchemeBase {
  public:
    enum class TruncationScheme { plain, ewald, wolf, poisson, qpotential };
    std::string doi;                  //!< DOI for original citation
    std::string name;                 //!< Descriptive name
    TruncationScheme scheme;          //!< Truncation scheme
    double cutoff;                    //!< Cut-off distance
    double self_energy_prefactor = 0; //!< Prefactor for self-energy
    inline SchemeBase(TruncationScheme scheme, double cutoff) : scheme(scheme), cutoff(cutoff) {}

    /**
     * @brief Splitting function
     * @param q q=r/Rcutoff
     * @todo How should this be expanded to higher order moments?
     */
    virtual double splitting_function(double) const = 0;
};

/**
 * @brief Class for calculation of interaction energies
 */
template <class Tscheme> class PairPotential : public Tscheme {
  private:
    double invcutoff; // inverse cutoff distance

  public:
    using Tscheme::cutoff;
    using Tscheme::self_energy_prefactor;
    using Tscheme::splitting_function;

    template <class... Args> PairPotential(Args &&... args) : Tscheme(args...) { invcutoff = 1.0 / cutoff; }

    /**
     * @brief ion-ion interaction energy
     * @returns interaction energy in electrostatic units
     * @param zz charge product
     * @param r charge separation
     */
    inline double ion_ion(double zz, double r) {
        if (r < cutoff)
            return zz / r * splitting_function(r * invcutoff);
        else
            return 0.0;
    }

    /**
     * @param zz charge product
     * @returns self energy in electrostatic units
     * @param mumu product between dipole moment scalars
     */
    inline double self_energy(double zz, double mumu) const {
        return self_energy_prefactor * invcutoff * (zz + mumu * invcutoff * invcutoff);
    }

    // double dip_dip(...) // expand with higher order interactions...
    // double dielectricconstant ...
};

/**
 * @brief No truncation scheme
 */
struct Plain : public SchemeBase {
    inline Plain() : SchemeBase(TruncationScheme::plain, infty){};
    inline double splitting_function(double q) const override { return 1.0; };
};

#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("[CoulombGalore] plain") {
    using doctest::Approx;
    double zz = 2.0 * 2.0; // charge product
    Point r = {10, 0, 0};  // distance vector

    PairPotential<Plain> pot;
    CHECK(pot.splitting_function(0.5) == Approx(1.0));
    CHECK(pot.ion_ion(zz, r.norm()) == Approx(zz / r.norm()));
}
#endif

/**
 * @brief Help-function for the q-potential in class CoulombGalore
 *
 * More information here: http://mathworld.wolfram.com/q-PochhammerSymbol.html
 * P = 300 gives an error of about 10^-17 for k < 4
 */
inline double qPochhammerSymbol(double q, int k = 1, int P = 300) {
    double value = 1.0;
    double temp = std::pow(q, k);
    for (int i = 0; i < P; i++) {
        value *= (1.0 - temp);
        temp *= q;
    }
    return value;
}

#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("qPochhammerSymbol") {
    double q = 0.5;
    CHECK(qPochhammerSymbol(q, 0, 0) == 1);
    CHECK(qPochhammerSymbol(0, 0, 1) == 0);
    CHECK(qPochhammerSymbol(1, 0, 1) == 0);
    CHECK(qPochhammerSymbol(1, 1, 2) == 0);
}
#endif

/**
 * @brief qPotential scheme
 */
struct qPotential : public SchemeBase {
    double order; //!< Number of moment to cancel

    /**
     * @param cutoff distance cutoff
     * @param order number of moments to cancel
     */
    inline qPotential(double cutoff, double order) : SchemeBase(TruncationScheme::qpotential, cutoff), order(order) {
        name = "qpotential";
        self_energy_prefactor = -1;
    }

    inline double splitting_function(double q) const override { return qPochhammerSymbol(q, 1, order); }
};

#ifdef DOCTEST_LIBRARY_INCLUDED
TEST_CASE("[CoulombGalore] qPotential") {
    using doctest::Approx;
    double cutoff = 18.0;  // cutoff distance
    double zz = 2.0 * 2.0; // charge product
    Point r = {10, 0, 0};  // distance vector

    PairPotential<qPotential> pot(cutoff, 3);
    CHECK(pot.splitting_function(0.5) == Approx(0.328125));
    CHECK(pot.ion_ion(zz, cutoff) == Approx(0));
    CHECK(pot.ion_ion(zz, r.norm()) == Approx(0.1018333173));
}
#endif

} // namespace CoulombGalore
