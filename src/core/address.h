/*
 * address.h - Descomposición de direcciones virtuales de 32 bits.
 *
 *   |  PT1 (10b)  |  PT2 (10b)  |  Offset (12b)  |     (página de 4 KB)
 *   31          22 21         12 11             0
 */
#ifndef ADDRESS_H
#define ADDRESS_H

#include <stdint.h>

#include "core/status.h"

#define VM_ADDRESS_BITS 32u
#define MIN_PAGE_SIZE   256u
#define MAX_PAGE_SIZE   65536u

typedef struct AddressLayout {
    uint32_t page_size;
    uint32_t offset_bits;
    uint32_t pt1_bits;
    uint32_t pt2_bits;
} AddressLayout;

/* Calcula la partición de bits; page_size debe ser potencia de 2 en rango. */
VmStatus layout_init(AddressLayout *layout, uint32_t page_size);

uint32_t layout_pt1_index(const AddressLayout *layout, uint32_t va);
uint32_t layout_pt2_index(const AddressLayout *layout, uint32_t va);
uint32_t layout_offset(const AddressLayout *layout, uint32_t va);
uint32_t layout_vpn(const AddressLayout *layout, uint32_t va);
uint32_t layout_vpn_to_va(const AddressLayout *layout, uint32_t vpn);

#endif
