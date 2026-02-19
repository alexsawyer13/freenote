#include "clib.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// ---------- Arenas ----------

void clib_arena_init(clib_arena *a, u64 block_size)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(block_size > 0, "block_size is 0");

	a->first_block = malloc(block_size);
	CLIB_ASSERT(a->first_block, "malloc failed");
	memset(a->first_block, 0, block_size);

	a->current_block = a->first_block;
	a->current_index = sizeof(clib_arena_block);

	a->block_size = block_size;
}

void clib_arena_destroy(clib_arena *a)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(a->first_block, "a has no blocks!");
	
	clib_arena_block *block = a->first_block;
	while (block)
	{
		clib_arena_block *new_block = block->next_block;
		free(block);
		block = new_block;
	}
}

void clib_arena_reset(clib_arena *a)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(a->first_block, "a has no blocks!");

	clib_arena_block *block = a->first_block->next_block;
	while (block)
	{
		clib_arena_block *new_block = block->next_block;
		free(block);
		block = new_block;
	}

	a->first_block->next_block = NULL;
	a->current_index = sizeof(clib_arena_block);
}

void* clib_arena_alloc(clib_arena *a, u64 size)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(size > 0, "size is 0");
	CLIB_ASSERT((size + sizeof(clib_arena_block)) < a->block_size, "allocation is bigger than a block!");


	// If it can't fit in current block, allocate a new one
	if (a->current_index + size > a->block_size)
	{
		clib_arena_block *new_block = malloc(a->block_size);
		CLIB_ASSERT(new_block, "Failed to allocate new block");
		a->current_block->next_block = new_block;
		a->current_block = new_block;
		a->current_index = sizeof(clib_arena_block);
	}

	// Now we definitely have a valid spot for the memory to go!
	void *ptr = a->current_block + a->current_index;
	a->current_index += size;
	return ptr;
}

// ---------- Vectors ----------

void clib_vector_init(clib_vector *vector, u64 type)
{
	clib_vector_init_reserve(vector, type, CLIB_VECTOR_DEFAULT_COUNT);
}

void clib_vector_init_reserve(clib_vector *vector, u64 type, u64 capacity)
{
	CLIB_ASSERT(vector, "vector is NULL");
	CLIB_ASSERT(type > 0, "type has size 0");
	CLIB_ASSERT(capacity > 0, "capacity has size 0");
	CLIB_ASSERT(vector->data == NULL, "Already has data!");

	vector->data = malloc(type * capacity);
	CLIB_ASSERT(vector->data, "malloc failed");

	vector->type = type;
	vector->count = 0;
	vector->capacity = capacity;
}

void clib_vector_destroy(clib_vector *vector)
{
	CLIB_ASSERT(vector, "vector is NULL");
	CLIB_ASSERT(vector->data, "vector has no data");
	free(vector->data);
	vector->data = NULL;
	vector->type = 0;
	vector->capacity = 0;
	vector->count = 0;
}

void clib_vector_resize(clib_vector *vector, u64 new_capacity)
{
	CLIB_ASSERT(vector, "vector is NULL");
	CLIB_ASSERT(vector->data, "vector has no data");
	CLIB_ASSERT(vector->type > 0, "type has size 0");
	CLIB_ASSERT(vector->capacity > 0, "capacity has size 0");
	CLIB_ASSERT(new_capacity > 0, "new_capacity has size 0");

	void *new = realloc(vector->data, new_capacity * vector->type);
	CLIB_ASSERT(new, "realloc failed");

	vector->data = new;
	vector->capacity = new_capacity;
}

void *clib_vector_at(clib_vector *vector, u64 index)
{
	CLIB_ASSERT(vector, "vector is NULL");
	CLIB_ASSERT(vector->data, "vector has no data");
	CLIB_ASSERT(vector->type > 0, "type has size 0");
	CLIB_ASSERT(vector->capacity > 0, "capacity has size 0");
	CLIB_ASSERT(vector->count > index, "index out of bounds!");
	return vector->data + index * vector->type;
}

void clib_vector_push(clib_vector *vector, void *element)
{
	CLIB_ASSERT(vector, "vector is NULL");
	CLIB_ASSERT(element, "element is NULL");
	CLIB_ASSERT(vector->data, "vector has no data");
	CLIB_ASSERT(vector->type > 0, "type has size 0");
	CLIB_ASSERT(vector->capacity > 0, "capacity has size 0");

	if (vector->count + 1 > vector->capacity)
	{
		clib_vector_resize(vector, vector->capacity * 2);
	}

	memcpy(vector->data + vector->count * vector->type, element, vector->type);
	vector->count++;
}

void clib_vector_pop(clib_vector *vector)
{
	CLIB_ASSERT(vector, "vector is NULL");
	CLIB_ASSERT(vector->data, "vector has no data");
	CLIB_ASSERT(vector->type > 0, "type has size 0");
	CLIB_ASSERT(vector->capacity > 0, "capacity has size 0");
	CLIB_ASSERT(vector->count > 0, "capacity has size 0");

	vector->count--;
}

// ---------- Random numbers ----------

void clib_prng_init(clib_prng *rng)
{
	CLIB_ASSERT(rng, "rng is NULL");
	clib_prng_init_seed(rng, (u64)time(NULL), (u64)&clib_prng_init_seed);
}

void clib_prng_init_seed(clib_prng *rng, u64 initstate, u64 initseq)
{
	CLIB_ASSERT(rng, "rng is NULL");
	rng->state = 0U;
    rng->inc = (initseq << 1u) | 1u;
    clib_prng_rand_u32(rng);
    rng->state += initstate;
    clib_prng_rand_u32(rng);
}

u32 clib_prng_rand_u32(clib_prng *rng)
{
	CLIB_ASSERT(rng, "rng is NULL");
    u64 oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + rng->inc;
    u32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    u32 rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

u32 clib_prng_rand_u32_range(clib_prng *rng, u32 min, u32 max)
{
	CLIB_ASSERT(max > min, "max !> min");
	return clib_prng_rand_u32(rng) % (max - min + 1) + min;
}

i32 clib_prng_rand_i32_range(clib_prng *rng, i32 min, i32 max)
{
	CLIB_ASSERT(max > min, "max !> min");
	return (i32)((i64)clib_prng_rand_u32(rng) % (i64)(max - min + 1) + (i64)min);
}

f32 clib_prng_rand_f32(clib_prng *rng)
{
	return ((f32)clib_prng_rand_u32(rng))/((f32)((u32)-1));
}

// ---------- Files ----------

i32 clib_file_read(clib_arena *arena, const char *path, char **out_data, u64 *out_size)
{
	FILE *f;
	char *buffer;
	u64 size;
	u64 result;

	*out_data = NULL;
	*out_size = 0;

	f = fopen(path, "r");
	if (!f)
	{
		printf("Warning: Failed to open file %s\n", path);
		return 0;
	}

	fseek(f, 0, SEEK_END);
	size = (u64)ftell(f);
	fseek(f, 0, SEEK_SET);

	if (arena)
		buffer = clib_arena_alloc(arena, size + 1);
	else
		buffer = malloc(size + 1);

	if (!buffer)
	{
		printf("Warning: Unable to allocate memory for file %s\n", path);
		return 0;
	}

	result = fread(buffer, 1, size, f);

	if (result != size)
	{
		printf("Warning: Failed to read full file %s\n", path);
		return 0;
	}

	fclose(f);

	buffer[size] = '\0';
	*out_data = buffer;
	*out_size = size;

	return 1;
}
