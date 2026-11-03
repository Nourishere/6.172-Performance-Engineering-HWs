// Copyright (c) 2012 MIT License by 6.172 Staff

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

typedef uint32_t data_t;
#define THRESHOLD 64 // For recursion coarsening

void mem_alloc(data_t** space, int size);
void mem_free(data_t** space);

#endif  // UTIL_H
