/*
 * virtual_memory.h - FACHADA del subsistema de memoria virtual (patrón FACADE).
 *
 */
#ifndef VIRTUAL_MEMORY_H
#define VIRTUAL_MEMORY_H

#include <stdint.h>

#include "core/address.h"
#include "core/config.h"
#include "core/stats.h"
#include "core/status.h"
#include "memory/page_table.h"
#include "memory/physical_memory.h"
#include "memory/region_table.h"
#include "memory/swap.h"
#include "policy/replacement.h"

typedef struct VirtualMemory {
    Config config;
    AddressLayout layout;
    PageTable *page_table;
    PhysicalMemory *phys;
    Swap *swap;
    RegionTable *regions;
    ReplacementPolicy *policy;
    Stats stats;
} VirtualMemory;

/* Construye todo el subsistema; si falla libera lo ya creado (sin fugas). */
VmStatus vm_create(VirtualMemory **out, const Config *config);
void vm_destroy(VirtualMemory *vm);

VmStatus vm_alloc(VirtualMemory *vm, uint32_t bytes, uint32_t *address);
VmStatus vm_free(VirtualMemory *vm, uint32_t address);
VmStatus vm_read(VirtualMemory *vm, uint32_t va, uint8_t *value);
VmStatus vm_write(VirtualMemory *vm, uint32_t va, uint8_t value);

const Stats *vm_stats(const VirtualMemory *vm);
const char *vm_policy_name(const VirtualMemory *vm);

/* Escribe en stdout solo si la verbosidad es VERBOSITY_VERBOSE. */
void vm_trace(const VirtualMemory *vm, const char *format, ...);

#endif
