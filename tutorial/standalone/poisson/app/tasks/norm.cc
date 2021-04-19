/*----------------------------------------------------------------------------*
  Copyright (c) 2020 Triad National Security, LLC
  All rights reserved
 *----------------------------------------------------------------------------*/

#include "tasks/norm.hh"

using namespace flecsi;

double
poisson::task::diff(mesh::accessor<ro> m,
  field<double>::accessor<ro, ro> aa,
  field<double>::accessor<ro, ro> ba) {
  auto a = m.mdspan<mesh::vertices>(aa);
  auto b = m.mdspan<mesh::vertices>(ba);

  double sum{0};
  for(auto j : m.vertices<mesh::y_axis>()) {
    for(auto i : m.vertices<mesh::x_axis>()) {
      sum += pow(a[i][j] - b[i][j], 2);
    } // for
  } // for

  return sum;
} // diff

double
poisson::task::scale(mesh::accessor<ro> m, double sum) {
  return pow(m.delta(), 2) * sum;
} // scale

void
poisson::task::discrete_operator(mesh::accessor<ro> m,
  field<double>::accessor<ro, ro> ua,
  field<double>::accessor<rw, ro> Aua) {
  auto u = m.mdspan<mesh::vertices>(ua);
  auto Au = m.mdspan<mesh::vertices>(Aua);

  const double w = 1.0 / pow(m.delta(), 2);

  // clang-format off
  for(auto j : m.vertices<mesh::y_axis>()) {
    for(auto i : m.vertices<mesh::x_axis>()) {
      Au[i][j] = w * (4.0 * u[i][j] -
        u[i + 1][j] - u[i - 1][j] - u[i][j + 1] - u[i][j - 1]);
    } // for
  } // for
  // clang-format on
} // residual
