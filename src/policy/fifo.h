/*
 * fifo.h - Política FIFO: se expulsa la página que lleva más tiempo en memoria.
 */
#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>

#include "policy/replacement.h"

ReplacementPolicy *fifo_create(uint32_t num_frames);

#endif
