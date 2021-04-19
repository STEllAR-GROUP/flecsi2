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

#include "flecsi/data/field.hh"
#include "flecsi/exec/task_attributes.hh"
#include "flecsi/util/type_traits.hh"

#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant> // monostate

namespace flecsi {
namespace exec {

namespace detail {
// We care about value category, so we want to use perfect forwarding.
// However, such a template is a better match for some arguments than any
// single non-template overload, so we use SFINAE to detect that we have
// no replacement defined for an argument.
// XREF: more specializations in accessor.hh
template<class = void>
struct task_param {};
template<class P, class A, class = void> // A is a reference type
struct replace_argument {
  static A replace(A a) {
    return static_cast<A>(a);
  }
};
template<class P, class A>
struct replace_argument<P,
  A,
  decltype(void(task_param<P>::replace(std::declval<A>())))> {
  static decltype(auto) replace(A a) {
    return task_param<P>::replace(static_cast<A>(a));
  }
};

// For each parameter-type/argument pair we have either a std::size_t (the
// size of a required index launch), std::monostate (for a required single
// launch), or std::nullptr_t (don't care).

template<class P, class A>
struct launch {
  static auto get(const A &) {
    return nullptr;
  }
};
template<class P,
  class T,
  data::layout L,
  class Topo,
  typename Topo::index_space S>
struct launch<P, data::field_reference<T, L, Topo, S>> {
  static std::size_t get(const data::field_reference<T, L, Topo, S> & r) {
    return r.topology().colors();
  }
};

template<class T>
struct launch_combine {
  launch_combine(const T & t) : t(t) {} // for CTAD
  // TIP: fold-expression allows different types at each level
  template<class U>
  auto & operator|(const launch_combine<U> & c) const {
    if constexpr(std::is_same_v<T, std::nullptr_t>)
      return c;
    else {
      if constexpr(!std::is_same_v<U, std::nullptr_t>) {
        static_assert(std::is_same_v<T, U>, "implied launch types conflict");
        if constexpr(!std::is_same_v<T, std::monostate>)
          flog_assert(t == c.t,
            "implied launch sizes " << t << " and " << c.t << " conflict");
      }
      return *this;
    }
  }
  auto get() const {
    if constexpr(std::is_same_v<T, std::nullptr_t>)
      return std::monostate();
    else
      return t;
  }

private:
  T t;
};

template<bool M, class... PP, class... AA>
auto
launch_size(std::tuple<PP...> *, const AA &... aa) {
  return (launch_combine([] {
    // An MPI task has a known launch domain:
    if constexpr(M)
      return run::context::instance().processes();
    else
      return nullptr;
  }()) | ... |
          launch_combine(launch<std::decay_t<PP>, AA>::get(aa)))
    .get();
}
} // namespace detail
// Replaces certain task arguments before conversion to the parameter type.
template<class P, class T>
decltype(auto)
replace_argument(T && t) {
  return detail::replace_argument<std::decay_t<P>, T &&>::replace(
    std::forward<T>(t));
}

// Return the number of task invocations for the given parameter tuple and
// arguments, or std::monostate() if a single launch is appropriate.
template<std::size_t A, class P, class... AA>
auto
launch_size(const AA &... aa) {
  return detail::launch_size<mask_to_processor_type(A) ==
                             task_processor_type_t::mpi>(
    static_cast<P *>(nullptr), aa...);
}

enum class launch_type_t : size_t { single, index };

/// An explicit launch domain size.
struct launch_domain {
  std::size_t size() {
    return size_;
  }
  std::size_t size_;
};

/// A simple version of C++20's \c bind_front that can be an argument to a
/// task template.
template<auto & F, class... AA>
struct partial : std::tuple<AA...> {
  using Base = typename partial::tuple;
  using Base::Base;
  // Clang 8.0.1--10.0 considers the inherited tuple::tuple() non-constexpr:
  constexpr partial() = default;
  constexpr partial(const Base & b) : Base(b) {}
  constexpr partial(Base && b) : Base(std::move(b)) {}

  template<class... TT>
  constexpr decltype(auto) operator()(TT &&... tt) const & {
    return std::apply(F,
      // we have to call static_cast over *this due to the bug in cuda10+gcc9.0
      // configuration
      std::tuple_cat(
        std::tuple<const AA &...>(static_cast<const Base &>(*this)),
        std::forward_as_tuple(std::forward<TT>(tt)...)));
  }
  template<class... TT>
  constexpr decltype(auto) operator()(TT &&... tt) && {
    return std::apply(F,
      std::tuple_cat(std::tuple<AA &&...>(static_cast<Base &&>(*this)),
        std::forward_as_tuple(std::forward<TT>(tt)...)));
  }

  static partial param; // not defined; use as "f<decltype(p.param)>"
};
} // namespace exec

template<auto & F, class... AA>
constexpr exec::partial<F, AA...>
make_partial(const AA &... aa) {
  return {aa...};
}

/*!
  Single or multiple future.
  A multi-valued future may be passed to a task expecting a single one
  (which is then executed once with each value).

  @tparam Return The return type of the task.
  @tparam Launch FleCSI launch type: single/index.

  @ingroup execution
*/
template<typename Return,
  exec::launch_type_t Launch = exec::launch_type_t::single>
struct future;

#ifdef DOXYGEN // implemented per-backend
template<typename Return>
struct future<Return> {
  /// Wait on the task to finish.
  void wait();
  /// Get the task's result.
  Return get(bool silence_warnings = false);
};

template<typename Return>
struct future<Return, exec::launch_type_t::index> {
  /// Wait on all the tasks to finish.
  void wait(bool silence_warnings = false);
  /// Get the result of one of the tasks.
  Return get(std::size_t index = 0, bool silence_warnings = false);
  /// Get the number of tasks.
  std::size_t size() const;
};
#endif

namespace exec {
template<class R>
struct detail::task_param<future<R>> {
  static future<R> replace(const future<R, launch_type_t::index> &) {
    return {};
  }
};

template<class P>
struct detail::launch<P, launch_domain> {
  static std::size_t get(const launch_domain & d) {
    return d.size_;
  }
};
template<class P, class T>
struct detail::launch<P, future<T, launch_type_t::index>> {
  static std::size_t get(const future<T, launch_type_t::index> & f) {
    return f.size();
  }
};
} // namespace exec

} // namespace flecsi
