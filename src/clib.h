#ifndef _CLIB_H_
#define _CLIB_H_

// Basic types
typedef unsigned long long u64;
typedef unsigned int u32;
typedef long long i64;
typedef int i32;
typedef double f64;
typedef float f32;

_Static_assert (sizeof(u64) == 8, "u64 is not 8 bytes");
_Static_assert (sizeof(u32) == 4, "u32 is not 4 bytes");
_Static_assert (sizeof(i64) == 8, "i64 is not 8 bytes");
_Static_assert (sizeof(i32) == 4, "i32 is not 4 bytes");
_Static_assert (sizeof(f64) == 8, "f64 is not 8 bytes");
_Static_assert (sizeof(f32) == 4, "f32 is not 4 bytes");

// This macro requires <stdlib.h> and <stdio.h>
#define CLIB_ASSERT(condition, msg) do {if (!(condition)) {fprintf(stderr, "Error in %s() %s:%d -> %s\n", __func__, __FILE__, __LINE__, msg); exit(-1);}} while(0)
//#define CLIB_ASSERT(condition, msg) do {} while (0)
#define CLIB_WARNING(msg) do {printf("Warning in %s() %s:%d -> %s\n", __func__, __FILE__, __LINE__, msg); exit(-1);} while(0)

// ---------- Arenas ----------

typedef struct clib_arena_block
{
	void *next_block;
} clib_arena_block;

typedef struct clib_arena_freelist
{
	u64 size;
	struct clib_arena_freelist *next;
	struct clib_arena_freelist *prev;
} clib_arena_freelist;

typedef struct clib_arena
{
	clib_arena_block block; // clib_arena is also a valid clib_arena_block
							// because its block metadata is stored within the first block!

	clib_arena_block *current_block;
	u64 current_index;

	clib_arena_freelist *freelist; // Freelist, ordered from biggest free block to smallest...

	u64 block_size;

	u64 total_allocation_size;
	u64 num_extra_blocks_allocated;
	u64 num_allocations;
} clib_arena;

clib_arena *clib_arena_init(u64 block_size);
void clib_arena_destroy(clib_arena **a); // Deallocates ALL blocks in arena and zeros it out. Sets arena ptr to NULL

void clib_arena_reset(clib_arena *a); // Sets arena back to beginning. Doesn't deallocate any blocks
void clib_arena_shrink(clib_arena *a); // Deallocate all unused blocks, keeping ones with memory in! TODO: 

void* clib_arena_alloc(clib_arena *a, u64 size);
void* clib_arena_calloc(clib_arena *a, u64 size); // Allocates AND zeroes the new memory

void clib_arena_print_info(clib_arena *a);

void _clib_arena_freelist_insert(clib_arena *a, clib_arena_freelist *f);

// ---------- Vectors ----------

#define CLIB_VECTOR_DEFAULT_COUNT 16

typedef struct clib_vector
{
	void *data;
	u64 count;
	u64 capacity;
	u64 type; // Size in bytes of the type stored in the vector
} clib_vector;

void clib_vector_init(clib_vector *vector, u64 type);
void clib_vector_init_reserve(clib_vector *vector, u64 type, u64 capacity);
void clib_vector_destroy(clib_vector *vector);

void clib_vector_resize(clib_vector *vector, u64 new_capacity);

void *clib_vector_at(clib_vector *vector, u64 index);
void clib_vector_push(clib_vector *vector, void *element);
void clib_vector_pop(clib_vector *vector);

// ---------- Random numbers ----------
// Adapted from https://www.pcg-random.org/, see full license at EOF

typedef struct clib_prng
{
	u64 state;
	u64 inc;
} clib_prng;

void clib_prng_init(clib_prng *rng);
void clib_prng_init_seed(clib_prng *rng, u64 initstate, u64 initseq);
u32 clib_prng_rand_u32(clib_prng *rng);

u32 clib_prng_rand_u32_range(clib_prng *rng, u32 min, u32 max);
i32 clib_prng_rand_i32_range(clib_prng *rng, i32 min, i32 max);
f32 clib_prng_rand_f32(clib_prng *rng);

// ---------- Files ----------

i32 clib_file_read(clib_arena *arena, const char *path, char **out_data, u64 *out_size);

/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *     http://www.pcg-random.org
 */

/*
 * This code is derived from the full C implementation, which is in turn
 * derived from the canonical C++ PCG implementation. The C++ version
 * has many additional features and is preferable if you can use C++ in
 * your project.
 */

#endif // _CLIB_H_
