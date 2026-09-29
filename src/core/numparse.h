/*
 * numparse.h - Conversión estricta de texto a enteros sin signo.
 * Acepta decimal ("4096") y hexadecimal ("0x1000"). Rechaza signos, espacios,
 * basura al final y desbordes (a diferencia de atoi/strtoul a secas).
 */
#ifndef NUMPARSE_H
#define NUMPARSE_H

#include <stdbool.h>
#include <stdint.h>

bool parse_u64(const char *text, uint64_t *out);
bool parse_u32(const char *text, uint32_t *out);

#endif
