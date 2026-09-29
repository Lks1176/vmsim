#include "memory/swap.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct Swap {
    uint32_t page_size;
    uint8_t **pages;     /* pages[slot] = buffer de page_size bytes        */
    bool *in_use;
    uint32_t *free_list; /* ranuras recicladas                             */
    size_t count;        /* ranuras creadas hasta ahora                    */
    size_t capacity;
    size_t free_count;
};

/* Duplica la capacidad de los tres arreglos; no altera nada si falla. */
static VmStatus swap_grow(Swap *swap)
{
    const size_t new_cap = swap->capacity ? swap->capacity * 2u : 16u;

    uint8_t **pages = realloc(swap->pages, new_cap * sizeof *pages);
    if (pages == NULL) {
        return VM_ERR_NO_MEMORY;
    }
    swap->pages = pages;

    bool *in_use = realloc(swap->in_use, new_cap * sizeof *in_use);
    if (in_use == NULL) {
        return VM_ERR_NO_MEMORY;
    }
    swap->in_use = in_use;

    uint32_t *free_list = realloc(swap->free_list, new_cap * sizeof *free_list);
    if (free_list == NULL) {
        return VM_ERR_NO_MEMORY;
    }
    swap->free_list = free_list;

    swap->capacity = new_cap; /* solo se actualiza si los tres crecieron */
    return VM_OK;
}

Swap *swap_create(uint32_t page_size)
{
    if (page_size == 0) {
        return NULL;
    }
    Swap *swap = calloc(1, sizeof *swap);
    if (swap != NULL) {
        swap->page_size = page_size;
    }
    return swap;
}

void swap_destroy(Swap *swap)
{
    if (swap == NULL) {
        return;
    }
    for (size_t i = 0; i < swap->count; i++) {
        free(swap->pages[i]);
    }
    free(swap->pages);
    free(swap->in_use);
    free(swap->free_list);
    free(swap);
}

VmStatus swap_alloc_slot(Swap *swap, uint32_t *slot)
{
    if (swap->free_count > 0) {
        *slot = swap->free_list[--swap->free_count];
        swap->in_use[*slot] = true;
        return VM_OK;
    }
    if (swap->count == swap->capacity) {
        const VmStatus st = swap_grow(swap);
        if (st != VM_OK) {
            return st;
        }
    }
    swap->pages[swap->count] = malloc(swap->page_size);
    if (swap->pages[swap->count] == NULL) {
        return VM_ERR_NO_MEMORY;
    }
    swap->in_use[swap->count] = true;
    *slot = (uint32_t)swap->count++;
    return VM_OK;
}

static bool slot_valid(const Swap *swap, uint32_t slot)
{
    return slot < swap->count && swap->in_use[slot];
}

VmStatus swap_free_slot(Swap *swap, uint32_t slot)
{
    if (!slot_valid(swap, slot)) {
        return VM_ERR_INTERNAL;
    }
    swap->in_use[slot] = false;
    swap->free_list[swap->free_count++] = slot;
    return VM_OK;
}

VmStatus swap_write(Swap *swap, uint32_t slot, const uint8_t *page)
{
    if (!slot_valid(swap, slot) || page == NULL) {
        return VM_ERR_INTERNAL;
    }
    memcpy(swap->pages[slot], page, swap->page_size);
    return VM_OK;
}

VmStatus swap_read(const Swap *swap, uint32_t slot, uint8_t *page)
{
    if (!slot_valid(swap, slot) || page == NULL) {
        return VM_ERR_INTERNAL;
    }
    memcpy(page, swap->pages[slot], swap->page_size);
    return VM_OK;
}
