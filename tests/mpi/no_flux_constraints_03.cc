// ------------------------------------------------------------------------
//
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2011 - 2023 by the deal.II authors
//
// This file is part of the deal.II library.
//
// Part of the source code is dual licensed under Apache-2.0 WITH
// LLVM-exception OR LGPL-2.1-or-later. Detailed license information
// governing the source code and code contributions can be found in
// LICENSE.md and CONTRIBUTING.md at the top level directory of deal.II.
//
// ------------------------------------------------------------------------



// check that the AffineConstraints<double> with hanging nodes and
// no-normal-flux constraints on an adaptively refined hyper_cube are the same
// independent of the number of CPUs

#include <deal.II/base/tensor.h>

#include <deal.II/distributed/tria.h>

#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_renumbering.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/mapping_q.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria_accessor.h>
#include <deal.II/grid/tria_iterator.h>

#include <deal.II/lac/trilinos_vector.h>

#include <deal.II/numerics/vector_tools.h>

#include "../tests.h"

template <int dim>
void
test()
{
  Assert(dim == 3, ExcNotImplemented());
  unsigned int myid     = Utilities::MPI::this_mpi_process(MPI_COMM_WORLD);
  unsigned int numprocs = Utilities::MPI::n_mpi_processes(MPI_COMM_WORLD);

  parallel::distributed::Triangulation<dim> triangulation(MPI_COMM_WORLD);

  GridGenerator::hyper_cube(triangulation, -1.0, 1.0);
  triangulation.refine_global(3);

  for (typename Triangulation<dim>::active_cell_iterator cell =
         triangulation.begin_active();
       cell != triangulation.end();
       ++cell)
    if (!cell->is_ghost() && !cell->is_artificial())
      if (cell->center().norm() < 0.3)
        {
          cell->set_refine_flag();
        }

  triangulation.prepare_coarsening_and_refinement();
  triangulation.execute_coarsening_and_refinement();

  if (myid == 0)
    deallog << "#cells = " << triangulation.n_global_active_cells()
            << std::endl;

  // create FE_System and fill in no-normal flux
  // conditions on boundary 1 (outer)
  static const FESystem<dim> fe(FE_Q<dim>(1), dim);
  DoFHandler<dim>            dofh(triangulation);
  dofh.distribute_dofs(fe);
  DoFRenumbering::hierarchical(dofh);

  if (myid == 0)
    deallog << "#dofs = " << dofh.locally_owned_dofs().size() << std::endl;

  const IndexSet relevant_set = DoFTools::extract_locally_relevant_dofs(dofh);

  AffineConstraints<double> constraints;
  constraints.reinit(dofh.locally_owned_dofs(), relevant_set);
  DoFTools::make_hanging_node_constraints(dofh, constraints);
  const std::set<types::boundary_id> no_normal_flux_boundaries = {0};
  const unsigned int                 degree                    = 1;
  VectorTools::compute_no_normal_flux_constraints(
    dofh, 0, no_normal_flux_boundaries, constraints, MappingQ<dim>(degree));
  constraints.close();

  std::string base = "";

  MPI_Barrier(MPI_COMM_WORLD);

  {
    // write the constraintmatrix to a file on each cpu
    std::string fname = base + "cm_" + Utilities::int_to_string(myid) + ".dot";
    std::ofstream file(fname);
    constraints.print(file);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  if (myid == 0)
    {
      // sort and merge the constraint matrices on proc 0, generate a checksum
      // and output that into the deallog
      int return_value = system((std::string("cat ") + base +
                                 "cm_*.dot | sort -n | uniq > " + base + "cm")
                                  .c_str());
      {
        std::ifstream     file(base + "cm");
        std::stringstream ss;
        ss << file.rdbuf();
        std::string str = ss.str();
        deallog << "checksum: " << checksum(str.begin(), str.end())
                << std::endl;
      }
      (void)return_value;
      // delete the file created by processor 0
      std::remove((base + "cm").c_str());
    }

  // remove tmp files again. wait
  // till processor 0 has done its
  // job with them
  MPI_Barrier(MPI_COMM_WORLD);
  std::remove((base + "cm_" + Utilities::int_to_string(myid) + ".dot").c_str());


  // print the number of constraints. since
  // processors might write info in different
  // orders, copy all numbers to root processor
  std::vector<unsigned int> n_constraints_glob(numprocs);
  unsigned int              n_constraints = constraints.n_constraints();
  MPI_Gather(&n_constraints,
             1,
             MPI_UNSIGNED,
             &n_constraints_glob[0],
             1,
             MPI_UNSIGNED,
             0,
             MPI_COMM_WORLD);
  if (myid == 0)
    for (unsigned int i = 0; i < numprocs; ++i)
      deallog << "#constraints on " << i << ": " << n_constraints_glob[i]
              << std::endl;

  // dummy assembly: put 1 in all components of
  // the vector
  TrilinosWrappers::MPI::Vector vector;
  vector.reinit(dofh.locally_owned_dofs(), MPI_COMM_WORLD);
  {
    const unsigned int                   dofs_per_cell = fe.dofs_per_cell;
    std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);
    Vector<double>                       local_vector(dofs_per_cell);
    for (unsigned int i = 0; i < dofs_per_cell; ++i)
      local_vector(i) = 1.;
    typename DoFHandler<dim>::active_cell_iterator cell = dofh.begin_active(),
                                                   endc = dofh.end();
    for (; cell != endc; ++cell)
      if (cell->subdomain_id() == triangulation.locally_owned_subdomain())
        {
          cell->get_dof_indices(local_dof_indices);
          constraints.distribute_local_to_global(local_vector,
                                                 local_dof_indices,
                                                 vector);
        }
    vector.compress(VectorOperation::add);
  }

  // now check that no entries were generated
  // for constrained entries on the locally
  // owned range.
  const std::pair<unsigned int, unsigned int> range = vector.local_range();
  for (unsigned int i = range.first; i < range.second; ++i)
    if (constraints.is_constrained(i))
      AssertThrow(vector(i) == 0, ExcInternalError());

  if (myid == 0)
    deallog << "OK" << std::endl;
}


int
main(int argc, char *argv[])
{
  {
    Utilities::MPI::MPI_InitFinalize mpi_initialization(
      argc, argv, testing_max_num_threads());
    unsigned int myid = Utilities::MPI::this_mpi_process(MPI_COMM_WORLD);


    deallog.push(Utilities::int_to_string(myid));

    if (myid == 0)
      {
        initlog();

        deallog.push("3d");
        test<3>();
        deallog.pop();
      }
    else
      test<3>();
  }

  return 0;
}
