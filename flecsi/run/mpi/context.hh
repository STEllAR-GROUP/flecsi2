// Copyright (C) 2016, Triad National Security, LLC
// All rights reserved.

#ifndef FLECSI_RUN_MPI_CONTEXT_HH
#define FLECSI_RUN_MPI_CONTEXT_HH

#include "flecsi/run/local/context.hh"

#include <boost/program_options.hpp>
#include <mpi.h>

#include <map>

namespace flecsi {
namespace run {
/// \defgroup mpi-runtime MPI Runtime
/// Global state.
/// \ingroup runtime
/// \{

struct config : config_base {};

struct context_t : local::context {

  //--------------------------------------------------------------------------//
  //  Runtime.
  //--------------------------------------------------------------------------//
  using context::context;

  /*!
    Documentation for this interface is in the top-level context type.
   */

  int start(const std::function<int()> &, bool);

  /*
    Documentation for this interface is in the top-level context type.
   */

  static int task_depth() {
    return depth;
  } // task_depth

  /*
    Documentation for this interface is in the top-level context type.
   */

  Color color() const {
    return process_;
  }

  /*
    Documentation for this interface is in the top-level context type.
   */

  Color colors() const {
    return processes_;
  }

  static inline int depth;

  struct depth_guard {
    depth_guard() {
      ++depth;
    }
    ~depth_guard() {
      --depth;
    }
  };
};

/// \}
} // namespace run

template<class T>
struct task_local : private run::task_local_base {
  T & operator*() noexcept {
    return *cur();
  }
  T * operator->() noexcept {
    return &**this;
  }

private:
  void emplace() override {
    cur().emplace();
  }
  void reset() noexcept override {
    cur().reset();
  }

  std::optional<T> & cur() {
    return run::context_t::task_depth() ? task : outer;
  }

  std::optional<T> outer, task;
};

} // namespace flecsi

#endif
