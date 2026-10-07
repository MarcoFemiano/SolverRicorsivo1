#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t variables;
    size_t clauses;
    size_t literals_count;
    size_t literals_capacity;
    size_t *offsets;
    int *literals;
} Formula;

static void free_formula(Formula *formula)
{
    free(formula->offsets);
    free(formula->literals);
    memset(formula, 0, sizeof(*formula));
}

/* Restituisce 1 per una riga, 0 per EOF, -1 per errore. */
static int read_line(FILE *input, char **buffer, size_t *capacity)
{
    size_t length = 0;
    int character;

    if (*buffer == NULL) {
        *capacity = 256;
        *buffer = malloc(*capacity);
        if (*buffer == NULL) {
            return -1;
        }
    }

    while ((character = fgetc(input)) != EOF) {
        if (character == '\0') {
            return -1;
        }
        if (character == '\n') {
            break;
        }
        if (length + 1 >= *capacity) {
            size_t new_capacity;
            char *new_buffer;

            if (*capacity > SIZE_MAX / 2) {
                return -1;
            }
            new_capacity = *capacity * 2;
            new_buffer = realloc(*buffer, new_capacity);
            if (new_buffer == NULL) {
                return -1;
            }
            *buffer = new_buffer;
            *capacity = new_capacity;
        }
        (*buffer)[length++] = (char)character;
    }

    if (ferror(input)) {
        return -1;
    }
    if (character == EOF && length == 0) {
        return 0;
    }
    (*buffer)[length] = '\0';
    return 1;
}

static char *next_token(char **position)
{
    char *start = *position;

    while (isspace((unsigned char)*start)) {
        ++start;
    }
    if (*start == '\0') {
        *position = start;
        return NULL;
    }

    *position = start;
    while (**position != '\0' && !isspace((unsigned char)**position)) {
        ++*position;
    }
    if (**position != '\0') {
        **position = '\0';
        ++*position;
    }
    return start;
}

static int parse_nonnegative(const char *text, size_t *number)
{
    unsigned long long value;
    char *end;
    const unsigned char *cursor = (const unsigned char *)text;

    if (*cursor == '\0') {
        return 0;
    }
    while (*cursor != '\0') {
        if (!isdigit(*cursor)) {
            return 0;
        }
        ++cursor;
    }

    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || value > SIZE_MAX) {
        return 0;
    }
    *number = (size_t)value;
    return 1;
}

static int append_literal(Formula *formula, int literal)
{
    if (formula->literals_count == formula->literals_capacity) {
        size_t new_capacity = formula->literals_capacity == 0
                                  ? 16
                                  : formula->literals_capacity * 2;
        int *new_literals;

        if (new_capacity < formula->literals_capacity ||
            new_capacity > SIZE_MAX / sizeof(*formula->literals)) {
            return 0;
        }
        new_literals = realloc(formula->literals,
                               new_capacity * sizeof(*formula->literals));
        if (new_literals == NULL) {
            return 0;
        }
        formula->literals = new_literals;
        formula->literals_capacity = new_capacity;
    }

    formula->literals[formula->literals_count++] = literal;
    return 1;
}

static int parse_formula(FILE *input, Formula *formula)
{
    char *line = NULL;
    size_t line_capacity = 0;
    size_t line_number = 0;
    size_t declared_clauses = 0;
    int has_header = 0;
    int line_result;

    while ((line_result = read_line(input, &line, &line_capacity)) == 1) {
        char *position = line;
        char *token;

        ++line_number;
        token = next_token(&position);
        if (token == NULL || strcmp(token, "c") == 0) {
            continue;
        }
        if (strcmp(token, "FINE") == 0) {
            if (next_token(&position) != NULL) {
                fprintf(stderr, "Errore alla riga %zu: FINE deve essere da solo.\n",
                        line_number);
                goto fail;
            }
            break;
        }

        if (!has_header) {
            char *format;
            char *variables_text;
            char *clauses_text;

            if (strcmp(token, "p") != 0) {
                fprintf(stderr, "Errore alla riga %zu: manca l'intestazione p cnf.\n",
                        line_number);
                goto fail;
            }
            format = next_token(&position);
            variables_text = next_token(&position);
            clauses_text = next_token(&position);
            if (format == NULL || strcmp(format, "cnf") != 0 ||
                variables_text == NULL || clauses_text == NULL ||
                next_token(&position) != NULL ||
                !parse_nonnegative(variables_text, &formula->variables) ||
                formula->variables > INT_MAX ||
                !parse_nonnegative(clauses_text, &declared_clauses) ||
                declared_clauses >= SIZE_MAX / sizeof(*formula->offsets)) {
                fprintf(stderr, "Errore alla riga %zu: intestazione p cnf non valida.\n",
                        line_number);
                goto fail;
            }
            formula->offsets = calloc(declared_clauses + 1,
                                      sizeof(*formula->offsets));
            if (formula->offsets == NULL) {
                fprintf(stderr, "Errore: memoria insufficiente.\n");
                goto fail;
            }
            has_header = 1;
            continue;
        }

        do {
            long long value;
            char *end;

            errno = 0;
            value = strtoll(token, &end, 10);
            if (errno == ERANGE || end == token || *end != '\0' ||
                value < -(long long)formula->variables ||
                value > (long long)formula->variables) {
                fprintf(stderr, "Errore alla riga %zu: letterale non valido: %s.\n",
                        line_number, token);
                goto fail;
            }
            if (formula->clauses == declared_clauses) {
                fprintf(stderr, "Errore alla riga %zu: troppe clausole.\n",
                        line_number);
                goto fail;
            }
            if (value == 0) {
                ++formula->clauses;
                formula->offsets[formula->clauses] = formula->literals_count;
            } else if (!append_literal(formula, (int)value)) {
                fprintf(stderr, "Errore: memoria insufficiente.\n");
                goto fail;
            }
        } while ((token = next_token(&position)) != NULL);
    }

    if (line_result == -1) {
        fprintf(stderr, "Errore: lettura dell'input non riuscita o memoria insufficiente.\n");
        goto fail;
    }
    if (!has_header) {
        fprintf(stderr, "Errore: manca l'intestazione p cnf.\n");
        goto fail;
    }
    if (formula->clauses != declared_clauses ||
        formula->literals_count != formula->offsets[formula->clauses]) {
        fprintf(stderr, "Errore: clausole incomplete o numero di clausole diverso dall'intestazione.\n");
        goto fail;
    }

    free(line);
    return 1;

fail:
    free(line);
    return 0;
}

/* -1 = non assegnata, 0 = falso, 1 = vero.
 * Un ritorno 0 indica una contraddizione da gestire nel livello precedente. */
static int solve(const Formula *formula, signed char *values, size_t clause)
{
    size_t index;
    int choice = 0;

    /* Caso base 4: tutte le clausole sono state visitate. */
    if (clause == formula->clauses) {
        size_t variable;

        puts("Formula soddisfacibile.");
        puts("Assegnazioni:");
        for (variable = 1; variable <= formula->variables; ++variable) {
            printf("x%zu = %s\n", variable,
                   values[variable] == 1 ? "vero" : "falso");
        }
        return 1;
    }

    for (index = formula->offsets[clause];
         index < formula->offsets[clause + 1]; ++index) {
        int literal = formula->literals[index];
        size_t variable = (size_t)(literal > 0 ? literal : -literal);

        if (values[variable] == -1) {
            if (choice == 0) {
                choice = literal;
            }
        } else if ((literal > 0 && values[variable] == 1) ||
                   (literal < 0 && values[variable] == 0)) {
            /* Caso base 3: questa clausola non richiede assegnazioni. */
            return solve(formula, values, clause + 1);
        }
    }

    /* Caso base 1: clausola falsa e nessuna variabile libera. */
    if (choice == 0) {
        return 0;
    }

    {
        size_t variable = (size_t)(choice > 0 ? choice : -choice);
        signed char satisfying_value = choice > 0 ? 1 : 0;

        /* Passo ricorsivo: soddisfa la clausola assegnando una variabile. */
        values[variable] = satisfying_value;
        if (solve(formula, values, clause + 1)) {
            return 1;
        }

        /* Prima contraddizione: cambia l'ultima assegnazione. */
        values[variable] = (signed char)(1 - satisfying_value);
        if (solve(formula, values, clause)) {
            return 1;
        }

        /* Seconda contraddizione: annulla l'assegnazione e torna indietro. */
        values[variable] = -1;
    }
    return 0;
}

int main(void)
{
    Formula formula = {0};
    signed char *values;
    size_t variable;

    if (!parse_formula(stdin, &formula)) {
        free_formula(&formula);
        return EXIT_FAILURE;
    }

    values = malloc((formula.variables + 1) * sizeof(*values));
    if (values == NULL) {
        fprintf(stderr, "Errore: memoria insufficiente.\n");
        free_formula(&formula);
        return EXIT_FAILURE;
    }
    for (variable = 0; variable <= formula.variables; ++variable) {
        values[variable] = -1;
    }

    if (!solve(&formula, values, 0)) {
        puts("Formula insoddisfacibile.");
    }

    free(values);
    free_formula(&formula);
    return EXIT_SUCCESS;
}
