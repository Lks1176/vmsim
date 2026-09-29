#include "core/address.h"

#include <stddef.h>

VmStatus layout_init(AddressLayout *layout, uint32_t page_size)
{
    if (layout == NULL || page_size < MIN_PAGE_SIZE || page_size > MAX_PAGE_SIZE ||
        (page_size & (page_size - 1u)) != 0u) {
        return VM_ERR_INVALID_ARG;
    }
    uint32_t bits = 0;
    while ((1u << bits) < page_size) {
        bits++;
    }
    const uint32_t vpn_bits = VM_ADDRESS_BITS - bits;
    layout->page_size   = page_size;
    layout->offset_bits = bits;
    layout->pt2_bits    = vpn_bits / 2u;
    layout->pt1_bits    = vpn_bits - layout->pt2_bits;
    return VM_OK;
}

uint32_t layout_vpn(const AddressLayout *layout, uint32_t va)
{
    return va >> layout->offset_bits;
}

uint32_t layout_pt1_index(const AddressLayout *layout, uint32_t va)
{
    return layout_vpn(layout, va) >> layout->pt2_bits;
}

uint32_t layout_pt2_index(const AddressLayout *layout, uint32_t va)
{
    return layout_vpn(layout, va) & ((1u << layout->pt2_bits) - 1u);
}

uint32_t layout_offset(const AddressLayout *layout, uint32_t va)
{
    return va & (layout->page_size - 1u);
}

uint32_t layout_vpn_to_va(const AddressLayout *layout, uint32_t vpn)
{
    return vpn << layout->offset_bits;
}
