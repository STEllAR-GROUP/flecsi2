/*----------------------------------------------------------------------------*
  Copyright (c) 2020 Triad National Security, LLC
  All rights reserved
 *----------------------------------------------------------------------------*/

#include "tasks/init.hh"

#include <cmath>

using namespace flecsi;

constexpr double PI = 3.14159;
constexpr double K = 12.0;
constexpr double L = 2.0;

void
poisson::task::eggcarton(mesh::accessor<ro> m,
  field<double>::accessor<wo, ro> ua,
  field<double>::accessor<wo, ro> fa,
  field<double>::accessor<wo, ro> sa) {
  auto u = m.mdspan<mesh::vertices>(ua);
  auto f = m.mdspan<mesh::vertices>(fa);
  auto s = m.mdspan<mesh::vertices>(sa);
  const double sq_klpi = pow(PI, 2) * (pow(K, 2) + pow(L, 2));

  flog(info) << "delta^2: " << pow(m.delta(), 2) << std::endl;

  for(auto j : m.vertices<mesh::y_axis, mesh::logical>()) {
    const double y = m.value<mesh::y_axis>(j);
    for(auto i : m.vertices<mesh::x_axis, mesh::logical>()) {
      const double x = m.value<mesh::x_axis>(i);

      f[i][j] = sq_klpi * sin(K * PI * x) * sin(L * PI * y);
      const double solution = sin(K * PI * x) * sin(L * PI * y);
      s[i][j] = solution;

      if((m.is_boundary<mesh::x_axis, mesh::low>(i) ||
           m.is_boundary<mesh::x_axis, mesh::high>(i)) ||
         (m.is_boundary<mesh::y_axis, mesh::low>(j) ||
           m.is_boundary<mesh::y_axis, mesh::high>(j))) {
        u[i][j] = solution;
      }
      else {
        u[i][j] = 0.0;
      } // if
    } // for
  } // for
} // eggcarton
