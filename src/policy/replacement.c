#include "policy/replacement.h"

#include <ctype.h>
#include <stddef.h>

#include "policy/fifo.h"

/* Registro de políticas disponibles: nombre de CLI -> tipo. */
static const struct {
    const char *name;
    PolicyType type;
} POLICY_TABLE[] = {
    {"fifo", POLICY_FIFO},
};

static bool equals_ignore_case(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

ReplacementPolicy *replacement_create(PolicyType type, uint32_t num_frames)
{
    switch (type) {
    case POLICY_FIFO:
        return fifo_create(num_frames);
    }
    return NULL;
}

bool replacement_parse_name(const char *name, PolicyType *out)
{
    if (name == NULL || out == NULL) {
        return false;
    }
    for (size_t i = 0; i < sizeof POLICY_TABLE / sizeof POLICY_TABLE[0]; i++) {
        if (equals_ignore_case(name, POLICY_TABLE[i].name)) {
            *out = POLICY_TABLE[i].type;
            return true;
        }
    }
    return false;
}

const char *replacement_type_name(PolicyType type)
{
    switch (type) {
    case POLICY_FIFO:
        return "FIFO";
    }
    return "?";
}
