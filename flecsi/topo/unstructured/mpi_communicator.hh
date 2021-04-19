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

#include "flecsi/flog.hh"
#include "flecsi/topo/unstructured/types.hh"
#include "flecsi/util/mpi.hh"
#include "flecsi/util/set_utils.hh"

namespace flecsi {
namespace topo {
namespace unstructured_impl {

class mpi_communicator
{
public:
  /// Default constructor
  mpi_communicator(MPI_Comm comm) : comm(comm) {}

  /// Copy constructor (disabled)
  mpi_communicator(const mpi_communicator &) = delete;

  /// Assignment operator (disabled)
  mpi_communicator & operator=(const mpi_communicator &) = delete;

  /// Destructor
  ~mpi_communicator() {}

  /*!
   Return the size of the communicatora
   @ingroup coloring
   */

  size_t size() const {
    int num;
    MPI_Comm_size(comm, &num);
    return num;
  }

  /*!
   Return the rank of the communicator
   @ingroup coloring
   */

  size_t rank() const {
    int rk;
    MPI_Comm_rank(comm, &rk);
    return rk;
  }

  /*!
   Set a barrier.
   */
  void barrier() const {
    MPI_Barrier(comm);
  };

  /*!
   Reduces info_indices from all MPI ranks

   @param request_indices  std::set of shared, ghost etc
   @param max_request_indices Maximum # of indices per rank
   @param colors Number of MPI ranks

   @return std::vector vith the infirmation for the info_indices from all
           MPI ranks

   @ingroup coloring
   */

  std::vector<size_t> get_info_indices(const std::set<size_t> & request_indices,
    size_t max_request_indices,
    int colors) {
    // Pad the request indices with size_t max. We will then set
    // the indices of the actual request. Each rank that receives
    // the request will try to provide information about the
    // non size_t max values in the request. The others will
    // be ignored.
    std::vector<size_t> input_indices(
      colors * max_request_indices, std::numeric_limits<size_t>::max());
    std::vector<size_t> info_indices(colors * max_request_indices);

    for(int c(0); c < colors; ++c) {
      size_t off(0);
      const size_t coff = c * max_request_indices;

      for(auto s : request_indices) {
        input_indices[coff + off++] = s;
      } // for
    } // for

    // Send the request indices to all other ranks.
    MPI_Alltoall(&input_indices[0],
      max_request_indices,
      util::mpi::type<std::size_t>(),
      &info_indices[0],
      max_request_indices,
      util::mpi::type<std::size_t>(),
      comm);

    return info_indices;
  } // get_info_indices

  /*!
   Rerturn a set containing the entity_info information for each
   member of the input set request_indices (from other ranks) and
   the information for the local indices in primary.

   @param FIXME

   @return FIXME

   @ingroup coloring
  */
  std::pair<std::vector<std::set<size_t>>, std::set<entity_info>>
  get_primary_info(const std::set<size_t> & primary,
    const std::set<size_t> & request_indices) {
    auto colors = size();
    auto color = rank();

    // Store the request in a vector for indexed access below.
    std::vector<size_t> request_indices_vector(
      request_indices.begin(), request_indices.end());

    size_t max_request_indices = get_max_request_size(request_indices.size());

    // Pad the request indices with size_t max. We will then set
    // the indices of the actual request. Each rank that receives
    // the request will try to provide information about the
    // non size_t max values in the request. The others will
    // be ignored.
    std::vector<size_t> input_indices(
      colors * max_request_indices, std::numeric_limits<size_t>::max());
    std::vector<size_t> info_indices(colors * max_request_indices);
    info_indices =
      get_info_indices(request_indices, max_request_indices, colors);

    // For now, we need two arrays for each all-to-all communication:
    // One for rank ownership of the request indices, and one
    // for the offsets. We could probably combine these. However,
    // we would probably have to define a custom MPI type. It
    // will only be worth the effort if this appraoch is slow.
    // The input offsets do not need to be initialized because
    // the information is available in the input_indices array.
    std::vector<size_t> input_offsets(colors * max_request_indices);
    std::vector<size_t> info_offsets(colors * max_request_indices);

    // Reset input indices to use to send back information
    std::fill(input_indices.begin(),
      input_indices.end(),
      std::numeric_limits<size_t>::max());

    // For the primary coloring, provide rank and entity information
    // on indices that are shared with other processes.
    std::vector<std::set<size_t>> local(primary.size());

    // See if we can fill any requests...
    for(size_t r(0); r < colors; ++r) {

      // Ignore our rank
      if(r == color) {
        continue;
      } // if

      // These array slices are just for convenience.
      size_t * info = &info_indices[r * max_request_indices];
      size_t * offset = &input_offsets[r * max_request_indices];
      size_t * input = &input_indices[r * max_request_indices];

      // See which requests we can fulfill.
      for(size_t i(0); i < max_request_indices; ++i) {

        auto match = primary.find(info[i]);

        if(match != primary.end()) {
          // This is a match, i.e., we own this entity, so we can
          // set the rank (ownership) and offset.
          input[i] = color;
          offset[i] = std::distance(primary.begin(), match);

          // We also need to register that this index is shared
          // with other ranks
          local[offset[i]].insert(r);
        } // if
      } // for
    } // for

    // Send the indices information back to all ranks.
    MPI_Alltoall(&input_indices[0],
      max_request_indices,
      util::mpi::type<std::size_t>(),
      &info_indices[0],
      max_request_indices,
      util::mpi::type<std::size_t>(),
      comm);

    // Send the offsets information back to all ranks.
    MPI_Alltoall(&input_offsets[0],
      max_request_indices,
      util::mpi::type<std::size_t>(),
      &info_offsets[0],
      max_request_indices,
      util::mpi::type<std::size_t>(),
      comm);

    std::set<entity_info> remote;

    // Collect all of the information for the remote entities.
    for(size_t r(0); r < colors; ++r) {
      // Skip these (we already know them!)
      if(r == color) {
        continue;
      } // if

      // Another slice for convenience.
      size_t * ranks = &info_indices[r * max_request_indices];
      size_t * offsets = &info_offsets[r * max_request_indices];

      for(size_t i(0); i < max_request_indices; ++i) {

        if(ranks[i] != std::numeric_limits<size_t>::max()) {
          // If this is not size_t max, this rank answered our request
          // and we can set the information.
          remote.insert(
            entity_info(request_indices_vector[i], ranks[i], offsets[i]));
        } // if
      } // for
    } // for

    return std::make_pair(local, remote);
  } // get_primary_info

  /*!
   Return FIXME

   @param request_indices FIXME...
                          information.
   @return A std::unordered_map<size_t, std::set<size_t>> FIXME ...

   @ingroup coloring
  */
  std::unordered_map<size_t, std::set<size_t>> get_intersection_info(
    const std::set<size_t> & request_indices) {
    auto colors = size();
    auto color = rank();

    // Store the request in a vector for indexed access below.
    std::vector<size_t> request_indices_vector(
      request_indices.begin(), request_indices.end());

    size_t max_request_indices = get_max_request_size(request_indices.size());

    // Pad the request indices with size_t max. We will then set
    // the indices of the actual request. Each rank that receives
    // the request will try to provide information about the
    // non size_t max values in the request. The others will
    // be ignored.
    std::vector<size_t> input_indices(
      colors * max_request_indices, std::numeric_limits<size_t>::max());
    std::vector<size_t> info_indices(colors * max_request_indices);
    info_indices =
      get_info_indices(request_indices, max_request_indices, colors);

    // Reset input indices to use to send back information
    std::fill(input_indices.begin(),
      input_indices.end(),
      std::numeric_limits<size_t>::max());

    // {
    //   clog_tag_guard(mpi_communicator);
    //   clog_container_one(info, "input_indices", info_indices, clog::space);
    // }

    //
    std::unordered_map<size_t, std::set<size_t>> intersection_map;

    for(size_t r(0); r < colors; ++r) {

      // Ignore our rank
      if(r == color) {
        continue;
      } // if

      // Array slice for convenience.
      size_t * info = &info_indices[r * max_request_indices];

      // Create a set of the off-color request indices.
      std::set<size_t> intersection_set;
      for(size_t i(0); i < max_request_indices; ++i) {
        if(info[i] != std::numeric_limits<size_t>::max()) {
          intersection_set.insert(info[i]);
        } // if
      } // for

      // {
      //   clog_tag_guard(mpi_communicator);
      //   clog_container_one(
      //     info, "intersection_set", intersection_set, clog::space);
      // }

      // Compute the intersection
      auto intersection =
        flecsi::util::set_intersection(intersection_set, request_indices);

      // {
      //   clog_tag_guard(mpi_communicator);
      //   clog_container_one(
      //     info, "rank " << r << " intersection", intersection, clog::space);
      // }

      // If the intersection is non-empty, add it to the return map
      if(intersection.size()) {
        intersection_map[r] = intersection;
      } // if
    } // for

    return intersection_map;
  } // get_intersection_info

  /*!
   Return a map containing the reduced index information for each color.

   @param local_indices The indices of the calling color.

   @return A std::unordered_map<size_t, std::set<size_t>> containing the
           indices of each rank for the given index space.

   @ingroup coloring
   */
  std::unordered_map<size_t, std::set<size_t>> get_entity_reduction(
    const std::set<size_t> & local_indices) {
    auto colors = size();

    size_t max_request_indices = get_max_request_size(local_indices.size());

    auto info_indices =
      get_info_indices(local_indices, max_request_indices, colors);

    std::unordered_map<size_t, std::set<size_t>> entity_reduction_map;

    for(size_t c(0); c < colors; ++c) {

      // Array slice for convenience.
      size_t * info = &info_indices[c * max_request_indices];

      // Create a set of the off-color request indices.
      std::set<size_t> reduction_set;
      for(size_t i(0); i < max_request_indices; ++i) {
        if(info[i] != std::numeric_limits<size_t>::max()) {
          reduction_set.insert(info[i]);
        } // if
      } // for

      entity_reduction_map[c] = reduction_set;
    } // for

    return entity_reduction_map;
  } // get_entity_reduction

  /*!
   Return a set containing the entity_info information for each
   member of the input set request_indices (from other ranks).

   @param entities FIXME...
   @param request_indices A set of entity ids for which to return
                          information.
   @return A std::vector<std::set<size_t>> containing the offset
           information for the requested indices.

   @ingroup coloring
   */
  std::vector<std::set<size_t>> get_entity_info(
    const std::set<entity_info> & entities,
    const std::vector<std::set<size_t>> & request_indices) {
    auto colors = size();

    // Collect the size of each rank request to send.
    std::vector<size_t> send_cnts(colors, 0);
    for(size_t r(0); r < colors; ++r) {
      send_cnts[r] = request_indices[r].size();
    } // for

    // Send the request size (in indices) to each rank.
    std::vector<size_t> recv_cnts(colors);
    MPI_Alltoall(&send_cnts[0],
      1,
      util::mpi::type<std::size_t>(),
      &recv_cnts[0],
      1,
      util::mpi::type<std::size_t>(),
      comm);

    // Start receive operations (non-blocking).
    std::vector<std::vector<size_t>> rbuffers(colors);
    std::vector<MPI_Request> requests;
    for(size_t r(0); r < colors; ++r) {
      if(recv_cnts[r]) {
        rbuffers[r].resize(recv_cnts[r]);
        requests.push_back({});
        MPI_Irecv(&rbuffers[r][0],
          recv_cnts[r],
          util::mpi::type<std::size_t>(),
          r,
          0,
          comm,
          &requests[requests.size() - 1]);
      } // if
    } // for

    // Start send operations (blocking is ok here).
    std::vector<std::vector<size_t>> sbuffers(colors);
    for(size_t r(0); r < colors; ++r) {
      if(send_cnts[r]) {
        std::copy(request_indices[r].begin(),
          request_indices[r].end(),
          std::back_inserter(sbuffers[r]));

        MPI_Send(&sbuffers[r][0],
          send_cnts[r],
          util::mpi::type<std::size_t>(),
          r,
          0,
          comm);
      } // if
    } // for

    // Create a map version of the entity info for lookups below.
    std::unordered_map<size_t, entity_info> entity_info_map;
    for(auto i : entities) {
      entity_info_map[i.id] = i;
    } // for

    // Wait on the receive operations
    std::vector<MPI_Status> status(requests.size());
    MPI_Waitall(requests.size(), &requests[0], &status[0]);

    // Set the offsets for each requested index in the send buffer.
    for(size_t r(0); r < colors; ++r) {
      sbuffers[r].resize(rbuffers[r].size());

      size_t offset(0);
      for(auto i : rbuffers[r]) {
        sbuffers[r][offset++] = entity_info_map[i].offset;
      } // for
    } // for

    // we are reusing requests in the following code, need to clear the content
    // first.
    requests.clear();

    // Start receive operations (non-blocking) to get back the
    // offsets we requested.
    for(size_t r(0); r < colors; ++r) {
      // If we sent a request, prepare to receive an answer.
      if(send_cnts[r]) {
        // We're done with our receive buffers, so we can re-use them.
        rbuffers[r].resize(send_cnts[r], 0);
        requests.push_back({});
        MPI_Irecv(&rbuffers[r][0],
          send_cnts[r],
          util::mpi::type<std::size_t>(),
          r,
          0,
          comm,
          &requests[requests.size() - 1]);
      } // if
    } // for

    // Start send operations (blocking is probably ok here).
    for(size_t r(0); r < colors; ++r) {
      // If we received a request, prepare to send an answer.
      if(recv_cnts[r]) {
        MPI_Send(&sbuffers[r][0],
          recv_cnts[r],
          util::mpi::type<std::size_t>(),
          r,
          0,
          comm);
      } // if
    } // for

    // Wait on the receive operations
    status.resize(requests.size());
    MPI_Waitall(requests.size(), &requests[0], &status[0]);

    std::vector<std::set<size_t>> remote(colors);
    for(size_t r(0); r < colors; ++r) {
      for(size_t i(0); i < send_cnts[r]; ++i) {
        remote[r].insert(rbuffers[r][i]);
      } // for
    } // for

    return remote;
  } // get_entity_info

  /*!
     Rerturn a map containing the coloring index and the number of indices
     for the given index set.

     @param size Size on current MPI rank

     @return indices_map

     @ingroup coloring
   */

  std::vector<size_t> gather_sizes(const size_t & size) {
    int colors;

    MPI_Comm_size(comm, &colors);

    std::vector<size_t> buffer(colors);

    MPI_Allgather(&size,
      1,
      util::mpi::type<std::size_t>(),
      buffer.data(),
      1,
      util::mpi::type<std::size_t>(),
      comm);

    return buffer;
  } // gather_sizes

  /*!
   exchange coloring info between all MPI ranks

   @param request_indices Shared_users or Ghost_owners
   @param function  Lambda function, that specify where do we want to
                     insert values

   @ingroup coloring
   */

  template<typename Lambda>
  void alltoall_coloring_info(std::set<size_t> & request_indices,
    Lambda && function) {
    auto colors = size();
    // auto color = rank();

    size_t max_request_indices = get_max_request_size(request_indices.size());

    // Pad the request indices with size_t max. We will then set
    // the indices of the actual request. Each rank that receives
    // the request will try to provide information about the
    // non size_t max values in the request. The others will
    // be ignored.
    std::vector<size_t> info_indices(colors * max_request_indices);
    info_indices =
      get_info_indices(request_indices, max_request_indices, colors);

    for(size_t c(0); c < colors; ++c) {

      size_t * info = &info_indices[c * max_request_indices];

      for(size_t i(0); i < max_request_indices; ++i) {
        if(info[i] != std::numeric_limits<size_t>::max()) {
          const size_t value = info[i];
          function(c, value);
        } // if
      } // for
    } // for

  } // alltoall_coloring_info

#if 0
  /*!
   gets coloring info between all MPI ranks

   @param coloring_info Coloring info for one particular rank

   @ingroup coloring
   */
  std::unordered_map<size_t, coloring_info_t> gather_coloring_info(
    coloring_info_t & color_info) {
    auto colors = size();
    auto color = rank();

    struct size_info_t {
      size_t exclusive;
      size_t shared;
      size_t ghost;
    }; // struct size_info_t

    size_info_t buffer[colors];
    const size_t bytes = sizeof(size_info_t);

    int result = MPI_Allgather(
      &color_info, bytes, MPI_BYTE, &buffer, bytes, MPI_BYTE, comm);

    std::unordered_map<size_t, coloring_info_t> coloring_info;

    for(size_t c(0); c < colors; ++c) {
      coloring_info[c].exclusive = buffer[c].exclusive;
      coloring_info[c].shared = buffer[c].shared;
      coloring_info[c].ghost = buffer[c].ghost;
    } // for

    alltoall_coloring_info(
      color_info.shared_users, [&](size_t c, size_t value) {
        coloring_info[c].shared_users.insert(value);
      });
    alltoall_coloring_info(
      color_info.ghost_owners, [&](size_t c, size_t value) {
        coloring_info[c].ghost_owners.insert(value);
      });

    return coloring_info;
  } // gather_coloring_info
#endif

  /*!
   Find maximum size for the "requested_indicies" - MPI reduction

   @param request_indices request_indices for local MPI rank

   @return max_request_indices Maximum size for the "requested_indicies"

   @ingroup coloring
   */

  size_t get_max_request_size(size_t request_indices) {
    size_t max_request_indices = 0;

    // This may be inefficient, but this call is doing a reduction
    // to determine the maximum number of indices requested by any rank
    // so that we can pad out the all-to-all communication below.
    MPI_Allreduce(&request_indices,
      &max_request_indices,
      1,
      util::mpi::type<std::size_t>(),
      MPI_MAX,
      comm);

    return max_request_indices;
  } // get_max_request_size

protected:
  MPI_Comm comm;
}; // mpi_communicator

} // namespace unstructured_impl
} // namespace topo
} // namespace flecsi
