/*
 * replacement.h - Interfaz de políticas de reemplazo (patrón STRATEGY).
 *
 * El resto del simulador solo conoce esta "interfaz" (tabla de punteros a
 * función).
 *
 * La política trabaja con NÚMEROS DE MARCO físico; no conoce tablas de páginas.
 * El manejador de fallos le avisa de los eventos relevantes:
 *   on_load     : una página virtual acaba de cargarse en `frame`
 *   on_access   : hubo un acceso (lectura/escritura) a la página en `frame`
 *   on_release  : el marco deja de estar ocupado (expulsión o free)
 *   select_victim: propone qué marco expulsar (sin modificar el estado)
 */
#ifndef REPLACEMENT_H
#define REPLACEMENT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum PolicyType {
    POLICY_FIFO = 0
} PolicyType;

typedef struct ReplacementPolicy ReplacementPolicy;

struct ReplacementPolicy {
    const char *name;
    void (*on_load)(ReplacementPolicy *self, uint32_t frame);
    void (*on_access)(ReplacementPolicy *self, uint32_t frame);
    void (*on_release)(ReplacementPolicy *self, uint32_t frame);
    bool (*select_victim)(const ReplacementPolicy *self, uint32_t *frame);
    void (*destroy)(ReplacementPolicy *self);
};

/* FACTORY: construye la política pedida; NULL si falla o no existe. */
ReplacementPolicy *replacement_create(PolicyType type, uint32_t num_frames);

/* Traducciones nombre <-> tipo (para la línea de comandos y los reportes). */
bool replacement_parse_name(const char *name, PolicyType *out);
const char *replacement_type_name(PolicyType type);

#endif
