/*
 *
 *    Copyright (c) 2014-2018
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#include "smash/scatteraction.h"

#include <cmath>

#include "Pythia8/Pythia.h"

#include "smash/angles.h"
#include "smash/constants.h"
#include "smash/crosssections.h"
#include "smash/cxx14compat.h"
#include "smash/fpenvironment.h"
#include "smash/kinematics.h"
#include "smash/logging.h"
#include "smash/parametrizations.h"
#include "smash/pdgcode.h"
#include "smash/potential_globals.h"
#include "smash/pow.h"
#include "smash/processstring.h"
#include "smash/random.h"

namespace smash {

ScatterAction::ScatterAction(const ParticleData &in_part_a,
                             const ParticleData &in_part_b, double time,
                             bool isotropic, double string_formation_time)
    : Action({in_part_a, in_part_b}, time),
      total_cross_section_(0.),
      isotropic_(isotropic),
      string_formation_time_(string_formation_time) {}

void ScatterAction::add_collision(CollisionBranchPtr p) {
  add_process<CollisionBranch>(p, collision_channels_, total_cross_section_);
}

void ScatterAction::add_collisions(CollisionBranchList pv) {
  add_processes<CollisionBranch>(std::move(pv), collision_channels_,
                                 total_cross_section_);
}

void ScatterAction::generate_final_state() {
  const auto &log = logger<LogArea::ScatterAction>();

  log.debug("Incoming particles: ", incoming_particles_);

  /* Decide for a particular final state. */
  const CollisionBranch *proc = choose_channel<CollisionBranch>(
      collision_channels_, total_cross_section_);
  process_type_ = proc->get_type();
  outgoing_particles_ = proc->particle_list();
  partial_cross_section_ = proc->weight();

  log.debug("Chosen channel: ", process_type_, outgoing_particles_);

  /* The production point of the new particles.  */
  FourVector middle_point = get_interaction_point();

  switch (process_type_) {
    case ProcessType::Elastic:
      /* 2->2 elastic scattering */
      elastic_scattering();
      break;
    case ProcessType::TwoToOne:
      /* resonance formation */
      resonance_formation();
      break;
    case ProcessType::TwoToTwo:
      /* 2->2 inelastic scattering */
      /* Sample the particle momenta in CM system. */
      inelastic_scattering();
      break;
    case ProcessType::StringSoftSingleDiffractiveAX:
    case ProcessType::StringSoftSingleDiffractiveXB:
    case ProcessType::StringSoftDoubleDiffractive:
    case ProcessType::StringSoftAnnihilation:
    case ProcessType::StringSoftNonDiffractive:
    case ProcessType::StringHard:
      string_excitation();
      break;
    default:
      throw InvalidScatterAction(
          "ScatterAction::generate_final_state: Invalid process type " +
          std::to_string(static_cast<int>(process_type_)) + " was requested. " +
          "(PDGcode1=" + incoming_particles_[0].pdgcode().string() +
          ", PDGcode2=" + incoming_particles_[1].pdgcode().string() + ")");
  }

  for (ParticleData &new_particle : outgoing_particles_) {
    // Boost to the computational frame
    new_particle.boost_momentum(
        -total_momentum_of_outgoing_particles().velocity());
    /* Set positions of the outgoing particles */
    if (proc->get_type() != ProcessType::Elastic) {
      new_particle.set_4position(middle_point);
    }
  }
}

void ScatterAction::add_all_scatterings(
    double elastic_parameter, bool two_to_one, ReactionsBitSet included_2to2,
    double low_snn_cut, bool strings_switch, bool use_AQM,
    bool strings_with_probability, NNbarTreatment nnbar_treatment) {
  CrossSections xs(incoming_particles_, sqrt_s(),
                   get_potential_at_interaction_point());
  CollisionBranchList processes = xs.generate_collision_list(
      elastic_parameter, two_to_one, included_2to2, low_snn_cut, strings_switch,
      use_AQM, strings_with_probability, nnbar_treatment, string_process_);

  /* Add various subprocesses.*/
  add_collisions(std::move(processes));

  /* If the string processes are not triggered by a probability, then they
   * always happen as long as the parametrized total cross section is larger
   * than the sum of the cross sections of the non-string processes, and the
   * square root s exceeds the threshold by at least 0.9 GeV. The cross section
   * of the string processes are counted by taking the difference between the
   * parametrized total and the sum of the non-strings. */
  if (!strings_with_probability &&
      xs.string_probability(strings_switch, strings_with_probability, use_AQM,
                            nnbar_treatment == NNbarTreatment::Strings) == 1.) {
    const double xs_diff = xs.high_energy() - cross_section();
    if (xs_diff > 0.) {
      add_collisions(xs.string_excitation(xs_diff, string_process_, use_AQM));
    }
  }
}

double ScatterAction::get_total_weight() const {
  return total_cross_section_ * incoming_particles_[0].xsec_scaling_factor() *
         incoming_particles_[1].xsec_scaling_factor();
}

double ScatterAction::get_partial_weight() const {
  return partial_cross_section_ * incoming_particles_[0].xsec_scaling_factor() *
         incoming_particles_[1].xsec_scaling_factor();
}

ThreeVector ScatterAction::beta_cm() const {
  return total_momentum().velocity();
}

double ScatterAction::gamma_cm() const {
  return (1. / std::sqrt(1.0 - beta_cm().sqr()));
}

double ScatterAction::mandelstam_s() const { return total_momentum().sqr(); }

double ScatterAction::cm_momentum() const {
  const double m1 = incoming_particles_[0].effective_mass();
  const double m2 = incoming_particles_[1].effective_mass();
  return pCM(sqrt_s(), m1, m2);
}

double ScatterAction::cm_momentum_squared() const {
  const double m1 = incoming_particles_[0].effective_mass();
  const double m2 = incoming_particles_[1].effective_mass();
  return pCM_sqr(sqrt_s(), m1, m2);
}

double ScatterAction::transverse_distance_sqr() const {
  // local copy of particles (since we need to boost them)
  ParticleData p_a = incoming_particles_[0];
  ParticleData p_b = incoming_particles_[1];
  /* Boost particles to center-of-momentum frame. */
  const ThreeVector velocity = beta_cm();
  p_a.boost(velocity);
  p_b.boost(velocity);
  const ThreeVector pos_diff =
      p_a.position().threevec() - p_b.position().threevec();
  const ThreeVector mom_diff =
      p_a.momentum().threevec() - p_b.momentum().threevec();

  const auto &log = logger<LogArea::ScatterAction>();
  log.debug("Particle ", incoming_particles_,
            " position difference [fm]: ", pos_diff,
            ", momentum difference [GeV]: ", mom_diff);

  const double dp2 = mom_diff.sqr();
  const double dr2 = pos_diff.sqr();
  /* Zero momentum leads to infite distance. */
  if (dp2 < really_small) {
    return dr2;
  }
  const double dpdr = pos_diff * mom_diff;

  /** UrQMD squared distance criterion:
   * \iref{Bass:1998ca} (3.27): in center of momentum frame
   * position of particle a: x_a
   * position of particle b: x_b
   * momentum of particle a: p_a
   * momentum of particle b: p_b
   * d^2_{coll} = (x_a - x_b)^2 - ((x_a - x_b) . (p_a - p_b))^2 / (p_a - p_b)^2
   */
  return dr2 - dpdr * dpdr / dp2;
}

/**
 * Computes the B coefficients from the Cugnon parametrization of the angular
 * distribution in elastic pp scattering.
 *
 * See equation (8) in \iref{Cugnon:1996kh}.
 * Note: The original Cugnon parametrization is only applicable for
 * plab < 6 GeV and keeps rising above that. We add an upper limit of b <= 9,
 * in order to be compatible with high-energy data (up to plab ~ 25 GeV).
 *
 * \param[in] plab Lab momentum in GeV.
 *
 * \return Cugnon B coefficient for elatic proton-proton scatterings.
 */
static double Cugnon_bpp(double plab) {
  if (plab < 2.) {
    double p8 = pow_int(plab, 8);
    return 5.5 * p8 / (7.7 + p8);
  } else {
    return std::min(9.0, 5.334 + 0.67 * (plab - 2.));
  }
}

/**
 * Computes the B coefficients from the Cugnon parametrization of the angular
 * distribution in elastic np scattering.
 *
 * See equation (10) in \iref{Cugnon:1996kh}.
 *
 * \param[in] plab Lab momentum in GeV.
 *
 * \return Cugnon B coefficient for elastic proton-neutron scatterings.
 */
static double Cugnon_bnp(double plab) {
  if (plab < 0.225) {
    return 0.;
  } else if (plab < 0.6) {
    return 16.53 * (plab - 0.225);
  } else if (plab < 1.6) {
    return -1.63 * plab + 7.16;
  } else {
    return Cugnon_bpp(plab);
  }
}

void ScatterAction::sample_angles(std::pair<double, double> masses,
                                  double kinetic_energy_cm) {
  if (is_string_soft_process(process_type_) ||
      (process_type_ == ProcessType::StringHard)) {
    // We potentially have more than two particles, so the following angular
    // distributions don't work. Instead we just keep the angular
    // distributions generated by string fragmentation.
    return;
  }
  assert(outgoing_particles_.size() == 2);
  const auto &log = logger<LogArea::ScatterAction>();

  // NN scattering is anisotropic currently
  const bool nn_scattering = incoming_particles_[0].type().is_nucleon() &&
                             incoming_particles_[1].type().is_nucleon();
  /* Elastic process is anisotropic and
   * the angular distribution is based on the NN elastic scattering. */
  const bool el_scattering = process_type_ == ProcessType::Elastic;

  const double mass_in_a = incoming_particles_[0].effective_mass();
  const double mass_in_b = incoming_particles_[1].effective_mass();

  ParticleData *p_a = &outgoing_particles_[0];
  ParticleData *p_b = &outgoing_particles_[1];

  const double mass_a = masses.first;
  const double mass_b = masses.second;

  const std::array<double, 2> t_range = get_t_range<double>(
      kinetic_energy_cm, mass_in_a, mass_in_b, mass_a, mass_b);
  Angles phitheta;
  if (el_scattering && !isotropic_) {
    /** NN → NN: Choose angular distribution according to Cugnon
     * parametrization,
     * see \iref{Cugnon:1996kh}. */
    double mandelstam_s_new = 0.;
    if (nn_scattering) {
      mandelstam_s_new = mandelstam_s();
    } else {
      /* In the case of elastic collisions other than NN collisions,
       * there is an ambiguity on how to get the lab-frame momentum (plab),
       * since the incoming particles can have different masses.
       * Right now, we first obtain the center-of-mass momentum
       * of the collision (pcom_now).
       * Then, the lab-frame momentum is evaluated from the mandelstam s,
       * which yields the original center-of-mass momentum
       * when nucleon mass is assumed. */
      const double pcm_now = pCM_from_s(mandelstam_s(), mass_in_a, mass_in_b);
      mandelstam_s_new =
          4. * std::sqrt(pcm_now * pcm_now + nucleon_mass * nucleon_mass);
    }
    double bb, a, plab = plab_from_s(mandelstam_s_new);
    if (nn_scattering &&
        p_a->pdgcode().antiparticle_sign() ==
            p_b->pdgcode().antiparticle_sign() &&
        std::abs(p_a->type().charge() + p_b->type().charge()) == 1) {
      // proton-neutron and antiproton-antineutron
      bb = std::max(Cugnon_bnp(plab), really_small);
      a = (plab < 0.8) ? 1. : 0.64 / (plab * plab);
    } else {
      /* all others including pp, nn and AQM elastic processes
       * This is applied for all particle pairs, which are allowed to
       * interact elastically. */
      bb = std::max(Cugnon_bpp(plab), really_small);
      a = 1.;
    }
    double t = random::expo(bb, t_range[0], t_range[1]);
    if (random::canonical() > 1. / (1. + a)) {
      t = t_range[0] + t_range[1] - t;
    }
    // determine scattering angles in center-of-mass frame
    phitheta = Angles(2. * M_PI * random::canonical(),
                      1. - 2. * (t - t_range[0]) / (t_range[1] - t_range[0]));
  } else if (nn_scattering && p_a->pdgcode().is_Delta() &&
             p_b->pdgcode().is_nucleon() &&
             p_a->pdgcode().antiparticle_sign() ==
                 p_b->pdgcode().antiparticle_sign() &&
             !isotropic_) {
    /** NN → NΔ: Sample scattering angles in center-of-mass frame from an
     * anisotropic angular distribution, using the same distribution as for
     * elastic pp scattering, as suggested in \iref{Cugnon:1996kh}. */
    const double plab = plab_from_s(mandelstam_s());
    const double bb = std::max(Cugnon_bpp(plab), really_small);
    double t = random::expo(bb, t_range[0], t_range[1]);
    if (random::canonical() > 0.5) {
      t = t_range[0] + t_range[1] - t;  // symmetrize
    }
    phitheta = Angles(2. * M_PI * random::canonical(),
                      1. - 2. * (t - t_range[0]) / (t_range[1] - t_range[0]));
  } else if (nn_scattering && p_b->pdgcode().is_nucleon() && !isotropic_ &&
             (p_a->type().is_Nstar() || p_a->type().is_Deltastar())) {
    /** NN → NR: Fit to HADES data, see \iref{Agakishiev:2014wqa}. */
    const std::array<double, 4> p{1.46434, 5.80311, -6.89358, 1.94302};
    const double a = p[0] + mass_a * (p[1] + mass_a * (p[2] + mass_a * p[3]));
    /*  If the resonance is so heavy that the index "a" exceeds 30,
     *  the power function turns out to be too sharp. Take t directly to be
     *  t_0 in such a case. */
    double t = t_range[0];
    if (a < 30) {
      t = random::power(-a, t_range[0], t_range[1]);
    }
    if (random::canonical() > 0.5) {
      t = t_range[0] + t_range[1] - t;  // symmetrize
    }
    phitheta = Angles(2. * M_PI * random::canonical(),
                      1. - 2. * (t - t_range[0]) / (t_range[1] - t_range[0]));
  } else {
    /* isotropic angular distribution */
    phitheta.distribute_isotropically();
  }

  ThreeVector pscatt = phitheta.threevec();
  // 3-momentum of first incoming particle in center-of-mass frame
  ThreeVector pcm =
      incoming_particles_[0].momentum().LorentzBoost(beta_cm()).threevec();
  pscatt.rotate_z_axis_to(pcm);

  // final-state CM momentum
  const double p_f = pCM(kinetic_energy_cm, mass_a, mass_b);
  if (!(p_f > 0.0)) {
    log.warn("Particle: ", p_a->pdgcode(), " radial momentum: ", p_f);
    log.warn("Etot: ", kinetic_energy_cm, " m_a: ", mass_a, " m_b: ", mass_b);
  }
  p_a->set_4momentum(mass_a, pscatt * p_f);
  p_b->set_4momentum(mass_b, -pscatt * p_f);

  /* Debug message is printed before boost, so that p_a and p_b are
   * the momenta in the center of mass frame and thus opposite to
   * each other.*/
  log.debug("p_a: ", *p_a, "\np_b: ", *p_b);
}

void ScatterAction::elastic_scattering() {
  // copy initial particles into final state
  outgoing_particles_[0] = incoming_particles_[0];
  outgoing_particles_[1] = incoming_particles_[1];
  // resample momenta
  sample_angles({outgoing_particles_[0].effective_mass(),
                 outgoing_particles_[1].effective_mass()},
                sqrt_s());
}

void ScatterAction::inelastic_scattering() {
  // create new particles
  sample_2body_phasespace();
  /* Set the formation time of the 2 particles to the larger formation time of
   * the incoming particles, if it is larger than the execution time;
   * execution time is otherwise taken to be the formation time */
  const double t0 = incoming_particles_[0].formation_time();
  const double t1 = incoming_particles_[1].formation_time();

  const size_t index_tmax = (t0 > t1) ? 0 : 1;
  const double form_time_begin =
      incoming_particles_[index_tmax].begin_formation_time();
  const double sc =
      incoming_particles_[index_tmax].initial_xsec_scaling_factor();
  if (t0 > time_of_execution_ || t1 > time_of_execution_) {
    /* The newly produced particles are supposed to continue forming exactly
     * like the latest forming ingoing particle. Therefore the details on the
     * formation are adopted. The initial cross section scaling factor of the
     * incoming particles is considered to also be the scaling factor of the
     * newly produced outgoing particles.
     */
    outgoing_particles_[0].set_slow_formation_times(form_time_begin,
                                                    std::max(t0, t1));
    outgoing_particles_[1].set_slow_formation_times(form_time_begin,
                                                    std::max(t0, t1));
    outgoing_particles_[0].set_cross_section_scaling_factor(sc);
    outgoing_particles_[1].set_cross_section_scaling_factor(sc);
  } else {
    outgoing_particles_[0].set_formation_time(time_of_execution_);
    outgoing_particles_[1].set_formation_time(time_of_execution_);
  }
}

void ScatterAction::resonance_formation() {
  const auto &log = logger<LogArea::ScatterAction>();

  if (outgoing_particles_.size() != 1) {
    std::string s =
        "resonance_formation: "
        "Incorrect number of particles in final state: ";
    s += std::to_string(outgoing_particles_.size()) + " (";
    s += incoming_particles_[0].pdgcode().string() + " + ";
    s += incoming_particles_[1].pdgcode().string() + ")";
    throw InvalidResonanceFormation(s);
  }
  // Set the momentum of the formed resonance in its rest frame.
  outgoing_particles_[0].set_4momentum(
      total_momentum_of_outgoing_particles().abs(), 0., 0., 0.);
  /* Set the formation time of the resonance to the larger formation time of the
   * incoming particles, if it is larger than the execution time; execution time
   * is otherwise taken to be the formation time */
  const double t0 = incoming_particles_[0].formation_time();
  const double t1 = incoming_particles_[1].formation_time();

  const size_t index_tmax = (t0 > t1) ? 0 : 1;
  const double begin_form_time =
      incoming_particles_[index_tmax].begin_formation_time();
  const double sc =
      incoming_particles_[index_tmax].initial_xsec_scaling_factor();
  if (t0 > time_of_execution_ || t1 > time_of_execution_) {
    outgoing_particles_[0].set_slow_formation_times(begin_form_time,
                                                    std::max(t0, t1));
    outgoing_particles_[0].set_cross_section_scaling_factor(sc);
  } else {
    outgoing_particles_[0].set_formation_time(time_of_execution_);
  }
  /* this momentum is evaluated in the computational frame. */
  log.debug("Momentum of the new particle: ",
            outgoing_particles_[0].momentum());
}

/* This function will generate outgoing particles in computational frame
 * from a hard process.
 * The way to excite soft strings is based on the UrQMD model */
void ScatterAction::string_excitation() {
  assert(incoming_particles_.size() == 2);
  const auto &log = logger<LogArea::Pythia>();
  // Disable floating point exception trap for Pythia
  {
    DisableFloatTraps guard;
    /* initialize the string_process_ object for this particular collision */
    string_process_->init(incoming_particles_, time_of_execution_);
    /* implement collision */
    bool success = false;
    int ntry = 0;
    const int ntry_max = 10000;
    while (!success && ntry < ntry_max) {
      ntry++;
      switch (process_type_) {
        case ProcessType::StringSoftSingleDiffractiveAX:
          /* single diffractive to A+X */
          success = string_process_->next_SDiff(true);
          break;
        case ProcessType::StringSoftSingleDiffractiveXB:
          /* single diffractive to X+B */
          success = string_process_->next_SDiff(false);
          break;
        case ProcessType::StringSoftDoubleDiffractive:
          /* double diffractive */
          success = string_process_->next_DDiff();
          break;
        case ProcessType::StringSoftNonDiffractive:
          /* soft non-diffractive */
          success = string_process_->next_NDiffSoft();
          break;
        case ProcessType::StringSoftAnnihilation:
          /* soft BBbar 2 mesonic annihilation */
          success = string_process_->next_BBbarAnn();
          break;
        case ProcessType::StringHard:
          success = string_process_->next_NDiffHard();
          break;
        default:
          log.error("Unknown string process required.");
          success = false;
      }
    }
    if (ntry == ntry_max) {
      /* If pythia fails to form a string, it is usually because the energy
       * is not large enough. In this case, an elastic scattering happens.
       *
       * Since particles are normally added after process selection for
       * strings, outgoing_particles is still uninitialized, and memory
       * needs to be allocated. We also shift the process_type_ to elastic
       * so that sample_angles does a proper treatment. */
      outgoing_particles_.reserve(2);
      outgoing_particles_.push_back(ParticleData{incoming_particles_[0]});
      outgoing_particles_.push_back(ParticleData{incoming_particles_[1]});
      process_type_ = ProcessType::Elastic;
      elastic_scattering();
    } else {
      outgoing_particles_ = string_process_->get_final_state();
      /* If the incoming particles already were unformed, the formation
       * times and cross section scaling factors need to be adjusted */
      const double tform_in = std::max(incoming_particles_[0].formation_time(),
                                       incoming_particles_[1].formation_time());
      if (tform_in > time_of_execution_) {
        const double fin =
            (incoming_particles_[0].formation_time() >
             incoming_particles_[1].formation_time())
                ? incoming_particles_[0].initial_xsec_scaling_factor()
                : incoming_particles_[1].initial_xsec_scaling_factor();
        for (size_t i = 0; i < outgoing_particles_.size(); i++) {
          const double tform_out = outgoing_particles_[i].formation_time();
          const double fout =
              outgoing_particles_[i].initial_xsec_scaling_factor();
          /* The new cross section scaling factor will be the product of the
           * cross section scaling factor of the ingoing particles and of the
           * outgoing ones (since the outgoing ones are also string fragments
           * and thus take time to form). */
          outgoing_particles_[i].set_cross_section_scaling_factor(fin * fout);
          /* If the unformed incoming particles' formation time is larger than
           * the current outgoing particle's formation time, then the latter
           * is overwritten by the former*/
          if (tform_in > tform_out) {
            outgoing_particles_[i].set_slow_formation_times(time_of_execution_,
                                                            tform_in);
          }
        }
      }
      /* Check momentum difference for debugging */
      FourVector out_mom;
      for (ParticleData data : outgoing_particles_) {
        out_mom += data.momentum();
      }
      log.debug("Incoming momenta string:", total_momentum());
      log.debug("Outgoing momenta string:", out_mom);
    }
  }
}

void ScatterAction::format_debug_output(std::ostream &out) const {
  out << "Scatter of " << incoming_particles_;
  if (outgoing_particles_.empty()) {
    out << " (not performed)";
  } else {
    out << " to " << outgoing_particles_;
  }
}

}  // namespace smash
