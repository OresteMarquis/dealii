// ------------------------------------------------------------------------
//
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2021 - 2025 by the deal.II authors
//
// This file is part of the deal.II library.
//
// Part of the source code is dual licensed under Apache-2.0 WITH
// LLVM-exception OR LGPL-2.1-or-later. Detailed license information
// governing the source code and code contributions can be found in
// LICENSE.md and CONTRIBUTING.md at the top level directory of deal.II.
//
// ------------------------------------------------------------------------


// verify restrictions on level differences imposed by
// hp::Refinement::limit_p_level_difference()
//
// sequentially increase the p-level of the center cell in a hyper_cross
// geometry and verify that all other comply to the level difference


#include <deal.II/base/point.h>
#include <deal.II/base/utilities.h>

#include <deal.II/distributed/tria.h>

#include <deal.II/dofs/dof_handler.h>

#include <deal.II/fe/fe_q.h>

#include <deal.II/grid/filtered_iterator.h>
#include <deal.II/grid/grid_generator.h>

#include <deal.II/hp/fe_collection.h>
#include <deal.II/hp/refinement.h>

#include <vector>

#include "../tests.h"


template <int dim>
void
test(const unsigned int fes_size, const unsigned int max_difference)
{
  Assert(fes_size > 0, ExcInternalError());
  Assert(max_difference > 0, ExcInternalError());

  // setup FE collection
  hp::FECollection<dim> fes;
  while (fes.size() < fes_size)
    fes.push_back(FE_Q<dim>(1));

  const unsigned int contains_fe_index = 0;
  const auto         sequence = fes.get_hierarchy_sequence(contains_fe_index);

  // setup cross-shaped mesh
  parallel::distributed::Triangulation<dim> tria(MPI_COMM_WORLD);
  {
    std::vector<unsigned int> sizes(Utilities::pow(2, dim),
                                    static_cast<unsigned int>(
                                      (sequence.size() - 1) / max_difference));
    GridGenerator::hyper_cross(tria, sizes);
  }

  deallog << "ncells:" << tria.n_cells() << ", nfes:" << fes.size() << std::endl
          << "sequence:" << sequence << std::endl;

  DoFHandler<dim> dofh(tria);
  dofh.distribute_dofs(fes);

  bool fe_indices_changed;
  tria.signals.post_p4est_refinement.connect(
    [&]() {
      const parallel::distributed::TemporarilyMatchRefineFlags<dim>
        refine_modifier(tria);
      fe_indices_changed =
        hp::Refinement::limit_p_level_difference(dofh,
                                                 max_difference,
                                                 contains_fe_index);
    },
    boost::signals2::at_front);

  // increase p-level in center subsequently
  for (unsigned int i = 0; i < sequence.size() - 1; ++i)
    {
      // find center cell
      for (const auto &cell : dofh.active_cell_iterators())
        if (cell->is_locally_owned() && cell->center() == Point<dim>())
          cell->set_future_fe_index(
            fes.next_in_hierarchy(cell->active_fe_index()));

      fe_indices_changed = false;
      tria.execute_coarsening_and_refinement();

      if (i >= max_difference)
        Assert(fe_indices_changed, ExcInternalError());

      // display number of cells for each FE index
      std::vector<unsigned int> count(fes.size(), 0);
      for (const auto &cell :
           dofh.active_cell_iterators() | IteratorFilters::LocallyOwnedCell())
        count[cell->active_fe_index()]++;
      Utilities::MPI::sum(count, tria.get_mpi_communicator(), count);
      deallog << "cycle:" << i << ", fe count:" << count << std::endl;
    }

  if constexpr (running_in_debug_mode())
    {
      // check each cell's active FE index by its distance from the center
      for (const auto &cell :
           dofh.active_cell_iterators() | IteratorFilters::LocallyOwnedCell())
        {
          const double       distance = cell->center().distance(Point<dim>());
          const unsigned int expected_level =
            (sequence.size() - 1) -
            max_difference * static_cast<unsigned int>(std::round(distance));

          Assert(cell->active_fe_index() == sequence[expected_level],
                 ExcInternalError());
        }
    }

  deallog << "OK" << std::endl;
}


int
main(int argc, char *argv[])
{
  Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);

  if (Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0)
    initlog();

  test<2>(4, 1);
  test<2>(8, 2);
  test<2>(12, 3);
}
