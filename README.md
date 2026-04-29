# StarByte 0.1.0

A fast, modern, general-purpose programming language — C-level performance, C#-style ergonomics, written in pure C.

![C](https://img.shields.io/badge/C-C11-A8B9CC?logo=c&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-green)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-FCC624?logo=linux&logoColor=black)
![Release](https://img.shields.io/badge/Release-Alpha-orange)

> **Alpha release** — core language, interpreter, and standard library work,
> but expect rough edges. Bug reports and feedback are welcome.

## What is this?

StarByte is a small programming language with familiar C/C# syntax,
implemented as a single self-contained C11 binary. It ships with a
tree-walking interpreter, a built-in standard library (`Console`, `Math`,
`Strings`) and a `module` system. Use it for quick scripts or as a
playground for language ideas.

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

| Flag           | Action                                            |
|----------------|---------------------------------------------------|
| `<file.sb>`    | File to run                                       |
| `-o <name>`    | Reserved for the future native compiler backend   |
| `--version`    | Print version                                     |
| `-h`, `--help` | Show help                                         |

If a `main()` function is defined it runs automatically and its return
value is the process exit code. Otherwise top-level statements run in
order.

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

A short tour — full reference in [docs/LANGUAGE.md](docs/LANGUAGE.md).

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

### Standard library

| Namespace                    | Functions                          |
|------------------------------|------------------------------------|
| `Console` / `System.Console` | `WriteLine`, `Write`, `ReadLine`   |
| `Math`    / `System.Math`    | `sqrt`, `abs`, `pow`               |
| `Strings` / `System.Strings` | `length`, `concat`                 |
| globals                      | `print`, `println`                 |

## Editor support

A VS Code syntax-highlighting extension lives in [editor/vscode/](editor/vscode/).
To use it locally, copy or symlink the folder into your VS Code extensions dir:

```bash
ln -s "$PWD/editor/vscode" ~/.vscode/extensions/starbyte-language
```

Then `.sb` files and ` ```starbyte ` Markdown blocks get highlighted.

## Files

Everything lives in the project directory:

| Path              | Contents                                |
|-------------------|-----------------------------------------|
| `src/`            | Compiler/interpreter source (C11)       |
| `examples/`       | Example `.sb` programs                  |
| `docs/LANGUAGE.md`| Complete language reference             |
| `editor/vscode/`  | VS Code syntax extension                |
| `Makefile`        | POSIX make build                        |
| `CMakeLists.txt`  | Cross-platform CMake build              |
| `build/`          | Build output (created on first build)   |

## Roadmap

- [x] Lexer, parser, tree-walking interpreter
- [x] Functions, recursion, modules, standard library
- [ ] `struct` / `enum` runtime support
- [ ] Classes, inheritance, interfaces
- [ ] Manual memory and garbage collector
- [ ] Exceptions (`try` / `catch` / `throw`)
- [ ] Generics, lambdas, coroutines
- [ ] Native compiler backend (`-o`)
- [ ] Expanded stdlib (`File`, `Network`, `Collections`)

## License

MIT — see [LICENSE](LICENSE).
