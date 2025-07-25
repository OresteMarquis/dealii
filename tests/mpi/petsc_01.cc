// ------------------------------------------------------------------------
//
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2011 - 2025 by the deal.II authors
//
// This file is part of the deal.II library.
//
// Part of the source code is dual licensed under Apache-2.0 WITH
// LLVM-exception OR LGPL-2.1-or-later. Detailed license information
// governing the source code and code contributions can be found in
// LICENSE.md and CONTRIBUTING.md at the top level directory of deal.II.
//
// ------------------------------------------------------------------------



// PETScWrappers: document bug with PETSc SparseMatrix and clear_rows()
// until now, also the PETSc-internal SparsityPattern removes the
// rows that are emptied with clear_rows(). This results in errors
// when reusing the matrix later.

#include <deal.II/base/mpi.h>
#include <deal.II/base/utilities.h>

#include <deal.II/lac/dynamic_sparsity_pattern.h>
#include <deal.II/lac/petsc_sparse_matrix.h>

#include "../tests.h"


void
test()
{
  unsigned int myid     = Utilities::MPI::this_mpi_process(MPI_COMM_WORLD);
  unsigned int numprocs = Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD);

  if (myid == 0)
    deallog << "Running on " << numprocs << " CPU(s)." << std::endl;

  DynamicSparsityPattern csp(2 * numprocs);
  for (unsigned int i = 0; i < numprocs * 2; ++i)
    csp.add(i, i);
  csp.add(1, 0);

  PETScWrappers::MPI::SparseMatrix     mat;
  std::vector<types::global_dof_index> local_rows(numprocs, 2);

  mat.reinit(MPI_COMM_WORLD, csp, local_rows, local_rows, myid);

  mat.add(2 * myid, 2 * myid, 1.0);
  mat.add(2 * myid + 1, 2 * myid + 1, 1.0);
  mat.add(1, 0, 42.0);

  mat.add((2 * myid + 2) % (2 * numprocs),
          (2 * myid + 2) % (2 * numprocs),
          0.1);

  mat.compress(VectorOperation::add);

  std::vector<types::global_dof_index> rows(1, 1);
  mat.clear_rows(rows);

  //    mat.write_ascii();
  if (myid == 0)
    deallog << "2nd try" << std::endl;

  mat = 0;
  mat.add(1, 0, 42.0);
  mat.add(2 * myid, 2 * myid, 1.0);
  mat.add(2 * myid + 1, 2 * myid + 1, 1.0);

  mat.add((2 * myid + 2) % (2 * numprocs),
          (2 * myid + 2) % (2 * numprocs),
          0.1);

  mat.compress(VectorOperation::add);
  //    mat.write_ascii();

  if (myid == 0)
    deallog << "done" << std::endl;
}


int
main(int argc, char *argv[])
{
  Utilities::MPI::MPI_InitFinalize mpi_initialization(
    argc, argv, testing_max_num_threads());

  if (Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0)
    {
      initlog();

      test();
    }
  else
    test();
}
