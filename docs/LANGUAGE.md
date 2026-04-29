# StarByte — Language Guide

A complete tour of the StarByte programming language as implemented in
v0.2.0. This document focuses on how to write StarByte programs.
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
11. [Standard Library](#11-standard-library)
12. [Program Entry Point](#12-program-entry-point)
13. [Exit Codes & Error Handling](#13-exit-codes--error-handling)
14. [Style Guide](#14-style-guide)
15. [Common Pitfalls](#15-common-pitfalls)
16. [Full Example](#16-full-example)
17. [What's Not Yet Supported](#17-whats-not-yet-supported)

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

## 11. Standard Library

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

### Globals

`println(...)` and `print(...)` are aliases of the matching
`Console` functions, handy for short scripts.

---

## 12. Program Entry Point

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

## 13. Exit Codes & Error Handling

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

> Exceptions (`try` / `catch` / `throw`) are on the roadmap.

---

## 14. Style Guide

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

## 15. Common Pitfalls

| Mistake                                  | Fix                                              |
|------------------------------------------|--------------------------------------------------|
| Forgetting `;` at end of statement       | Add it - every statement ends with `;`.          |
| Calling `main` yourself                  | Don't - the runtime invokes it automatically.    |
| Assigning to a `const`                   | Remove `const`, or pick a different variable.    |
| `if x > 0 { ... }` (no parens)           | Parentheses around conditions are required.     |
| Mixing `int` and `string` w/o `+`        | Convert via concatenation: `"n=" + n`.           |
| Using struct fields                      | Not implemented yet (roadmap).                   |

---

## 16. Full Example

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

## 17. What's Not Yet Supported

These are planned but not in v0.2.0:

- `struct` / `enum` at runtime
- Classes, inheritance, interfaces
- Arrays and collections
- Manual `alloc` / `free` and a garbage collector
- Exceptions (`try` / `catch` / `throw`)
- Generics, lambdas, coroutines
- Multi-file projects with user-defined modules
- `File`, `Network` standard libraries

See the roadmap in [README.md](../README.md) for current status.
