from spack.package import *
from spack.pkg.builtin.flecsi import Flecsi

class Flecsi(Flecsi):
    # since we compile with -Wextra -Werror for development
    # and https://gitlab.kitware.com/cmake/cmake/-/issues/23141
    depends_on('cmake@3.19:3.21,3.22.3:')

    depends_on("legion@cr-16:cr-99", when="backend=legion")
    depends_on("kokkos@3.7:", when="+kokkos")

    depends_on("hpx@1.10: max_cpu_count=128 networking=mpi",
               when='backend=hpx')
    conflicts('^hpx networking=tcp', when='backend=hpx')
