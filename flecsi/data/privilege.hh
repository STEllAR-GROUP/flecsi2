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

#if !defined(__FLECSI_PRIVATE__)
#error Do not include this file directly!
#endif

#include "flecsi/util/bitutils.hh"

#include <cstddef>
#include <utility>

namespace flecsi {

using Privileges = unsigned;
using PrivilegeCount = unsigned short;

/*!
  Enumeration for specifying access privleges for data that are passed
  to FleCSI tasks.

  @param na no access: consistency update coalesced with next access
  @param ro Read-Only access: data are mapped, updates are performed for
            consistency, but the data are read-only.
  @param wo Write-Only access: data are mapped, no updates are done to the
            state, and the data can be written.
  @param rw Read-Write access: data are mapped, updated are performend for
            consistency, and the data are read-write.
 */

enum partition_privilege_t : Privileges {
  na = 0b00,
  ro = 0b01,
  wo = 0b10,
  rw = 0b11
}; // enum partition_privilege_t

inline constexpr short privilege_bits = 2;

// The leading tag bit which indicates the number of privileges in a pack:
template<PrivilegeCount N>
inline constexpr Privileges privilege_empty = [] {
  const Privileges ret = 1 << privilege_bits * N;
  static_assert(bool(ret), "too many privileges");
  return ret;
}();

/*!
  Utility to allow general privilege components that will match the old
  style of specifying permissions, e.g., <EX, SH, GH> (The old approach was
  only valid for mesh type topologies, and didn't make sense for all topology
  types).

  \tparam PP privileges
 */
template<partition_privilege_t... PP>
inline constexpr Privileges privilege_pack = [] {
  static_assert(((PP < 1 << privilege_bits) && ...));
  Privileges ret = 0;
  ((ret <<= privilege_bits, ret |= PP), ...);
  return ret | privilege_empty<sizeof...(PP)>;
}();

/*!
  Return the number of privileges stored in a privilege pack.

  \param PACK a \c privilege_pack value
 */

constexpr PrivilegeCount
privilege_count(Privileges PACK) {
  return (util::bit_width(PACK) - 1) / privilege_bits;
} // privilege_count

/*!
  Get a privilege out of a pack for the specified id.

  \param i privilege index
  \param pack a \c privilege_pack value
 */

constexpr partition_privilege_t
get_privilege(PrivilegeCount i, Privileges pack) {
  return partition_privilege_t(
    pack >> (privilege_count(pack) - 1 - i) * privilege_bits &
    ((1 << privilege_bits) - 1));
} // get_privilege

// Return whether the privilege allows reading _without_ writing first.
constexpr bool
privilege_read(partition_privilege_t p) {
  return p & 1;
}
constexpr bool
privilege_write(partition_privilege_t p) {
  return p & 2;
}

constexpr bool
privilege_read(Privileges pack) noexcept {
  for(auto i = privilege_count(pack); i--;)
    if(privilege_read(get_privilege(i, pack)))
      return true;
  return false;
}
constexpr bool
privilege_write(Privileges pack) noexcept {
  for(auto i = privilege_count(pack); i--;)
    if(privilege_write(get_privilege(i, pack)))
      return true;
  return false;
}

// Return whether the privileges destroy any existing data.
constexpr bool
privilege_discard(Privileges pack) noexcept {
  // With privilege_pack<na,wo>, the non-ghost can be read later.  With
  // privilege_pack<wo,na>, the ghost data is invalidated either by a
  // subsequent wo or by a ghost copy.
  auto i = privilege_count(pack);
  bool ghost = i > 1;
  for(; i--; ghost = false)
    switch(get_privilege(i, pack)) {
      case wo:
        break;
      case na:
        if(ghost)
          break; // else fall through
      default:
        return false;
    }
  return true;
}

// privilege_pack<P,P,...> (N times)
template<partition_privilege_t P, PrivilegeCount N>
inline constexpr Privileges
  privilege_repeat = privilege_empty<N> |
                     (privilege_empty<N> - 1) / ((1 << privilege_bits) - 1) * P;
template<Privileges A, Privileges B>
inline constexpr Privileges privilege_cat = [] {
  // Check for overflow:
  (void)privilege_empty<privilege_count(A) + privilege_count(B)>;
  const auto e = privilege_empty<privilege_count(B)>;
  return A * e | B & e - 1;
}();

constexpr partition_privilege_t
privilege_merge(Privileges p) {
  return privilege_discard(p)
           ? wo
           : privilege_write(p) ? rw : privilege_read(p) ? ro : na;
}

} // namespace flecsi
