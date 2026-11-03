/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/


#include "./util.h"

// Function prototypes
static void merge_f(data_t* A, int p, int q, int r, data_t * scratch);
static void copy_f(data_t* source, data_t* dest, int n);
static inline void sort_impl(data_t* A, int p, int r, data_t * scratch);
void isort(data_t* begin, data_t* end);

// A basic merge sort routine that sorts the subarray A[p..r]
void sort_f(data_t* A, int p, int r) {
  int n = r - p + 1;
  data_t* scratch = NULL;
  mem_alloc(&scratch, (n + 1) / 2);
  if (scratch == NULL)
    return;
  sort_impl(A, p, r, scratch);
  mem_free(&scratch);
}

static inline void sort_impl(data_t* A, int p, int r, data_t * scratch)
{
  assert(A);
  if (r - p + 1 < THRESHOLD) {
    isort(A + p, A + r);
    return;
  }

  if (p < r) {
    int q = (p + r) / 2;
    sort_impl(A, p, q, scratch);
    sort_impl(A, q + 1, r, scratch);
    merge_f(A, p, q, r, scratch);
  }
}

// A merge routine. Merges the sub-arrays A [p..q] and A [q + 1..r].
// Uses two arrays 'left' and 'right' in the merge operation.
static void merge_f(data_t* A, int p, int q, int r, data_t * scratch) {
  assert(A);
  assert(p <= q);
  assert((q + 1) <= r);
  int n1 = q - p + 1;


  copy_f(&(A[p]), scratch, n1);
  scratch[n1] = UINT_MAX;

  int i = 0;
  int j = 0;

  for (int k = p; k <= r; k++) {
    if (scratch[i] <= A[j]) {
      A[k] = scratch[i];
      i++;
    } else {
      A[k] = A[j];
      j++;
    }
  }
}

static inline void copy_f(data_t* source, data_t* dest, int n) {
  assert(dest);
  assert(source);

  for (int i = 0 ; i < n ; i++) {
    dest[i] = source[i];
  }
}
