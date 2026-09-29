#include "vm/page_fault.h"

#include <string.h>

/* Expulsa el contenido del marco `frame` y lo devuelve a la lista de libres. */
static VmStatus evict_frame(VirtualMemory *vm, uint32_t frame)
{
    const uint32_t vpn = pm_get_owner(vm->phys, frame);
    const uint32_t victim_va = layout_vpn_to_va(&vm->layout, vpn);
    PageTableEntry *victim = pt_lookup(vm->page_table, victim_va);
    if (victim == NULL || !victim->valid || victim->frame != frame) {
        return VM_ERR_INTERNAL; /* la tabla y el marco no concuerdan */
    }

    /* Solo las páginas modificadas necesitan escribirse a swap. */
    const bool write_back = victim->dirty;
    if (write_back) {
        if (!victim->swapped) {
            const VmStatus st = swap_alloc_slot(vm->swap, &victim->swap_slot);
            if (st != VM_OK) {
                return st;
            }
            victim->swapped = 1;
        }
        const VmStatus st = swap_write(vm->swap, victim->swap_slot,
                                       pm_frame_data(vm->phys, frame));
        if (st != VM_OK) {
            return st;
        }
        stats_record_swap_io(&vm->stats, true, vm->config.disk_io_ns);
    }

    victim->valid = 0;
    victim->accessed = 0;
    victim->dirty = 0;
    vm->policy->on_release(vm->policy, frame);
    const VmStatus st = pm_free_frame(vm->phys, frame);
    if (st != VM_OK) {
        return st;
    }
    vm->stats.replacements++;
    vm_trace(vm, "  REEMPLAZO: sale VPN=0x%05X (marco %u, %s)\n", vpn, frame,
             write_back ? "sucia -> swap" : "limpia, se descarta");
    return VM_OK;
}

/* Obtiene un marco libre; si no hay, aplica la política de reemplazo. */
static VmStatus obtain_frame(VirtualMemory *vm, uint32_t *frame)
{
    if (pm_alloc_frame(vm->phys, frame)) {
        return VM_OK;
    }
    uint32_t victim;
    if (!vm->policy->select_victim(vm->policy, &victim)) {
        return VM_ERR_NO_VICTIM;
    }
    VmStatus st = evict_frame(vm, victim);
    if (st != VM_OK) {
        return st;
    }
    return pm_alloc_frame(vm->phys, frame) ? VM_OK : VM_ERR_INTERNAL;
}

/* Deja en el marco el contenido de la página: swap o ceros. */
static VmStatus fill_frame(VirtualMemory *vm, const PageTableEntry *pte, uint32_t frame)
{
    uint8_t *data = pm_frame_data(vm->phys, frame);
    if (data == NULL) {
        return VM_ERR_INTERNAL;
    }
    if (pte->swapped) {
        const VmStatus st = swap_read(vm->swap, pte->swap_slot, data);
        if (st != VM_OK) {
            return st;
        }
        stats_record_swap_io(&vm->stats, false, vm->config.disk_io_ns);
    } else {
        memset(data, 0, vm->layout.page_size); /* primera vez: página en ceros */
    }
    return VM_OK;
}

VmStatus handle_page_fault(VirtualMemory *vm, uint32_t va, PageTableEntry **out_pte)
{
    bool created = false;
    PageTableEntry *pte = pt_get_or_create(vm->page_table, va, &created);
    if (pte == NULL) {
        return VM_ERR_NO_MEMORY;
    }
    if (created) {
        vm->stats.l2_tables_created++;
        vm_trace(vm, "  tabla de nivel 2 creada para PT1=%u\n",
                 layout_pt1_index(&vm->layout, va));
    }
    stats_record_fault(&vm->stats, vm->config.fault_ns);

    uint32_t frame;
    VmStatus st = obtain_frame(vm, &frame);
    if (st != VM_OK) {
        return st;
    }
    st = fill_frame(vm, pte, frame);
    if (st != VM_OK) {
        pm_free_frame(vm->phys, frame); /* deshace la reserva del marco */
        return st;
    }

    const uint32_t vpn = layout_vpn(&vm->layout, va);
    pte->frame = frame;
    pte->valid = 1;
    pte->accessed = 0;
    pte->dirty = 0;
    pm_set_owner(vm->phys, frame, vpn);
    vm->policy->on_load(vm->policy, frame);

    vm_trace(vm, "  FALLO DE PÁGINA: VPN=0x%05X cargada en marco %u\n", vpn, frame);
    *out_pte = pte;
    return VM_OK;
}
