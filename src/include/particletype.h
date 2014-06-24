/*
 *    Copyright (c) 2012-2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 */
#ifndef SRC_INCLUDE_PARTICLETYPE_H_
#define SRC_INCLUDE_PARTICLETYPE_H_

#include "forwarddeclarations.h"
#include "macros.h"
#include "pdgcode.h"

#include <string>

namespace Smash {

/**
 * Particle type contains the static properties of a particle
 *
 * Before creation of Experiment, SMASH initializes the list of particles
 * (\ref list_all). After construction these values are immutable.
 *
 * The list of particles is stored in such a way that look up of a ParticleType
 * object (\ref find) is efficient for PDG codes (\f$\mathcal O(\log N)\f$).
 */
class ParticleType {
 public:
  /**
   * Creates a fully initialized ParticleType object.
   *
   * \param n The name of the particle (only used for debug output).
   * \param m The mass of the particle.
   * \param w The width of the particle.
   * \param id The PDG code of the particle.
   *
   * \note The remaining properties ParticleType provides are derived from the
   *       PDG code and therefore cannot be set explicitly (this avoids the
   *       chance of introducing inconsistencies).
   */
  ParticleType(std::string n, float m, float w, PdgCode id)
      : name_(n),
        mass_(m),
        width_(w),
        pdgcode_(id),
        isospin_(pdgcode_.isospin_total()),
        charge_(pdgcode_.charge())
        {}

  /// Returns the name of the particle (for debug output only).
  const std::string &name() const { return name_; }

  /// Returns the particle mass.
  float mass() const { return mass_; }

  /// Returns the particle width (at the mass pole).
  float width_at_pole() const { return width_; }

  /// Returns the PDG code of the particle.
  PdgCode pdgcode() const { return pdgcode_; }

  /// \copydoc PdgCode::isospin_total
  int isospin() const { return isospin_; }

  /// \copydoc PdgCode::charge
  int charge() const { return charge_; }

  /// \copydoc PdgCode::spin
  int spin() const { return pdgcode_.spin(); }

  /// \copydoc PdgCode::is_hadron
  bool is_hadron() const { return pdgcode_.is_hadron(); }

  /// Check if the particle is stable
  inline bool is_stable() const;

  /**
   * The minimum mass of the resonance.
   *
   * Calculate the minimum rest energy the resonance must have
   * for any decay channel to be kinematically available.
   * (In other words, find the smallest sum of final-state particle masses.)
   *
   * \return The minimum mass that a particle of this type can assume.
   */
  float minimum_mass() const;

  /**
   * Get the mass-dependent partial width of a particle with mass m
   * in a particular decay mode.
   *
   * \param m Invariant mass of the decaying particle.
   * \param mode Decay mode to consider.
   */
  float partial_width(const float m, const DecayBranch &mode) const;

  /**
   * Get the mass-dependent total width of a particle with mass m.
   *
   * \param m Invariant mass of the decaying particle.
   */
  float total_width(const float m) const;

  /**
   * Get the mass-dependent partial widths of a particle with mass m.
   * Returns a list of process branches, whose weights correspond to the
   * actual partial widths.
   *
   * \param m Invariant mass of the decaying particle.
   */
  ProcessBranchList get_partial_widths(const float m) const;

  /**
   * Returns a list of all ParticleType objects.
   *
   * \note The order of the list may be sorted by PDG codes, but do not rely on
   *       this.
   */
  static const ParticleTypeList &list_all();

  /**
   * Returns the ParticleType object for the given \p pdgcode.
   *
   * \note The complexity of the search is \f$\mathcal O(\log N)\f$.
   */
  static const ParticleType &find(PdgCode pdgcode) SMASH_CONST;

  /**
   * Returns whether the ParticleType with the given \p pdgcode exists.
   *
   * \note The complexity of the search is \f$\mathcal O(\log N)\f$.
   */
  static bool exists(PdgCode pdgcode) SMASH_CONST;

  /**
   * Initialize the global ParticleType list (list_all) from the given input
   * data.
   *
   * \param particles A string that contains the definition of ParticleTypes to
   *                  be created.
   */
  static void create_type_list(const std::string &particles);

 private:
  /// name of the particle
  /// \todo This variable is only used for debug output. Maybe `ifdef` it out
  ///       for non-debug builds to save the memory?
  std::string name_;
  /// mass of the particle
  float mass_;
  /// width of the particle
  float width_;
  /// PDG Code of the particle
  PdgCode pdgcode_;
  /** twice the isospin of the particle
   *
   * This is filled automatically from pdgcode_.
   */
  int isospin_;
  /** charge of the particle
   *
   * This is filled automatically from pdgcode_.
   */
  int charge_;
};

inline bool ParticleType::is_stable() const {
  /* We currently regard a particle type as stable if its on-shell width is
   * less than 10 keV. */
  return width_<1E-5;
}

}  // namespace Smash

#endif  // SRC_INCLUDE_PARTICLETYPE_H_
