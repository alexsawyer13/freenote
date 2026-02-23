#include "clib.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// ---------- Arenas ----------

clib_arena *clib_arena_init(u64 block_size)
{
	CLIB_ASSERT(block_size > 0, "block_size is 0");
	CLIB_ASSERT(block_size > sizeof(clib_arena), "block_size is too small to fit arena metadata");

	clib_arena *a;
	a = malloc(block_size);
	CLIB_ASSERT(a, "Failed to allocate block");

	*a = (clib_arena){0};
	a->current_block = (clib_arena_block*)a;
	a->current_index = sizeof(clib_arena);
	a->block_size = block_size;

	return a;
}

void clib_arena_destroy(clib_arena **a)
{
	CLIB_ASSERT(a, "a is NULL");
	
	clib_arena_block *block = (clib_arena_block*)(*a);
	while (block != NULL)
	{
		clib_arena_block *next_block = block->next_block;
		free(block);
		block = next_block;
	}

	*a = NULL;
}

void clib_arena_reset(clib_arena *a)
{
	CLIB_ASSERT(a, "a is NULL");
	a->current_block = (clib_arena_block*)a;
	a->current_index = sizeof(clib_arena);
	a->total_allocation_size = 0;
	a->num_allocations = 0;
}

void clib_arena_start_scratch(clib_arena *a)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(a->scratch_block == NULL, "There is already a scratch block");

	a->scratch_block = a->current_block;
	a->scratch_index = a->current_index;
	a->scratch_total_allocation_size = a->total_allocation_size;
	a->scratch_num_allocations = a->num_allocations;
}

void clib_arena_stop_scratch(clib_arena *a)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(a->scratch_block != NULL, "There is no scratch block");

	a->current_block = a->scratch_block;
	a->current_index = a->scratch_index;
	a->total_allocation_size = a->scratch_total_allocation_size;
	a->num_allocations = a->scratch_num_allocations;

	a->scratch_block = NULL;
	a->scratch_index = 0;
	a->scratch_total_allocation_size = 0;
	a->scratch_num_allocations = 0;
}

void clib_arena_print_info(clib_arena *a)
{
	CLIB_ASSERT(a, "a is NULL");
	printf("Arena with block size %llu\n", a->block_size);
	printf("\t%llu allocation(s)\n", a->num_allocations);
	printf("\tin %llu block(s)\n", a->num_extra_blocks_allocated + 1);
	printf("\ttotalling %llu bytes of user data\n", a->total_allocation_size);
	printf("\tand %llu bytes of metadata\n", sizeof(clib_arena) + a->num_extra_blocks_allocated * sizeof(clib_arena_block));
}

void clib_arena_freelist_insert(clib_arena *a, clib_arena_freelist *f)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(f, "f is NULL");
	CLIB_ASSERT(f->size > 0, "f has size 0");

	f->next = NULL;
	f->prev = NULL;

	// Loop through freelist
	clib_arena_freelist *current = a->freelist;
	while (current != NULL)
	{
		// If this element is bigger than the current one,
		// is should be placed before it
		// prev -> current becomes
		// prev -> f -> current
		// If prev doesn't exist then current must be the beginning!
		// so a->freelist -> f -> current
		if (f->size > current->size)
		{
			clib_arena_freelist *prev = current->prev;

			if (!prev)
			{
				a->freelist = f;
				f->prev = NULL;
				f->next = current;
				return;
			}

			prev->next = f;
			f->prev = prev;
			f->next = current;
			current->prev = f;

			return;
		}

		// If we make it here, we reached the end of the freelist
		// without finding a smaller element. We must go at the end.
		current->next = f;
		f->prev = current;
		return;
	}
	// If we make it here, current was always NULL,
	// meaning the freelist doesn't exist yet.
	
	a->freelist = f;
}

void* clib_arena_alloc(clib_arena *a, u64 size)
{
	CLIB_ASSERT(a, "a is NULL");
	CLIB_ASSERT(size > 0, "size is 0");
	CLIB_ASSERT(size <= a->block_size, "allocation is too big to fit in a block");
	CLIB_ASSERT((size + sizeof(clib_arena_block)) <= a->block_size, "allocation is too big to fit in a block alongside metadata");

	clib_arena_freelist *freelist = a->freelist;
	while (freelist != NULL)
	{
		// If it can't fit here, move on
		if (size > freelist->size)
		{
			freelist = freelist->next;
			continue;
		}

		// The allocation can now DEFINITELY fit in this freelist block

		// Can it fit while keeping the freelist in tact?
		if (size + sizeof(clib_arena_freelist) <= freelist->size)
		{
			// Yes! Put it at the end of the free block, and shrink it
			void *ptr = (void*)(freelist) + freelist->size - size;
			freelist->size -= size;
			a->num_allocations++;
			a->total_allocation_size += size;
			return ptr;
		}

		// Now we know the freelist block is big enough for the allocation,
		// but too small to also contain enough metadata for a freelist block.
		// Remove it from the freelist and give the memory to the allocation
		
		// Remove from linked list
		if (freelist->prev)
			freelist->prev->next = freelist->next;
		else
			a->freelist = freelist->next;
		if (freelist->next)
			freelist->next->prev = freelist->prev;

		a->num_allocations++;
		a->total_allocation_size += size;
		return (void*)freelist;
	}

	// If it can't fit in current block, need a new one
	if (a->current_index + size > a->block_size)
	{
		// If the wasted space at the end of the old block
		// is more than sizeof(clib_freelist), add the space
		// to the freelist for future allocations.

		u64 wasted_space = a->block_size - a->current_index;
		if (wasted_space > sizeof(clib_arena_freelist))
		{
			clib_arena_freelist *free_spot = (void*)(a->current_block) + a->current_index;
			free_spot->size = wasted_space;
			free_spot->next = NULL;
			free_spot->prev = NULL;
			clib_arena_freelist_insert(a, free_spot);
		}

		// Now we need to actually get a new block...

		// If there's already a next block, use that
		if (a->current_block->next_block != NULL)
		{
			a->current_block = a->current_block->next_block;
			a->current_index = sizeof(clib_arena_block);
		}
		// Otherwise allocate a new block
		else
		{
			clib_arena_block *new_block = malloc(a->block_size);
			CLIB_ASSERT(new_block, "Failed to malloc new block");
			a->current_block->next_block = new_block;
			a->current_block = new_block;
			a->current_index = sizeof(clib_arena_block);
			a->num_extra_blocks_allocated++;
		}
	}

	// Now we definitely have a valid spot for the memory to go!
	void *ptr = (void*)(a->current_block) + a->current_index;
	a->current_index += size;
	a->num_allocations++;
	a->total_allocation_size += size;
	return ptr;
}

void* clib_arena_calloc(clib_arena *a, u64 size)
{
	CLIB_ASSERT(a, "a is NULL");
	void *ptr = clib_arena_alloc(a, size);
	memset(a, 0, size);
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
