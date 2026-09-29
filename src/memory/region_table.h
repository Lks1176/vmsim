/*
 * region_table.h - Registro de reservas hechas con `alloc`.
 *
 * `alloc` solo RESERVA espacio virtual (asignación perezosa / demand paging):
 * no consume marcos hasta el primer acceso. Este registro permite distinguir
 * un acceso válido (que produce un fallo de página) de uno ilegal (segfault).
 * Las direcciones se entregan con un asignador secuencial y alineado a página.
 */
#ifndef REGION_TABLE_H
#define REGION_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/address.h"
#include "core/status.h"

typedef struct Region {
    uint32_t start;     /* dirección virtual inicial */
    uint32_t num_pages;
} Region;

typedef struct RegionTable RegionTable;

RegionTable *rt_create(const AddressLayout *layout);
void rt_destroy(RegionTable *rt); /* seguro con NULL */

VmStatus rt_reserve(RegionTable *rt, uint32_t bytes, Region *out);
bool rt_contains(const RegionTable *rt, uint32_t va);
VmStatus rt_release(RegionTable *rt, uint32_t start, Region *out);

#endif
