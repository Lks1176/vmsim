#include "app/command_parser.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

#include "core/numparse.h"

#define TOKEN_MAX 32

typedef enum TokenResult { TOKEN_OK, TOKEN_EOF, TOKEN_TOO_LONG } TokenResult;

/* Descripción de cada comando: palabra clave y cantidad de argumentos. */
static const struct {
    const char *name;
    int arg_count;
} COMMAND_SPECS[CMD_TYPE_COUNT] = {
    [CMD_ALLOC] = {"alloc", 1},
    [CMD_WRITE] = {"write", 2},
    [CMD_READ]  = {"read", 1},
    [CMD_FREE]  = {"free", 1},
};

void parser_init(CommandParser *parser, FILE *input)
{
    parser->input = input;
    parser->line = 1;
    parser->token_line = 1;
    parser->error[0] = '\0';
}

const char *parser_error(const CommandParser *parser)
{
    return parser->error;
}

const char *command_name(CommandType type)
{
    return type < CMD_TYPE_COUNT ? COMMAND_SPECS[type].name : "?";
}

static ParseStatus fail(CommandParser *parser, const char *message, const char *detail)
{
    snprintf(parser->error, sizeof parser->error, "%s%s%s%s", message,
             detail ? " '" : "", detail ? detail : "", detail ? "'" : "");
    return PARSE_ERROR;
}

/* Lee un token saltando espacios y comentarios; cuenta las líneas. */
static TokenResult next_token(CommandParser *parser, char *buffer, size_t capacity)
{
    int c;
    for (;;) {
        c = fgetc(parser->input);
        if (c == EOF) {
            return TOKEN_EOF;
        }
        if (c == '#') { /* comentario: descartar hasta el fin de línea */
            while ((c = fgetc(parser->input)) != EOF && c != '\n') {
            }
            if (c == EOF) {
                return TOKEN_EOF;
            }
            parser->line++;
        } else if (c == '\n') {
            parser->line++;
        } else if (!isspace((unsigned char)c)) {
            break;
        }
    }

    parser->token_line = parser->line;
    size_t length = 0;
    bool too_long = false;
    do {
        if (length + 1u < capacity) {
            buffer[length++] = (char)c;
        } else {
            too_long = true;
        }
        c = fgetc(parser->input);
    } while (c != EOF && c != '#' && !isspace((unsigned char)c));
    if (c != EOF) {
        ungetc(c, parser->input); /* el separador se procesa en la próxima llamada */
    }
    buffer[length] = '\0';
    return too_long ? TOKEN_TOO_LONG : TOKEN_OK;
}

ParseStatus parser_next(CommandParser *parser, Command *command)
{
    char token[TOKEN_MAX];

    const TokenResult first = next_token(parser, token, sizeof token);
    if (first == TOKEN_EOF) {
        return PARSE_EOF;
    }
    if (first == TOKEN_TOO_LONG) {
        return fail(parser, "comando demasiado largo", NULL);
    }

    for (char *p = token; *p != '\0'; p++) {
        *p = (char)tolower((unsigned char)*p);
    }
    int type = -1;
    for (int i = 0; i < CMD_TYPE_COUNT; i++) {
        if (strcmp(token, COMMAND_SPECS[i].name) == 0) {
            type = i;
            break;
        }
    }
    if (type < 0) {
        return fail(parser, "comando desconocido", token);
    }

    memset(command, 0, sizeof *command);
    command->type = (CommandType)type;
    command->line = parser->token_line;

    uint32_t args[2] = {0, 0};
    for (int i = 0; i < COMMAND_SPECS[type].arg_count; i++) {
        if (next_token(parser, token, sizeof token) != TOKEN_OK) {
            return fail(parser, "falta un argumento para", COMMAND_SPECS[type].name);
        }
        if (!parse_u32(token, &args[i])) {
            return fail(parser, "argumento numérico inválido", token);
        }
    }

    switch (command->type) {
    case CMD_ALLOC:
        command->bytes = args[0];
        break;
    case CMD_WRITE:
        if (args[1] > UINT8_MAX) {
            return fail(parser, "el valor de write debe estar entre 0 y 255", NULL);
        }
        command->address = args[0];
        command->value = (uint8_t)args[1];
        break;
    case CMD_READ:
    case CMD_FREE:
        command->address = args[0];
        break;
    case CMD_TYPE_COUNT:
        break;
    }
    return PARSE_OK;
}
