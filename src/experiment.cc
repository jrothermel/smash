/*
 *
 *    Copyright (c) 2012-2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */


#include <algorithm>
#include <cinttypes>
#include <cstdlib>
#include <list>
#include <string>
#include <vector>

#include "include/action.h"
#include "include/boxmodus.h"
#include "include/clock.h"
#include "include/collidermodus.h"
#include "include/configuration.h"
#include "include/experiment.h"
#include "include/forwarddeclarations.h"
#include "include/logging.h"
#include "include/macros.h"
#include "include/nucleusmodus.h"
#include "include/outputroutines.h"
#include "include/random.h"
#include "include/spheremodus.h"

namespace std {
/**
 * Print time span in a human readable way:
 * time < 10 min => seconds
 * 10 min < time < 3 h => minutes
 * time > 3h => hours
 *
 * \note This operator has to be in the \c std namespace for argument dependent
 * lookup to find it. If it were in the Smash namespace then the code would not
 * compile since none of its arguments is a type from the Smash namespace.
 */
template <typename T, typename Ratio>
static ostream &operator<<(ostream &out,
                           const chrono::duration<T, Ratio> &seconds) {
  using namespace chrono;
  using Seconds = duration<float>;
  using Minutes = duration<float, std::ratio<60>>;
  using Hours = duration<float, std::ratio<60 * 60>>;
  constexpr Minutes threshold_for_minutes{10};
  constexpr Hours threshold_for_hours{3};
  if (seconds < threshold_for_minutes) {
    return out << Seconds(seconds).count() << " [s]";
  }
  if (seconds < threshold_for_hours) {
    return out << Minutes(seconds).count() << " [min]";
  }
  return out << Hours(seconds).count() << " [h]";
}
}  // namespace std

namespace Smash {

/* ExperimentBase carries everything that is needed for the evolution */
std::unique_ptr<ExperimentBase> ExperimentBase::create(Configuration config) {
  const auto &log = logger<LogArea::Experiment>();
  log.trace() << source_location;
  const std::string modus_chooser = config.take({"General", "MODUS"});
  log.info() << "Modus for this calculation: " << modus_chooser;

  // remove config maps of unused Modi
  config["Modi"].remove_all_but(modus_chooser);

  typedef std::unique_ptr<ExperimentBase> ExperimentPointer;
  if (modus_chooser.compare("Box") == 0) {
    return ExperimentPointer(new Experiment<BoxModus>(config));
  } else if (modus_chooser.compare("Collider") == 0) {
    return ExperimentPointer(new Experiment<ColliderModus>(config));
  } else if (modus_chooser.compare("Nucleus") == 0) {
      return ExperimentPointer(new Experiment<NucleusModus>(config));
  } else if (modus_chooser.compare("Sphere") == 0) {
      return ExperimentPointer(new Experiment<SphereModus>(config));
  } else {
    throw InvalidModusRequest("Invalid Modus (" + modus_chooser +
                              ") requested from ExperimentBase::create.");
  }
}

namespace {
/** Gathers all general Experiment parameters
 *
 * \param[in, out] config Configuration element
 * \return The ExperimentParameters struct filled with values from the
 * Configuration
 */
ExperimentParameters create_experiment_parameters(Configuration config) {
  const auto &log = logger<LogArea::Experiment>();
  log.trace() << source_location;
  const int testparticles = config.take({"General", "TESTPARTICLES"});
  float cross_section = config.take({"General", "SIGMA"});

  /* reducing cross section according to number of test particle */
  if (testparticles > 1) {
    log.info() << "IC test particle: " << testparticles;
    cross_section /= testparticles;
    log.info() << "Elastic cross section: " << cross_section << " mb";
  }

  // The clock initializers are only read here and taken later when
  // assigning initial_clock_.
  return {{0.0f, config.read({"General", "DELTA_TIME"})},
           config.take({"General", "OUTPUT_INTERVAL"}),
           cross_section, testparticles};
}
}  // unnamed namespace

/**
 * Creates a verbose textual description of the setup of the Experiment.
 */
template <typename Modus>
std::ostream &operator<<(std::ostream &out, const Experiment<Modus> &e) {
  out << "Elastic cross section: " << e.parameters_.cross_section << " mb\n";
  out << "Starting with temporal stepsize: " << e.parameters_.timestep_duration()
      << " fm/c\n";
  out << "End time: " << e.end_time_ << " fm/c\n";
  out << e.modus_;
  return out;
}

template <typename Modus>
Experiment<Modus>::Experiment(Configuration config)
    : parameters_(create_experiment_parameters(config)),
      modus_(config["Modi"], parameters_),
      particles_(),
      decay_finder_(parameters_),
      scatter_finder_(parameters_),
      nevents_(config.take({"General", "NEVENTS"})),
      end_time_(config.take({"General", "END_TIME"})),
      delta_time_startup_(config.take({"General", "DELTA_TIME"})) {
  const auto &log = logger<LogArea::Experiment>();
  int64_t seed_ = config.take({"General", "RANDOMSEED"});
  if (seed_ < 0) {
    seed_ = time(nullptr);
  }
  Random::set_seed(seed_);
  log.info() << "Random number seed: " << seed_;
  log.info() << *this;
}

/* This method reads the particle type and cross section information
 * and does the initialization of the system (fill the particles map)
 */
template <typename Modus>
void Experiment<Modus>::initialize_new_event() {
  const auto &log = logger<LogArea::Experiment>();
  particles_.reset();

  /* Sample particles according to the initial conditions */
  float start_time = modus_.initial_conditions(&particles_, parameters_);

  // reset the clock:
  Clock clock_for_this_event(start_time, delta_time_startup_);
  parameters_.labclock = std::move(clock_for_this_event);

  /* Save the initial conserved quantum numbers and total momentum in
   * the system for conservation checks */
  conserved_initial_.count_conserved_values(particles_);
  /* Print output headers */
  log.info() << "--------------------------------------------------------------------------------";
  log.info() << " Time       <Ediff>      <pdiff>  <scattrate>    <scatt>  <particles>   <timing>";
  log.info() << "--------------------------------------------------------------------------------";
}

static std::string format_measurements(const Particles &particles,
                                       size_t scatterings_total,
                                       size_t scatterings_this_interval,
                                       const QuantumNumbers &conserved_initial,
                                       SystemTimePoint time_start,
                                       double time) {
  SystemTimeSpan elapsed_seconds = SystemClock::now() - time_start;

  QuantumNumbers current_values(particles);
  QuantumNumbers difference = conserved_initial - current_values;

  std::string line(81, ' ');
  if (likely(time > 0))
    snprintf(&line[0], line.size(), "%6.2f %12g %12g %12g %10zu %12zu %12g",
             time, difference.momentum().x0(), difference.momentum().abs3(),
             scatterings_total * 2 / (particles.size() * time),
             scatterings_this_interval, particles.size(),
             elapsed_seconds.count());
  else
    snprintf(&line[0], line.size(), "%+6.2f %12g %12g %12g %10i %12zu %12g",
             time, difference.momentum().x0(), difference.momentum().abs3(),
             0.0, 0, particles.size(), elapsed_seconds.count());
  return line;
}

/* This is the loop over timesteps, carrying out collisions and decays
 * and propagating particles. */
template <typename Modus>
void Experiment<Modus>::run_time_evolution(const int evt_num) {
  const auto &log = logger<LogArea::Experiment>();
  modus_.sanity_check(&particles_);
  size_t interactions_total = 0, previous_interactions_total = 0,
         interactions_this_interval = 0;
  log.info() << format_measurements(
      particles_, interactions_total, interactions_this_interval,
      conserved_initial_, time_start_, parameters_.labclock.current_time());

  while (!(++parameters_.labclock > end_time_)) {
    std::vector<ActionPtr> actions;  // XXX: a std::list might be better suited
                                     // for the task: lots of appending, then
                                     // sorting and finally a single linear
                                     // iteration

    /* (1.a) Find possible decays. */
    actions += decay_finder_.find_possible_actions(&particles_);
    /* (1.b) Find possible collisions. */
    actions += scatter_finder_.find_possible_actions(&particles_);
    /* (1.c) Sort action list by time. */
    std::sort(actions.begin(), actions.end(),
              [](const ActionPtr &a, const ActionPtr &b) { return *a < *b; });

    /* (2.a) Perform actions. */
    if (!actions.empty()) {
      for (const auto &action : actions) {
        if (action->is_valid(particles_)) {
          const ParticleList incoming_particles = action->incoming_particles();
          action->perform(&particles_, interactions_total);
          const ParticleList outgoing_particles = action->outgoing_particles();
          for (const auto &output : outputs_) {
            output->at_interaction(incoming_particles, outgoing_particles);
          }
        }
      }
      actions.clear();
      log.trace() << source_location << " Action list done.";
    }

    /* (3) Do propagation. */
    modus_.propagate(&particles_, parameters_, outputs_);

    /* (4) Physics output during the run. */
    // if the timestep of labclock is different in the next tick than
    // in the current one, I assume it has been changed already. In that
    // case, I know what the next tick is and I can check whether the
    // output time is crossed within the next tick.
    if (parameters_.need_intermediate_output()) {
      interactions_this_interval =
          interactions_total - previous_interactions_total;
      previous_interactions_total = interactions_total;
      log.info() << format_measurements(
          particles_, interactions_total, interactions_this_interval,
          conserved_initial_, time_start_, parameters_.labclock.current_time());
      /* save evolution data */
      for (const auto &output : outputs_) {
        output->at_intermediate_time(particles_, evt_num, parameters_.labclock);
      }
    }
    // check conservation of conserved quantities:
    std::string err_msg = conserved_initial_.report_deviations(particles_);
    if (!err_msg.empty()) {
      log.error() << err_msg;
      throw std::runtime_error("Violation of conserved quantities!");
    }
  }
  // make sure the experiment actually ran (note: we should compare this
  // to the start time, but we don't know that. Therefore, we check that
  // the time is positive, which should heuristically be the same).
  if (likely(parameters_.labclock > 0)) {
    log.info() << "--------------------------------------------------------------------------------";
    log.info() << "Time real: " << SystemClock::now() - time_start_;
    /* if there are no particles no interactions happened */
    log.info() << "Final scattering rate: "
               << (particles_.empty() ? 0 : (interactions_total * 2 /
                                             particles_.time() /
                                             particles_.size())) << " [fm-1]";
  }
}

template <typename Modus>
void Experiment<Modus>::run() {
  for (int j = 0; j < nevents_; j++) {
    /* Sample initial particles, start clock, some printout and book-keeping */
    initialize_new_event();

    /* Output at event start */
    for (const auto &output : outputs_) {
      output->at_eventstart(particles_, j);
    }

    /* the time evolution of the relevant subsystem */
    run_time_evolution(j);

    /* Output at event end */
    for (const auto &output : outputs_) {
      output->at_eventend(particles_, j);
    }
  }
}

}  // namespace Smash
