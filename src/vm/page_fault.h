/*
 * page_fault.h - Manejo de fallos de página.
 */
#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#include <stdint.h>

#include "vm/virtual_memory.h"

/*
 * Atiende un fallo sobre `va` (que ya se sabe válida). Al terminar la PTE
 * devuelta en `out_pte` es válida y apunta a un marco con el contenido correcto:
 *   1. Crea la tabla de nivel 2 si no existía.
 *   2. Consigue un marco: libre, o expulsando la víctima que elija la política.
 *   3. Rellena el marco: desde swap si la página fue expulsada antes, o con ceros.
 *   4. Actualiza PTE, dueño del marco y política.
 */
VmStatus handle_page_fault(VirtualMemory *vm, uint32_t va, PageTableEntry **out_pte);

#endif
