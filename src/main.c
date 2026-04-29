#include "common.h"
#include "parser.h"
#include "interpreter.h"
#include <errno.h>

#ifndef STARBYTE_VERSION
#define STARBYTE_VERSION "0.1.0"
#endif

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "starbyte: cannot open '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = (char*)sb_xmalloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "StarByte %s\n"
        "Usage: %s <file.sb> [-o <output>] [--ast]\n"
        "\n"
        "Options:\n"
        "  -o <name>   Output name (reserved for future compiler).\n"
        "  --run       Force run (default behavior).\n"
        "  --version   Print version and exit.\n"
        "  -h, --help  Show this help.\n",
        STARBYTE_VERSION, prog);
}

int main(int argc, char **argv) {
    const char *input = NULL;
    const char *output = NULL;
    SB_UNUSED(output);

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]); return 0;
        } else if (strcmp(a, "--version") == 0) {
            printf("starbyte %s\n", STARBYTE_VERSION); return 0;
        } else if (strcmp(a, "-o") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "starbyte: -o requires an argument\n"); return 2; }
            output = argv[++i];
        } else if (strcmp(a, "--run") == 0) {
            /* default */
        } else if (a[0] == '-') {
            fprintf(stderr, "starbyte: unknown option '%s'\n", a);
            return 2;
        } else {
            if (input) { fprintf(stderr, "starbyte: only one input file supported\n"); return 2; }
            input = a;
        }
    }

    if (!input) { print_usage(argv[0]); return 2; }

    char *src = read_file(input);
    if (!src) return 1;

    Parser p;
    parser_init(&p, src, input);
    Node *prog = parser_parse_program(&p);
    parser_dispose(&p);

    Interp I;
    interp_init(&I, input);
    int code = interp_run(&I, prog);
    interp_dispose(&I);
    node_free(prog);
    free(src);
    return code;
}
