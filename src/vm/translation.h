/*
 * translation.h - Traducción de direcciones VA -> PA (recorrido de 2 niveles).
 */
#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <stdint.h>

#include "vm/virtual_memory.h"

typedef enum AccessType { ACCESS_READ, ACCESS_WRITE } AccessType;

/*
 * Traduce `va` a dirección física. Pasos:
 *   1. ¿va pertenece a una reserva?           no -> VM_ERR_SEGFAULT
 *   2. Recorre dir[PT1] -> PTE[PT2]
 *   3. Si la PTE no es válida -> fallo de página (page_fault.c)
 *   4. Marca bits accessed/dirty y avisa a la política
 *   5. PA = frame * page_size + offset
 */
VmStatus vm_translate(VirtualMemory *vm, uint32_t va, AccessType type, uint32_t *pa);

#endif
