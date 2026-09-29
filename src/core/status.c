#include "core/status.h"

const char *vm_status_str(VmStatus status)
{
    switch (status) {
    case VM_OK:                       return "ok";
    case VM_ERR_INVALID_ARG:          return "argumento inválido";
    case VM_ERR_NO_MEMORY:            return "memoria del host agotada";
    case VM_ERR_SEGFAULT:             return "dirección virtual no asignada (segmentation fault)";
    case VM_ERR_OUT_OF_VIRTUAL_SPACE: return "no hay espacio virtual suficiente (límite de 4 GB)";
    case VM_ERR_BAD_FREE:             return "free inválido: la dirección no es el inicio de una reserva activa";
    case VM_ERR_NO_VICTIM:            return "error interno: no hay marco víctima";
    case VM_ERR_INTERNAL:             return "error interno: invariante violado";
    }
    return "estado desconocido";
}

bool vm_status_is_fatal(VmStatus status)
{
    return status == VM_ERR_NO_MEMORY || status == VM_ERR_NO_VICTIM ||
           status == VM_ERR_INTERNAL;
}
