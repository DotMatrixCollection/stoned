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

Current coverage in the tree: `136 passed, 0 failed, 136 total`.

What is working today:

- Core values: `nil`, booleans, integers, floats, UTF-8 strings with interpolation, symbols, arrays, hashes, ranges
- Operators: arithmetic, comparison, bitwise, string/array `+`, array `<<`, `**`, `<=>`, `&&`, `||`
- Control flow: `if`, `unless`, `while`, `until`, modifier forms, `break`, `next`, `return`; `case`/`when` with value equality, range membership, class membership (`===`), multi-pattern clauses, optional `else`, caseless form, `then` keyword; `case` is an expression
- Exceptions: `raise`, `begin` / `rescue` / `ensure`, typed rescue clauses, typed rescue lists, rescue variable binding, `retry`, re-raise, uncaught backtraces, typed runtime failures including `NameError`, `NoMethodError`, `TypeError`, `SystemStackError`, `EncodingError`, and existing core error classes; exception instances support `new(message)`, `message`, `to_s`, `inspect`, `exception`, `backtrace`, and `set_backtrace`
- Methods: `def`, default params, splat params, nested destructuring params, blocks, `yield`, closures, bare command-style calls, multiline command-style arg lists after commas, nested command-call comma precedence, space-before-`(` grouped command args, unary-leading command args, delayed `do`/`end` attachment through grouped and collection contexts for covered nested command-call cases, command-style and parenthesized keyword-hash args, `send`, `__send__`, `public_send`, `alias`
- Classes: instance methods, `def self.foo`, inheritance, `initialize`, instance variables, `super` with checked arg forwarding, class reopening, `attr_reader`/`attr_writer`/`attr_accessor`
- Visibility: `public`, `private`, `protected`, `private_class_method`/`public_class_method`/`protected_class_method`, `respond_to?` with `include_private`, explicit receiver restrictions, protected same-family receiver calls, `public_send` refusing hidden real methods
- Modules: `module Foo ... end`, `include`, `prepend`, `extend`, instance method lookup through included and prepended modules, `super` through module ancestors
- Built-in mixins: `Comparable` (`<`, `<=`, `>`, `>=`, `between?`, `clamp`) and `Enumerable` (`find`, `detect`, `entries`, `to_a`, `first`, `take`, `drop`, `count`, `map`/`collect`, `select`, `reject`, `reduce`/`inject`, `any?`, `all?`, `none?`, `include?`/`member?`, `min`, `max`, `sort`, `sum`, `flat_map`, `each_with_object`, `min_by`, `max_by`, `sort_by`, `zip`, `group_by`, `tally`)
- File loading: `require_relative`, `require`, `$LOAD_PATH` search, duplicate-load skipping, `LoadError` on load failures
- Dispatch hooks: `method_missing` and `respond_to_missing?` for objects, classes, and primitive-backed reopened classes; class/module reflection is now consistent across `class`, `is_a?`, and `instance_of?`, with stricter arity enforcement on core reflection built-ins and user-defined methods
- Operator method defs like `def <=>` and generic operator dispatch for user-defined operator methods
- Collections: array and hash literals, array/hash mutation, common built-ins on `Array` and `Hash`
- Range: `..` and `...` literals; `begin`/`end`/`first`/`last` (with n-arg forms), `exclude_end?`, `include?`/`member?`/`cover?`/`===`, `each`, `each_with_index`, `to_a`, `size`/`count`/`length`, `min`, `max`, `sum`, `step`, `map`, `select`, `reject`, `reduce`, `any?`/`all?`/`none?`; `Range` includes `Enumerable`; integer and string ranges supported
- Hash syntax: both `{:a => 1}` and modern label syntax like `{a: 1}`
- Assignment: parallel assignment, swap, splat capture, nested destructuring, destructured method and block params
- Proc/lambda: `Proc.new {}`, `proc {}`, `lambda {}`, `-> (...) {}`, `call`, `[]`, `lambda?`, `arity`, lambda `return`, proc non-local `return`; arrow lambdas and block literals now parse defaulted params, arrow lambdas honor omitted default args, non-lambda procs/blocks now autosplat a single array argument across multi-slot parameter lists, proc/lambda `arity` reports Ruby-like negative values for optional/splat forms, lambda `break` returns from the lambda call, lambda `return` now works in top-level and block-passed lambda literals, top-level proc literals may now carry `return` as a non-local exit instead of being rejected at sema time, proc non-local `return` now propagates correctly through helper methods when a proc is passed as `&proc`, proc-object `break` via direct `call` raises `LocalJumpError`, and escaped proc objects passed back through `&proc` now preserve proc identity so invalid `break`/`return` raise `LocalJumpError` instead of being misbound or silently collapsing; `Symbol#to_proc` now returns lambda-like proc objects and `&:symbol` block-pass works in calls; `&proc` block-pass; `*arr` splat args in calls; `block_given?`; `Object#itself`
- Integer: `gcd`, `lcm`, `pow` (with optional modulus), `divmod`, `digits`, `chr`, `succ`/`pred`, `ceil`/`floor`/`round`/`truncate`, `positive?`/`negative?`/`nonzero?`/`integer?`, `abs2`, `between?`, `clamp`, `step`, `to_s(base)`
- Float: `nan?`, `infinite?`, `finite?`, `divmod`, `ceil`/`floor`/`round`/`truncate` with optional ndigits, `positive?`/`negative?`/`nonzero?`/`integer?`, `abs2`, `between?`, `clamp`, `step`; `0.0/0.0` now returns `NaN` (IEEE 754)
- String: UTF-8-only strings; codepoint-aware `length`/`size`, `chars`, `split("")`, `each_char`, `reverse`, `ord`, `index`, `rindex`, `[]`/`slice`, `chop`, `strip`, `lstrip`, `rstrip`, `ljust`, `rjust`, `center`, `upcase`, `downcase`, `capitalize`, `swapcase`, `succ`, `tr`, `count`, `delete`, `squeeze`; plus `chomp`, `hex`, `oct`, `bytes`, `<<`, `lines`, `each_line`, `scan`, `sub`, `gsub` (string and block forms), `inspect`. Current Unicode semantics are codepoint-based with simple case mapping rather than full locale- or grapheme-aware behavior.
- Kernel: `puts`, `print`, `p`, `raise`, `lambda`, `rand`, `exit`
- IO: `$stdout`, `$stderr`, `$stdin`, `STDOUT`, `STDERR`, `STDIN` as IO objects with `puts`, `print`, `write`, `<<`, `flush`, `sync`, `sync=`, `fileno`; `$stdin.gets` / `$stdin.read`
- File: `File.read`, `File.write`, `File.open` (block and non-block), `File.delete`, `File.exist?`; file objects with `read`, `write`, `print`, `puts`, `path`, `mode`, `close`, `closed?`; modes `r`, `w`, `a` enforced
- Globals and constants

Known limitations:

- This is not Ruby-compatible enough for real-world code yet
- Text is UTF-8-only across source loading and runtime strings; invalid UTF-8 is rejected in source, `require`, `File.read`, and stdin text reads
- File/IO coverage is path-backed and stream-mode basic; no socket IO, no binary mode, no seek/tell
- Exceptions work, but they still need broader standard exception coverage and fuller Ruby rescue semantics beyond the current typed clauses, lists, variable binding, and `retry`
- Proc/lambda semantics exist, but there are still edge cases around proc-vs-lambda control flow beyond the current top-level/lambda-return and direct/escaped-proc `break`/`return` cases, and around argument handling, that are not Ruby-complete
- Compatibility around edge-case parsing and method semantics is still being tightened, especially outside the now-covered command-call spacing, unary-arg, grouped-call, and delayed `do`/`end` attachment cases
- File loading still needs stronger path canonicalization and more Ruby-complete search behavior

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
