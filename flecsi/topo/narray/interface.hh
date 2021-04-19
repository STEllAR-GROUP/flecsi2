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
#include "flecsi/data/copy_plan.hh"
#include "flecsi/data/layout.hh"
#include "flecsi/data/privilege.hh"
#include "flecsi/flog.hh"
#include "flecsi/topo/core.hh"
#include "flecsi/topo/index.hh"
#include "flecsi/topo/narray/coloring_utils.hh"
#include "flecsi/topo/narray/types.hh"
#include "flecsi/topo/utility_types.hh"
#include "flecsi/util/array_ref.hh"

#include <utility>

namespace flecsi {
namespace topo {

/*----------------------------------------------------------------------------*
  Narray Topology.
 *----------------------------------------------------------------------------*/

template<typename Policy>
struct narray : narray_base, with_ragged<Policy>, with_meta<Policy> {

  using index_space = typename Policy::index_space;
  using index_spaces = typename Policy::index_spaces;
  using axis = typename Policy::axis;
  using axes = typename Policy::axes;
  using coord = narray_impl::coord;
  using hypercube = narray_impl::hypercube;
  using coloring_definition = narray_impl::coloring_definition;
  using id = util::id;

  static constexpr std::size_t dimension = Policy::dimension;

  template<std::size_t>
  struct access;

  narray(coloring const & c)
    : with_ragged<Policy>(c.colors), with_meta<Policy>(c.colors),
      part_(make_partitions(c,
        index_spaces(),
        std::make_index_sequence<index_spaces::size>())),
      plan_(make_plans(c,
        index_spaces(),
        std::make_index_sequence<index_spaces::size>())) {
    init_ragged(index_spaces());
    init_meta(c);
    init_policy_meta(c);
  }

  struct meta_data {
    using scoord = std::array<std::size_t, dimension>;
    using shypercube = std::array<scoord, 2>;
    std::array<std::uint32_t, index_spaces::size> faces;
    std::array<scoord, index_spaces::size> global, offset, extents;
    std::array<shypercube, index_spaces::size> logical, extended;
  };

  static inline const typename field<meta_data,
    data::single>::template definition<meta<Policy>>
    meta_field;

  static inline const typename field<typename Policy::meta_data,
    data::single>::template definition<meta<Policy>>
    policy_meta_field;

  util::key_array<repartitioned, index_spaces> part_;
  util::key_array<data::copy_plan, index_spaces> plan_;

  std::size_t colors() const {
    return part_.front().colors();
  }

  template<index_space S>
  data::region & get_region() {
    return part_.template get<S>();
  }

  template<index_space S>
  const data::partition & get_partition(field_id_t) const {
    return part_.template get<S>();
  }

  template<typename Type,
    data::layout Layout,
    typename Topo,
    typename Topo::index_space Space>
  void ghost_copy(data::field_reference<Type, Layout, Topo, Space> const & f) {
    plan_.template get<Space>().issue_copy(f.fid());
  }

private:
  template<auto... Value, std::size_t... Index>
  util::key_array<repartitioned, util::constants<Value...>> make_partitions(
    narray_base::coloring const & c,
    util::constants<Value...> /* index spaces to deduce pack */,
    std::index_sequence<Index...>) {
    flog_assert(c.idx_colorings.size() == sizeof...(Value),
      c.idx_colorings.size()
        << " sizes for " << sizeof...(Value) << " index spaces");
    return {{make_repartitioned<Policy, Value>(
      c.colors, make_partial<idx_size>(c.idx_colorings[Index]))...}};
  }

  template<index_space S>
  data::copy_plan make_plan(index_coloring const & ic, MPI_Comm const & comm) {
    std::vector<std::size_t> num_intervals;

    execute<idx_itvls, mpi>(ic, num_intervals, comm);

    // clang-format off
    auto dest_task = [&ic, &comm](auto f) {
      execute<set_dests, mpi>(f, ic.intervals, comm);
    };

    auto ptrs_task = [&ic, &comm](auto f) {
      execute<set_ptrs<Policy::template privilege_count<S>>, mpi>(
        f, ic.points, comm);
    };

    return {*this, num_intervals, dest_task, ptrs_task, util::constant<S>()};
    // clang-format on
  }

  template<auto... Value, std::size_t... Index>
  util::key_array<data::copy_plan, util::constants<Value...>> make_plans(
    narray_base::coloring const & c,
    util::constants<Value...> /* index spaces to deduce pack */,
    std::index_sequence<Index...>) {
    flog_assert(c.idx_colorings.size() == sizeof...(Value),
      c.idx_colorings.size()
        << " sizes for " << sizeof...(Value) << " index spaces");
    return {{make_plan<Value>(c.idx_colorings[Index], c.comm)...}};
  }

  static void set_meta(
    typename field<meta_data, data::single>::template accessor<wo> m,
    narray_base::coloring const & c) {
    meta_data & md = m;

    for(std::size_t i{0}; i < index_spaces::size; ++i) {
      static constexpr auto copy = [](const coord & c,
                                     typename meta_data::scoord & s) {
        const auto n = s.size();
        flog_assert(
          c.size() == n, "invalid #axes(" << c.size() << ") must be: " << n);
        std::copy_n(c.begin(), n, s.begin());
      };
      static constexpr auto copy2 = [](const hypercube & h,
                                      typename meta_data::shypercube & s) {
        for(auto i = h.size(); i--;)
          copy(h[i], s[i]);
      };

      const auto & ci = c.idx_colorings[i];
      md.faces[i] = ci.faces;
      copy(ci.global, md.global[i]);
      copy(ci.offset, md.offset[i]);
      copy(ci.extents, md.extents[i]);
      copy2(ci.logical, md.logical[i]);
      copy2(ci.extended, md.extended[i]);
    } // for
  } // set_meta

  void init_meta(narray_base::coloring const & c) {
    execute<set_meta, mpi>(meta_field(this->meta), c);
  }

  // Default initilaization for user policy meta data
  static void set_policy_meta(typename field<typename Policy::meta_data,
    data::single>::template accessor<wo>) {}

  void init_policy_meta(narray_base::coloring const &) {
    execute<set_policy_meta, mpi>(policy_meta_field(this->meta));
  }

  template<index_space... SS>
  void init_ragged(util::constants<SS...>) {
    (this->template extend_offsets<SS>(), ...);
  }
}; // struct narray

/*----------------------------------------------------------------------------*
  Narray Access.
 *----------------------------------------------------------------------------*/

template<typename Policy>
template<std::size_t Privileges>
struct narray<Policy>::access {
  util::key_array<data::scalar_access<topo::resize::field>, index_spaces> size_;

  data::scalar_access<narray::meta_field> meta_;
  data::scalar_access<narray::policy_meta_field> policy_meta_;

  access() {}

  enum class range : std::size_t {
    logical,
    extended,
    all,
    boundary_low,
    boundary_high,
    ghost_low,
    ghost_high,
    global
  };

  using hypercubes = index::has<range::logical,
    range::extended,
    range::all,
    range::boundary_low,
    range::boundary_high,
    range::ghost_low,
    range::ghost_high,
    range::global>;

  template<index_space S, axis A>
  bool is_low() {
    return (meta_->faces[S] >> A * 2) & narray_impl::low;
  }

  template<index_space S, axis A>
  bool is_high() {
    return (meta_->faces[S] >> A * 2) & narray_impl::high;
  }

  template<axis A>
  bool is_interior() {
    return !is_low<A>() && !is_high<A>();
  }

  template<index_space S, axis A, range SE>
  std::size_t size() {
    static_assert(
      std::size_t(SE) < hypercubes::size, "invalid size identifier");
    if constexpr(SE == range::logical) {
      return meta_->logical[S][1][A] - meta_->logical[S][0][A];
    }
    else if constexpr(SE == range::extended) {
      return meta_->extended[S][1][A] - meta_->extended[S][0][A];
    }
    else if constexpr(SE == range::all) {
      return meta_->extents[S][A];
    }
    else if constexpr(SE == range::boundary_low) {
      return meta_->logical[S][0][A] - meta_->extended[S][0][A];
    }
    else if constexpr(SE == range::boundary_high) {
      return meta_->extended[S][1][A] - meta_->logical[S][1][A];
    }
    else if constexpr(SE == range::ghost_low) {
      if(!is_low<S, A>())
        return meta_->logical[S][0][A];
      else
        return 0;
    }
    else if constexpr(SE == range::ghost_high) {
      if(!is_high<S, A>())
        return meta_->extents[S][A] - meta_->logical[S][1][A];
      else
        return 0;
    }
    else if constexpr(SE == range::global) {
      return meta_->global[S][A];
    }
  }

  template<index_space S, axis A, range SE>
  auto extents() {
    static_assert(
      std::size_t(SE) < hypercubes::size, "invalid extents identifier");
    if constexpr(SE == range::logical) {
      return make_ids<S>(util::iota_view<util::id>(
        meta_->logical[S][0][A], meta_->logical[S][1][A]));
    }
    else if constexpr(SE == range::extended) {
      return make_ids<S>(util::iota_view<util::id>(
        meta_->extended[S][0][A], meta_->extended[S][1][A]));
    }
    else if constexpr(SE == range::all) {
      return make_ids<S>(util::iota_view<util::id>(0, meta_->extents[S][A]));
    }
    else if constexpr(SE == range::boundary_low) {
      return make_ids<S>(util::iota_view<util::id>(0, size<S, A, SE>()));
    }
    else if constexpr(SE == range::boundary_high) {
      return make_ids<S>(util::iota_view<util::id>(
        meta_->logical[S][1][A], meta_->logical[S][1][A] + size<S, A, SE>()));
    }
    else if constexpr(SE == range::ghost_low) {
      return make_ids<S>(util::iota_view<util::id>(0, size<S, A, SE>()));
    }
    else if constexpr(SE == range::ghost_high) {
      return make_ids<S>(util::iota_view<util::id>(
        meta_->logical[S][1][A], meta_->logical[S][1][A] + size<S, A, SE>()));
    }
    else {
      flog_error("invalid range");
    }
  }

  template<index_space S, axis A, range SE>
  std::size_t offset() {
    static_assert(
      std::size_t(SE) < hypercubes::size, "invalid offset identifier");
    if constexpr(SE == range::logical) {
      return meta_->logical[S][0][A];
    }
    else if constexpr(SE == range::extended) {
      return meta_->extended[S][0][A];
    }
    else if constexpr(SE == range::all) {
      return 0;
    }
    else if constexpr(SE == range::boundary_low) {
      return meta_->extended[S][0][A];
    }
    else if constexpr(SE == range::boundary_high) {
      return meta_->logical[S][1][A];
    }
    else if constexpr(SE == range::ghost_low) {
      return 0;
    }
    else if constexpr(SE == range::ghost_high) {
      return meta_->logical[S][1][A];
    }
    else if constexpr(SE == range::global) {
      return meta_->offset[S][A];
    }
  }

  template<index_space S, typename T, std::size_t P>
  auto mdspan(data::accessor<data::dense, T, P> const & a) {
    auto const s = a.span();
    return util::mdspan<typename decltype(s)::element_type, dimension>(
      s.data(), meta_->extents[S].data());
  }

  template<class F>
  void send(F && f) {
    std::size_t i{0};
    for(auto & a : size_)
      a.topology_send(
        f, [&i](narray & n) -> auto & { return n.part_[i++].sz; });

    meta_.topology_send(f, &narray::meta);
    policy_meta_.topology_send(f, &narray::meta);
  }
}; // struct narray<Policy>::access

/*----------------------------------------------------------------------------*
  Define Base.
 *----------------------------------------------------------------------------*/

template<>
struct detail::base<narray> {
  using type = narray_base;
}; // struct detail::base<narray>

} // namespace topo
} // namespace flecsi
