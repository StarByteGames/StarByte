#include "common.h"
#include "parser.h"
#include "interpreter.h"
#include "codegen.h"
#include <errno.h>

#ifndef STARBYTE_VERSION
#define STARBYTE_VERSION "0.6.0"
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
        "Usage: %s <file.sb> [options]\n"
        "\n"
        "Options:\n"
        "  -o <name>     Compile to a native executable named <name>\n"
        "                (transpiles to C, then invokes $CC, default 'cc').\n"
        "  --emit-c <p>  With -o: keep the generated .c file at path <p>.\n"
        "                Without -o: emit C only, do not compile.\n"
        "  --cc <prog>   Use this C compiler (overrides $CC).\n"
        "  --run         Force interpreter mode (default when no -o).\n"
        "  --version     Print version and exit.\n"
        "  -h, --help    Show this help.\n",
        STARBYTE_VERSION, prog);
}

int main(int argc, char **argv) {
    const char *input    = NULL;
    const char *output   = NULL;
    const char *emit_c   = NULL;
    const char *cc_over  = NULL;
    bool        force_run = false;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            print_usage(argv[0]); return 0;
        } else if (strcmp(a, "--version") == 0) {
            printf("starbyte %s\n", STARBYTE_VERSION); return 0;
        } else if (strcmp(a, "-o") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "starbyte: -o requires an argument\n"); return 2; }
            output = argv[++i];
        } else if (strcmp(a, "--emit-c") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "starbyte: --emit-c requires an argument\n"); return 2; }
            emit_c = argv[++i];
        } else if (strcmp(a, "--cc") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "starbyte: --cc requires an argument\n"); return 2; }
            cc_over = argv[++i];
        } else if (strcmp(a, "--run") == 0) {
            force_run = true;
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

    int code = 0;

    if (!force_run && (output || emit_c)) {
        /* Native compile path */
        char tmp_c[1024];
        const char *c_path = emit_c;
        bool delete_c = false;
        if (!c_path) {
            const char *base = output ? output : "a";
            snprintf(tmp_c, sizeof tmp_c, "%s.sb.c", base);
            c_path = tmp_c;
            delete_c = true;
        }
        code = codegen_emit_c(prog, c_path, input);
        if (code == 0 && output) {
            code = codegen_compile_c(c_path, output, cc_over);
            if (delete_c) remove(c_path);
        } else if (code == 0 && !output) {
            fprintf(stderr, "starbyte: emitted C to %s (no -o, skipping compile)\n", c_path);
        }
    } else {
        /* Interpreter path (default) */
        Interp I;
        interp_init(&I, input);
        code = interp_run(&I, prog);
        interp_dispose(&I);
    }

    node_free(prog);
    free(src);
    return code;
}
