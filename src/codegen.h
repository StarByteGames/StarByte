#ifndef STARBYTE_CODEGEN_H
#define STARBYTE_CODEGEN_H

#include "ast.h"

/* Emit a complete C translation unit for `program` to file path `c_out_path`.
   Returns 0 on success, non-zero on error. */
int codegen_emit_c(Node *program, const char *c_out_path, const char *src_filename);

/* Compile a generated .c file into an executable using $CC (or "cc"). */
int codegen_compile_c(const char *c_path, const char *exe_path, const char *cc_override);

#endif
