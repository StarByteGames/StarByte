# How to Use the StarByte Compiler

A practical guide to the `starbyte` command-line tool itself —
how to build it, run it, configure it, and ship the binaries it
produces. For the language syntax, see [LANGUAGE.md](LANGUAGE.md).

---

## Contents

1. [Building `starbyte` from source](#1-building-starbyte-from-source)
2. [Installing system-wide or per-user](#2-installing-system-wide-or-per-user)
3. [Verifying the install](#3-verifying-the-install)
4. [Command-line synopsis](#4-command-line-synopsis)
5. [Interpreter mode (default)](#5-interpreter-mode-default)
6. [Native compile mode (`-o`)](#6-native-compile-mode--o)
7. [Choosing a C compiler (`--cc`, `$CC`)](#7-choosing-a-c-compiler---cc-cc)
8. [Emitting and inspecting generated C (`--emit-c`)](#8-emitting-and-inspecting-generated-c---emit-c)
9. [Forcing interpreter mode (`--run`)](#9-forcing-interpreter-mode---run)
10. [Exit codes](#10-exit-codes)
11. [Cross-compiling](#11-cross-compiling)
12. [Static binaries and stripping](#12-static-binaries-and-stripping)
13. [Optimisation flags for the C backend](#13-optimisation-flags-for-the-c-backend)
14. [Make targets reference](#14-make-targets-reference)
15. [Uninstalling](#15-uninstalling)
16. [Troubleshooting](#16-troubleshooting)

---

## 1. Building `starbyte` from source

Requirements:

| Tool          | Minimum | Notes                          |
|---------------|---------|--------------------------------|
| C11 compiler  | gcc 7+ / clang 6+ / MSVC 2019+ | Used to build `starbyte` itself |
| `make`        | any POSIX make | Default build driver |
| `cmake`       | 3.15+   | Optional alternative           |

```bash
git clone https://github.com/StarByteGames/StarByte.git
cd StarByte

# default = release build (-O2)
make -j

# debug build (-O0 -g3)
make debug

# alternative: CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The resulting binary is `build/starbyte`.

---

## 2. Installing system-wide or per-user

System-wide (requires root):

```bash
sudo make install                 # -> /usr/local/bin/starbyte
sudo make install PREFIX=/opt/starbyte
```

Per-user (no root needed):

```bash
make install PREFIX=$HOME/.local
# make sure $HOME/.local/bin is on your PATH
```

`DESTDIR` is honoured for packaging:

```bash
make install DESTDIR=/tmp/pkg PREFIX=/usr
```

---

## 3. Verifying the install

```bash
starbyte --version
# starbyte 0.3.0

which starbyte
# /usr/local/bin/starbyte

starbyte --help
```

---

## 4. Command-line synopsis

```
starbyte <file.sb> [options]
```

| Flag           | Effect                                                        |
|----------------|---------------------------------------------------------------|
| `<file.sb>`    | Source file to run or compile (exactly one).                  |
| `-o <name>`    | Compile to a native executable named `<name>`.                |
| `--emit-c <p>` | Write the generated C file to `<p>`. With `-o` the C file is kept; without `-o` only C is produced. |
| `--cc <prog>`  | Use this C compiler for `-o` (overrides `$CC`).               |
| `--run`        | Force interpreter mode even if `-o` is given.                 |
| `--version`    | Print version and exit.                                       |
| `-h`, `--help` | Print help and exit.                                          |

Exactly one input file is allowed per invocation.

---

## 5. Interpreter mode (default)

Without `-o`, StarByte parses the file and executes it directly with
its tree-walking interpreter:

```bash
starbyte hello.sb
```

Use this for development, scripts, and quick experimentation.
No external compiler is required.

---

## 6. Native compile mode (`-o`)

```bash
starbyte hello.sb -o hello
./hello
```

Pipeline:

1. Parse `hello.sb` into an AST.
2. Transpile the AST into a self-contained C11 file
   (`<output>.sb.c` next to the binary, removed afterwards).
3. Invoke the system C compiler with:
   ```
   $CC -O2 -std=c11 -o <output> <generated>.c -lm
   ```
4. Delete the temporary C file unless `--emit-c` was given.

The resulting executable has **no runtime dependency** on `starbyte` —
it is an ordinary native binary.

---

## 7. Choosing a C compiler (`--cc`, `$CC`)

Resolution order, highest priority first:

1. `--cc <prog>` flag
2. `$CC` environment variable
3. `cc` on `$PATH`

```bash
# Pick clang explicitly
starbyte app.sb -o app --cc clang

# Use a specific gcc via the environment
CC=gcc-13 starbyte app.sb -o app

# zig as a drop-in C compiler
starbyte app.sb -o app --cc "zig cc"
```

Anything you put in `--cc` is invoked verbatim before the StarByte
flags, so a string like `"clang -fsanitize=address"` works too.

---

## 8. Emitting and inspecting generated C (`--emit-c`)

Just look at the generated C, do not compile:

```bash
starbyte hello.sb --emit-c hello.c
less hello.c
```

Compile **and** keep the C source (handy for debugging or shipping it
elsewhere):

```bash
starbyte hello.sb -o hello --emit-c hello.c
```

Compile the emitted C yourself:

```bash
cc -O2 -std=c11 hello.c -lm -o hello
```

The generated file always begins with the StarByte runtime
(`sb_value` tagged union plus built-in helpers) followed by your
translated functions and a normal `int main(...)` entry point.

---

## 9. Forcing interpreter mode (`--run`)

If both `-o` and `--run` are given, `--run` wins and the program is
executed by the interpreter (handy in scripts that may or may not
also produce a binary):

```bash
starbyte tool.sb -o tool --run    # runs in interpreter, ignores -o
```

---

## 10. Exit codes

| Code  | Meaning                                                   |
|-------|-----------------------------------------------------------|
| `0`   | Success.                                                  |
| `1`   | I/O error (cannot read input, cannot write generated C).  |
| `2`   | Bad CLI arguments.                                        |
| other | Forwarded from your program's `main()` or from the C compiler. |

In native mode, the produced binary's own exit code is whatever your
StarByte `main()` returns.

---

## 11. Cross-compiling

Use any cross-compiling C toolchain via `--cc`:

```bash
# Linux -> Windows .exe
starbyte app.sb -o app.exe --cc x86_64-w64-mingw32-gcc

# Linux x86_64 -> Linux aarch64
starbyte app.sb -o app --cc aarch64-linux-gnu-gcc

# Anywhere -> anywhere via zig
starbyte app.sb -o app.exe --cc "zig cc -target x86_64-windows-gnu"
```

`starbyte` itself only generates portable ISO C11 — anything your C
toolchain can build, you can target.

---

## 12. Static binaries and stripping

Static linking on Linux (glibc ≥ recent or musl):

```bash
starbyte app.sb -o app --cc "gcc -static"
```

Strip symbols for a smaller artifact:

```bash
strip app
```

Combine both:

```bash
starbyte app.sb -o app --cc "musl-gcc -static" && strip app
```

---

## 13. Optimisation flags for the C backend

The backend hard-codes `-O2 -std=c11 ... -lm`. If you need different
flags, use the `--emit-c` workflow:

```bash
starbyte app.sb --emit-c app.c
gcc -O3 -march=native -flto app.c -lm -o app
```

A first-class `--cflags` passthrough is on the roadmap.

---

## 14. Make targets reference

These are the targets exposed by the project Makefile (used for
building `starbyte` itself, not for compiling `.sb` files):

| Target             | Description                                       |
|--------------------|---------------------------------------------------|
| `make` / `release` | Optimised build → `build/starbyte`                |
| `make debug`       | Debug build (`-O0 -g3`)                           |
| `make run`         | Build and run `examples/demo.sb`                  |
| `make examples`    | Run every `examples/*.sb` file                    |
| `make install`     | Install to `$(PREFIX)/bin` (default `/usr/local`) |
| `make uninstall`   | Remove the installed binary                       |
| `make clean`       | Remove `build/`                                   |

Common variable overrides:

```bash
make CC=clang OPT="-O3 -march=native"
make install PREFIX=$HOME/.local
make install DESTDIR=$PWD/stage PREFIX=/usr
```

---

## 15. Uninstalling

```bash
sudo make uninstall                      # /usr/local
make uninstall PREFIX=$HOME/.local       # per-user
```

This removes `$(PREFIX)/bin/starbyte`. Generated `.c` files and
binaries built with `-o` are not touched — delete them manually.

---

## 16. Troubleshooting

| Symptom                                                     | Fix |
|-------------------------------------------------------------|-----|
| `starbyte: command not found`                               | Make sure `$PREFIX/bin` is on your `PATH`, or call the binary by full path. |
| `starbyte: cannot open 'foo.sb': No such file or directory` | Wrong working directory or typo. |
| `starbyte: -o requires an argument`                         | Add the output name: `-o myapp`. |
| `starbyte: only one input file supported`                   | Pass exactly one `.sb` file per invocation. |
| `starbyte: cannot write '...sb.c'`                          | The current directory is read-only or full; pass an explicit `--emit-c /tmp/out.c`. |
| `starbyte: C compiler failed`                               | Install a C compiler or use `--cc` to point at one. Try the same command without `-o` to confirm the program itself runs. |
| `starbyte: unknown option '...'`                            | Re-check the [synopsis](#4-command-line-synopsis). |
| Native binary behaves differently from interpreter          | Re-run with `--emit-c`, inspect the C file, and report a bug with the minimal `.sb` reproducer. |

If something else goes wrong, please open an issue with:

- The exact command line you ran.
- The output of `starbyte --version`.
- The minimal `.sb` file that triggers the problem.
