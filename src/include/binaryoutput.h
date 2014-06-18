/*
 *
 *    Copyright (c) 2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 *
 */

#ifndef SRC_INCLUDE_BINARYOUTPUT_H_
#define SRC_INCLUDE_BINARYOUTPUT_H_

#include <string>

#include "outputinterface.h"
#include "filedeleter.h"
#include "forwarddeclarations.h"

/**
 * \brief SMASH output to binary file
 * ----------------------------------------------------------------------------
 * SMASH output to binary file is similar to OSCAR output,
 * but it is stored in a binary format. Suc format is faster
 * to read and write, but may be system architecture dependent.
 *
 * Binary file contains 
 * 1) General header:
 *   - SMASH version
 *   - Binary format version
 *   - List of variables stored in the file and their units
 * 2) Arbitrary number of data blocks in the following format:
 *   - block header: number of particles, number of event
 *   - N records (id, pdgid, t,x,y,z, p0,px,py,pz),
 *     where N = number of particles,
 *     id - particle unique id in SMASH simulation
 *     pdgid - particle PDG code
 *     t,x,y,z - space coordinates,
 *     p0,px,py,pz - 4-momenta
 *
 *  Output is performed every equal amount of time defined by option,
 *  at event start and at event end.
 *
 *  TODO: Options that make this output more configurable
 *
 *  Example of python script to read this output:
 *  \code
 *  import struct
 *  import numpy as np
 *  
 *  bfile = open("particles_binary.bin", "rb")
 *  
 *  # Read header
 *  particles_header_type = np.dtype('2i4')
 *  particle_data_type = np.dtype([('id','i4'),('pdgid','i4'),('r','d',4),('p','d',4)])
 *  
 *  magic, format_version, len = struct.unpack('4sii', bfile.read(12))
 *  smash_version, number_of_records, len = struct.unpack('%dsii' % len, bfile.read(len + 8))
 *  records = struct.unpack('%ds' % len, bfile.read(len))
 *  
 *  print "SMASH version: ", smash_version
 *  print "Format version: ", format_version
 *  print records
 *  
 *  dt_counter = 0
 *  prev_event_number = 0
 *  event_number = 0
 *  
 *  # Event and deltat cycle
 *  while True:
 *      try:
 *          pheader = np.fromfile(bfile, dtype=particles_header_type, count=1)
 *      except:
 *          print "Finished reading file <= exception caught"
 *          bfile.close()
 *          break
 *  
 *      if (not pheader.any()):
 *          break
 *  
 *      number_of_particles = pheader[0][0]
 *      event_number = pheader[0][1]
 *      if (event_number != prev_event_number):
 *          dt_counter = 0
 *  
 *      # Read all the particles at once
 *      particles = np.fromfile(bfile, dtype=particle_data_type, count=number_of_particles)
 *  
 *      # Find dt - output interval in fm/c
 *      t = particles["r"][0,0]
 *      if (event_number == 0 and dt_counter == 0):
 *          dt = t
 *      if (event_number ==0 and dt_counter == 1):
 *          dt = t - dt
 *  
 *      #Do some calculations here
 *
 *      dt_counter += 1
 *      prev_event_number = event_number
 *  
 *  print "Events read: ", event_number + 1
 *  print "Number of output moments per event: ", dt_counter
 *  print "Output dt = ", dt, "fm/c"
 *  print "Total time of one event: ", dt*(dt_counter - 1)
 *  \endcode
 **/

namespace Smash {

class BinaryOutput : public OutputInterface {
 public:
  explicit BinaryOutput(bf::path path);

  /// writes the initial particle information of an event
  void at_eventstart(const Particles &particles,
                     const int event_number) override;

  /// writes the final particle information of an event
  void at_eventend(const Particles &particles, const int event_number) override;

  void write_interaction(const ParticleList &incoming_particles,
                         const ParticleList &outgoing_particles) override;
  /// writes particles every time interval fixed by option OUTPUT_INTERVAL 
  void after_Nth_timestep(const Particles &particles, const int event_number,
                          const Clock &clock) override;

 private:
  void write(const std::string &s);
  void write(const FourVector &v);
  void write(std::int32_t x);
  void write(const Particles &particles, const int event_number);

  FilePtr file_;
};
}  // namespace Smash

#endif  // SRC_INCLUDE_BINARYOUTPUT_H_
