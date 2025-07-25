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

#ifndef dealii_full_matrix_templates_h
#define dealii_full_matrix_templates_h


// TODO: this file has a lot of operations between matrices and matrices or
// matrices and vectors of different precision. we should go through the
// file and in each case pick the more accurate data type for intermediate
// results. currently, the choice is pretty much random. this may also allow
// us some operations where one operand is complex and the other is not
// -> use ProductType<T,U> type trait for the results

#include <deal.II/base/config.h>

#include <deal.II/base/template_constraints.h>

#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/lapack_full_matrix.h>
#include <deal.II/lac/lapack_templates.h>
#include <deal.II/lac/vector.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <vector>

DEAL_II_NAMESPACE_OPEN


template <typename number>
FullMatrix<number>::FullMatrix(const size_type n)
  : Table<2, number>(n, n)
{}


template <typename number>
FullMatrix<number>::FullMatrix(const size_type m, const size_type n)
  : Table<2, number>(m, n)
{}


template <typename number>
FullMatrix<number>::FullMatrix(const size_type m,
                               const size_type n,
                               const number   *entries)
  : Table<2, number>(m, n)
{
  this->fill(entries);
}



template <typename number>
FullMatrix<number>::FullMatrix(const IdentityMatrix &id)
  : Table<2, number>(id.m(), id.n())
{
  for (size_type i = 0; i < id.m(); ++i)
    (*this)(i, i) = 1;
}



template <typename number>
template <typename number2>
FullMatrix<number> &
FullMatrix<number>::operator=(const FullMatrix<number2> &M)
{
  TableBase<2, number>::operator=(M);
  return *this;
}



template <typename number>
FullMatrix<number> &
FullMatrix<number>::operator=(const IdentityMatrix &id)
{
  this->reinit(id.m(), id.n());
  for (size_type i = 0; i < id.m(); ++i)
    (*this)(i, i) = 1.;

  return *this;
}



template <typename number>
template <typename number2>
FullMatrix<number> &
FullMatrix<number>::operator=(const LAPACKFullMatrix<number2> &M)
{
  Assert(this->m() == M.n_rows(), ExcDimensionMismatch(this->m(), M.n_rows()));
  Assert(this->n() == M.n_cols(), ExcDimensionMismatch(this->n(), M.n_cols()));
  for (size_type i = 0; i < this->m(); ++i)
    for (size_type j = 0; j < this->n(); ++j)
      (*this)(i, j) = M(i, j);

  return *this;
}



template <typename number>
bool
FullMatrix<number>::all_zero() const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  const number       *p = this->values.data();
  const number *const e = this->values.data() + this->n_elements();
  while (p != e)
    if (*p++ != number(0.0))
      return false;

  return true;
}



template <typename number>
FullMatrix<number> &
FullMatrix<number>::operator*=(const number factor)
{
  AssertIsFinite(factor);
  for (number &v : this->values)
    v *= factor;

  return *this;
}



template <typename number>
FullMatrix<number> &
FullMatrix<number>::operator/=(const number factor)
{
  AssertIsFinite(factor);
  const number factor_inv = number(1.) / factor;

  return *this *= factor_inv;
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::vmult(Vector<number2>       &dst,
                          const Vector<number2> &src,
                          const bool             adding) const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(dst.size() == m(), ExcDimensionMismatch(dst.size(), m()));
  Assert(src.size() == n(), ExcDimensionMismatch(src.size(), n()));

  Assert(&src != &dst, ExcSourceEqualsDestination());

  const number *e = this->values.data();
  // get access to the data in order to
  // avoid copying it when using the ()
  // operator
  const number2  *src_ptr = src.begin();
  const size_type size_m = m(), size_n = n();
  for (size_type i = 0; i < size_m; ++i)
    {
      number2 s = adding ? dst(i) : 0.;
      for (size_type j = 0; j < size_n; ++j)
        s += src_ptr[j] * number2(*(e++));
      dst(i) = s;
    }
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::Tvmult(Vector<number2>       &dst,
                           const Vector<number2> &src,
                           const bool             adding) const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(dst.size() == n(), ExcDimensionMismatch(dst.size(), n()));
  Assert(src.size() == m(), ExcDimensionMismatch(src.size(), m()));

  Assert(&src != &dst, ExcSourceEqualsDestination());

  const number   *e       = this->values.data();
  number2        *dst_ptr = &dst(0);
  const size_type size_m = m(), size_n = n();

  // zero out data if we are not adding
  if (!adding)
    for (size_type j = 0; j < size_n; ++j)
      dst_ptr[j] = 0.;

  // write the loop in a way that we can
  // access the data contiguously
  for (size_type i = 0; i < size_m; ++i)
    {
      const number2 d = src(i);
      for (size_type j = 0; j < size_n; ++j)
        dst_ptr[j] += d * number2(*(e++));
    };
}


template <typename number>
template <typename number2, typename number3>
number
FullMatrix<number>::residual(Vector<number2>       &dst,
                             const Vector<number2> &src,
                             const Vector<number3> &right) const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(dst.size() == m(), ExcDimensionMismatch(dst.size(), m()));
  Assert(src.size() == n(), ExcDimensionMismatch(src.size(), n()));
  Assert(right.size() == m(), ExcDimensionMismatch(right.size(), m()));

  Assert(&src != &dst, ExcSourceEqualsDestination());

  number          res    = 0.;
  const size_type size_m = m(), size_n = n();
  for (size_type i = 0; i < size_m; ++i)
    {
      number s = number(right(i));
      for (size_type j = 0; j < size_n; ++j)
        s -= number(src(j)) * (*this)(i, j);
      dst(i) = s;
      res += s * s;
    }
  return std::sqrt(res);
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::forward(Vector<number2>       &dst,
                            const Vector<number2> &src) const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(dst.size() == m(), ExcDimensionMismatch(dst.size(), m()));
  Assert(src.size() == n(), ExcDimensionMismatch(src.size(), n()));

  size_type i, j;
  size_type nu = ((m() < n()) ? m() : n());
  for (i = 0; i < nu; ++i)
    {
      typename ProductType<number, number2>::type s = src(i);
      for (j = 0; j < i; ++j)
        s -=
          typename ProductType<number, number2>::type(dst(j)) * (*this)(i, j);
      dst(i) = number2(s) / number2((*this)(i, i));
      AssertIsFinite(dst(i));
    }
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::backward(Vector<number2>       &dst,
                             const Vector<number2> &src) const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  size_type j;
  size_type nu = (m() < n() ? m() : n());
  for (std::make_signed_t<size_type> i = nu - 1; i >= 0; --i)
    {
      typename ProductType<number, number2>::type s = src(i);
      for (j = i + 1; j < nu; ++j)
        s -=
          typename ProductType<number, number2>::type(dst(j)) * (*this)(i, j);
      dst(i) = number2(s) / number2((*this)(i, i));
      AssertIsFinite(dst(i));
    }
}



template <typename number>
void
FullMatrix<number>::compress(VectorOperation::values)
{}



template <typename number>
template <typename number2>
void
FullMatrix<number>::fill(const FullMatrix<number2> &src,
                         const size_type            dst_offset_i,
                         const size_type            dst_offset_j,
                         const size_type            src_offset_i,
                         const size_type            src_offset_j)
{
  AssertIndexRange(dst_offset_i, m());
  AssertIndexRange(dst_offset_j, n());
  AssertIndexRange(src_offset_i, src.m());
  AssertIndexRange(src_offset_j, src.n());

  // Compute maximal size of copied block
  const size_type rows = std::min(m() - dst_offset_i, src.m() - src_offset_i);
  const size_type cols = std::min(n() - dst_offset_j, src.n() - src_offset_j);

  for (size_type i = 0; i < rows; ++i)
    for (size_type j = 0; j < cols; ++j)
      (*this)(dst_offset_i + i, dst_offset_j + j) =
        src(src_offset_i + i, src_offset_j + j);
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::fill_permutation(const FullMatrix<number2>    &src,
                                     const std::vector<size_type> &p_rows,
                                     const std::vector<size_type> &p_cols)
{
  Assert(p_rows.size() == this->n_rows(),
         ExcDimensionMismatch(p_rows.size(), this->n_rows()));
  Assert(p_cols.size() == this->n_cols(),
         ExcDimensionMismatch(p_cols.size(), this->n_cols()));

  for (size_type i = 0; i < this->n_rows(); ++i)
    for (size_type j = 0; j < this->n_cols(); ++j)
      (*this)(i, j) = src(p_rows[i], p_cols[j]);
}



template <typename number>
void
FullMatrix<number>::add_row(const size_type i,
                            const number    s,
                            const size_type j)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  for (size_type k = 0; k < n(); ++k)
    (*this)(i, k) += s * (*this)(j, k);
}


template <typename number>
void
FullMatrix<number>::add_row(const size_type i,
                            const number    s,
                            const size_type j,
                            const number    t,
                            const size_type k)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  const size_type size_m = n();
  for (size_type l = 0; l < size_m; ++l)
    (*this)(i, l) += s * (*this)(j, l) + t * (*this)(k, l);
}


template <typename number>
void
FullMatrix<number>::add_col(const size_type i,
                            const number    s,
                            const size_type j)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  for (size_type k = 0; k < m(); ++k)
    (*this)(k, i) += s * (*this)(k, j);
}


template <typename number>
void
FullMatrix<number>::add_col(const size_type i,
                            const number    s,
                            const size_type j,
                            const number    t,
                            const size_type k)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  for (std::size_t l = 0; l < m(); ++l)
    (*this)(l, i) += s * (*this)(l, j) + t * (*this)(l, k);
}



template <typename number>
void
FullMatrix<number>::swap_row(const size_type i, const size_type j)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  for (size_type k = 0; k < n(); ++k)
    std::swap((*this)(i, k), (*this)(j, k));
}


template <typename number>
void
FullMatrix<number>::swap_col(const size_type i, const size_type j)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  for (size_type k = 0; k < m(); ++k)
    std::swap((*this)(k, i), (*this)(k, j));
}


template <typename number>
void
FullMatrix<number>::diagadd(const number src)
{
  Assert(!this->empty(), ExcEmptyMatrix());
  Assert(m() == n(), ExcDimensionMismatch(m(), n()));

  for (size_type i = 0; i < n(); ++i)
    (*this)(i, i) += src;
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::equ(const number a, const FullMatrix<number2> &A)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == A.m(), ExcDimensionMismatch(m(), A.m()));
  Assert(n() == A.n(), ExcDimensionMismatch(n(), A.n()));

  for (size_type i = 0; i < m(); ++i)
    for (size_type j = 0; j < n(); ++j)
      (*this)(i, j) = a * number(A(i, j));
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::equ(const number               a,
                        const FullMatrix<number2> &A,
                        const number               b,
                        const FullMatrix<number2> &B)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == A.m(), ExcDimensionMismatch(m(), A.m()));
  Assert(n() == A.n(), ExcDimensionMismatch(n(), A.n()));
  Assert(m() == B.m(), ExcDimensionMismatch(m(), B.m()));
  Assert(n() == B.n(), ExcDimensionMismatch(n(), B.n()));

  for (size_type i = 0; i < m(); ++i)
    for (size_type j = 0; j < n(); ++j)
      (*this)(i, j) = a * number(A(i, j)) + b * number(B(i, j));
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::equ(const number               a,
                        const FullMatrix<number2> &A,
                        const number               b,
                        const FullMatrix<number2> &B,
                        const number               c,
                        const FullMatrix<number2> &C)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == A.m(), ExcDimensionMismatch(m(), A.m()));
  Assert(n() == A.n(), ExcDimensionMismatch(n(), A.n()));
  Assert(m() == B.m(), ExcDimensionMismatch(m(), B.m()));
  Assert(n() == B.n(), ExcDimensionMismatch(n(), B.n()));
  Assert(m() == C.m(), ExcDimensionMismatch(m(), C.m()));
  Assert(n() == C.n(), ExcDimensionMismatch(n(), C.n()));

  for (size_type i = 0; i < m(); ++i)
    for (size_type j = 0; j < n(); ++j)
      (*this)(i, j) =
        a * number(A(i, j)) + b * number(B(i, j)) + c * number(C(i, j));
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::mmult(FullMatrix<number2>       &dst,
                          const FullMatrix<number2> &src,
                          const bool                 adding) const
{
  Assert(!this->empty(), ExcEmptyMatrix());
  Assert(n() == src.m(), ExcDimensionMismatch(n(), src.m()));
  Assert(dst.n() == src.n(), ExcDimensionMismatch(dst.n(), src.n()));
  Assert(dst.m() == m(), ExcDimensionMismatch(m(), dst.m()));
  if constexpr (std::is_same_v<number, number2>)
    {
      Assert(&dst != this,
             ExcMessage(
               "The output matrix cannot be the same as the current matrix."));
      Assert(&dst != &src,
             ExcMessage(
               "The output matrix cannot be the same as the input matrix."));
    }

    // see if we can use BLAS algorithms for this and if the type for 'number'
    // works for us (it is usually not efficient to use BLAS for very small
    // matrices):
#ifdef DEAL_II_WITH_LAPACK
  const size_type max_blas_int = std::numeric_limits<types::blas_int>::max();
  if ((std::is_same_v<number, double> ||
       std::is_same_v<number, float>)&&std::is_same_v<number, number2>)
    if (this->n() * this->m() * src.n() > 300 && src.n() <= max_blas_int &&
        this->m() <= max_blas_int && this->n() <= max_blas_int)
      {
        // In case we have the BLAS function gemm detected by CMake, we
        // use that algorithm for matrix-matrix multiplication since it
        // provides better performance than the deal.II native function (it
        // uses cache and register blocking in order to access local data).
        //
        // Note that BLAS/LAPACK stores matrix elements column-wise (i.e., all
        // values in one column, then all in the next, etc.), whereas the
        // FullMatrix stores them row-wise.  We ignore that difference, and
        // give our row-wise data to BLAS, let BLAS build the product of
        // transpose matrices, and read the result as if it were row-wise
        // again. In other words, we calculate (B^T A^T)^T, which is AB.

        const types::blas_int m       = static_cast<types::blas_int>(src.n());
        const types::blas_int n       = static_cast<types::blas_int>(this->m());
        const types::blas_int k       = static_cast<types::blas_int>(this->n());
        const char           *notrans = "n";

        const number alpha = 1.;
        const number beta  = (adding == true) ? 1. : 0.;

        // Use the BLAS function gemm for calculating the matrix-matrix
        // product.
        gemm(notrans,
             notrans,
             &m,
             &n,
             &k,
             &alpha,
             &src(0, 0),
             &m,
             this->values.data(),
             &k,
             &beta,
             &dst(0, 0),
             &m);

        return;
      }

#endif

  const size_type m = this->m(), n = src.n(), l = this->n();

  // arrange the loops in a way that we keep write operations low, (writing is
  // usually more costly than reading), even though we need to access the data
  // in src not in a contiguous way.
  for (size_type i = 0; i < m; ++i)
    for (size_type j = 0; j < n; ++j)
      {
        number2 add_value = adding ? dst(i, j) : 0.;
        for (size_type k = 0; k < l; ++k)
          add_value += static_cast<number2>((*this)(i, k)) *
                       static_cast<number2>((src(k, j)));
        dst(i, j) = add_value;
      }
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::Tmmult(FullMatrix<number2>       &dst,
                           const FullMatrix<number2> &src,
                           const bool                 adding) const
{
  Assert(!this->empty(), ExcEmptyMatrix());
  Assert(m() == src.m(), ExcDimensionMismatch(m(), src.m()));
  Assert(n() == dst.m(), ExcDimensionMismatch(n(), dst.m()));
  Assert(src.n() == dst.n(), ExcDimensionMismatch(src.n(), dst.n()));


  // see if we can use BLAS algorithms for this and if the type for 'number'
  // works for us (it is usually not efficient to use BLAS for very small
  // matrices):
#ifdef DEAL_II_WITH_LAPACK
  const size_type max_blas_int = std::numeric_limits<types::blas_int>::max();
  if ((std::is_same_v<number, double> ||
       std::is_same_v<number, float>)&&std::is_same_v<number, number2>)
    if (this->n() * this->m() * src.n() > 300 && src.n() <= max_blas_int &&
        this->n() <= max_blas_int && this->m() <= max_blas_int)
      {
        // In case we have the BLAS function gemm detected by CMake, we
        // use that algorithm for matrix-matrix multiplication since it
        // provides better performance than the deal.II native function (it
        // uses cache and register blocking in order to access local data).
        //
        // Note that BLAS/LAPACK stores matrix elements column-wise (i.e., all
        // values in one column, then all in the next, etc.), whereas the
        // FullMatrix stores them row-wise.  We ignore that difference, and
        // give our row-wise data to BLAS, let BLAS build the product of
        // transpose matrices, and read the result as if it were row-wise
        // again. In other words, we calculate (B^T A)^T, which is A^T B.

        const types::blas_int m       = static_cast<types::blas_int>(src.n());
        const types::blas_int n       = static_cast<types::blas_int>(this->n());
        const types::blas_int k       = static_cast<types::blas_int>(this->m());
        const char           *trans   = "t";
        const char           *notrans = "n";

        const number alpha = 1.;
        const number beta  = (adding == true) ? 1. : 0.;

        // Use the BLAS function gemm for calculating the matrix-matrix
        // product.
        gemm(notrans,
             trans,
             &m,
             &n,
             &k,
             &alpha,
             &src(0, 0),
             &m,
             this->values.data(),
             &n,
             &beta,
             &dst(0, 0),
             &m);

        return;
      }

#endif

  const size_type m = n(), n = src.n(), l = this->m();

  // symmetric matrix if the two matrices are the same
  if (PointerComparison::equal(this, &src))
    for (size_type i = 0; i < m; ++i)
      for (size_type j = i; j < m; ++j)
        {
          number2 add_value = 0.;
          for (size_type k = 0; k < l; ++k)
            add_value += static_cast<number2>((*this)(k, i)) *
                         static_cast<number2>((*this)(k, j));
          if (adding)
            {
              dst(i, j) += add_value;
              if (i < j)
                dst(j, i) += add_value;
            }
          else
            dst(i, j) = dst(j, i) = add_value;
        }
  // arrange the loops in a way that we keep write operations low, (writing is
  // usually more costly than reading), even though we need to access the data
  // in src not in a contiguous way. However, we should usually end up in the
  // optimized gemm operation in case the matrix is big, so this shouldn't be
  // too bad.
  else
    for (size_type i = 0; i < m; ++i)
      for (size_type j = 0; j < n; ++j)
        {
          number2 add_value = adding ? dst(i, j) : 0.;
          for (size_type k = 0; k < l; ++k)
            add_value += static_cast<number2>((*this)(k, i)) *
                         static_cast<number2>((src(k, j)));
          dst(i, j) = add_value;
        }
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::mTmult(FullMatrix<number2>       &dst,
                           const FullMatrix<number2> &src,
                           const bool                 adding) const
{
  Assert(!this->empty(), ExcEmptyMatrix());
  Assert(n() == src.n(), ExcDimensionMismatch(n(), src.n()));
  Assert(dst.n() == src.m(), ExcDimensionMismatch(dst.n(), src.m()));
  Assert(dst.m() == m(), ExcDimensionMismatch(m(), dst.m()));

  // see if we can use BLAS algorithms for this and if the type for 'number'
  // works for us (it is usually not efficient to use BLAS for very small
  // matrices):
#ifdef DEAL_II_WITH_LAPACK
  const size_type max_blas_int = std::numeric_limits<types::blas_int>::max();
  if ((std::is_same_v<number, double> ||
       std::is_same_v<number, float>)&&std::is_same_v<number, number2>)
    if (this->n() * this->m() * src.m() > 300 && src.m() <= max_blas_int &&
        this->n() <= max_blas_int && this->m() <= max_blas_int)
      {
        // In case we have the BLAS function gemm detected by CMake, we
        // use that algorithm for matrix-matrix multiplication since it
        // provides better performance than the deal.II native function (it
        // uses cache and register blocking in order to access local data).
        //
        // Note that BLAS/LAPACK stores matrix elements column-wise (i.e., all
        // values in one column, then all in the next, etc.), whereas the
        // FullMatrix stores them row-wise.  We ignore that difference, and
        // give our row-wise data to BLAS, let BLAS build the product of
        // transpose matrices, and read the result as if it were row-wise
        // again. In other words, we calculate (B A^T)^T, which is AB^T.

        const types::blas_int m       = static_cast<types::blas_int>(src.m());
        const types::blas_int n       = static_cast<types::blas_int>(this->m());
        const types::blas_int k       = static_cast<types::blas_int>(this->n());
        const char           *notrans = "n";
        const char           *trans   = "t";

        const number alpha = 1.;
        const number beta  = (adding == true) ? 1. : 0.;

        // Use the BLAS function gemm for calculating the matrix-matrix
        // product.
        gemm(trans,
             notrans,
             &m,
             &n,
             &k,
             &alpha,
             &src(0, 0),
             &k,
             this->values.data(),
             &k,
             &beta,
             &dst(0, 0),
             &m);

        return;
      }

#endif

  const size_type m = this->m(), n = src.m(), l = this->n();

  // symmetric matrix if the two matrices are the same
  if (PointerComparison::equal(this, &src))
    for (size_type i = 0; i < m; ++i)
      for (size_type j = i; j < m; ++j)
        {
          number2 add_value = 0.;
          for (size_type k = 0; k < l; ++k)
            add_value += static_cast<number2>((*this)(i, k)) *
                         static_cast<number2>((*this)(j, k));
          if (adding)
            {
              dst(i, j) += add_value;
              if (i < j)
                dst(j, i) += add_value;
            }
          else
            dst(i, j) = dst(j, i) = add_value;
        }
  else
    // arrange the loops in a way that we keep write operations low, (writing is
    // usually more costly than reading).
    for (size_type i = 0; i < m; ++i)
      for (size_type j = 0; j < n; ++j)
        {
          number2 add_value = adding ? dst(i, j) : 0.;
          for (size_type k = 0; k < l; ++k)
            add_value += static_cast<number2>((*this)(i, k)) *
                         static_cast<number2>(src(j, k));
          dst(i, j) = add_value;
        }
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::TmTmult(FullMatrix<number2>       &dst,
                            const FullMatrix<number2> &src,
                            const bool                 adding) const
{
  Assert(!this->empty(), ExcEmptyMatrix());
  Assert(m() == src.n(), ExcDimensionMismatch(m(), src.n()));
  Assert(n() == dst.m(), ExcDimensionMismatch(n(), dst.m()));
  Assert(src.m() == dst.n(), ExcDimensionMismatch(src.m(), dst.n()));


  // see if we can use BLAS algorithms for this and if the type for 'number'
  // works for us (it is usually not efficient to use BLAS for very small
  // matrices):
#ifdef DEAL_II_WITH_LAPACK
  const size_type max_blas_int = std::numeric_limits<types::blas_int>::max();
  if ((std::is_same_v<number, double> ||
       std::is_same_v<number, float>)&&std::is_same_v<number, number2>)
    if (this->n() * this->m() * src.m() > 300 && src.m() <= max_blas_int &&
        this->n() <= max_blas_int && this->m() <= max_blas_int)
      {
        // In case we have the BLAS function gemm detected by CMake, we
        // use that algorithm for matrix-matrix multiplication since it
        // provides better performance than the deal.II native function (it
        // uses cache and register blocking in order to access local data).
        //
        // Note that BLAS/LAPACK stores matrix elements column-wise (i.e., all
        // values in one column, then all in the next, etc.), whereas the
        // FullMatrix stores them row-wise.  We ignore that difference, and
        // give our row-wise data to BLAS, let BLAS build the product of
        // transpose matrices, and read the result as if it were row-wise
        // again. In other words, we calculate (B A)^T, which is A^T B^T.

        const types::blas_int m     = static_cast<types::blas_int>(src.m());
        const types::blas_int n     = static_cast<types::blas_int>(this->n());
        const types::blas_int k     = static_cast<types::blas_int>(this->m());
        const char           *trans = "t";

        const number alpha = 1.;
        const number beta  = (adding == true) ? 1. : 0.;

        // Use the BLAS function gemm for calculating the matrix-matrix
        // product.
        gemm(trans,
             trans,
             &m,
             &n,
             &k,
             &alpha,
             &src(0, 0),
             &k,
             this->values.data(),
             &n,
             &beta,
             &dst(0, 0),
             &m);

        return;
      }

#endif

  const size_type m = n(), n = src.m(), l = this->m();

  // arrange the loops in a way that we keep write operations low, (writing is
  // usually more costly than reading), even though we need to access the data
  // in the calling matrix in a non-contiguous way, possibly leading to cache
  // misses. However, we should usually end up in the optimized gemm operation
  // in case the matrix is big, so this shouldn't be too bad.
  for (size_type i = 0; i < m; ++i)
    for (size_type j = 0; j < n; ++j)
      {
        number2 add_value = adding ? dst(i, j) : 0.;
        for (size_type k = 0; k < l; ++k)
          add_value += static_cast<number2>((*this)(k, i)) *
                       static_cast<number2>(src(j, k));
        dst(i, j) = add_value;
      }
}


template <typename number>
void
FullMatrix<number>::triple_product(const FullMatrix<number> &A,
                                   const FullMatrix<number> &B,
                                   const FullMatrix<number> &D,
                                   const bool                transpose_B,
                                   const bool                transpose_D,
                                   const number              scaling)
{
  if (transpose_B)
    {
      AssertDimension(B.m(), A.m());
      AssertDimension(B.n(), m());
    }
  else
    {
      AssertDimension(B.n(), A.m());
      AssertDimension(B.m(), m());
    }
  if (transpose_D)
    {
      AssertDimension(D.n(), A.n());
      AssertDimension(D.m(), n());
    }
  else
    {
      AssertDimension(D.m(), A.n());
      AssertDimension(D.n(), n());
    }

  // For all entries of the product
  // AD
  for (size_type i = 0; i < A.m(); ++i)
    for (size_type j = 0; j < n(); ++j)
      {
        // Compute the entry
        number ADij = 0.;
        if (transpose_D)
          for (size_type k = 0; k < A.n(); ++k)
            ADij += A(i, k) * D(j, k);
        else
          for (size_type k = 0; k < A.n(); ++k)
            ADij += A(i, k) * D(k, j);
        // And add it to this after
        // multiplying with the right
        // factor from B
        if (transpose_B)
          for (size_type k = 0; k < m(); ++k)
            this->operator()(k, j) += scaling * ADij * B(i, k);
        else
          for (size_type k = 0; k < m(); ++k)
            this->operator()(k, j) += scaling * ADij * B(k, i);
      }
}


template <typename number>
void
FullMatrix<number>::kronecker_product(const FullMatrix<number> &A,
                                      const FullMatrix<number> &B,
                                      const bool                adding)
{
  Assert(!A.empty(), ExcEmptyMatrix());
  Assert(!B.empty(), ExcEmptyMatrix());

  const size_type m = A.m() * B.m();
  const size_type n = A.n() * B.n();

  if (adding)
    {
      AssertDimension(m, this->m());
      AssertDimension(n, this->n());
    }
  else
    this->reinit(m, n);

  for (size_type i = 0; i < m; ++i)
    for (size_type j = 0; j < n; ++j)
      (*this)(i, j) += A(i / B.m(), j / B.n()) * B(i % B.m(), j % B.n());
}


template <typename number>
template <typename number2>
number2
FullMatrix<number>::matrix_norm_square(const Vector<number2> &v) const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == v.size(), ExcDimensionMismatch(m(), v.size()));
  Assert(n() == v.size(), ExcDimensionMismatch(n(), v.size()));

  number2         sum     = 0.;
  const size_type n_rows  = m();
  const number   *val_ptr = this->values.data();

  for (size_type row = 0; row < n_rows; ++row)
    {
      number2             s              = 0.;
      const number *const val_end_of_row = val_ptr + n_rows;
      const number2      *v_ptr          = v.begin();
      while (val_ptr != val_end_of_row)
        s += number2(*val_ptr++) * number2(*v_ptr++);

      sum += s * numbers::NumberTraits<number2>::conjugate(v(row));
    }

  return sum;
}


template <typename number>
template <typename number2>
number2
FullMatrix<number>::matrix_scalar_product(const Vector<number2> &u,
                                          const Vector<number2> &v) const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == u.size(), ExcDimensionMismatch(m(), u.size()));
  Assert(n() == v.size(), ExcDimensionMismatch(n(), v.size()));

  number2         sum     = 0.;
  const size_type n_rows  = m();
  const size_type n_cols  = n();
  const number   *val_ptr = this->values.data();

  for (size_type row = 0; row < n_rows; ++row)
    {
      number2             s              = number2(0.);
      const number *const val_end_of_row = val_ptr + n_cols;
      const number2      *v_ptr          = v.begin();
      while (val_ptr != val_end_of_row)
        s += number2(*val_ptr++) * number2(*v_ptr++);

      sum += s * u(row);
    }

  return sum;
}



template <typename number>
void
FullMatrix<number>::symmetrize()
{
  Assert(m() == n(), ExcNotQuadratic());

  const size_type N = m();
  for (size_type i = 0; i < N; ++i)
    for (size_type j = i + 1; j < N; ++j)
      {
        const number t = ((*this)(i, j) + (*this)(j, i)) / number(2.);
        (*this)(i, j) = (*this)(j, i) = t;
      };
}


template <typename number>
typename FullMatrix<number>::real_type
FullMatrix<number>::l1_norm() const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  real_type       sum = 0, max = 0;
  const size_type n_rows = m(), n_cols = n();

  for (size_type col = 0; col < n_cols; ++col)
    {
      sum = 0;
      for (size_type row = 0; row < n_rows; ++row)
        sum += std::abs((*this)(row, col));
      if (sum > max)
        max = sum;
    }
  return max;
}



template <typename number>
typename FullMatrix<number>::real_type
FullMatrix<number>::linfty_norm() const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  real_type       sum = 0, max = 0;
  const size_type n_rows = m(), n_cols = n();

  for (size_type row = 0; row < n_rows; ++row)
    {
      sum = 0;
      for (size_type col = 0; col < n_cols; ++col)
        sum += std::abs((*this)(row, col));
      if (sum > max)
        max = sum;
    }
  return max;
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::add(const number a, const FullMatrix<number2> &A)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == A.m(), ExcDimensionMismatch(m(), A.m()));
  Assert(n() == A.n(), ExcDimensionMismatch(n(), A.n()));

  for (size_type i = 0; i < m(); ++i)
    for (size_type j = 0; j < n(); ++j)
      (*this)(i, j) += a * number(A(i, j));
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::add(const number               a,
                        const FullMatrix<number2> &A,
                        const number               b,
                        const FullMatrix<number2> &B)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == A.m(), ExcDimensionMismatch(m(), A.m()));
  Assert(n() == A.n(), ExcDimensionMismatch(n(), A.n()));
  Assert(m() == B.m(), ExcDimensionMismatch(m(), B.m()));
  Assert(n() == B.n(), ExcDimensionMismatch(n(), B.n()));

  for (size_type i = 0; i < m(); ++i)
    for (size_type j = 0; j < n(); ++j)
      (*this)(i, j) += a * number(A(i, j)) + b * number(B(i, j));
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::add(const number               a,
                        const FullMatrix<number2> &A,
                        const number               b,
                        const FullMatrix<number2> &B,
                        const number               c,
                        const FullMatrix<number2> &C)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == A.m(), ExcDimensionMismatch(m(), A.m()));
  Assert(n() == A.n(), ExcDimensionMismatch(n(), A.n()));
  Assert(m() == B.m(), ExcDimensionMismatch(m(), B.m()));
  Assert(n() == B.n(), ExcDimensionMismatch(n(), B.n()));
  Assert(m() == C.m(), ExcDimensionMismatch(m(), C.m()));
  Assert(n() == C.n(), ExcDimensionMismatch(n(), C.n()));


  for (size_type i = 0; i < m(); ++i)
    for (size_type j = 0; j < n(); ++j)
      (*this)(i, j) +=
        a * number(A(i, j)) + b * number(B(i, j)) + c * number(C(i, j));
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::add(const FullMatrix<number2> &src,
                        const number               factor,
                        const size_type            dst_offset_i,
                        const size_type            dst_offset_j,
                        const size_type            src_offset_i,
                        const size_type            src_offset_j)
{
  AssertIndexRange(dst_offset_i, m());
  AssertIndexRange(dst_offset_j, n());
  AssertIndexRange(src_offset_i, src.m());
  AssertIndexRange(src_offset_j, src.n());

  // Compute maximal size of copied block
  const size_type rows = std::min(m() - dst_offset_i, src.m() - src_offset_i);
  const size_type cols = std::min(n() - dst_offset_j, src.n() - src_offset_j);

  for (size_type i = 0; i < rows; ++i)
    for (size_type j = 0; j < cols; ++j)
      (*this)(dst_offset_i + i, dst_offset_j + j) +=
        factor * number(src(src_offset_i + i, src_offset_j + j));
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::Tadd(const FullMatrix<number2> &src,
                         const number               factor,
                         const size_type            dst_offset_i,
                         const size_type            dst_offset_j,
                         const size_type            src_offset_i,
                         const size_type            src_offset_j)
{
  AssertIndexRange(dst_offset_i, m());
  AssertIndexRange(dst_offset_j, n());
  AssertIndexRange(src_offset_i, src.n());
  AssertIndexRange(src_offset_j, src.m());

  // Compute maximal size of copied block
  const size_type rows = std::min(m() - dst_offset_i, src.n() - src_offset_j);
  const size_type cols = std::min(n() - dst_offset_j, src.m() - src_offset_i);


  for (size_type i = 0; i < rows; ++i)
    for (size_type j = 0; j < cols; ++j)
      (*this)(dst_offset_i + i, dst_offset_j + j) +=
        factor * number(src(src_offset_i + j, src_offset_j + i));
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::Tadd(const number a, const FullMatrix<number2> &A)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(m() == n(), ExcNotQuadratic());
  Assert(m() == A.m(), ExcDimensionMismatch(m(), A.m()));
  Assert(n() == A.n(), ExcDimensionMismatch(n(), A.n()));

  for (size_type i = 0; i < n(); ++i)
    for (size_type j = 0; j < m(); ++j)
      (*this)(i, j) += a * number(A(j, i));
}


template <typename number>
bool
FullMatrix<number>::operator==(const FullMatrix<number> &M) const
{
  // simply pass down to the base class
  return Table<2, number>::operator==(M);
}


namespace internal
{
  // LAPACKFullMatrix is not implemented for
  // complex numbers or long doubles
  template <typename number, typename = void>
  struct Determinant
  {
    static number
    value(const FullMatrix<number> &)
    {
      AssertThrow(false, ExcNotImplemented());
      return 0.0;
    }
  };


  // LAPACKFullMatrix is only implemented for
  // floats and doubles
  template <typename number>
  struct Determinant<number,
                     std::enable_if_t<std::is_same_v<number, float> ||
                                      std::is_same_v<number, double>>>
  {
#ifdef DEAL_II_WITH_LAPACK
    static number
    value(const FullMatrix<number> &A)
    {
      using s_type = typename LAPACKFullMatrix<number>::size_type;
      AssertIndexRange(A.m() - 1, std::numeric_limits<s_type>::max());
      AssertIndexRange(A.n() - 1, std::numeric_limits<s_type>::max());
      LAPACKFullMatrix<number> lp_A(static_cast<s_type>(A.m()),
                                    static_cast<s_type>(A.n()));
      lp_A = A;
      lp_A.compute_lu_factorization();
      return lp_A.determinant();
    }
#else
    static number
    value(const FullMatrix<number> &)
    {
      AssertThrow(false, ExcNeedsLAPACK());
      return 0.0;
    }
#endif
  };

} // namespace internal


template <typename number>
number
FullMatrix<number>::determinant() const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(this->n_cols() == this->n_rows(),
         ExcDimensionMismatch(this->n_cols(), this->n_rows()));

  switch (this->n_cols())
    {
      case 1:
        return (*this)(0, 0);
      case 2:
        return (*this)(0, 0) * (*this)(1, 1) - (*this)(1, 0) * (*this)(0, 1);
      case 3:
        return ((*this)(0, 0) * (*this)(1, 1) * (*this)(2, 2) -
                (*this)(0, 0) * (*this)(1, 2) * (*this)(2, 1) -
                (*this)(1, 0) * (*this)(0, 1) * (*this)(2, 2) +
                (*this)(1, 0) * (*this)(0, 2) * (*this)(2, 1) +
                (*this)(2, 0) * (*this)(0, 1) * (*this)(1, 2) -
                (*this)(2, 0) * (*this)(0, 2) * (*this)(1, 1));
      default:
        return internal::Determinant<number>::value(*this);
    };
}



template <typename number>
number
FullMatrix<number>::trace() const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(this->n_cols() == this->n_rows(),
         ExcDimensionMismatch(this->n_cols(), this->n_rows()));

  number tr = 0;
  for (size_type i = 0; i < this->n_rows(); ++i)
    tr += (*this)(i, i);

  return tr;
}



template <typename number>
typename FullMatrix<number>::real_type
FullMatrix<number>::frobenius_norm() const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  real_type s = 0.;
  for (size_type i = 0; i < this->n_rows() * this->n_cols(); ++i)
    s += numbers::NumberTraits<number>::abs_square(this->values[i]);
  return std::sqrt(s);
}



template <typename number>
typename FullMatrix<number>::real_type
FullMatrix<number>::relative_symmetry_norm2() const
{
  Assert(!this->empty(), ExcEmptyMatrix());

  real_type s = 0.;
  real_type a = 0.;
  for (size_type i = 0; i < this->n_rows(); ++i)
    for (size_type j = 0; j < this->n_cols(); ++j)
      {
        const number x_ij = (*this)(i, j);
        const number x_ji = (*this)(j, i);

        a += numbers::NumberTraits<number>::abs_square(x_ij - x_ji);
        s += numbers::NumberTraits<number>::abs_square(x_ij);
      }

  if (s != 0.)
    return std::sqrt(a) / std::sqrt(s);
  return 0;
}



template <typename number>
template <typename number2>
void
FullMatrix<number>::invert(const FullMatrix<number2> &M)
{
  Assert(!this->empty(), ExcEmptyMatrix());

  Assert(this->n_cols() == this->n_rows(), ExcNotQuadratic());
  Assert(this->n_cols() == M.n_cols(),
         ExcDimensionMismatch(this->n_cols(), M.n_cols()));
  Assert(this->n_rows() == M.n_rows(),
         ExcDimensionMismatch(this->n_rows(), M.n_rows()));

  switch (this->n_cols())
    {
      case 1:
        (*this)(0, 0) = number2(1.0) / M(0, 0);
        return;
      case 2:
        {
          const number2 M00 = M(0, 0);
          const number2 M01 = M(0, 1);
          const number2 M10 = M(1, 0);
          const number2 M11 = M(1, 1);
          const number2 t4  = number2(1.0) / (M00 * M11 - M01 * M10);
          (*this)(0, 0)     = M11 * t4;
          (*this)(0, 1)     = -M01 * t4;
          (*this)(1, 0)     = -M10 * t4;
          (*this)(1, 1)     = M00 * t4;
          return;
        };

      case 3:
        {
          const number2 M00 = M(0, 0);
          const number2 M01 = M(0, 1);
          const number2 M02 = M(0, 2);
          const number2 M10 = M(1, 0);
          const number2 M11 = M(1, 1);
          const number2 M12 = M(1, 2);
          const number2 M20 = M(2, 0);
          const number2 M21 = M(2, 1);
          const number2 M22 = M(2, 2);
          const number2 t00 = M11 * M22 - M12 * M21;
          const number2 t10 = M12 * M20 - M10 * M22;
          const number2 t20 = M10 * M21 - M11 * M20;
          const number2 inv_det =
            number2(1.0) / (M00 * t00 + M01 * t10 + M02 * t20);
          (*this)(0, 0) = t00 * inv_det;
          (*this)(0, 1) = (M02 * M21 - M01 * M22) * inv_det;
          (*this)(0, 2) = (M01 * M12 - M02 * M11) * inv_det;
          (*this)(1, 0) = t10 * inv_det;
          (*this)(1, 1) = (M00 * M22 - M02 * M20) * inv_det;
          (*this)(1, 2) = (M02 * M10 - M00 * M12) * inv_det;
          (*this)(2, 0) = t20 * inv_det;
          (*this)(2, 1) = (M01 * M20 - M00 * M21) * inv_det;
          (*this)(2, 2) = (M00 * M11 - M01 * M10) * inv_det;
          return;
        };

      case 4:
        {
          // Initially derived from the following maple script
          //
          // with (linalg);
          // a:=matrix(4,4);
          // evalm(a);
          // ai:=inverse(a);
          // readlib(C);
          // C(ai,optimized,filename=x4);
          //
          // but then combined re-occurring terms via distributive law in an
          // FMA-friendly format
          const number2 M00 = M(0, 0);
          const number2 M01 = M(0, 1);
          const number2 M02 = M(0, 2);
          const number2 M03 = M(0, 3);
          const number2 M10 = M(1, 0);
          const number2 M11 = M(1, 1);
          const number2 M12 = M(1, 2);
          const number2 M13 = M(1, 3);
          const number2 M20 = M(2, 0);
          const number2 M21 = M(2, 1);
          const number2 M22 = M(2, 2);
          const number2 M23 = M(2, 3);
          const number2 M30 = M(3, 0);
          const number2 M31 = M(3, 1);
          const number2 M32 = M(3, 2);
          const number2 M33 = M(3, 3);

          const number2 t14 = M00 * M11 - M10 * M01;
          const number2 t15 = M22 * M33 - M23 * M32;
          const number2 t19 = M00 * M21 - M20 * M01;
          const number2 t20 = M12 * M33 - M13 * M32;
          const number2 t24 = M00 * M31 - M30 * M01;
          const number2 t25 = M12 * M23 - M13 * M22;
          const number2 t32 = M10 * M21 - M20 * M11;
          const number2 t33 = M02 * M33 - M03 * M32;
          const number2 t37 = M10 * M31 - M30 * M11;
          const number2 t38 = M02 * M23 - M03 * M22;
          const number2 t49 = M20 * M31 - M30 * M21;
          const number2 t50 = M02 * M13 - M03 * M12;
          const number2 det = t14 * t15 - t19 * t20 + t24 * t25 + t32 * t33 -
                              t37 * t38 + t49 * t50;
          const number2 inv_det = number2(1.0) / det;
          const number2 t81     = M01 * M12 - M02 * M11;
          const number2 t83     = M01 * M13 - M03 * M11;
          const number2 t93     = M01 * M22 - M02 * M21;
          const number2 t95     = M11 * M23 - M13 * M21;
          const number2 t97     = M01 * M23 - M03 * M21;
          const number2 t99     = M11 * M22 - M12 * M21;
          const number2 t101    = M10 * M22 - M20 * M12;
          const number2 t103    = M10 * M23 - M20 * M13;
          const number2 t115    = M00 * M22 - M20 * M02;
          const number2 t117    = M00 * M23 - M20 * M03;
          const number2 t129    = M00 * M12 - M10 * M02;
          const number2 t131    = M00 * M13 - M10 * M03;

          (*this)(0, 0) = (M11 * t15 - M21 * t20 + M31 * t25) * inv_det;
          (*this)(0, 1) = -(M01 * t15 - M21 * t33 + M31 * t38) * inv_det;
          (*this)(0, 2) = (M33 * t81 - M32 * t83 + M31 * t50) * inv_det;
          (*this)(0, 3) = -(M23 * t81 - M22 * t83 + M21 * t50) * inv_det;
          (*this)(1, 0) = -(M33 * t101 - M32 * t103 + M30 * t25) * inv_det;
          (*this)(1, 1) = (M33 * t115 - M32 * t117 + M30 * t38) * inv_det;
          (*this)(1, 2) = -(M33 * t129 - M32 * t131 + M30 * t50) * inv_det;
          (*this)(1, 3) = (M23 * t129 - M22 * t131 + M20 * t50) * inv_det;
          (*this)(2, 0) = (M33 * t32 - M31 * t103 + M30 * t95) * inv_det;
          (*this)(2, 1) = -(M33 * t19 - M31 * t117 + M30 * t97) * inv_det;
          (*this)(2, 2) = (M33 * t14 - M31 * t131 + M30 * t83) * inv_det;
          (*this)(2, 3) = -(M23 * t14 - M21 * t131 + M20 * t83) * inv_det;
          (*this)(3, 0) = -(M32 * t32 - M31 * t101 + M30 * t99) * inv_det;
          (*this)(3, 1) = (M32 * t19 - M31 * t115 + M30 * t93) * inv_det;
          (*this)(3, 2) = -(M32 * t14 - M31 * t129 + M30 * t81) * inv_det;
          (*this)(3, 3) = (M22 * t14 - M21 * t129 + M20 * t81) * inv_det;

          break;
        }


      default:
        // if no inversion is
        // hardcoded, fall back
        // to use the
        // Gauss-Jordan algorithm
        if (!PointerComparison::equal(&M, this))
          *this = M;
        gauss_jordan();
    };
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::cholesky(const FullMatrix<number2> &A)
{
  Assert(!A.empty(), ExcEmptyMatrix());
  Assert(A.n() == A.m(), ExcNotQuadratic());
  // Matrix must be symmetric.
  Assert(A.relative_symmetry_norm2() < 1.0e-10,
         ExcMessage("A must be symmetric."));

  if (PointerComparison::equal(&A, this))
    {
      // avoid overwriting source
      // by destination matrix:
      const FullMatrix<number> A2 = *this;
      cholesky(A2);
    }
  else
    {
      /* reinit *this to 0 */
      this->reinit(A.m(), A.n());

      for (size_type i = 0; i < this->n_cols(); ++i)
        {
          double SLik2 = 0.0;
          for (size_type j = 0; j < i; ++j)
            {
              double SLikLjk = 0.0;
              for (size_type k = 0; k < j; ++k)
                {
                  SLikLjk += (*this)(i, k) * (*this)(j, k);
                };
              (*this)(i, j) = (1. / (*this)(j, j)) * (A(i, j) - SLikLjk);
              SLik2 += (*this)(i, j) * (*this)(i, j);
            }
          AssertThrow(A(i, i) - SLik2 >= 0, ExcMatrixNotPositiveDefinite());

          (*this)(i, i) = std::sqrt(A(i, i) - SLik2);
        }
    }
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::outer_product(const Vector<number2> &V,
                                  const Vector<number2> &W)
{
  Assert(V.size() == W.size(),
         ExcMessage("Vectors V, W must be the same size."));
  this->reinit(V.size(), V.size());

  for (size_type i = 0; i < this->n(); ++i)
    {
      for (size_type j = 0; j < this->n(); ++j)
        {
          (*this)(i, j) = V(i) * W(j);
        }
    }
}


template <typename number>
template <typename number2>
void
FullMatrix<number>::left_invert(const FullMatrix<number2> &A)
{
  Assert(!A.empty(), ExcEmptyMatrix());

  // If the matrix is square, simply do a
  // standard inversion
  if (A.m() == A.n())
    {
      FullMatrix<number2> left_inv(A.n(), A.m());
      left_inv.invert(A);
      *this = std::move(left_inv);
      return;
    }

  Assert(A.m() > A.n(), ExcDimensionMismatch(A.m(), A.n()));
  Assert(this->m() == A.n(), ExcDimensionMismatch(this->m(), A.n()));
  Assert(this->n() == A.m(), ExcDimensionMismatch(this->n(), A.m()));

  FullMatrix<number2> A_t(A.n(), A.m());
  FullMatrix<number2> A_t_times_A(A.n(), A.n());
  FullMatrix<number2> A_t_times_A_inv(A.n(), A.n());
  FullMatrix<number2> left_inv(A.n(), A.m());

  A_t.Tadd(A, 1);
  A_t.mmult(A_t_times_A, A);
  if (number(A_t_times_A.determinant()) == number(0))
    {
      Assert(false, ExcSingular());
    }
  else
    {
      A_t_times_A_inv.invert(A_t_times_A);
      A_t_times_A_inv.mmult(left_inv, A_t);

      *this = left_inv;
    }
}

template <typename number>
template <typename number2>
void
FullMatrix<number>::right_invert(const FullMatrix<number2> &A)
{
  Assert(!A.empty(), ExcEmptyMatrix());

  // If the matrix is square, simply do a
  // standard inversion
  if (A.m() == A.n())
    {
      FullMatrix<number2> right_inv(A.n(), A.m());
      right_inv.invert(A);
      *this = std::move(right_inv);
      return;
    }

  Assert(A.n() > A.m(), ExcDimensionMismatch(A.n(), A.m()));
  Assert(this->m() == A.n(), ExcDimensionMismatch(this->m(), A.n()));
  Assert(this->n() == A.m(), ExcDimensionMismatch(this->n(), A.m()));

  FullMatrix<number> A_t(A.n(), A.m());
  FullMatrix<number> A_times_A_t(A.m(), A.m());
  FullMatrix<number> A_times_A_t_inv(A.m(), A.m());
  FullMatrix<number> right_inv(A.n(), A.m());

  A_t.Tadd(A, 1);
  A.mmult(A_times_A_t, A_t);
  if (number(A_times_A_t.determinant()) == number(0))
    {
      Assert(false, ExcSingular());
    }
  else
    {
      A_times_A_t_inv.invert(A_times_A_t);
      A_t.mmult(right_inv, A_times_A_t_inv);

      *this = right_inv;
    }
}



template <typename number>
template <typename somenumber>
void
FullMatrix<number>::precondition_Jacobi(Vector<somenumber>       &dst,
                                        const Vector<somenumber> &src,
                                        const number              om) const
{
  Assert(m() == n(), ExcNotQuadratic());
  Assert(dst.size() == n(), ExcDimensionMismatch(dst.size(), n()));
  Assert(src.size() == n(), ExcDimensionMismatch(src.size(), n()));

  const std::size_t n       = src.size();
  somenumber       *dst_ptr = dst.begin();
  const somenumber *src_ptr = src.begin();

  for (size_type i = 0; i < n; ++i, ++dst_ptr, ++src_ptr)
    *dst_ptr = somenumber(om) * *src_ptr / somenumber((*this)(i, i));
}



template <typename number>
void
FullMatrix<number>::print_formatted(std::ostream      &out,
                                    const unsigned int precision,
                                    const bool         scientific,
                                    const unsigned int width_,
                                    const char        *zero_string,
                                    const double       denominator,
                                    const double       threshold,
                                    const char        *separator) const
{
  unsigned int width = width_;

  Assert((!this->empty()) || (this->n_cols() + this->n_rows() == 0),
         ExcInternalError());

  // set output format, but store old
  // state
  std::ios::fmtflags old_flags     = out.flags();
  std::streamsize    old_precision = out.precision(precision);

  if (scientific)
    {
      out.setf(std::ios::scientific, std::ios::floatfield);
      if (width == 0u)
        width = precision + 7;
    }
  else
    {
      out.setf(std::ios::fixed, std::ios::floatfield);
      if (width == 0u)
        width = precision + 2;
    }

  for (size_type i = 0; i < m(); ++i)
    {
      for (size_type j = 0; j < n(); ++j)
        // we might have complex numbers, so use abs also to check for nan
        // since there is no isnan on complex numbers
        if (numbers::is_nan(std::abs((*this)(i, j))))
          out << std::setw(width) << (*this)(i, j) << separator;
        else if (std::abs((*this)(i, j)) > threshold)
          out << std::setw(width) << (*this)(i, j) * number(denominator)
              << separator;
        else
          out << std::setw(width) << zero_string << separator;
      out << std::endl;
    };

  AssertThrow(out.fail() == false, ExcIO());
  // reset output format
  out.flags(old_flags);
  out.precision(old_precision);
}


template <typename number>
void
FullMatrix<number>::gauss_jordan()
{
  Assert(!this->empty(), ExcEmptyMatrix());
  Assert(this->n_cols() == this->n_rows(), ExcNotQuadratic());

  // see if we can use Lapack algorithms
  // for this and if the type for 'number'
  // works for us (it is usually not
  // efficient to use Lapack for very small
  // matrices):
#ifdef DEAL_II_WITH_LAPACK
  if (std::is_same_v<number, double> || std::is_same_v<number, float>)
    if (this->n_cols() > 15 && static_cast<types::blas_int>(this->n_cols()) <=
                                 std::numeric_limits<types::blas_int>::max())
      {
        // In case we have the LAPACK functions
        // getrf and getri detected by CMake,
        // we use these algorithms for inversion
        // since they provide better performance
        // than the deal.II native functions.
        //
        // Note that BLAS/LAPACK stores matrix
        // elements column-wise (i.e., all values in
        // one column, then all in the next, etc.),
        // whereas the FullMatrix stores them
        // row-wise.
        // We ignore that difference, and give our
        // row-wise data to LAPACK,
        // let LAPACK build the inverse of the
        // transpose matrix, and read the result as
        // if it were row-wise again. In other words,
        // we just got ((A^T)^{-1})^T, which is
        // A^{-1}.

        const types::blas_int nn = static_cast<types::blas_int>(this->n());

        // workspace for permutations
        std::vector<types::blas_int> ipiv(nn);
        types::blas_int              info;

        // Use the LAPACK function getrf for
        // calculating the LU factorization.
        getrf(&nn, &nn, this->values.data(), &nn, ipiv.data(), &info);

        Assert(info >= 0, ExcInternalError());
        Assert(info == 0, LACExceptions::ExcSingular());

        // scratch array
        std::vector<number> inv_work(nn);

        // Use the LAPACK function getri for
        // calculating the actual inverse using
        // the LU factorization.
        getri(&nn,
              this->values.data(),
              &nn,
              ipiv.data(),
              inv_work.data(),
              &nn,
              &info);

        Assert(info >= 0, ExcInternalError());
        Assert(info == 0, LACExceptions::ExcSingular());

        return;
      }

#endif

  // otherwise do it by hand. use the
  // Gauss-Jordan-Algorithm from
  // Stoer & Bulirsch I (4th Edition)
  // p. 153
  const size_type N = n();

  // first get an estimate of the
  // size of the elements of this
  // matrix, for later checks whether
  // the pivot element is large
  // enough, or whether we have to
  // fear that the matrix is not
  // regular
  double diagonal_sum = 0;
  for (size_type i = 0; i < N; ++i)
    diagonal_sum += std::abs((*this)(i, i));
  const double typical_diagonal_element = diagonal_sum / N;

  // initialize the array that holds
  // the permutations that we find
  // during pivot search
  std::vector<size_type> p(N);
  for (size_type i = 0; i < N; ++i)
    p[i] = i;

  for (size_type j = 0; j < N; ++j)
    {
      // pivot search: search that
      // part of the line on and
      // right of the diagonal for
      // the largest element
      real_type max = std::abs((*this)(j, j));
      size_type r   = j;
      for (size_type i = j + 1; i < N; ++i)
        {
          if (std::abs((*this)(i, j)) > max)
            {
              max = std::abs((*this)(i, j));
              r   = i;
            }
        }
      // check whether the pivot is
      // too small
      Assert(max > 1.e-16 * typical_diagonal_element, ExcNotRegular(max));

      // row interchange
      if (r > j)
        {
          for (size_type k = 0; k < N; ++k)
            std::swap((*this)(j, k), (*this)(r, k));

          std::swap(p[j], p[r]);
        }

      // transformation
      const number hr = number(1.) / (*this)(j, j);
      (*this)(j, j)   = hr;
      for (size_type k = 0; k < N; ++k)
        {
          if (k == j)
            continue;
          for (size_type i = 0; i < N; ++i)
            {
              if (i == j)
                continue;
              (*this)(i, k) -= (*this)(i, j) * (*this)(j, k) * hr;
            }
        }
      for (size_type i = 0; i < N; ++i)
        {
          (*this)(i, j) *= hr;
          (*this)(j, i) *= -hr;
        }
      (*this)(j, j) = hr;
    }
  // column interchange
  std::vector<number> hv(N);
  for (size_type i = 0; i < N; ++i)
    {
      for (size_type k = 0; k < N; ++k)
        hv[p[k]] = (*this)(i, k);
      for (size_type k = 0; k < N; ++k)
        (*this)(i, k) = hv[k];
    }
}



template <typename number>
std::size_t
FullMatrix<number>::memory_consumption() const
{
  return (sizeof(*this) - sizeof(Table<2, number>) +
          Table<2, number>::memory_consumption());
}


DEAL_II_NAMESPACE_CLOSE

#endif
