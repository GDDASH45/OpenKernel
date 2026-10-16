
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 4096

struct config_line {
    char *text;
    char *symbol;
    struct config_line *next;
};

static void free_lines(struct config_line *head)
{
    while (head != NULL) {
        struct config_line *next = head->next;
        free(head->text);
        free(head->symbol);
        free(head);
        head = next;
    }
}

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text))
        text++;

    end = text + strlen(text);

    while (end > text && isspace((unsigned char)end[-1]))
        end--;

    *end = '\0';
    return text;
}

static int valid_symbol(const char *symbol)
{
    const unsigned char *p;

    if (strncmp(symbol, "CONFIG_", 7) != 0)
        return 0;

    if (symbol[7] == '\0')
        return 0;

    for (p = (const unsigned char *)symbol; *p; p++) {
        if (*p == '=')
            return 0;

        if (!isalnum(*p) && *p != '_')
            return 0;
    }

    return 1;
}

/*
 * Return the configuration symbol from either:
 *
 * CONFIG_FOO=y
 * # CONFIG_FOO is not set
 *
 * Return NULL for comments and unrelated lines.
 */
static char *extract_symbol(const char *line)
{
    const char *start;
    const char *end;
    size_t length;
    char *symbol;

    if (strncmp(line, "CONFIG_", 7) == 0) {
        end = strchr(line, '=');

        if (end == NULL)
            return NULL;

        length = (size_t)(end - line);
        symbol = malloc(length + 1);

        if (symbol == NULL)
            return NULL;

        memcpy(symbol, line, length);
        symbol[length] = '\0';

        if (!valid_symbol(symbol)) {
            free(symbol);
            return NULL;
        }

        return symbol;
    }

    if (strncmp(line, "# CONFIG_", 9) == 0) {
        start = line + 2;
        end = strstr(start, " is not set");

        if (end == NULL || end[strlen(" is not set")] != '\0')
            return NULL;

        length = (size_t)(end - start);
        symbol = malloc(length + 1);

        if (symbol == NULL)
            return NULL;

        memcpy(symbol, start, length);
        symbol[length] = '\0';

        if (!valid_symbol(symbol)) {
            free(symbol);
            return NULL;
        }

        return symbol;
    }

    return NULL;
}

static int symbol_exists(
    const struct config_line *head,
    const char *symbol)
{
    for (; head != NULL; head = head->next) {
        if (head->symbol != NULL &&
            strcmp(head->symbol, symbol) == 0)
            return 1;
    }

    return 0;
}

static int append_line(
    struct config_line **head,
    struct config_line **tail,
    const char *text,
    const char *symbol)
{
    struct config_line *entry = calloc(1, sizeof(*entry));

    if (entry == NULL)
        return -1;

    entry->text = strdup(text);

    if (entry->text == NULL) {
        free(entry);
        return -1;
    }

    if (symbol != NULL) {
        entry->symbol = strdup(symbol);

        if (entry->symbol == NULL) {
            free(entry->text);
            free(entry);
            return -1;
        }
    }

    if (*tail != NULL)
        (*tail)->next = entry;
    else
        *head = entry;

    *tail = entry;
    return 0;
}

static int read_config(
    const char *path,
    struct config_line **head,
    struct config_line **tail,
    int optional)
{
    FILE *file;
    char buffer[MAX_LINE];
    unsigned long line_number = 0;

    file = fopen(path, "r");

    if (file == NULL) {
        if (optional && errno == ENOENT)
            return 0;

        fprintf(stderr, "oconf: cannot open %s: %s\n",
                path, strerror(errno));
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        char *newline;
        char *symbol;

        line_number++;

        if (strchr(buffer, '\n') == NULL && !feof(file)) {
            fprintf(stderr, "oconf: line too long in %s:%lu\n",
                    path, line_number);
            fclose(file);
            return -1;
        }

        newline = strchr(buffer, '\n');

        if (newline != NULL)
            *newline = '\0';

        newline = strchr(buffer, '\r');

        if (newline != NULL)
            *newline = '\0';

        symbol = extract_symbol(buffer);

        if (append_line(head, tail, buffer, symbol) != 0) {
            free(symbol);
            fprintf(stderr, "oconf: out of memory\n");
            fclose(file);
            return -1;
        }

        free(symbol);
    }

    if (ferror(file)) {
        fprintf(stderr, "oconf: error reading %s\n", path);
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int validate_default(const char *line, char **symbol_out)
{
    char *copy;
    char *equals;
    char *symbol;
    char *value;

    copy = strdup(line);

    if (copy == NULL)
        return -1;

    symbol = trim(copy);
    equals = strchr(symbol, '=');

    if (equals == NULL) {
        free(copy);
        return 0;
    }

    *equals = '\0';
    value = trim(equals + 1);
    symbol = trim(symbol);

    if (!valid_symbol(symbol)) {
        free(copy);
        return 0;
    }

    if (strcmp(value, "y") != 0 && strcmp(value, "n") != 0) {
        fprintf(stderr, "oconf: invalid default: %s=%s\n",
                symbol, value);
        free(copy);
        return -1;
    }

    *symbol_out = strdup(symbol);

    if (*symbol_out == NULL) {
        free(copy);
        return -1;
    }

    free(copy);
    return 1;
}

static int write_config(
    const char *path,
    const struct config_line *head)
{
    size_t path_length = strlen(path);
    char *temporary = malloc(path_length + 16);
    FILE *file;
    int failed = 0;

    if (temporary == NULL) {
        fprintf(stderr, "oconf: out of memory\n");
        return -1;
    }

    snprintf(temporary, path_length + 16, "%s.tmp.XXXXXX", path);

    {
        int fd = mkstemp(temporary);

        if (fd < 0) {
            fprintf(stderr, "oconf: cannot create temporary config: %s\n",
                    strerror(errno));
            free(temporary);
            return -1;
        }

        file = fdopen(fd, "w");

        if (file == NULL) {
            fprintf(stderr, "oconf: cannot open temporary config: %s\n",
                    strerror(errno));
            close(fd);
            unlink(temporary);
            free(temporary);
            return -1;
        }
    }

    for (; head != NULL; head = head->next) {
        if (fprintf(file, "%s\n", head->text) < 0) {
            failed = 1;
            break;
        }
    }

    if (fflush(file) != 0)
        failed = 1;

    if (fclose(file) != 0)
        failed = 1;

    if (failed) {
        fprintf(stderr, "oconf: failed writing %s\n", path);
        unlink(temporary);
        free(temporary);
        return -1;
    }

    if (rename(temporary, path) != 0) {
        fprintf(stderr, "oconf: cannot replace %s: %s\n",
                path, strerror(errno));
        unlink(temporary);
        free(temporary);
        return -1;
    }

    free(temporary);
    return 0;
}

static int olddefconfig(const char *config_path, const char *defaults_path)
{
    struct config_line *config = NULL;
    struct config_line *config_tail = NULL;
    struct config_line *defaults = NULL;
    struct config_line *defaults_tail = NULL;
    struct config_line *entry;
    int result = -1;

    if (read_config(config_path, &config, &config_tail, 1) != 0)
        goto done;

    if (read_config(defaults_path, &defaults, &defaults_tail, 0) != 0)
        goto done;

    for (entry = defaults; entry != NULL; entry = entry->next) {
        char *symbol = NULL;
        int valid;

        if (entry->text[0] == '\0' || entry->text[0] == '#')
            continue;

        valid = validate_default(entry->text, &symbol);

        if (valid < 0)
            goto done;

        if (valid == 0)
            continue;

        if (!symbol_exists(config, symbol)) {
            char *default_line = strdup(entry->text);

            if (default_line == NULL ||
                append_line(&config, &config_tail,
                            default_line, symbol) != 0) {
                free(default_line);
                free(symbol);
                fprintf(stderr, "oconf: out of memory\n");
                goto done;
            }

            free(default_line);
        }

        free(symbol);
    }

    if (write_config(config_path, config) != 0)
        goto done;

    printf("oconf: updated %s\n", config_path);
    result = 0;

done:
    free_lines(config);
    free_lines(defaults);
    return result;
}

static void print_usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s --olddefconfig CONFIG DEFAULTS\n",
            program);
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "--olddefconfig") == 0)
        return olddefconfig(argv[2], argv[3]) == 0 ? EXIT_SUCCESS
                                                   : EXIT_FAILURE;

    print_usage(argv[0]);
    return EXIT_FAILURE;
}
