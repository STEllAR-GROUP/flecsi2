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

#include "flecsi/data.hh"
#include "flecsi/data/accessor.hh"
#include "flecsi/exec/kernel_interface.hh"
#include "flecsi/topo/unstructured/interface.hh"
#include "flecsi/util/unit.hh"

#include <Kokkos_Core.hpp>

using namespace flecsi;

struct canon : topo::specialization<topo::unstructured, canon> {
  enum index_space { vertices, cells };
  using index_spaces = has<cells, vertices>;
  using connectivities = util::types<from<cells, has<vertices>>>;

  static coloring color(std::string const &) {
    return {2, {40, 60}, {{10}}};
  } // color
};

canon::slot mesh;
canon::cslot coloring;

const field<int>::definition<canon, canon::cells> cell_field;

const int pvalue = 35;

int
init(field<int>::accessor<wo> c) {
  UNIT {
    flecsi::exec::parallel_for(
      c.span(), KOKKOS_LAMBDA(auto & cv) { cv = pvalue; }, std::string("test"));
  };
} // init

void
local_kokkos(field<int>::accessor<rw> c) {
  // Parallel for
  flecsi::exec::parallel_for(
    c.span(),
    KOKKOS_LAMBDA(auto cv) { assert(cv == pvalue); },
    std::string("pfor1"));

  forall(cv, c.span(), "pfor2") {
    assert(cv == pvalue);
  }; // forall
  // Reduction
  std::size_t res = exec::parallel_reduce<exec::fold::sum, std::size_t>(
    c.span(),
    KOKKOS_LAMBDA(auto cv, std::size_t & up) { up += cv; },
    std::string("pred1"));
  assert(pvalue * c.span().size() == res);

  res = reduceall(cv, up, c.span(), exec::fold::sum, std::size_t, "pred2") {
    up += cv;
  };
  assert(pvalue * c.span().size() == res);
}

int
kokkos_driver() {
  UNIT {

    Kokkos::print_configuration(std::cerr);

    // use mesh
    const std::string filename = "input.txt";
    coloring.allocate(filename);
    mesh.allocate(coloring.get());
    const auto pressure = cell_field(mesh);
    flecsi::execute<init, default_accelerator>(pressure);
    flecsi::execute<local_kokkos, default_accelerator>(pressure);
  };
} // driver

//----------------------------------------------------------------------------//
// TEST.
//----------------------------------------------------------------------------//

flecsi::unit::driver<kokkos_driver> driver;
