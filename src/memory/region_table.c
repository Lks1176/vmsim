#include "memory/region_table.h"

#include <stdlib.h>
#include <string.h>

#define ADDRESS_SPACE_SIZE (1ULL << VM_ADDRESS_BITS)

struct RegionTable {
    uint32_t page_size;
    Region *items;      /* ordenado por `start` (el asignador es creciente) */
    size_t count;
    size_t capacity;
    uint64_t next_free; /* próxima dirección libre; uint64 para detectar overflow */
};

RegionTable *rt_create(const AddressLayout *layout)
{
    if (layout == NULL) {
        return NULL;
    }
    RegionTable *rt = calloc(1, sizeof *rt);
    if (rt != NULL) {
        rt->page_size = layout->page_size;
    }
    return rt;
}

void rt_destroy(RegionTable *rt)
{
    if (rt == NULL) {
        return;
    }
    free(rt->items);
    free(rt);
}

/* Índice de la región con mayor `start` <= va (búsqueda binaria). */
static bool find_candidate(const RegionTable *rt, uint32_t va, size_t *index)
{
    size_t lo = 0;
    size_t hi = rt->count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2u;
        if (rt->items[mid].start <= va) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo == 0) {
        return false;
    }
    *index = lo - 1u;
    return true;
}

VmStatus rt_reserve(RegionTable *rt, uint32_t bytes, Region *out)
{
    if (bytes == 0 || out == NULL) {
        return VM_ERR_INVALID_ARG;
    }
    const uint64_t pages = ((uint64_t)bytes + rt->page_size - 1u) / rt->page_size;
    const uint64_t span  = pages * rt->page_size;
    if (rt->next_free + span > ADDRESS_SPACE_SIZE) {
        return VM_ERR_OUT_OF_VIRTUAL_SPACE;
    }
    if (rt->count == rt->capacity) {
        const size_t new_cap = rt->capacity ? rt->capacity * 2u : 8u;
        Region *items = realloc(rt->items, new_cap * sizeof *items);
        if (items == NULL) {
            return VM_ERR_NO_MEMORY;
        }
        rt->items = items;
        rt->capacity = new_cap;
    }
    out->start = (uint32_t)rt->next_free;
    out->num_pages = (uint32_t)pages;
    rt->items[rt->count++] = *out;
    rt->next_free += span;
    return VM_OK;
}

bool rt_contains(const RegionTable *rt, uint32_t va)
{
    size_t idx;
    if (!find_candidate(rt, va, &idx)) {
        return false;
    }
    const Region *r = &rt->items[idx];
    return (uint64_t)va < (uint64_t)r->start + (uint64_t)r->num_pages * rt->page_size;
}

VmStatus rt_release(RegionTable *rt, uint32_t start, Region *out)
{
    size_t idx;
    if (!find_candidate(rt, start, &idx) || rt->items[idx].start != start) {
        return VM_ERR_BAD_FREE;
    }
    if (out != NULL) {
        *out = rt->items[idx];
    }
    memmove(&rt->items[idx], &rt->items[idx + 1u], (rt->count - idx - 1u) * sizeof *rt->items);
    rt->count--;
    return VM_OK;
}
