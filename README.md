# stoned

A Ruby interpreter written in C. It is still prototype-grade, but it now has a coherent end-to-end pipeline, a regression suite, and a growing subset of Ruby semantics that work reliably.

## Building

```sh
make
make test
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

## Tests

The regression suite is fixture-driven and runs the interpreter end to end:

```sh
make test
```

Each case lives under `tests/cases/` and is named by stem:

- `name.rb` runs the interpreter on a file
- `name.stdin` runs the interpreter with stdin input
- `name.out` is expected stdout
- `name.err` is expected stderr
- `name.status` is the expected exit code (defaults to `0`)

## Current status

The interpreter currently builds cleanly and the regression suite passes:

```sh
make test
```

Current coverage in the tree: `60 passed, 0 failed, 60 total`.

What is working today:

- Core values: `nil`, booleans, integers, floats, strings with interpolation, symbols, arrays, hashes
- Operators: arithmetic, comparison, bitwise, string/array `+`, array `<<`, `**`, `<=>`, `&&`, `||`
- Control flow: `if`, `unless`, `while`, `until`, modifier forms, `break`, `next`, `return`
- Exceptions: `raise`, `begin` / `rescue` / `ensure`, typed rescue clauses, rescue variable binding, re-raise, uncaught backtraces
- Methods: `def`, default params, splat params, nested destructuring params, blocks, `yield`, closures, bare command-style calls, `send`, `__send__`, `public_send`
- Classes: instance methods, `def self.foo`, inheritance, `initialize`, instance variables, `super`, class reopening, `attr_reader`/`attr_writer`/`attr_accessor`
- Visibility: `public`, `private`, `protected`, `private_class_method`/`public_class_method`/`protected_class_method`, public-only `respond_to?`, explicit receiver restrictions, protected same-family receiver calls
- Modules: `module Foo ... end`, `include`, `prepend`, `extend`, instance method lookup through included and prepended modules, `super` through module ancestors
- File loading: `require_relative`, minimal `require`, duplicate-load skipping, `LoadError` on load failures
- Dispatch hooks: `method_missing` and `respond_to_missing?` for objects and classes
- Collections: array and hash literals, array/hash mutation, common built-ins on `Array` and `Hash`
- Hash syntax: both `{:a => 1}` and modern label syntax like `{a: 1}`
- Assignment: parallel assignment, swap, splat capture, nested destructuring, destructured method and block params
- Proc/lambda: `Proc.new {}`, `lambda {}`, `-> (...) {}`, `call`, `[]`, `lambda?`, `arity`, lambda `return`, proc non-local `return`
- Kernel: `puts`, `print`, `p`, `raise`, `lambda`, `rand`, `exit`
- Globals and constants

Known limitations:

- This is not Ruby-compatible enough for real-world code yet
- IO is still missing, and file loading is still not Ruby-complete
- Exceptions work, but they still need broader standard exception coverage and more Ruby-complete rescue syntax
- Proc/lambda semantics exist, but there are still edge cases around control flow and argument handling that are not Ruby-complete
- Compatibility around edge-case parsing and method semantics is still being tightened
- `require` still has only a minimal search path, not a real `$LOAD_PATH`

## Architecture

The pipeline is: **source** → **lexer** → **parser** → **semantic pass** → **tree-walking evaluator**.

| Component | File(s) | Notes |
|---|---|---|
| Arena allocator | `arena.c/h` | Block-based (64KB blocks), 8-byte aligned. All AST and runtime allocations go here. |
| Rope | `rope.c/h` | Rope tree for string interpolation — no intermediate allocations during parse. |
| Lexer | `lexer.c/h` | Full Ruby token vocabulary, context-sensitive mode stack for nested `#{}` interpolation. |
| Parser | `parser.c/h`, `parser_expr.c`, `parser_stmt.c`, `parser_internal.h` | Recursive descent for statements, Pratt (TDOP) for expressions. Split by concern so syntax work is no longer concentrated in one file. |
| Semantic pass | `sema.c/h` | Two-phase: collect local assignments (Ruby hoisting rule), then resolve bare names as locals or method calls. Hard scope boundaries at `def`. |
| Values | `value.c/h` | Tagged union `Value`. Control flow and exceptions are carried as signal values that unwind the evaluator. |
| Environment | `env.c/h` | Linked-list env chain. `is_def` flag enforces def scope boundaries for assignment. `env_define` for method/class definitions. |
| Evaluator | `eval.c`, `eval_support.c`, `eval_dispatch*.c`, `eval_internal.h` | Tree-walking evaluator split into AST walking, shared helpers, and dispatch by receiver family, including exception unwind and rescue/ensure handling. |

## What's not yet implemented

See [ROADMAP.md](ROADMAP.md).

## License

MIT — see [LICENSE](LICENSE).
