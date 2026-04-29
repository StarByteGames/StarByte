# StarByte

StarByte ist eine moderne Allzweck-Programmiersprache mit C/C#-ähnlicher Syntax,
implementiert in **C** für maximale Geschwindigkeit und Portabilität.

Diese Implementierung enthält die Version **0.1.0** mit einem Tree-Walking-Interpreter
und der Kommandozeile `starbyte`.

## Features (v0.1.0)

- **Datentypen:** `int`, `float`, `char`, `bool`, `string`, `void`
- **Variablen** mit `const`-Modifier
- **Funktionen** (mit Rückgabetyp, Parametern, Rekursion)
- **Kontrollstrukturen:** `if`/`else`, `while`, `for`, `break`, `continue`, `return`
- **Operatoren:** `+ - * / %`, `== != < > <= >=`, `&& || !`, `=`, `+= -= *= /= %=`, `++ --` (postfix)
- **Strings:** dynamisch, `+` für Konkatenation
- **Module/Namespaces:** `module System.Console;`
- **Standardbibliothek (eingebaut):**
  - `Console.WriteLine`, `Console.Write`, `Console.ReadLine`
  - `Math.sqrt`, `Math.abs`, `Math.pow`
  - `Strings.length`, `Strings.concat`
- **Kommentare:** `//` und `/* ... */`
- **Cross-Platform:** Linux (Arch im Fokus), Windows

## Build

Voraussetzung: CMake ≥ 3.15 und ein C11-Compiler (gcc, clang, MSVC).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Das Binary liegt danach unter `build/starbyte`.

## Nutzung

```sh
./build/starbyte examples/hello.sb
./build/starbyte examples/demo.sb
```

CLI:

```
starbyte <file.sb> [-o <output>] [--version] [--help]
```

> Hinweis: `-o` ist für den geplanten Compiler-Backend reserviert.
> Aktuell wird der Code direkt vom Interpreter ausgeführt.
> Falls eine `main()`-Funktion existiert, wird sie aufgerufen und ihr Rückgabewert
> als Exit-Code zurückgegeben; ansonsten wird der Top-Level-Code ausgeführt.

## Beispiel

```starbyte
module System.Console;

int add(int a, int b) {
    return a + b;
}

void main() {
    Console.WriteLine("2 + 3 =", add(2, 3));
}
```

## Projektstruktur

```
src/
  common.h         Allgemeine Helfer
  lexer.{h,c}      Tokenizer
  ast.{h,c}        AST-Definitionen
  parser.{h,c}     Recursive-Descent-Parser
  value.{h,c}      Laufzeitwerte
  interpreter.{h,c} Tree-Walking-Interpreter (inkl. Builtins)
  main.c           CLI
examples/
  hello.sb
  demo.sb
```

## Roadmap

- [ ] Structs, Enums (Parser-Stubs vorhanden)
- [ ] Klassen, Vererbung, Interfaces
- [ ] Garbage Collector + manuelles `alloc`/`free`
- [ ] Exceptions
- [ ] Generics, Lambdas, Coroutines
- [ ] Compiler-Backend (`-o`)
- [ ] Erweiterte Standardbibliothek (`File`, `Network`)
