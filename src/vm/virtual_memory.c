#include "vm/virtual_memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "vm/translation.h"

VmStatus vm_create(VirtualMemory **out, const Config *config)
{
    if (out == NULL || config == NULL || config_validate(config) != NULL) {
        return VM_ERR_INVALID_ARG;
    }
    VirtualMemory *vm = calloc(1, sizeof *vm);
    if (vm == NULL) {
        return VM_ERR_NO_MEMORY;
    }
    vm->config = *config;

    VmStatus st = layout_init(&vm->layout, config->page_size);
    if (st != VM_OK) {
        goto fail;
    }
    const uint32_t frames = config_num_frames(config);
    vm->page_table = pt_create(&vm->layout);
    vm->phys       = pm_create(frames, config->page_size);
    vm->swap       = swap_create(config->page_size);
    vm->regions    = rt_create(&vm->layout);
    vm->policy     = replacement_create(config->policy, frames);
    if (!vm->page_table || !vm->phys || !vm->swap || !vm->regions || !vm->policy) {
        st = VM_ERR_NO_MEMORY;
        goto fail;
    }
    stats_init(&vm->stats);
    *out = vm;
    return VM_OK;

fail: /* cada destroy tolera NULL, así que basta una sola ruta de limpieza */
    vm_destroy(vm);
    return st;
}

void vm_destroy(VirtualMemory *vm)
{
    if (vm == NULL) {
        return;
    }
    if (vm->policy != NULL) {
        vm->policy->destroy(vm->policy);
    }
    rt_destroy(vm->regions);
    swap_destroy(vm->swap);
    pm_destroy(vm->phys);
    pt_destroy(vm->page_table);
    free(vm);
}

VmStatus vm_alloc(VirtualMemory *vm, uint32_t bytes, uint32_t *address)
{
    Region region;
    const VmStatus st = rt_reserve(vm->regions, bytes, &region);
    if (st != VM_OK) {
        return st;
    }
    *address = region.start;
    vm->stats.allocs++;
    vm_trace(vm, "alloc: %u bytes -> VA=0x%08X (%u páginas reservadas)\n", bytes,
             region.start, region.num_pages);
    return VM_OK;
}

VmStatus vm_free(VirtualMemory *vm, uint32_t address)
{
    Region region;
    VmStatus st = rt_release(vm->regions, address, &region);
    if (st != VM_OK) {
        return st;
    }

    /* Devuelve marcos y ranuras de swap de cada página de la reserva. */
    for (uint32_t k = 0; k < region.num_pages; k++) {
        PageTableEntry *pte = pt_lookup(vm->page_table,
                                        region.start + k * vm->layout.page_size);
        if (pte == NULL) {
            continue;
        }
        if (pte->valid) {
            vm->policy->on_release(vm->policy, pte->frame);
            st = pm_free_frame(vm->phys, pte->frame);
            if (st != VM_OK) {
                return st;
            }
        }
        if (pte->swapped) {
            st = swap_free_slot(vm->swap, pte->swap_slot);
            if (st != VM_OK) {
                return st;
            }
        }
    }
    const size_t freed = pt_release_range(vm->page_table, region.start, region.num_pages);
    vm->stats.l2_tables_freed += freed;
    vm->stats.frees++;
    vm_trace(vm, "free: VA=0x%08X (%u páginas liberadas, %zu tablas nivel 2 liberadas)\n",
             address, region.num_pages, freed);
    return VM_OK;
}

VmStatus vm_read(VirtualMemory *vm, uint32_t va, uint8_t *value)
{
    uint32_t pa;
    const VmStatus st = vm_translate(vm, va, ACCESS_READ, &pa);
    if (st != VM_OK) {
        return st;
    }
    return pm_read_byte(vm->phys, pa, value);
}

VmStatus vm_write(VirtualMemory *vm, uint32_t va, uint8_t value)
{
    uint32_t pa;
    const VmStatus st = vm_translate(vm, va, ACCESS_WRITE, &pa);
    if (st != VM_OK) {
        return st;
    }
    return pm_write_byte(vm->phys, pa, value);
}

const Stats *vm_stats(const VirtualMemory *vm)
{
    return &vm->stats;
}

const char *vm_policy_name(const VirtualMemory *vm)
{
    return vm->policy->name;
}

void vm_trace(const VirtualMemory *vm, const char *format, ...)
{
    if (vm->config.verbosity != VERBOSITY_VERBOSE) {
        return;
    }
    va_list args;
    va_start(args, format);
    fputs("[trace] ", stdout);
    vfprintf(stdout, format, args);
    va_end(args);
}
