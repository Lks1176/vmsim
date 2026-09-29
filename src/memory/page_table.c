#include "memory/page_table.h"

#include <stdlib.h>
#include <string.h>

struct PageTable {
    AddressLayout layout;
    PageTableEntry **directory; /* 2^pt1_bits punteros a tablas de nivel 2 */
    uint32_t l2_entries;        /* 2^pt2_bits PTE por tabla                */
};

PageTable *pt_create(const AddressLayout *layout)
{
    if (layout == NULL) {
        return NULL;
    }
    PageTable *pt = calloc(1, sizeof *pt);
    if (pt == NULL) {
        return NULL;
    }
    pt->layout = *layout;
    pt->l2_entries = 1u << layout->pt2_bits;
    pt->directory = calloc((size_t)1u << layout->pt1_bits, sizeof *pt->directory);
    if (pt->directory == NULL) {
        free(pt);
        return NULL;
    }
    return pt;
}

void pt_destroy(PageTable *pt)
{
    if (pt == NULL) {
        return;
    }
    if (pt->directory != NULL) {
        const size_t l1_entries = (size_t)1u << pt->layout.pt1_bits;
        for (size_t i = 0; i < l1_entries; i++) {
            free(pt->directory[i]);
        }
    }
    free(pt->directory);
    free(pt);
}

PageTableEntry *pt_lookup(const PageTable *pt, uint32_t va)
{
    PageTableEntry *table = pt->directory[layout_pt1_index(&pt->layout, va)];
    if (table == NULL) {
        return NULL;
    }
    return &table[layout_pt2_index(&pt->layout, va)];
}

PageTableEntry *pt_get_or_create(PageTable *pt, uint32_t va, bool *created)
{
    const uint32_t i1 = layout_pt1_index(&pt->layout, va);
    if (created != NULL) {
        *created = false;
    }
    if (pt->directory[i1] == NULL) {
        /* calloc deja todas las PTE con valid = 0 (página no presente). */
        pt->directory[i1] = calloc(pt->l2_entries, sizeof(PageTableEntry));
        if (pt->directory[i1] == NULL) {
            return NULL;
        }
        if (created != NULL) {
            *created = true;
        }
    }
    return &pt->directory[i1][layout_pt2_index(&pt->layout, va)];
}

/* Libera la tabla de nivel 2 `i1` si ninguna PTE está en uso. */
static size_t reclaim_if_empty(PageTable *pt, uint32_t i1)
{
    PageTableEntry *table = pt->directory[i1];
    if (table == NULL) {
        return 0;
    }
    for (uint32_t k = 0; k < pt->l2_entries; k++) {
        if (table[k].valid || table[k].swapped) {
            return 0;
        }
    }
    free(table);
    pt->directory[i1] = NULL;
    return 1;
}

size_t pt_release_range(PageTable *pt, uint32_t start_va, uint32_t num_pages)
{
    size_t freed = 0;
    bool have_previous = false;
    uint32_t previous_i1 = 0;

    for (uint32_t k = 0; k < num_pages; k++) {
        const uint32_t va = start_va + k * pt->layout.page_size;
        const uint32_t i1 = layout_pt1_index(&pt->layout, va);
        PageTableEntry *pte = pt_lookup(pt, va);
        if (pte != NULL) {
            memset(pte, 0, sizeof *pte);
        }
        /* Al cambiar de tabla, revisamos la anterior una sola vez. */
        if (have_previous && i1 != previous_i1) {
            freed += reclaim_if_empty(pt, previous_i1);
        }
        previous_i1 = i1;
        have_previous = true;
    }
    if (have_previous) {
        freed += reclaim_if_empty(pt, previous_i1);
    }
    return freed;
}
