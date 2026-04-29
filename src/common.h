#ifndef STARBYTE_COMMON_H
#define STARBYTE_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_UNUSED(x) ((void)(x))

static inline void *sb_xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "starbyte: out of memory\n"); exit(1); }
    return p;
}
static inline void *sb_xcalloc(size_t n, size_t s) {
    void *p = calloc(n, s);
    if (!p) { fprintf(stderr, "starbyte: out of memory\n"); exit(1); }
    return p;
}
static inline void *sb_xrealloc(void *ptr, size_t n) {
    void *p = realloc(ptr, n);
    if (!p) { fprintf(stderr, "starbyte: out of memory\n"); exit(1); }
    return p;
}
static inline char *sb_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *r = (char*)sb_xmalloc(n);
    memcpy(r, s, n);
    return r;
}
static inline char *sb_strndup(const char *s, size_t n) {
    char *r = (char*)sb_xmalloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

#endif
