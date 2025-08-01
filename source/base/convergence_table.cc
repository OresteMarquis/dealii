// ------------------------------------------------------------------------
//
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 1999 - 2025 by the deal.II authors
//
// This file is part of the deal.II library.
//
// Part of the source code is dual licensed under Apache-2.0 WITH
// LLVM-exception OR LGPL-2.1-or-later. Detailed license information
// governing the source code and code contributions can be found in
// LICENSE.md and CONTRIBUTING.md at the top level directory of deal.II.
//
// ------------------------------------------------------------------------

#include <deal.II/base/convergence_table.h>
#include <deal.II/base/exceptions.h>
#include <deal.II/base/table_handler.h>

#include <cmath>

DEAL_II_NAMESPACE_OPEN

void
ConvergenceTable::evaluate_convergence_rates(
  const std::string &data_column_key,
  const std::string &reference_column_key,
  const RateMode     rate_mode,
  const unsigned int dim)
{
  Assert(columns.count(data_column_key), ExcColumnNotExistent(data_column_key));
  Assert(columns.count(reference_column_key),
         ExcColumnNotExistent(reference_column_key));

  if (rate_mode == none)
    return;

  // reset the auto fill mode flag since we are going to fill columns from
  // the top that don't yet exist
  set_auto_fill_mode(false);

  const std::vector<internal::TableEntry> &entries =
    columns[data_column_key].entries;
  const std::vector<internal::TableEntry> &ref_entries =
    columns[reference_column_key].entries;
  std::string rate_key = data_column_key + "...";

  const unsigned int n     = entries.size();
  const unsigned int n_ref = ref_entries.size();
  Assert(n == n_ref, ExcDimensionMismatch(n, n_ref));

  std::vector<double> values(n);
  std::vector<double> ref_values(n_ref);

  for (unsigned int i = 0; i < n; ++i)
    {
      values[i]     = entries[i].get_numeric_value();
      ref_values[i] = ref_entries[i].get_numeric_value();
    }

  unsigned int no_rate_entries = 0;

  switch (rate_mode)
    {
      // case none: already considered above
      case reduction_rate:
        rate_key += "red.rate";
        no_rate_entries = columns[rate_key].entries.size();
        // Calculate all missing rate values:
        for (unsigned int i = no_rate_entries; i < n; ++i)
          {
            if (i == 0)
              {
                // no value available for the first row
                add_value(rate_key, "-");
              }
            else
              {
                add_value(rate_key,
                          values[i - 1] / values[i] * ref_values[i] /
                            ref_values[i - 1]);
              }
          }
        break;
      case reduction_rate_log2:
        rate_key += "red.rate.log2";
        no_rate_entries = columns[rate_key].entries.size();
        // Calculate all missing rate values:
        for (unsigned int i = no_rate_entries; i < n; ++i)
          {
            if (i == 0)
              {
                // no value available for the first row
                add_value(rate_key, "-");
              }
            else
              {
                add_value(rate_key,
                          dim * std::log(std::fabs(values[i - 1] / values[i])) /
                            std::log(
                              std::fabs(ref_values[i] / ref_values[i - 1])));
              }
          }
        break;
      default:
        AssertThrow(false, ExcNotImplemented());
    }

  Assert(columns.count(rate_key), ExcInternalError());
  columns[rate_key].flag = 1;
  set_precision(rate_key, 2);

  const std::string &superkey = data_column_key;
  if (supercolumns.count(superkey) == 0u)
    {
      add_column_to_supercolumn(data_column_key, superkey);
      set_tex_supercaption(superkey, columns[data_column_key].tex_caption);
    }

  // only add rate_key to the supercolumn once
  if (no_rate_entries == 0)
    {
      add_column_to_supercolumn(rate_key, superkey);
    }
}



void
ConvergenceTable::evaluate_convergence_rates(const std::string &data_column_key,
                                             const RateMode     rate_mode)
{
  Assert(columns.count(data_column_key), ExcColumnNotExistent(data_column_key));

  // reset the auto fill mode flag since we are going to fill columns from
  // the top that don't yet exist
  set_auto_fill_mode(false);

  const std::vector<internal::TableEntry> &entries =
    columns[data_column_key].entries;
  std::string rate_key = data_column_key + "...";

  const unsigned int n = entries.size();

  std::vector<double> values(n);
  for (unsigned int i = 0; i < n; ++i)
    values[i] = entries[i].get_numeric_value();

  unsigned int no_rate_entries = 0;

  switch (rate_mode)
    {
      case none:
        break;

      case reduction_rate:
        rate_key += "red.rate";
        no_rate_entries = columns[rate_key].entries.size();
        // Calculate all missing rate values:
        for (unsigned int i = no_rate_entries; i < n; ++i)
          {
            if (i == 0)
              {
                // no value available for the first row
                add_value(rate_key, "-");
              }
            else
              {
                add_value(rate_key, values[i - 1] / values[i]);
              }
          }
        break;

      case reduction_rate_log2:
        rate_key += "red.rate.log2";
        no_rate_entries = columns[rate_key].entries.size();
        // Calculate all missing rate values:
        for (unsigned int i = no_rate_entries; i < n; ++i)
          {
            if (i == 0)
              {
                // no value available for the first row
                add_value(rate_key, "-");
              }
            else
              {
                add_value(rate_key,
                          std::log(std::fabs(values[i - 1] / values[i])) /
                            std::log(2.0));
              }
          }
        break;

      default:
        AssertThrow(false, ExcNotImplemented());
    }

  Assert(columns.count(rate_key), ExcInternalError());
  columns[rate_key].flag = 1;
  set_precision(rate_key, 2);

  // set the superkey equal to the key
  const std::string &superkey = data_column_key;
  // and set the tex caption of the supercolumn to the tex caption of the
  // data_column.
  if (supercolumns.count(superkey) == 0u)
    {
      add_column_to_supercolumn(data_column_key, superkey);
      set_tex_supercaption(superkey, columns[data_column_key].tex_caption);
    }

  // only add rate_key to the supercolumn once
  if (no_rate_entries == 0)
    {
      add_column_to_supercolumn(rate_key, superkey);
    }
}



void
ConvergenceTable::omit_column_from_convergence_rate_evaluation(
  const std::string &key)
{
  Assert(columns.count(key), ExcColumnNotExistent(key));

  const std::map<std::string, Column>::iterator col_iter = columns.find(key);
  col_iter->second.flag                                  = 1;
}



void
ConvergenceTable::evaluate_all_convergence_rates(
  const std::string &reference_column_key,
  const RateMode     rate_mode)
{
  for (std::map<std::string, Column>::const_iterator col_iter = columns.begin();
       col_iter != columns.end();
       ++col_iter)
    if (col_iter->second.flag == 0u)
      evaluate_convergence_rates(col_iter->first,
                                 reference_column_key,
                                 rate_mode);
}



void
ConvergenceTable::evaluate_all_convergence_rates(const RateMode rate_mode)
{
  for (std::map<std::string, Column>::const_iterator col_iter = columns.begin();
       col_iter != columns.end();
       ++col_iter)
    if (col_iter->second.flag == 0u)
      evaluate_convergence_rates(col_iter->first, rate_mode);
}

DEAL_II_NAMESPACE_CLOSE
