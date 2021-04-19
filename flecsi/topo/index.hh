/*
    @@@@@@@@  @@           @@@@@@   @@@@@@@@ @@
   /@@/////  /@@          @@////@@ @@////// /@@
   /@@       /@@  @@@@@  @@    // /@@       /@@
   /@@@@@@@  /@@ @@///@@/@@       /@@@@@@@@@/@@
   /@@////   /@@/@@@@@@@/@@       ////////@@/@@
   /@@       /@@/@@//// //@@    @@       /@@/@@
   /@@       @@@//@@@@@@ //@@@@@@  @@@@@@@@ /@@
   //       ///  //////   //////  ////////  //

   Copyright (c) 2016, Triad National Security, LLC
   All rights reserved.
                                                                              */
#pragma once

/*! @file */

#include "flecsi/data/accessor.hh"
#include "flecsi/exec/launch.hh"
#include "flecsi/topo/size.hh"

namespace flecsi {
namespace topo {

namespace zero {
inline std::size_t
function(std::size_t) {
  return 0;
}
inline constexpr auto partial = make_partial<function>();
} // namespace zero

// A partition with a field for dynamically resizing it.
struct repartition : with_size, data::prefixes {
  // Construct a partition with an initial size.
  // f is passed as a task argument, so it must be serializable;
  // consider using make_partial.
  template<class F = decltype(zero::partial)>
  repartition(data::region & r, F f = zero::partial)
    : with_size(r.size().first), prefixes(r, [&] {
        const auto r = sizes();
        execute<fill<F>>(r, f);
        return r;
      }()) {}
  void resize() { // apply sizes stored in the field
    update(sizes());
  }

private:
  template<class F>
  static void fill(resize::Field::accessor<wo> a, const F & f) {
    a = f(run::context::instance().color());
  }
};

using repartitioned = data::partitioned<repartition>;

template<class T, typename T::index_space S = T::default_space(), class F>
repartitioned
make_repartitioned(std::size_t r, F f) {
  return {data::make_region<T, S>({r, data::logical_size}), std::move(f)};
}

// Stores the flattened elements of the ragged fields on an index space.
struct ragged_partitioned : data::region {
  template<class Topo, typename Topo::index_space S>
  ragged_partitioned(std::size_t r, util::key_type<S, Topo> kt)
    : region({r, data::logical_size}, kt) {
    for(const auto & fi :
      run::context::instance().get_field_info_store<Topo, S>())
      part.try_emplace(fi->fid, *this);
  }
  repartition & operator[](field_id_t i) {
    return part.at(i);
  }
  const repartition & operator[](field_id_t i) const {
    return part.at(i);
  }

private:
  std::map<field_id_t, repartition> part;
};

struct ragged_base {
  using coloring = std::size_t;
};
template<class P>
struct ragged_category : ragged_base {
  using index_spaces = typename P::index_spaces;
  using index_space = typename P::index_space;

  ragged_category(coloring c) : part(make_partitions(c, index_spaces())) {}

  std::size_t colors() const {
    return part.front().size().first;
  }

  template<index_space S>
  data::region & get_region() {
    return part.template get<S>();
  }

  template<index_space S>
  const repartition & get_partition(field_id_t i) const {
    return part.template get<S>()[i];
  }
  template<index_space S>
  repartition & get_partition(field_id_t i) {
    return part.template get<S>()[i];
  }

  // These can't just be default template arguments, since they would be
  // instantiated even if unused.
  const repartition & get_partition(field_id_t i) const {
    return const_cast<ragged_category &>(*this).get_partition(i);
  }
  repartition & get_partition(field_id_t i) {
    return get_partition<P::default_space()>(i);
  }

  // Ragged ghost copies must be handled at the level of the host topology.
  template<class R>
  void ghost_copy(const R &) {}

private:
  template<auto... VV>
  static util::key_array<ragged_partitioned, util::constants<VV...>>
  make_partitions(std::size_t n,
    util::constants<VV...> /* index_spaces, to deduce a pack */
  ) {
    return {{ragged_partitioned(n, util::key_type<VV, P>())...}};
  }
  util::key_array<ragged_partitioned, index_spaces> part;
};
template<class T>
struct ragged : specialization<ragged_category, ragged<T>> {
  using index_space = typename T::index_space;
  using index_spaces = typename T::index_spaces;

  template<index_space S>
  static constexpr std::size_t privilege_count = T::template privilege_count<S>;
};

struct with_ragged_base {
  template<class F, std::size_t N>
  static void extend(
    field<std::size_t, data::raw>::accessor1<privilege_repeat(rw, N)> a,
    F old) {
    const auto s = a.span();
    const std::size_t i = old(run::context::instance().color());
    // The accessor (chosen to support a resized field) constructs nothing:
    std::uninitialized_fill(s.begin() + i, s.end(), i ? s.back() : 0);
  }
};
template<class P>
struct with_ragged : private with_ragged_base {
  with_ragged(std::size_t n) : ragged(n) {}

  // Extend an offsets field to define empty rows for the suffix.
  template<typename P::index_space S, class F = decltype(zero::partial)>
  void extend_offsets(
    F old = zero::partial) // serializable function from color to old size
  {
    for(auto f :
      run::context::instance().get_field_info_store<topo::ragged<P>, S>())
      execute<extend<F, P::template privilege_count<S>>>(
        data::field_reference<std::size_t, data::raw, P, S>(
          *f, static_cast<typename P::core &>(*this)),
        old);
  }

  typename topo::ragged<P>::core ragged;
};

template<>
struct detail::base<ragged_category> {
  using type = ragged_base;
};

// The user-facing variant of the color category supports ragged fields.
struct index_base {
  struct coloring {
    coloring(size_t size) : size_(size) {}

    size_t size() const {
      return size_;
    }

  private:
    size_t size_;
  };
};

template<class P>
struct index_category : index_base, color<P>, with_ragged<P> {
  using index_base::coloring; // override color_base::coloring
  index_category(const index_base::coloring & c)
    : color<P>({c.size(), 1}), with_ragged<P>(c.size()) {
    this->template extend_offsets<elements>();
  }
};
template<>
struct detail::base<index_category> {
  using type = index_base;
};

// A subtopology for holding topology-specific metadata per color.
template<class P>
struct meta : specialization<index_category, meta<P>> {};

template<class P>
struct with_meta { // for interface consistency
  with_meta(std::size_t n) : meta(n) {}
  typename topo::meta<P>::core meta;
};

struct array_base {
  using coloring = std::vector<std::size_t>;

protected:
  static std::size_t index(const coloring & c, std::size_t i) {
    return c[i];
  }
};
template<class P>
struct array_category : array_base, repartitioned {
  explicit array_category(const coloring & c)
    : partitioned(make_repartitioned<P>(c.size(), make_partial<index>(c))) {
    resize();
  }
};
template<>
struct detail::base<array_category> {
  using type = array_base;
};

template<class P>
struct array : topo::specialization<array_category, array<P>> {};

/*!
  The \c index type allows users to register data on an
  arbitrarily-sized set of indices that have an implicit one-to-one coloring.

  @ingroup topology
 */
struct index : specialization<index_category, index> {
  static coloring color(size_t size) {
    return {size};
  } // color

}; // struct index

} // namespace topo
} // namespace flecsi
