# StarByte — Language Guide

A complete tour of the StarByte programming language as implemented in
v0.8.0. This document focuses on how to write StarByte programs.
For build and install instructions see [README.md](../README.md).

---

## Table of Contents

1. [Hello, World](#1-hello-world)
2. [Running Programs](#2-running-programs)
3. [Comments](#3-comments)
4. [Variables & Constants](#4-variables--constants)
5. [Primitive Types](#5-primitive-types)
6. [Operators](#6-operators)
7. [Control Flow](#7-control-flow)
8. [Functions](#8-functions)
9. [Modules & Namespaces](#9-modules--namespaces)
10. [Strings](#10-strings)
11. [Structs](#11-structs)
12. [Enums](#12-enums)
13. [Memory Management](#13-memory-management)
14. [Standard Library](#14-standard-library)
15. [Program Entry Point](#15-program-entry-point)
16. [Exit Codes & Error Handling](#16-exit-codes--error-handling)
17. [Exceptions](#17-exceptions)
18. [Style Guide](#18-style-guide)
19. [Common Pitfalls](#19-common-pitfalls)
20. [Full Example](#20-full-example)
21. [What's Not Yet Supported](#21-whats-not-yet-supported)

---

## 1. Hello, World

Create a file `hello.sb`:

```cs
module System.Console;

int main() {
    Console.WriteLine("Hello, StarByte!");
    return 0;
}
```

Run it:

```sh
starbyte hello.sb
```

Output:

```
Hello, StarByte!
```

---

## 2. Running Programs

```
starbyte <file.sb> [options]
```

| Option         | Meaning                                                  |
|----------------|----------------------------------------------------------|
| `-o <name>`    | Compile to a native executable (transpiles to C, calls `$CC`). |
| `--emit-c <p>` | Keep or emit the generated C source.                     |
| `--cc <prog>`  | Override the C compiler used by `-o`.                    |
| `--run`        | Force interpreter mode (default if `-o` is not given).   |
| `--version`    | Print the StarByte version.                              |
| `-h`, `--help` | Show CLI help.                                           |

Behavior:

- If a function `main()` exists, it is called automatically.
- If `main()` returns an `int`, that value becomes the process exit code.
- If no `main()` exists, all top-level statements run top-to-bottom.

```cs
// no main() needed:
Console.WriteLine("script-style code works too");
```

---

## 3. Comments

```cs
// single-line comment

/*
   multi-line
   comment
*/
```

---

## 4. Variables & Constants

Variables are declared with their type:

```cs
int     count   = 10;
float   ratio   = 0.75;
string  name    = "Ada";
bool    active  = true;
char    initial = 'A';
```

Use `const` for immutable bindings:

```cs
const int MAX_USERS = 100;
// MAX_USERS = 200;  // runtime error: cannot assign to const
```

Variables follow block scope (`{ ... }`). Inner declarations may
shadow outer ones inside their block.

```cs
int x = 1;
{
    int x = 99;           // shadows outer x
    Console.WriteLine(x); // 99
}
Console.WriteLine(x);     // 1
```

---

## 5. Primitive Types

| Type     | Description                                  | Example literal |
|----------|----------------------------------------------|-----------------|
| `int`    | 64-bit signed integer                        | `42`, `-7`      |
| `float`  | 64-bit floating point                        | `3.14`, `1e-3`  |
| `bool`   | `true` or `false`                            | `true`          |
| `char`   | single byte character                        | `'A'`, `'\n'`   |
| `string` | dynamic-length text                          | `"hello"`       |
| `void`   | no value (function return only)              | -               |

Numeric coercion happens automatically in arithmetic:
mixing `int` and `float` produces `float`.

Special literal: `null` (untyped, falsy).

---

## 6. Operators

### Arithmetic

```
+   -   *   /   %
```

`+` also concatenates when either side is a `string`:

```cs
string s = "x = " + 42;   // "x = 42"
```

### Comparison

```
==  !=  <  >  <=  >=
```

### Logical (short-circuit)

```
&&   ||   !
```

### Assignment

```
=   +=   -=   *=   /=   %=
```

### Increment / Decrement (postfix)

```cs
int i = 0;
i++;   // i is now 1
i--;
```

### Precedence (high to low)

1. unary  `-x  +x  !x`
2. `*  /  %`
3. `+  -`
4. `<  >  <=  >=`
5. `==  !=`
6. `&&`
7. `||`
8. assignment (right-associative)

Use parentheses to make intent obvious.

---

## 7. Control Flow

### `if` / `else`

```cs
if (x > 0) {
    Console.WriteLine("positive");
} else if (x < 0) {
    Console.WriteLine("negative");
} else {
    Console.WriteLine("zero");
}
```

The braces are optional for single statements:

```cs
if (ready) Console.WriteLine("go!");
```

### `while`

```cs
int n = 5;
while (n > 0) {
    Console.WriteLine(n);
    n--;
}
```

### `for`

C-style three-part `for`:

```cs
for (int i = 0; i < 10; i++) {
    Console.WriteLine(i);
}
```

Any of the three parts may be empty:

```cs
for (;;) {        // infinite loop
    if (done) break;
}
```

### `break` / `continue`

```cs
for (int i = 0; i < 100; i++) {
    if (i == 10) break;
    if (i % 2 == 0) continue;
    Console.WriteLine(i);   // 1,3,5,7,9
}
```

### `return`

```cs
int abs(int x) {
    if (x < 0) return -x;
    return x;
}
```

---

## 8. Functions

Declaration form:

```cs
<return-type> <name>(<params>) { <body> }
```

```cs
int add(int a, int b) {
    return a + b;
}

void greet(string who) {
    Console.WriteLine("Hello,", who);
}
```

- Parameters are typed and passed by value.
- `void` means the function returns nothing.
- Functions are hoisted: you can call a function defined later in the file.
- Recursion is fully supported.

```cs
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}

int main() {
    Console.WriteLine(fact(6));   // 720
    return 0;
}
```

---

## 9. Modules & Namespaces

A `module` statement declares which logical namespace this file
participates in. It also makes the standard-library namespaces
explicitly available.

```cs
module System.Console;
module Math;
```

You can write either the short name or the fully qualified one:

```cs
Console.WriteLine("hi");
System.Console.WriteLine("hi");   // equivalent
```

`module` accepts dotted names: `module a.b.c;`.

> User-defined modules across multiple files are on the roadmap.
> Today, `module` mainly serves as a documentation/import declaration.

---

## 10. Strings

Strings are dynamically sized, immutable values.

```cs
string a = "Hello";
string b = "World";
string c = a + ", " + b + "!";   // "Hello, World!"
```

Escape sequences: `\n  \t  \r  \\  \"  \'  \0`.

Helpers:

```cs
int    n = Strings.length(c);            // 13
string j = Strings.concat("foo", 42);    // "foo42"
```

---

## 11. Structs

User-defined record types are introduced with `struct`:

```cs
struct Point {
    int x;
    int y;
};
```

The trailing `;` after the closing `}` is optional (C-style is
accepted).

### Creating instances

Brace initializer (positional, in field-declaration order):

```cs
Point p = {3, 4};
```

Fewer values than fields is allowed; the missing fields stay `null`.
More values than fields is a compile-time error.

Default construction (no initializer) gives an instance whose fields
are all `null`:

```cs
Point q;
q.x = 100;
q.y = 200;
```

### Reading and writing fields

```cs
Console.WriteLine(p.x, p.y);    // 3 4
p.x = p.x + 10;
p.y *= 2;                       // compound assignment works too
```

### Printing

Passing a struct to `Console.WriteLine` (or any string context)
produces a debug-friendly form:

```
Point{x=13, y=8}
```

### Semantics

- Structs are heap-allocated and **reference-counted**: assigning
  a struct value (`Point r = p;`) shares the same instance, so
  mutating `r.x` also changes `p.x`.
- Structs themselves can be `const` (the binding cannot be
  reassigned), but their fields remain mutable.
- Nested structs and structs-as-fields are supported as long as
  the field type is a known struct.

---

## 12. Enums

`enum` declares a set of named integer constants:

```cs
enum Color { RED, GREEN, BLUE };
enum Status { OK = 0, WARN = 10, ERR = 20 };
```

- Values auto-increment from `0` (or from the previous explicit value).
- Explicit values must be integer literals (negative is allowed).
- A trailing comma after the last member is permitted.

Members are reachable both fully qualified and unqualified:

```cs
int c = Color.GREEN;     // 1
int s = WARN;            // 10  -- bare names are also defined
```

Enums are simply typed-named integers; you can use them anywhere an
`int` is allowed.

```cs
if (status == Status.ERR) {
    Console.WriteLine("failure");
}
```

> Both the interpreter (default) and the native backend (`-o`) fully
> support `struct` and `enum`.

---

## 12b. Classes & Interfaces

> Available in the **interpreter** only (v0.5.0). Programs that use
> classes or interfaces with `-o` exit with a clear error.

A class bundles fields and methods, optionally extends a single base
class, and may implement any number of interfaces.

```cs
interface IGreeter {
    string greet();
}

class Animal {
    string name;
    int    age = 0;        // fields can have default initializers

    Animal(string n, int a) {        // constructor: same name as the class
        this.name = n;
        this.age = a;
    }

    string greet() {
        return "Hello, I am " + this.name;
    }

    void describe() {
        Console.WriteLine(this.greet(), "(age", this.age + ")");
    }
}

class Dog : Animal, IGreeter {       // ': Base[, IFoo, IBar...]'
    string breed;

    Dog(string n, int a, string b) {
        super(n, a);                 // call the parent constructor
        this.breed = b;
    }

    string greet() {                 // override
        return super.greet() + " - a " + this.breed;
    }
}

int main() {
    Animal a = new Animal("Pip", 4);
    Dog    d = Dog("Rex", 7, "Labrador");   // 'new' is optional
    a.describe();
    d.describe();
    return 0;
}
```

Key points:

- `class Name : Base, IFoo, IBar { ... }` — the **first** type after
  `:` is the base class (single inheritance); the rest are interfaces.
- A constructor is a method with the same name as the class and no
  return type. A class may have at most one constructor.
- `this` refers to the current instance and is implicitly available
  inside methods and field initializers.
- `super.method(args)` calls the parent's implementation. `super(args)`
  inside a constructor delegates to the parent constructor.
- Methods are dispatched dynamically — overriding works without a
  `virtual`/`override` keyword.
- `new ClassName(args)` and `ClassName(args)` are equivalent; the
  `new` keyword is optional.
- Interfaces declare method signatures only. At class-declaration
  time, StarByte verifies that every interface method is implemented
  somewhere in the class hierarchy.
- Fields without an initializer default to `null`.

---

## 13. Memory Management

StarByte gives you both **manual memory** and an **optional
garbage collector**. The unit of allocation is a *buffer*: a
fixed-size, heap-allocated array of dynamically-typed slots. Buffers
are indexed with `[]`.

### Allocation

| Form              | Lifetime           | Free with                |
|-------------------|--------------------|--------------------------|
| `alloc(n)`        | manual             | `free(buf)`              |
| `gc_alloc(n)`     | garbage-collected  | `gc_collect()` (or auto) |

All allocators take a non-negative element count and return a buffer
of `null` slots. They’re also exposed as `Memory.alloc`,
`Memory.free`, `Memory.gcAlloc`, `Memory.gcCollect`, `Memory.length`
(and the `System.Memory.*` aliases) for users who prefer namespaces.

### Indexing

```cs
int xs = alloc(3);
xs[0] = 1;
xs[1] = 2;
xs[2] = xs[0] + xs[1];     // 3
int n = len(xs);           // 3
```

Reads and writes both use `buf[i]`. Compound assignment
(`buf[i] += 1`) works as well. Out-of-bounds access aborts with a
clear runtime error.

Buffer slots are dynamically typed, so the same buffer can hold any
mix of `int`, `string`, `bool`, structs, objects, even other buffers.

### Manual mode (`alloc` / `free`)

```cs
int xs = alloc(5);
for (int i = 0; i < len(xs); i++) xs[i] = i * i;
free(xs);                  // releases the storage
// reading xs after free() is a runtime error: "buffer has been freed"
```

`free()` only releases the underlying storage — the buffer object
itself is dropped when the last reference goes away. Calling `free()`
on a `gc_alloc`-ed buffer is a runtime error.

### Garbage-collected mode (`gc_alloc` / `gc_collect`)

```cs
int ys = gc_alloc(3);
ys[0] = "hello";
ys[1] = 42;
ys[2] = true;

ys = null;                 // drop the only reference
int freed = gc_collect();  // mark/sweep, returns count freed
```

The collector walks the global environment plus the current return
value, marks every reachable buffer, and frees the rest. It only
touches buffers created with `gc_alloc` — manually-allocated buffers
are never collected. Any GC buffers still alive at program exit are
reclaimed automatically.

> The native backend (`-o`) provides the same API. Because the
> generated C uses real local variables for roots, its `gc_collect()`
> is a slightly simpler best-effort sweep — use `alloc`/`free` for
> deterministic native lifetimes.

### `len`

`len(x)` returns the size of a buffer or the byte length of a
`string`. For other values it returns `0`.

---

## 14. Standard Library

All built-ins are available without import. Both short and `System.`-prefixed
forms work.

### `Console`

| Function                  | Description                                       |
|---------------------------|---------------------------------------------------|
| `Console.WriteLine(...)`  | Print all args separated by spaces, then newline  |
| `Console.Write(...)`      | Print without newline                             |
| `Console.ReadLine()`      | Read a line from stdin -> `string`                |

```cs
Console.Write("Name: ");
string n = Console.ReadLine();
Console.WriteLine("Hi", n);
```

### `Math`

| Function          | Description                  |
|-------------------|------------------------------|
| `Math.sqrt(x)`    | Square root, returns `float` |
| `Math.abs(x)`     | Absolute value               |
| `Math.pow(a, b)`  | a raised to the b            |

### `Strings`

| Function                | Description                                   |
|-------------------------|-----------------------------------------------|
| `Strings.length(s)`     | Number of bytes in `s`                        |
| `Strings.concat(...)`   | Concatenate arbitrary values into one string  |

### `Memory`

| Function                  | Description                                   |
|---------------------------|-----------------------------------------------|
| `Memory.alloc(n)`         | Allocate a manually-managed buffer            |
| `Memory.free(buf)`        | Release a manually-managed buffer             |
| `Memory.gcAlloc(n)`       | Allocate a GC-managed buffer                  |
| `Memory.gcCollect()`      | Run mark/sweep, returns count freed           |
| `Memory.length(x)`        | Length of a buffer or string                  |

All of the above are also exposed as bare globals: `alloc`, `free`,
`gc_alloc`, `gc_collect`, `len`.

### Globals

`println(...)` and `print(...)` are aliases of the matching
`Console` functions, handy for short scripts.

---

## 15. Program Entry Point

Two valid styles:

Script-style (no `main`):

```cs
Console.WriteLine("Quick script");
```

App-style (`main` returns exit code):

```cs
int main() {
    Console.WriteLine("Doing work");
    return 0;     // success
}
```

If `main` is `void`, the exit code is `0`.

---

## 16. Exit Codes & Error Handling

Today StarByte uses return codes for application logic:

```cs
int openOrFail(string path) {
    // ... pretend we tried opening
    if (path == "") return -1;
    return 0;
}

int main() {
    int rc = openOrFail("");
    if (rc != 0) {
        Console.WriteLine("error:", rc);
        return rc;
    }
    return 0;
}
```

Runtime errors (e.g. divide by zero, undefined variable, wrong arity)
abort the program with a `<file>:<line>: runtime error: ...` message
and a non-zero exit code.

> User-level recoverable errors are handled with `throw` /
> `try` / `catch`. See [§17. Exceptions](#17-exceptions).

---

## 17. Exceptions

StarByte supports `throw`, `try`, `catch`, and `finally`. Any value
can be thrown — strings, ints, structs, class instances, etc.

```cs
int safeDiv(int a, int b) {
    if (b == 0) throw "division by zero";
    return a / b;
}

try {
    int r = safeDiv(10, 0);
    Console.WriteLine(r);
} catch (string e) {
    Console.WriteLine("caught:", e);
} finally {
    Console.WriteLine("always runs");
}
```

### Syntax

```
try { ... }
catch ([Type] name) { ... }
[finally { ... }]
```

- The catch clause's type annotation is **optional** and currently
  ignored at runtime — every catch handles every thrown value.
- The catch variable name is required if a catch clause is present.
- `finally` is optional. It runs whether the try body completed
  normally, threw, or was caught. If `finally` itself throws or
  returns, that supersedes the try/catch outcome.
- A try block may omit `catch` and provide only `finally`, in which
  case in-flight exceptions still propagate after `finally` runs.

### Throwing

```cs
throw "something went wrong";   // string
throw 404;                       // int
throw MyError("bad input");      // class instance
```

### Uncaught exceptions

If no enclosing `try` catches a thrown value, the program aborts with:

```
<file>:<line>: uncaught exception: <stringified value>
```

and a non-zero exit code.

---

## 18. Style Guide

- Indentation: 4 spaces, no tabs.
- Braces: opening brace on the same line.
- Naming:
  - `lowerCamelCase` for variables and functions
  - `PascalCase` for namespaces / future classes
  - `UPPER_SNAKE` for `const` values
- Statements end with `;`.
- Always wrap loop and `if` bodies with `{}` once they exceed one line.

```cs
const int MAX_RETRIES = 3;

int doWork(int n) {
    for (int i = 0; i < n; i++) {
        Console.WriteLine("step", i);
    }
    return 0;
}
```

---

## 19. Common Pitfalls

| Mistake                                  | Fix                                              |
|------------------------------------------|--------------------------------------------------|
| Forgetting `;` at end of statement       | Add it - every statement ends with `;`.          |
| Calling `main` yourself                  | Don't - the runtime invokes it automatically.    |
| Assigning to a `const`                   | Remove `const`, or pick a different variable.    |
| `if x > 0 { ... }` (no parens)           | Parentheses around conditions are required.     |
| Mixing `int` and `string` w/o `+`        | Convert via concatenation: `"n=" + n`.           |

---

## 20. Full Example

```cs
module System.Console;
module Math;

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

bool isPrime(int n) {
    if (n < 2) return false;
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return false;
    }
    return true;
}

int main() {
    const int N = 10;

    Console.WriteLine("Factorials up to", N);
    for (int i = 1; i <= N; i++) {
        Console.WriteLine(i, "! =", factorial(i));
    }

    Console.WriteLine("Primes below 30:");
    for (int i = 2; i < 30; i++) {
        if (isPrime(i)) Console.Write(i + " ");
    }
    Console.WriteLine("");

    Console.WriteLine("sqrt(2) =", Math.sqrt(2));
    Console.WriteLine("2^10    =", Math.pow(2, 10));

    return 0;
}
```

Run:

```sh
starbyte example.sb
```

---

## 21. What's Not Yet Supported

These are planned but not in v0.8.0:

- Arrays with built-in iteration syntax (use buffers + `for`)
- Catch-by-type filtering (every catch is a catch-all today)
- Generics, lambdas, coroutines
- Multi-file projects with user-defined modules
- `File`, `Network` standard libraries

See the roadmap in [README.md](../README.md) for current status.
