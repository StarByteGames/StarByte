# StarByte 0.8.0

A fast, modern, general-purpose programming language — C-level performance, C#-style ergonomics, written in pure C.

![C](https://img.shields.io/badge/C-C11-A8B9CC?logo=c&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-FCC624?logo=linux&logoColor=black)
![Release](https://img.shields.io/badge/Release-Alpha-orange)

> **Alpha release** — core language, interpreter, native compiler backend,
> and standard library work, but expect rough edges. Bug reports and
> feedback are welcome.

## What's new in 0.8.0

- **Exceptions** — `try` / `catch` / `finally` and `throw` are now
  available in both the interpreter and the native backend (`-o`):

  ```cs
  int safeDiv(int a, int b) {
      if (b == 0) throw "division by zero";
      return a / b;
  }

  try {
      Console.WriteLine(safeDiv(10, 0));
  } catch (string e) {
      Console.WriteLine("caught:", e);
  } finally {
      Console.WriteLine("cleanup runs either way");
  }
  ```

  Any value can be thrown (string, int, struct, object, ...). The
  catch clause accepts an optional type annotation that is currently
  ignored at runtime — every catch is a catch-all. A `try` block may
  omit `catch` and use only `finally`, in which case in-flight
  exceptions propagate to the next handler. Uncaught exceptions abort
  the program with `<file>:<line>: uncaught exception: <value>`.
  See [examples/exceptions.sb](examples/exceptions.sb).

## What's new in 0.7.0

- **Manual memory and a garbage collector** — heap-allocated buffers are
  now a first-class value type, with bracket indexing for read and write:

  ```cs
  int xs = alloc(10);          // manual: explicit free()
  xs[0] = 42;
  Console.WriteLine(xs[0], len(xs));
  free(xs);

  int ys = gc_alloc(3);        // GC-managed: collected on demand
  ys[0] = "hello"; ys[1] = 1; ys[2] = true;
  ys = null;
  gc_collect();                // mark/sweep, returns count freed
  ```

  Both forms are available globally and as `Memory.alloc`,
  `Memory.free`, `Memory.gcAlloc`, `Memory.gcCollect`, `Memory.length`
  (and the `System.Memory.*` aliases). Buffers participate in the same
  reference-count machinery as strings/objects, so passing them around
  works as expected. Both the interpreter and the native backend
  (`-o`) implement the new builtins. See
  [examples/memory.sb](examples/memory.sb).
- New AST node and parser rule for `expr[expr]` indexing, usable on
  buffers (and convertible to a write target via `expr[expr] = value`,
  including compound assignment).

## What's new in 0.6.0

- **Classes and interfaces in the native backend** — `starbyte file.sb -o file`
  now compiles `class`, `interface`, single inheritance, multiple interface
  implementation, methods, fields with default initializers, constructors,
  `this`, `super` (both `super.method(...)` and `super(...)` for constructor
  delegation), and the optional `new` keyword. See
  [examples/classes.sb](examples/classes.sb).
- Interface conformance is now also checked at codegen time; missing methods
  raise a clear compile-time error instead of being deferred to the
  interpreter.
- Method dispatch is dynamic (virtual): overrides in derived classes are
  picked up at runtime, just like in the interpreter.

## What's new in 0.5.0

- **Object orientation** — `class`, `interface`, single inheritance and
  multiple interface implementation, methods, fields with default
  initializers, constructors, `this`, `super` (both `super.method(...)`
  and `super(...)` for constructor delegation), and the optional `new`
  keyword. See [examples/classes.sb](examples/classes.sb).
- Interface conformance is checked at class-declaration time; missing
  methods raise a clear runtime error.
- Classes and interfaces currently run in both the interpreter and the
  native backend (`-o`).

## What's new in 0.4.0

- `struct` and `enum` are now also supported by the native backend
  (`starbyte file.sb -o file`). Brace initializers, default
  construction, field reads/writes (including compound assignment),
  and dotted/unqualified enum access all work end-to-end through
  the C transpiler.
- Generated programs print structs in the same `Name{f=v, ...}`
  form as the interpreter.

## What's new in 0.3.0

- `struct` types with brace initializers, field reads, and field
  assignment (including compound assignment) — see
  [examples/structs.sb](examples/structs.sb).
- `enum` types with auto-incrementing or explicit integer values,
  accessible as `Color.RED` or unqualified `RED`.
- Interpreter only in 0.3.0; the native backend (`-o`) gained
  full support in 0.4.0.

## What's new in 0.2.0

- Native compiler backend: `starbyte file.sb -o file` produces a
  standalone executable by transpiling to C and invoking the system
  C compiler.
- New flags: `-o`, `--emit-c`, `--cc`, `--run`.
- Both short (`Console.WriteLine`) and fully-qualified
  (`System.Console.WriteLine`) namespace forms are accepted.
- Documentation overhaul (README + `docs/LANGUAGE.md`).
- VS Code syntax-highlighting extension under `editor/vscode/`.

## What is this?

StarByte is a small programming language with familiar C/C# syntax,
implemented as a single self-contained C11 binary. It ships with a
tree-walking interpreter, a native compiler backend, a built-in
standard library (`Console`, `Math`, `Strings`) and a `module`
system. Use it for quick scripts or as a playground for language ideas.

## Installation

### Quick install
(automatic)

```bash
git clone https://github.com/StarByteGames/StarByte.git
cd StarByte
make -j
sudo make install        # -> /usr/local/bin/starbyte
```

This builds the `starbyte` binary and installs it system-wide.
Drop the `sudo` and add `PREFIX=$HOME/.local` to install into your home.

### Manual install

#### Prerequisites

| Package    | Why                              |
|------------|----------------------------------|
| `gcc` / `clang` | C11 compiler                |
| `make`     | Build driver (POSIX make)        |
| `cmake` (optional) | Alternative build path   |

```bash
# Arch
sudo pacman -S base-devel cmake

# Ubuntu / Debian
sudo apt install build-essential cmake

# Fedora
sudo dnf install gcc make cmake
```

#### Build & Run

```bash
git clone https://github.com/StarByteGames/StarByte.git
cd StarByte
make -j
./build/starbyte examples/hello.sb
```

Or with CMake (works on Windows too):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/starbyte examples/demo.sb
```

## Usage

Write a `.sb` file and run it:

```bash
starbyte hello.sb
```

Hello, world:

```cs
module System.Console;

int main() {
    Console.WriteLine("Hello, StarByte!");
    return 0;
}
```

### CLI

| Flag           | Action                                                          |
|----------------|-----------------------------------------------------------------|
| `<file.sb>`    | File to run or compile                                          |
| `-o <name>`    | Compile to a native executable (transpiles to C, calls `$CC`)   |
| `--emit-c <p>` | Keep / write the generated C file                               |
| `--cc <prog>`  | Override the C compiler used by `-o` (default: `$CC` or `cc`)   |
| `--run`        | Force interpreter mode (default when `-o` is not given)         |
| `--version`    | Print version                                                   |
| `-h`, `--help` | Show help                                                       |

If a `main()` function is defined it runs automatically and its return
value is the process exit code. Otherwise top-level statements run in
order.

### Native compilation

```bash
starbyte hello.sb -o hello   # produces ./hello
./hello
```

Under the hood StarByte transpiles your program to a single self-contained
C11 file and invokes the system C compiler. Override the compiler with
`--cc clang` or by setting `CC=clang`. To inspect the generated C, pass
`--emit-c hello.c`.

### Build targets (Makefile)

| Target             | Description                                       |
|--------------------|---------------------------------------------------|
| `make` / `release` | Optimized build -> `build/starbyte`               |
| `make debug`       | Debug build (`-O0 -g3`)                           |
| `make run`         | Build and run `examples/demo.sb`                  |
| `make examples`    | Run every `examples/*.sb` file                    |
| `make install`     | Install to `$(PREFIX)/bin` (default `/usr/local`) |
| `make uninstall`   | Remove the installed binary                       |
| `make clean`       | Remove `build/`                                   |

Override variables freely:

```bash
make CC=clang OPT="-O3 -march=native"
make install PREFIX=$HOME/.local
```

## Language

A short tour — guide for the `starbyte` CLI in
[docs/HOWTO.md](docs/HOWTO.md), full language reference in
[docs/LANGUAGE.md](docs/LANGUAGE.md).

```cs
module System.Console;
module Math;

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main() {
    const int N = 6;
    Console.WriteLine(N, "! =", factorial(N));
    Console.WriteLine("sqrt(2) =", Math.sqrt(2));
    return 0;
}
```

| Feature       | Supported                                                |
|---------------|----------------------------------------------------------|
| Types         | `int`, `float`, `char`, `bool`, `string`, `void`, `const`|
| Control flow  | `if`/`else`, `while`, `for`, `break`, `continue`, `return`|
| Functions     | typed params, recursion, hoisting                        |
| Operators     | `+ - * / %`, comparisons, `&& || !`, `+= -= *= /= %=`, postfix `++`/`--` |
| Modules       | `module a.b.c;`                                          |
| Strings       | dynamic, `+` concatenates with anything                  |
| Comments      | `//` and `/* ... */`                                     |
| OOP           | `class`, `interface`, single inheritance, `this`, `super`, `new` (interpreter + native) |
| Memory        | `alloc` / `free`, `gc_alloc` / `gc_collect`, `len`, `buf[i]` |
| Exceptions    | `try` / `catch` / `finally`, `throw <expr>;` (interpreter + native) |

### Standard library

| Namespace                    | Functions                          |
|------------------------------|------------------------------------|
| `Console` / `System.Console` | `WriteLine`, `Write`, `ReadLine`   |
| `Math`    / `System.Math`    | `sqrt`, `abs`, `pow`               |
| `Strings` / `System.Strings` | `length`, `concat`                 |
| `Memory`  / `System.Memory`  | `alloc`, `free`, `gcAlloc`, `gcCollect`, `length` |
| globals                      | `print`, `println`, `alloc`, `free`, `gc_alloc`, `gc_collect`, `len` |

## Editor support

A VS Code syntax-highlighting extension is published on the
Marketplace: [StarByteGames.starbyte-language](https://marketplace.visualstudio.com/items?itemName=StarByteGames.starbyte-language).

Install it from the command line:

```bash
code --install-extension StarByteGames.starbyte-language
```

Or search for **StarByte-Language** in the VS Code Extensions view.

The extension source also lives in [editor/vscode/](editor/vscode/)

## Files

Everything lives in the project directory:

| Path              | Contents                                |
|-------------------|-----------------------------------------|
| `src/`            | Compiler/interpreter source (C11)       |
| `examples/`       | Example `.sb` programs                  |
| `docs/HOWTO.md`   | How to use the `starbyte` compiler/CLI  |
| `docs/LANGUAGE.md`| Complete language reference             |
| `editor/vscode/`  | VS Code syntax extension                |
| `Makefile`        | POSIX make build                        |
| `CMakeLists.txt`  | Cross-platform CMake build              |
| `build/`          | Build output (created on first build)   |

## Roadmap

- [x] Lexer, parser, tree-walking interpreter
- [x] Functions, recursion, modules, standard library
- [x] Native compiler backend (`-o`, via C transpilation)
- [x] `struct` / `enum` runtime support (interpreter)
- [x] `struct` / `enum` in native backend
- [x] Classes, inheritance, interfaces (interpreter)
- [x] Classes/interfaces in native backend (`-o`)
- [x] Manual memory and garbage collector
- [x] Exceptions (`try` / `catch` / `throw`)
- [ ] Generics, lambdas, coroutines
- [ ] Expanded stdlib (`File`, `Network`, `Collections`)

## License

MIT — see [LICENSE](LICENSE).
