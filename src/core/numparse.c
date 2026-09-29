#include "core/numparse.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

bool parse_u64(const char *text, uint64_t *out)
{
    if (text == NULL || out == NULL || !isdigit((unsigned char)text[0])) {
        return false; /* también descarta '-', '+' y espacios iniciales */
    }
    const int base = (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) ? 16 : 10;
    char *end = NULL;
    errno = 0;
    const unsigned long long value = strtoull(text, &end, base);
    if (errno == ERANGE || end == text || *end != '\0') {
        return false;
    }
    *out = (uint64_t)value;
    return true;
}

bool parse_u32(const char *text, uint32_t *out)
{
    uint64_t wide;
    if (out == NULL || !parse_u64(text, &wide) || wide > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)wide;
    return true;
}
