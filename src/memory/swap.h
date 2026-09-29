/*
 * swap.h - Almacén de respaldo ("disco") en memoria del host.
 * Guarda el contenido de las páginas sucias expulsadas para poder devolverlo
 * si la página se vuelve a usar. Las ranuras se crean bajo demanda y se
 * reciclan con una lista de libres.
 */
#ifndef SWAP_H
#define SWAP_H

#include <stdint.h>

#include "core/status.h"

typedef struct Swap Swap;

Swap *swap_create(uint32_t page_size);
void swap_destroy(Swap *swap); /* seguro con NULL */

VmStatus swap_alloc_slot(Swap *swap, uint32_t *slot);
VmStatus swap_free_slot(Swap *swap, uint32_t slot);
VmStatus swap_write(Swap *swap, uint32_t slot, const uint8_t *page);
VmStatus swap_read(const Swap *swap, uint32_t slot, uint8_t *page);

#endif
