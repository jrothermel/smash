/*
 *    Copyright (c) 2012-2014
 *      SMASH Team
 *
 *    GNU General Public License (GPLv3 or later)
 */
#ifndef SRC_INCLUDE_PARTICLES_H_
#define SRC_INCLUDE_PARTICLES_H_

#include <memory>
#include <type_traits>
#include <vector>

#include "macros.h"
#include "particledata.h"
#include "particletype.h"
#include "pdgcode.h"

namespace Smash {

/**
 * \ingroup data
 *
 * The Particles class abstracts the storage and manipulation of particles.
 *
 * There is one Particles object per Experiment. It stores
 * the data about all existing particles in the experiment (ParticleData).
 *
 * \note
 * The Particles object cannot be copied, because it does not make sense
 * semantically. Move semantics make sense and can be implemented when needed.
 */
class Particles {
 public:
  /**
   * Creates a new (empty) Particles object.
   */
  Particles();

  /// Cannot be copied
  Particles(const Particles &) = delete;
  /// Cannot be copied
  Particles &operator=(const Particles &) = delete;

  /**
   * Returns a copy of all particles as a std::vector<ParticleData>.
   */
  ParticleList copy_to_vector() const {
    if (dirty_.empty()) {
      return {&data_[0], &data_[data_size_]};
    }
    return {begin(), end()};
  }

  /**
   * Inserts the particle \p into the list of particles.
   * The argument \p will afterwards not be a valid copy of a particle of the
   * internal list. I.e.
   * \code
   * ParticleData particle(type);
   * particles.insert(particle);
   * particles.is_valid(particle);  // returns false
   * \endcode
   */
  void insert(const ParticleData &p);

  /// Add \p n particles of the same type (\p pdg) to the list.
  void create(size_t n, PdgCode pdg);

  /// Add one particle of the given \p pdg code and return a reference to it
  ParticleData &create(const PdgCode pdg);

  /// Returns the number of particles in the list.
  size_t size() const { return data_size_ - dirty_.size(); }

  /// Returns whether the list of particles is empty.
  bool is_empty() const { return data_size_ == 0; }

  /** Returns the time of the computational frame.
   *
   * \return computation time which is reduced by the start up time
   *
   * \note This function may only be called if the list of particles is not
   * empty.
   *
   * \fpPrecision Why \c double?
   */
  double time() const {
    assert(!is_empty());
    return front().position().x0();
  }

  /**
   * Reset the state of the Particles object to an empty list and a new id
   * counter. The object is thus in the same state as right after construction.
   */
  void reset();

  /**
   * Return whether the ParticleData copy is still a valid copy of the one
   * stored in the Particles object. If not, then the particle either never was
   * a valid copy or it has interacted (e.g. scatter, decay) since it was
   * copied.
   */
  bool is_valid(const ParticleData &copy) const {
    if (data_size_ <= copy.index_) {
      return false;
    }
    return data_[copy.index_].id() ==
               copy.id()  // Check if the particles still exists. If it decayed
                          // or scattered inelastically it is gone.
           &&
           data_[copy.index_].id_process() ==
               copy.id_process();  // If the particle has scattered elastically,
                                   // its id_process has changed and we consider
                                   // it invalid.
  }

  /**
   * Remove the given particle \p p from the list. The argument \p p must be a
   * valid copy obtained from Particles, i.e. a call to \ref is_valid must
   * return \c true.
   *
   * \note The validity of \p p is only enforced in DEBUG builds.
   */
  void remove(const ParticleData &p);

  /**
   * Replace the particles in \p to_remove with the particles in \p to_add in
   * the list of current particles. The particles in \p to_remove must be valid
   * copies obtained from Particles. The particles in \p to_add will not be
   * modified by this function call and therefore the ParticleData instances in
   * \p to_add will not be valid copies of the new particles in the Particles
   * list.
   *
   * \note The validity of \p to_remove is only enforced in DEBUG builds.
   */
  void replace(const ParticleList &to_remove, const ParticleList &to_add);

  /**
   * \internal
   * Iterator type that skips over the holes in data_. It implements a standard
   * bidirectional iterator over the ParticleData objects in Particles.
   */
  template <typename T>
  class GenericIterator
      : public std::iterator<std::bidirectional_iterator_tag, T> {
    friend class Particles;

   public:
    using value_type = typename std::remove_const<T>::type;
    using pointer = typename std::add_pointer<T>::type;
    using reference = typename std::add_lvalue_reference<T>::type;
    using const_pointer = typename std::add_const<pointer>::type;
    using const_reference = typename std::add_const<reference>::type;

   private:
    /**
     * Constructs an iterator pointing to the ParticleData pointed to by \p p.
     * This constructor may only be called from the Particles class.
     */
    GenericIterator(pointer p) : ptr_(p) {}  // NOLINT(runtime/explicit)

    /// The entry in Particles this iterator points to.
    pointer ptr_;

   public:
    /**
     * Advance the iterator to the next valid (not a hole) entry in Particles.
     * Holes are identified by the ParticleData::hole_ member and thus the
     * internal pointer is incremented until all holes are skipped. It is
     * important that the Particles entry pointed to by Particles::end() is not
     * identified as a hole as otherwise the iterator would advance too far.
     */
    GenericIterator &operator++() {
      do {
        ++ptr_;
      } while (ptr_->hole_);
      return *this;
    }
    /// Postfix variant of the above prefix increment operator.
    GenericIterator operator++(int) {
      GenericIterator old = *this;
      operator++();
      return old;
    }

    /**
     * Advance the iterator to the previous valid (not a hole) entry in Particles.
     * Holes are identified by the ParticleData::hole_ member and thus the
     * internal pointer is decremented until all holes are skipped. It is
     * irrelevant whether Particles::data_[0] is a hole because the iteration
     * typically ends at Particles::begin(), which points to a non-hole entry in
     * Particles::data_.
     */
    GenericIterator &operator--() {
      do {
        --ptr_;
      } while (ptr_->hole_);
      return *this;
    }
    /// Postfix variant of the above prefix decrement operator.
    GenericIterator operator--(int) {
      GenericIterator old = *this;
      operator--();
      return old;
    }

    /// Dereferences the iterator.
    reference operator*() { return *ptr_; }
    /// Dereferences the iterator.
    const_reference operator*() const { return *ptr_; }

    /// Dereferences the iterator.
    pointer operator->() { return ptr_; }
    /// Dereferences the iterator.
    const_pointer operator->() const { return ptr_; }

    /// Returns whether two iterators point to the same object.
    bool operator==(const GenericIterator &rhs) const {
      return ptr_ == rhs.ptr_;
    }
    /// Returns whether two iterators point to different objects.
    bool operator!=(const GenericIterator &rhs) const {
      return ptr_ != rhs.ptr_;
    }
    /// Returns whether this iterator comes before the iterator \p rhs.
    bool operator<(const GenericIterator &rhs) const { return ptr_ < rhs.ptr_; }
    /// Returns whether this iterator comes after the iterator \p rhs.
    bool operator>(const GenericIterator &rhs) const { return ptr_ > rhs.ptr_; }
    /// Returns whether this iterator comes before the iterator \p rhs or points
    /// to the same object.
    bool operator<=(const GenericIterator &rhs) const {
      return ptr_ <= rhs.ptr_;
    }
    /// Returns whether this iterator comes after the iterator \p rhs or points
    /// to the same object.
    bool operator>=(const GenericIterator &rhs) const {
      return ptr_ >= rhs.ptr_;
    }
  };
  using iterator = GenericIterator<ParticleData>;
  using const_iterator = GenericIterator<const ParticleData>;

  /// Returns a reference to the first particle in the list.
  ParticleData &front() { return *begin(); }
  /// const overload of the above
  const ParticleData &front() const { return *begin(); }

  /// Returns a reference to the last particle in the list.
  ParticleData &back() { return *(--end()); }
  /// const overload of the above
  const ParticleData &back() const { return *(--end()); }

  /**
   * Returns an iterator pointing to the first particle in the list. Use it to
   * iterate over all particles in the list.
   */
  iterator begin() {
    ParticleData *first = &data_[0];
    while (first->hole_) {
      ++first;
    }
    return first;
  }
  /// const overload of the above
  const_iterator begin() const {
    ParticleData *first = &data_[0];
    while (first->hole_) {
      ++first;
    }
    return first;
  }

  /**
   * Returns an iterator pointing behind the last particle in the list. Use it
   * to iterate over all particles in the list.
   */
  iterator end() { return &data_[data_size_]; }
  /// const overload of the above
  const_iterator end() const { return &data_[data_size_]; }

  /// Returns a const begin iterator.
  const_iterator cbegin() const { return begin(); }
  /// Returns a const end iterator.
  const_iterator cend() const { return end(); }

  /**
   * \ingroup logging
   * Print effective mass and type name for all particles to the stream.
   */
  friend std::ostream &operator<<(std::ostream &out, const Particles &p);

  /////////////////////// deprecated functions ///////////////////////////

  SMASH_DEPRECATED("use the begin() and end() iterators of Particles directly")
  Particles &data() { return *this; }
  SMASH_DEPRECATED("use the begin() and end() iterators of Particles directly")
  const Particles &data() const { return *this; }

  SMASH_DEPRECATED("don't reference particles by id") ParticleData
      &data(int id) {
    for (ParticleData &x : *this) {
      if (x.id() == id) {
        return x;
      }
    }
    throw std::out_of_range("missing particle id");
  }

  SMASH_DEPRECATED("don't reference particles by id") const ParticleData
      &data(int id) const {
    for (const ParticleData &x : *this) {
      if (x.id() == id) {
        return x;
      }
    }
    throw std::out_of_range("missing particle id");
  }

  SMASH_DEPRECATED("don't reference particles by id") int id_max(void) const {
    return id_max_;
  }

  SMASH_DEPRECATED("don't reference particles by id") void remove(int id) {
    for (auto it = begin(); it != end(); ++it) {
      if (it->id() == id) {
        remove(*it);
        return;
      }
    }
  }

  SMASH_DEPRECATED("don't reference particles by id") bool has_data(
      int id) const {
    for (auto &&x : *this) {
      if (x.id() == id) {
        return true;
      }
    }
    return false;
  }

  SMASH_DEPRECATED("use insert instead") int add_data(
      const ParticleData &particle_data) {
    insert(particle_data);
    return id_max_;
  }

  SMASH_DEPRECATED("use is_empty instead") bool empty() const {
    return is_empty();
  }

 private:
  /// Highest id of a given particle. The first particle added to data_ will
  /// have id 0.
  int id_max_ = -1;

  /**\internal
   * Increases the capacity of data_ to \p new_capacity. \p new_capacity is
   * expected to be larger than data_capacity_. This is enforced in DEBUG
   * builds.
   */
  void increase_capacity(unsigned new_capacity);
  /**\internal
   * Ensure that the capacity of data_ is large enough to hold \p to_add more
   * entries. If the capacity does not suffice increase_capacity is called.
   */
  inline void ensure_capacity(unsigned to_add);
  /**\internal
   * Common implementation for copying the relevant data of a ParticleData
   * object into data_. This does not implement a 1:1 copy, instead:
   * \li The particle id in data_ is set to the next id for a new particle.
   * \li The id_process, type, momentum, and position are copied from \p from.
   * \li The ParticleData::index_ members is not modified since it already has
   *     the correct value
   * \li The ParticleData::hole_ member is not modified and might need
   *     adjustment in the calling code.
   */
  inline void copy_in(ParticleData &to, const ParticleData &from);

  /**\internal
   * The number of elements in data_ (including holes, but excluding entries
   * behind the last valid particle)
   */
  unsigned data_size_ = 0u;
  /**\internal
   * The capacity of the memory pointed to by data_.
   */
  unsigned data_capacity_ = 100u;
  /**
   * Points to a dynamically allocated array of ParticleData objects. The
   * allocated size is stored in data_capacity_ and the used range (starting
   * from index 0) is stored in data_size_.
   */
  std::unique_ptr<ParticleData[]> data_;

  /**
   * Stores the indexes in data_ that do not hold valid particle data and should
   * be reused when new particles are added.
   */
  std::vector<unsigned> dirty_;
};

}  // namespace Smash

#endif  // SRC_INCLUDE_PARTICLES_H_
