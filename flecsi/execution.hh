// Copyright (C) 2016, Triad National Security, LLC
// All rights reserved.

#ifndef FLECSI_EXECUTION_HH
#define FLECSI_EXECUTION_HH

#include "flecsi/exec/backend.hh"
#include "flecsi/exec/fold.hh"
#include "flecsi/exec/kernel.hh"

#include "flecsi/flog.hh"
#include "flecsi/run/backend.hh"
#include "flecsi/run/options.hh"

namespace flecsi {

/// \defgroup runtime Runtime Model
/// Environmental information and tools for organizing applications.
/// \code#include "flecsi/execution.hh"\endcode
/// \{

inline std::string argv0;

void finalize();

/*!
  Perform FleCSI runtime initialization. If \em dependent is true, this call
  will also initialize any runtime on which FleCSI depends.

  The following options are interpreted in addition to any \c program_option
  objects:
  - \c \--Xbackend=arg

    Provide a command-line option to the backend.  May be used more than once.
  - \c \--backend-args=args

    Provide command-line options to the backend.
    May be used more than once; word splitting is applied.
  - \c \--flog-tags=tags

    Enable comma-separated output \a tags.
    \c all enables all and is the default; \c unscoped disables all.
    \c none disables normal Flog output entirely.
  - <tt>\--flog-verbose[=level]</tt>

    Enable verbose output if \a level is omitted or positive; suppress
    decorations if it is negative.  The default is 0.
  - \c \--flog-process=p

    Select output from process \a p (default 0), or from all if &minus;1.
  - \c \--control-model

    Write \c <em>program</em>-control-model.dot with the control points and
    the actions for each.
  - \c \--control-model-sorted

    Write \c <em>program</em>-control-model-sorted.dot containing linearized
    actions.

  The Flog options are recognized only when that feature is enabled.
  The control model options require Graphviz support and take effect via
  \c control::check_status.

  As a debugging aid, if the \c FLECSI_SLEEP environment variable is set to an
  integer, the runtime will delay for that number of seconds.

  @param argc number of command-line arguments to process
  @param argv command-line arguments to process
  @param dependent A boolean telling FleCSI whether or not to initialize
                   runtimes on which it depends.

  @return An integer indicating the initialization status. This may be
          interpreted as a \em flecsi::run::status enumeration, e.g.,
          a value of 1 is equivalent to flecsi::run::status::help.
 */

inline int
initialize(int argc, char ** argv, bool dependent = true) {
  run::arguments args(argc, argv);
  argv0 = args.act.program;
  const auto make = [](auto & o, auto & x) -> auto & {
    flog_assert(!o, "already initialized");
    return o.emplace(x);
  };
  if(dependent)
    make(run::dependent, args.dep);
  auto & ctx = make(run::context::ctx, args.cfg);
  ctx.check_config(args.act);
  const auto c = args.act.status();
  if(c) {
    if(!ctx.process())
      std::cerr << args.act.stderr;
    finalize();
  }
  return c;
}

/*!
  Perform FleCSI runtime start. This causes the runtime to begin execution
  of the top-level action.

  \param  action
          The top-level action, i.e., the entry point for FleCSI to begin
          execution.

  \return An integer indicating the finalization status. This will be
          either 0 for successful completion or an error code from
          flecsi::run::status.
 */

inline int
start(const std::function<int()> & action) {
  return run::context::instance().start(action);
}

/*!
  Perform FleCSI runtime finalization. If FleCSI was initialized with the \em
  dependent flag set to true, FleCSI will also finalize any runtimes on which
  it depends.
 */

inline void
finalize() {
  run::context::ctx.reset();
  run::dependent.reset();
}

/*!
  Return the program name.
 */

inline std::string const &
program() {
  return argv0;
}

/*!
  Return the current process id.
 */

inline Color
process() {
  return run::context::instance().process();
}

/*!
  Return the number of processes.
 */

inline Color
processes() {
  return run::context::instance().processes();
}

/*!
  Return the number of threads per process.
 */

inline Color
threads_per_process() {
  return run::context::instance().threads_per_process();
}

/*!
  Return the number of execution instances with which the runtime was
  invoked. In this context a \em thread is defined as an instance of
  execution, and does not imply any other properties. This interface can be
  used to determine the full subscription of the execution instances of the
  running process that invoked the FleCSI runtime.
 */

inline Color
threads() {
  return run::context::instance().threads();
}

/*!
  Return the color of the current execution instance. This function is only
  valid if invoked from within a non MPI task. For MPI task, use \c #process,
  which in that case equals to the color used from any topology.
 */

inline Color
color() {
  return run::context::instance().color();
}

/*!
  Return the number of colors of the current task invocation. This function is
  only valid if invoked from within a non MPI task. For MPI task, use \c
  #processes which in that case equals to the number of colors used from any
  topology.
 */

inline Color
colors() {
  return run::context::instance().colors();
}

/// \}

namespace flog {

/*!
  Explicitly flush buffered flog output.
  \code#include "flecsi/execution.hh"\endcode

  @ingroup flog
 */

inline void
flush() {
#if defined(FLECSI_ENABLE_FLOG) && defined(FLOG_ENABLE_MPI)
  flecsi::exec::reduce_internal<flog::state::gather, void, flecsi::mpi>(
    flog::state::instance());
  flecsi::run::context::instance().flog_task_count() = 0;
#endif
} // flush

inline void
maybe_flush() {
#if defined(FLECSI_ENABLE_FLOG) && defined(FLOG_ENABLE_MPI)
  auto & flecsi_context = run::context::instance();
  std::size_t & flog_task_count = flecsi_context.flog_task_count();
  if(flog_task_count >= FLOG_SERIALIZATION_INTERVAL)
    flush();
#endif
} // maybe_flush

} // namespace flog

/// \defgroup execution Execution Model
/// Launching tasks and kernels.  Tasks are coarse-grained and use
/// distributed-memory with restricted side effects; kernels are fine-grained
/// and data-parallel, possibly using an accelerator.
/// \code#include "flecsi/execution.hh"\endcode
/// \{

/// A global variable with a task-specific value.
/// Must be constructed before calling \c start.
/// The value for a task has the lifetime of that task; the value outside of
/// any task has the lifetime of \c start.  Each is value-initialized.
/// \note Thread-local variables do not function correctly in all backends.
template<class T>
struct task_local
#ifdef DOXYGEN // implemented per-backend
{
  /// Create a task-local variable.
  task_local();
  /// It would not be clear whether moving a \c task_local applied to the
  /// (current) value or the identity of the variable.
  task_local(task_local &&) = delete;

  /// Get the current task's value.
  T & operator*() & noexcept;
  /// Access a member of the current task's value.
  T * operator->() noexcept;
}
#endif
;

/*!
  Execute a reduction task.

  @tparam Task       The user task.
  @tparam Reduction  The reduction operation type.
  @tparam Attributes The task attributes mask.
  @tparam Args       The user-specified task arguments.
  \return a \ref future providing the reduced return value

  \see \c execute about parameter and argument types.
 */

// To avoid compile- and runtime recursion, only user tasks trigger logging.
template<auto & Task,
  class Reduction,
  TaskAttributes Attributes = flecsi::loc | flecsi::leaf,
  typename... Args>
auto
reduce(Args &&... args) {
  using namespace exec;

  ++run::context::instance().flog_task_count();
  flog::maybe_flush();

  return reduce_internal<Task, Reduction, Attributes, Args...>(
    std::forward<Args>(args)...);
} // reduce

template<auto & TASK, TaskAttributes ATTRIBUTES, typename... ARGS>
auto
execute(ARGS &&... args) {
  return reduce<TASK, void, ATTRIBUTES>(std::forward<ARGS>(args)...);
} // execute

/// \}

/*!
  Execute a test task. This interface is provided for FleCSI's unit testing
  framework. Test tasks must return an integer that is non-zero on failure,
  and zero otherwise.

  @tparam TASK       The user task. Its parameters may be of any
                     default-constructible, trivially-move-assignable,
                     non-pointer type, any type that supports the Legion
                     return-value serialization interface, or any of several
                     standard containers of such types. If \a ATTRIBUTES
                     specifies an MPI task, parameters need merely be movable.
  @tparam ATTRIBUTES The task attributes mask.
  @tparam ARGS       The user-specified task arguments, implicitly converted to
                     the parameter types for \a TASK.

  @return zero on success, non-zero on failure.
 */

template<auto & TASK,
  TaskAttributes ATTRIBUTES = flecsi::loc | flecsi::leaf,
  typename... ARGS>
int
test(ARGS &&... args) {
  return reduce<TASK, exec::fold::sum, ATTRIBUTES>(std::forward<ARGS>(args)...)
    .get();
} // test

namespace exec {
/// \addtogroup execution
/// \{

#ifdef DOXYGEN // implemented per-backend
/// Records execution of a loop whose iterations all execute the same sequence
/// of tasks.  With the Legion backend, subsequent iterations run faster if
/// traced.  Some \c data::mutator specializations cannot be traced.  The
/// first iteration should be ignored if it might perform different
/// ghost copies.
struct trace {

  using id_t = int;

  /// Construct a trace with auto generated id
  trace();
  /// Construct a trace with user defined id
  /// \param id User defined id for the trace
  explicit trace(id_t id);

  /// Default move constructor.
  trace(trace &&) = default;

  struct guard;

  /// Create a <code>\ref guard</code> for this \c trace.
  inline guard make_guard();

  /// Skip the next call to the tracer
  void skip();

private:
  void start();
  void stop();
};
#endif

/// RAII guard for executing a trace.
/// Flog output is deferred to the end of the trace as needed.
struct trace::guard {
  /// Immovable.
  guard(guard &&) = delete;

  /// Start a trace.  Required in certain contexts like use of \c
  /// std::optional; otherwise prefer \c trace::make_guard.
  explicit guard(trace & t_) : t(t_) {
    current_flog_task_count =
      std::exchange(flecsi::run::context::instance().flog_task_count(), 0);
    t.start();
  }

  // Destroy a guard by stopping the tracing.
  // The flog count is merged and triggered if needed.
  ~guard() {
    t.stop();
    flecsi::run::context::instance().flog_task_count() +=
      current_flog_task_count;
    flog::maybe_flush();
  }

private:
  trace & t;
  std::size_t current_flog_task_count;

}; // struct trace::guard

/// \}

trace::guard
trace::make_guard() {
  return guard(*this);
}
} // namespace exec

} // namespace flecsi

#endif
