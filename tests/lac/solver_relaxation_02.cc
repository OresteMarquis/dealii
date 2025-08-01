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


// Compare preconditioned Richardson with block relaxation. All output diffs
// should be zero.

#include <deal.II/lac/precondition_block.h>
#include <deal.II/lac/relaxation_block.h>
#include <deal.II/lac/solver_control.h>
#include <deal.II/lac/solver_relaxation.h>
#include <deal.II/lac/solver_richardson.h>
#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/vector.h>
#include <deal.II/lac/vector_memory.h>

#include "../tests.h"

#include "../testmatrix.h"

template <typename SolverType,
          typename MatrixType,
          typename VectorType,
          class PRECONDITION>
double
check_solve(SolverType         &solver,
            const MatrixType   &A,
            VectorType         &u,
            VectorType         &f,
            const PRECONDITION &P)
{
  double result = 0.;
  u             = 0.;
  f             = 1.;
  try
    {
      solver.solve(A, u, f, P);
    }
  catch (SolverControl::NoConvergence &e)
    {
      deallog << "Failure step " << e.last_step << " value " << e.last_residual
              << std::endl;
      result = e.last_residual;
    }
  return result;
}

int
main()
{
  const std::string logname = "output";
  std::ofstream     logfile(logname);
  //  logfile.setf(std::ios::fixed);
  deallog << std::setprecision(4);
  deallog.attach(logfile);

  SolverControl      control(10, 1.e-3);
  SolverRichardson<> rich(control);
  SolverRelaxation<> relax(control);

  for (unsigned int size = 33; size <= 33; size *= 3)
    {
      unsigned int dim = (size - 1) * (size - 1);

      deallog << "Size " << size << " Unknowns " << dim << std::endl;

      // Make matrix
      FDMatrix        testproblem(size, size);
      SparsityPattern structure(dim, dim, 5);
      testproblem.five_point_structure(structure);
      structure.compress();
      SparseMatrix<double> A(structure);
      testproblem.five_point(A);

      for (unsigned int blocksize = 2; blocksize < 32; blocksize <<= 1)
        {
          deallog << "Block size " << blocksize << std::endl;

          const unsigned int n_blocks = dim / blocksize;
          RelaxationBlock<SparseMatrix<double>, double>::AdditionalData
            relax_data(0.8);
          PreconditionBlock<SparseMatrix<double>, double>::AdditionalData
            prec_data(blocksize, 0.8);

          relax_data.block_list.reinit(n_blocks, dim, blocksize);
          for (unsigned int block = 0; block < n_blocks; ++block)
            {
              for (unsigned int i = 0; i < blocksize; ++i)
                relax_data.block_list.add(block, i + block * blocksize);
            }
          relax_data.block_list.compress();

          PreconditionBlockJacobi<SparseMatrix<double>, double> prec_jacobi;
          prec_jacobi.initialize(A, prec_data);
          PreconditionBlockSOR<SparseMatrix<double>, double> prec_sor;
          prec_sor.initialize(A, prec_data);
          PreconditionBlockSSOR<SparseMatrix<double>, double> prec_ssor;
          prec_ssor.initialize(A, prec_data);

          RelaxationBlockJacobi<SparseMatrix<double>, double> relax_jacobi;
          relax_jacobi.initialize(A, relax_data);
          RelaxationBlockSOR<SparseMatrix<double>, double> relax_sor;
          relax_sor.initialize(A, relax_data);
          RelaxationBlockSSOR<SparseMatrix<double>, double> relax_ssor;
          relax_ssor.initialize(A, relax_data);

          Vector<double> f(dim);
          Vector<double> u(dim);
          Vector<double> res(dim);

          f = 1.;
          u = 1.;

          try
            {
              double r1, r2;

              deallog.push("Jacobi");
              r1 = check_solve(rich, A, u, f, prec_jacobi);
              r2 = check_solve(relax, A, u, f, prec_jacobi);
              deallog << "diff " << std::fabs(r1 - r2) / r1 << std::endl;
              r2 = check_solve(relax, A, u, f, relax_jacobi);
              deallog << "diff " << std::fabs(r1 - r2) / r1 << std::endl;
              deallog.pop();

              deallog.push("SOR   ");
              r1 = check_solve(rich, A, u, f, prec_sor);
              r2 = check_solve(relax, A, u, f, prec_sor);
              deallog << "diff " << std::fabs(r1 - r2) / r1 << std::endl;
              r2 = check_solve(relax, A, u, f, relax_sor);
              deallog << "diff " << std::fabs(r1 - r2) / r1 << std::endl;
              deallog.pop();

              deallog.push("SSOR  ");
              r1 = check_solve(rich, A, u, f, prec_ssor);
              r2 = check_solve(relax, A, u, f, prec_ssor);
              deallog << "diff " << std::fabs(r1 - r2) / r1 << std::endl;
              r2 = check_solve(relax, A, u, f, relax_ssor);
              deallog << "diff " << std::fabs(r1 - r2) / r1 << std::endl;
              deallog.pop();
            }
          catch (const std::exception &e)
            {
              std::cerr << "Exception: " << e.what() << std::endl;
            }
        }
    }
}
