# stoned

A Ruby interpreter written in C. Designed for correctness and eventual full Ruby compatibility, not as a toy.

## Building

```sh
make
```

Requires a C23 compiler and `libm`. GCC 13+ or Clang 18+ will do. Produces a `stoned` binary.

```sh
make clean   # remove build artifacts
```

## Usage

```sh
./stoned script.rb        # run a file
./stoned                  # read from stdin
```

## Architecture

The pipeline is: **source** → **lexer** → **parser** → **semantic pass** → **tree-walking evaluator**.

| Component | File(s) | Notes |
|---|---|---|
| Arena allocator | `arena.c/h` | Block-based (64KB blocks), 8-byte aligned. All AST and runtime allocations go here. |
| Rope | `rope.c/h` | Rope tree for string interpolation — no intermediate allocations during parse. |
| Lexer | `lexer.c/h` | Full Ruby token vocabulary, context-sensitive mode stack for nested `#{}` interpolation. |
| Parser | `parser.c/h` | Recursive descent for statements, Pratt (TDOP) for expressions. Designed to extend to the full Ruby grammar. |
| Semantic pass | `sema.c/h` | Two-phase: collect local assignments (Ruby hoisting rule), then resolve bare names as locals or method calls. Hard scope boundaries at `def`. |
| Values | `value.c/h` | Tagged union `Value`. Control flow (return/break/next) is carried as signal values that unwind the call stack. |
| Environment | `env.c/h` | Linked-list env chain. `is_def` flag enforces def scope boundaries for assignment. `env_define` for method/class definitions. |
| Evaluator | `eval.c/h` | Tree-walking. Class hierarchy, instance variables, `super`, blocks, yield. |

## What works

- All primitive types: `nil`, `true`/`false`, integers, floats, strings (with interpolation), symbols, arrays
- Operators: arithmetic, comparison, bitwise, string/array `+`/`<<`, `**`, `<=>`, `&&`/`||` with short-circuit
- Control flow: `if`/`unless`/`while`/`until` (including modifier forms), `break`/`next`/`return`
- Methods: `def`, default parameters, splat (`*args`), blocks, `yield`, closures
- Classes: `class`, inheritance (`<`), instance variables (`@ivar`), `initialize`, `super`, class reopening
- Built-in methods on `Integer`, `Float`, `String`, `Array` — see `eval.c` for the full list
- Kernel: `puts`, `print`, `p`, `raise`, `rand`, `exit`
- Global variables (`$foo`), constants (`FOO`)

## What's not yet implemented

See [ROADMAP.md](ROADMAP.md).

## License

MIT — see [LICENSE](LICENSE).
