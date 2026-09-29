#include "memory/physical_memory.h"

#include <stdlib.h>

struct PhysicalMemory {
    uint32_t num_frames;
    uint32_t page_size;
    uint8_t *data;        /* num_frames * page_size bytes contiguos      */
    uint32_t *owner;      /* owner[frame] = vpn que lo ocupa              */
    bool *in_use;         /* detecta doble free                           */
    uint32_t *free_stack; /* pila de marcos libres                        */
    uint32_t free_top;    /* cantidad de marcos libres                    */
};

PhysicalMemory *pm_create(uint32_t num_frames, uint32_t page_size)
{
    if (num_frames == 0 || page_size == 0) {
        return NULL;
    }
    PhysicalMemory *pm = calloc(1, sizeof *pm);
    if (pm == NULL) {
        return NULL;
    }
    pm->num_frames = num_frames;
    pm->page_size  = page_size;
    pm->data       = calloc(num_frames, page_size);
    pm->owner      = calloc(num_frames, sizeof *pm->owner);
    pm->in_use     = calloc(num_frames, sizeof *pm->in_use);
    pm->free_stack = malloc(num_frames * sizeof *pm->free_stack);
    if (pm->data == NULL || pm->owner == NULL || pm->in_use == NULL ||
        pm->free_stack == NULL) {
        pm_destroy(pm);
        return NULL;
    }
    /* Se apila al revés para que el primer marco entregado sea el 0. */
    for (uint32_t i = 0; i < num_frames; i++) {
        pm->free_stack[i] = num_frames - 1u - i;
    }
    pm->free_top = num_frames;
    return pm;
}

void pm_destroy(PhysicalMemory *pm)
{
    if (pm == NULL) {
        return;
    }
    free(pm->data);
    free(pm->owner);
    free(pm->in_use);
    free(pm->free_stack);
    free(pm);
}

bool pm_alloc_frame(PhysicalMemory *pm, uint32_t *frame)
{
    if (pm->free_top == 0) {
        return false;
    }
    *frame = pm->free_stack[--pm->free_top];
    pm->in_use[*frame] = true;
    return true;
}

VmStatus pm_free_frame(PhysicalMemory *pm, uint32_t frame)
{
    if (frame >= pm->num_frames || !pm->in_use[frame]) {
        return VM_ERR_INTERNAL;
    }
    pm->in_use[frame] = false;
    pm->free_stack[pm->free_top++] = frame;
    return VM_OK;
}

uint32_t pm_free_count(const PhysicalMemory *pm)
{
    return pm->free_top;
}

void pm_set_owner(PhysicalMemory *pm, uint32_t frame, uint32_t vpn)
{
    if (frame < pm->num_frames) {
        pm->owner[frame] = vpn;
    }
}

uint32_t pm_get_owner(const PhysicalMemory *pm, uint32_t frame)
{
    return frame < pm->num_frames ? pm->owner[frame] : 0;
}

uint8_t *pm_frame_data(PhysicalMemory *pm, uint32_t frame)
{
    if (frame >= pm->num_frames) {
        return NULL;
    }
    return pm->data + (size_t)frame * pm->page_size;
}

VmStatus pm_read_byte(const PhysicalMemory *pm, uint32_t pa, uint8_t *out)
{
    if ((uint64_t)pa >= (uint64_t)pm->num_frames * pm->page_size) {
        return VM_ERR_INTERNAL;
    }
    *out = pm->data[pa];
    return VM_OK;
}

VmStatus pm_write_byte(PhysicalMemory *pm, uint32_t pa, uint8_t value)
{
    if ((uint64_t)pa >= (uint64_t)pm->num_frames * pm->page_size) {
        return VM_ERR_INTERNAL;
    }
    pm->data[pa] = value;
    return VM_OK;
}
