/*
 * physical_memory.h - RAM simulada: un arreglo de bytes dividido en marcos.
 * Lleva el control de marcos libres (pila) y de qué página virtual ocupa cada
 * marco (necesario para saber a quién expulsar cuando se elige una víctima).
 */
#ifndef PHYSICAL_MEMORY_H
#define PHYSICAL_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

#include "core/status.h"

typedef struct PhysicalMemory PhysicalMemory;

PhysicalMemory *pm_create(uint32_t num_frames, uint32_t page_size);
void pm_destroy(PhysicalMemory *pm); /* seguro con NULL */

/* Toma un marco libre (siempre el de menor número disponible al inicio). */
bool pm_alloc_frame(PhysicalMemory *pm, uint32_t *frame);
VmStatus pm_free_frame(PhysicalMemory *pm, uint32_t frame);
uint32_t pm_free_count(const PhysicalMemory *pm);

/* Dueño del marco: número de página virtual que lo ocupa. */
void pm_set_owner(PhysicalMemory *pm, uint32_t frame, uint32_t vpn);
uint32_t pm_get_owner(const PhysicalMemory *pm, uint32_t frame);

/* Acceso al contenido de un marco completo (para swap y puesta en cero). */
uint8_t *pm_frame_data(PhysicalMemory *pm, uint32_t frame);

/* Acceso por DIRECCIÓN FÍSICA, con verificación de límites. */
VmStatus pm_read_byte(const PhysicalMemory *pm, uint32_t pa, uint8_t *out);
VmStatus pm_write_byte(PhysicalMemory *pm, uint32_t pa, uint8_t value);

#endif
