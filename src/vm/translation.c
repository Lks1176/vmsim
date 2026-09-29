#include "vm/translation.h"

#include "vm/page_fault.h"

VmStatus vm_translate(VirtualMemory *vm, uint32_t va, AccessType type, uint32_t *pa)
{
    if (vm == NULL || pa == NULL) {
        return VM_ERR_INVALID_ARG;
    }
    if (!rt_contains(vm->regions, va)) {
        return VM_ERR_SEGFAULT;
    }

    const bool is_write = (type == ACCESS_WRITE);
    stats_record_access(&vm->stats, is_write, vm->config.mem_access_ns);

    PageTableEntry *pte = pt_lookup(vm->page_table, va);
    bool faulted = false;
    if (pte == NULL || !pte->valid) {
        const VmStatus st = handle_page_fault(vm, va, &pte);
        if (st != VM_OK) {
            return st;
        }
        faulted = true;
    }

    pte->accessed = 1;
    if (is_write) {
        pte->dirty = 1;
    }
    vm->policy->on_access(vm->policy, pte->frame);

    *pa = pte->frame * vm->layout.page_size + layout_offset(&vm->layout, va);

    vm_trace(vm, "%c VA=0x%08X [PT1=%u PT2=%u off=0x%03X] -> %s PA=0x%08X\n",
             is_write ? 'W' : 'R', va, layout_pt1_index(&vm->layout, va),
             layout_pt2_index(&vm->layout, va), layout_offset(&vm->layout, va),
             faulted ? "FALLO" : "HIT  ", *pa);
    return VM_OK;
}
