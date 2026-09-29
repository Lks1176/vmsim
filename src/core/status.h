/*
 * status.h - Códigos de estado compartidos por todos los módulos.
 *
 * Estrategia de manejo de errores del proyecto: ninguna función de librería
 * llama a exit() ni imprime errores; todas devuelven un VmStatus y quien
 * orquesta (simulator) decide qué hacer con él.
 */
#ifndef STATUS_H
#define STATUS_H

#include <stdbool.h>

typedef enum VmStatus {
    VM_OK = 0,
    VM_ERR_INVALID_ARG,          /* argumento fuera de rango o nulo            */
    VM_ERR_NO_MEMORY,            /* malloc/calloc devolvió NULL                */
    VM_ERR_SEGFAULT,             /* acceso a una dirección virtual no asignada */
    VM_ERR_OUT_OF_VIRTUAL_SPACE, /* alloc no cabe en los 4 GB virtuales        */
    VM_ERR_BAD_FREE,             /* free de una dirección que no es inicio     */
    VM_ERR_NO_VICTIM,            /* no hay marco que reemplazar (bug interno)  */
    VM_ERR_INTERNAL              /* invariante violado (bug interno)           */
} VmStatus;

/* Mensaje legible (en español) para un código de estado. */
const char *vm_status_str(VmStatus status);

/* true si el error deja al simulador en estado no confiable (hay que abortar). */
bool vm_status_is_fatal(VmStatus status);

#endif
