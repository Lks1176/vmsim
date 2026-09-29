/*
 * fifo.c - Implementación de FIFO.
 *
 * Estructura: cola de marcos como LISTA DOBLEMENTE ENLAZADA "intrusiva"
 * indexada por número de marco (arreglos next[] y prev[]).
 *   - head = marco más antiguo (la víctima), tail = el más reciente.
 *   - on_load   -> inserta al final:            O(1)
 *   - select    -> lee head:                    O(1)
 *   - on_release-> desenlaza desde cualquier posición (necesario en free): O(1)
 * FIFO ignora los accesos: la "edad" solo cuenta desde que la página se cargó.
 */
#include "policy/fifo.h"

#include <stdbool.h>
#include <stdlib.h>

#define NIL UINT32_MAX

typedef struct FifoPolicy {
    ReplacementPolicy base; /* DEBE ser el primer campo (herencia estilo C) */
    uint32_t *next;
    uint32_t *prev;
    bool *queued;           /* ¿el marco está actualmente en la cola?       */
    uint32_t head;
    uint32_t tail;
    uint32_t num_frames;
} FifoPolicy;

static void fifo_on_load(ReplacementPolicy *self, uint32_t frame)
{
    FifoPolicy *fifo = (FifoPolicy *)self;
    if (frame >= fifo->num_frames || fifo->queued[frame]) {
        return; /* defensivo: ignorar marcos inválidos o ya encolados */
    }
    fifo->prev[frame] = fifo->tail;
    fifo->next[frame] = NIL;
    if (fifo->tail != NIL) {
        fifo->next[fifo->tail] = frame;
    } else {
        fifo->head = frame;
    }
    fifo->tail = frame;
    fifo->queued[frame] = true;
}

static void fifo_on_access(ReplacementPolicy *self, uint32_t frame)
{
    (void)self;
    (void)frame;
}

static void fifo_on_release(ReplacementPolicy *self, uint32_t frame)
{
    FifoPolicy *fifo = (FifoPolicy *)self;
    if (frame >= fifo->num_frames || !fifo->queued[frame]) {
        return;
    }
    const uint32_t before = fifo->prev[frame];
    const uint32_t after  = fifo->next[frame];
    if (before != NIL) {
        fifo->next[before] = after;
    } else {
        fifo->head = after;
    }
    if (after != NIL) {
        fifo->prev[after] = before;
    } else {
        fifo->tail = before;
    }
    fifo->queued[frame] = false;
}

static bool fifo_select_victim(const ReplacementPolicy *self, uint32_t *frame)
{
    const FifoPolicy *fifo = (const FifoPolicy *)self;
    if (fifo->head == NIL) {
        return false;
    }
    *frame = fifo->head;
    return true;
}

static void fifo_destroy(ReplacementPolicy *self)
{
    FifoPolicy *fifo = (FifoPolicy *)self;
    if (fifo == NULL) {
        return;
    }
    free(fifo->next);
    free(fifo->prev);
    free(fifo->queued);
    free(fifo);
}

ReplacementPolicy *fifo_create(uint32_t num_frames)
{
    if (num_frames == 0) {
        return NULL;
    }
    FifoPolicy *fifo = calloc(1, sizeof *fifo);
    if (fifo == NULL) {
        return NULL;
    }
    fifo->next   = malloc(num_frames * sizeof *fifo->next);
    fifo->prev   = malloc(num_frames * sizeof *fifo->prev);
    fifo->queued = calloc(num_frames, sizeof *fifo->queued);
    if (fifo->next == NULL || fifo->prev == NULL || fifo->queued == NULL) {
        fifo_destroy(&fifo->base);
        return NULL;
    }
    fifo->head = NIL;
    fifo->tail = NIL;
    fifo->num_frames = num_frames;

    fifo->base.name          = "FIFO";
    fifo->base.on_load       = fifo_on_load;
    fifo->base.on_access     = fifo_on_access;
    fifo->base.on_release    = fifo_on_release;
    fifo->base.select_victim = fifo_select_victim;
    fifo->base.destroy       = fifo_destroy;
    return &fifo->base;
}
