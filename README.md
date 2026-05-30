# stoned

## ELI5 Summary

This project is a from-scratch Ruby interpreter written in C so you can see how the language works instead of treating it like magic. Think of it like taking apart a toy robot and rebuilding the brain, one piece at a time, until it can walk and talk on its own.

Right now it is past the "barely starts" stage and into "serious prototype" territory. The parser, semantic pass, evaluator, and regression suite are all in place, `613` tests are passing, birb's REPL fully evaluates expressions and echoes results, and a substantial RubyGems/Bundler bringup path now exists for compatibility probes. It is still honest about not being Ruby-complete yet.

A Ruby interpreter written in C. It is still prototype-grade, but it now has a coherent end-to-end pipeline, a regression suite, and a growing subset of Ruby semantics that work reliably.

## Building

```sh
make
make test
```

Requires a C23 compiler and `libm`. GCC 13+ or Clang 18+ will do. Produces a `stoned` binary.

```sh
make clean   # remove build artifacts
make install PREFIX=/tmp/stoned-root
```

`make install` lays out a Ruby-style prefix with `bin/ruby`, `bin/stoned`, and `lib/ruby/4.0.0/rbconfig.rb`.
That makes the build consumable by version managers that only need a valid Ruby install tree.

To register a local build directly with `mise`:

```sh
make mise-install MISE_VERSION=0.1.0-stoned-dev
mise use ruby@0.1.0-stoned-dev
```

`mise-install` writes to `~/.local/share/mise/installs/ruby/<version>` by default. Override
`MISE_DATA_DIR` if you keep `mise` data somewhere else.

## Usage

```sh
./stoned script.rb        # run a file
./stoned                  # read from stdin
./stoned --version
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

Current coverage in the tree: `617 passed, 0 failed, 617 total`. Recent additions include pattern matching, Fiber basics, broader method/binding reflection, native `IO.pipe`, Rational/Complex numeric parity, exact-ish `Float#to_r` plus `Float#rationalize`, frozen collection mutation guards, collection clone/singleton preservation, Struct keyword-init and enumerator helpers, a method/ancestor/visibility compatibility matrix, embedded-NUL string coverage, broader Enumerable grep/filter aliases, `one?`, `find_index`/`index`, `reverse_each`, `cycle`, `partition`, `collect_concat`, and chunking helpers, and continued RubyGems/Bundler command-surface bringup around local install, lockfile, config, binstub, activation, and `bundle exec` workflows.

Bundler bringup note:

- Receiverless `instance_eval`-style DSL calls, `class_eval { def }` method registration, `instance_exec`, rescuable `SystemExit`, and multi-param custom exception classes are all now working — these were the interpreter-level gaps previously blocking Bundler-style patterns.
- The current shim layer can support a surprising amount of Bundler surface area.

What is working today:

- Core values: `nil`, booleans, integers, floats, UTF-8 strings with interpolation, symbols, arrays, hashes, ranges
- Literal sugar: `%w[...]` word arrays and `%i[...]` symbol arrays
- Operators: arithmetic, comparison, bitwise, string/array `+`, array `<<`, `**`, `<=>`, `&&`, `||`
- Control flow: `if`, `unless`, `while`, `until`, modifier forms, `break`, `next`, `return` (including bare comma-separated multi-value returns), `defined?`, ternary `?:`; `case`/`when` with value equality, range membership, class membership (`===`), multi-pattern clauses, optional `else`, caseless form, `then` keyword; `case` is an expression; `for var in iterable ... end` loops (loop variable leaks into enclosing scope, matching MRI); `begin ... end while/until` post-test loops; modifier `rescue` (`expr rescue fallback` in assignment and bare-expression position); parenthesized statement groups `(stmt; stmt)` evaluate to their last expression
- Exceptions: `raise`, `begin` / `rescue` / `else` / `ensure`, typed rescue clauses, typed rescue lists, rescue variable binding, `retry`, re-raise, block-body `rescue` / `ensure`, uncaught backtraces, typed runtime failures including `NameError`, `NoMethodError`, `TypeError`, `SystemStackError`, `EncodingError`, `IOError`, `SystemCallError`, and existing core error classes; `Errno` module with `ENOENT`, `EACCES`, `EEXIST`, `EBADF`, `EPERM` — file-not-found operations raise `Errno::ENOENT` with MRI-style messages; exception instances support `new(message)`, `message`, `to_s`, `inspect`, `exception`, `backtrace`, and `set_backtrace`
- Methods: `def`, endless `def foo = expr`, default params, splat params, keyword params (`key:` / `key: default` / `**opts`), nested destructuring params, blocks, `yield`, closures, bare command-style calls, multiline command-style arg lists after commas, nested command-call comma precedence, space-before-`(` grouped command args, unary-leading command args, delayed `do`/`end` attachment through grouped and collection contexts for covered nested command-call cases, command-style and parenthesized keyword-hash args, `**hash` keyword expansion in calls with Ruby-like later-argument-wins merge order, `.()` call shorthand, safe navigation `&.`, `send`, `__send__`, `public_send`, `alias`, `tap`, `then`, `yield_self`; blockless iterator entry points such as `times`, `upto`, `downto`, `step`, `each`, `each_with_index`, `each_key`, `each_value`, and range `each`/`each_with_index` now return `Enumerator` objects, enabling chained forms like `3.times.map { }`, `arr.each_with_index.select { }`, and `arr.each.with_index`
- Classes: instance methods, `def self.foo`, `class << self` / singleton-class bodies, inheritance, `initialize`, instance variables, `super` with checked arg forwarding, class reopening, `attr_reader`/`attr_writer`/`attr_accessor`
- Visibility: `public`, `private`, `protected`, `private_class_method`/`public_class_method`/`protected_class_method`, `respond_to?` with `include_private`, explicit receiver restrictions, protected same-family receiver calls, `public_send` refusing hidden real methods
- Modules: `module Foo ... end`, `include`, `prepend`, `extend`, instance method lookup through included and prepended modules, `super` through module ancestors
- Built-in mixins: `Comparable` (`<`, `<=`, `>`, `>=`, `between?`, `clamp`) and `Enumerable` (`find`, `detect`, `find_index`/`index`, `entries`, `to_a`, `reverse_each`, `cycle`, `partition`, `first`, `take`, `drop`, `count`, `map`/`collect`, `select`/`find_all`/`filter`, `reject`, `grep`, `grep_v`, `reduce`/`inject`, `any?`, `all?`, `none?`, `one?`, `include?`/`member?`, `min`, `max`, `minmax`, `min_by`, `max_by`, `minmax_by`, `sort`, `sort_by`, `sum`, `flat_map`/`collect_concat`, `each_with_object`, `zip`, `group_by`, `tally`, `filter_map`, `each_slice`, `each_cons`, `chunk`, `chunk_while`, `slice_when`, `slice_before`, `take_while`, `drop_while`)
- File loading: `require_relative`, `require`, `$LOAD_PATH` search, duplicate-load skipping, canonicalized feature identity across normalized relative, absolute, and mixed spellings, readable `LoadError` reporting on load failures, and `__dir__`
- Dispatch hooks: `method_missing` and `respond_to_missing?` for objects, classes, and primitive-backed reopened classes; class/module reflection is now consistent across `class`, `is_a?`, and `instance_of?`, with stricter arity enforcement on core reflection built-ins and user-defined methods
- Method reflection: `Object#method`, `Class#instance_method`, `Method#name`/`owner`/`receiver`/`arity`/`parameters`/`source_location`/`super_method`/`to_proc`/`curry`/`unbind`, `UnboundMethod#name`/`owner`/`arity`/`parameters`/`source_location`/`super_method`/`bind`/`bind_call`, plus method-object equality, hashing, `===`, and stable `inspect`/`to_s` semantics for repeated lookups
- Operator method defs like `def <=>` and generic operator dispatch for user-defined operator methods
- Collections: array and hash literals, array/hash mutation, common built-ins on `Array` and `Hash`, `Array.new(n, val)`, `Array.new(n) { |i| ... }`, `Hash.new(default)`, and `Hash.new { |h, k| ... }`; `Array#flatten`/`flatten!` with optional depth; `Array#rotate`/`rotate!`; `Array#map!`/`collect!`, `select!`/`filter!`/`keep_if`, `reject!`/`delete_if`, `sort!`, `uniq!`, `compact!` (bang mutators return `nil` on no-change per MRI); set operators `Array#-`, `Array#&`, `Array#|`; repeat `Array#*` (integer or join-with-string); `Array#combination`, `Array#permutation`, `Array#product` with optional block; `Hash#transform_values`/`transform_values!`, `Hash#transform_keys`/`transform_keys!`, `Hash#filter_map`, `Hash#count` with and without block, `Hash#slice`, `Hash#except`, `Hash#invert`, `Hash#to_a`, `Hash#key`/`index`, `Hash#assoc`, `Hash#rassoc`
- Range: `..` and `...` literals; `begin`/`end`/`first`/`last` (with n-arg forms), `exclude_end?`, `include?`/`member?`/`cover?`/`===`, `each`, `each_with_index`, `to_a`, `size`/`count`/`length`, `min`, `max`, `sum`, `step`, `map`, `select`, `reject`, `reduce`, `any?`/`all?`/`none?`; `Range` includes `Enumerable`; integer and string ranges supported
- Hash syntax: both `{:a => 1}` and modern label syntax like `{a: 1}`
- Assignment: parallel assignment, swap, leading and trailing splat capture, nested destructuring, destructured method and block params
- Proc/lambda: `Proc.new {}`, `proc {}`, `lambda {}`, `-> (...) {}`, `call`, `[]`, `lambda?`, `arity`, lambda `return`, proc non-local `return`; arrow lambdas and block literals now parse defaulted params, arrow lambdas honor omitted default args, non-lambda procs/blocks now autosplat a single array argument across multi-slot parameter lists, proc/lambda `arity` reports Ruby-like negative values for optional/splat forms, lambda `break` returns from the lambda call, lambda `return` now works in top-level and block-passed lambda literals, top-level proc literals may now carry `return` as a non-local exit instead of being rejected at sema time, proc non-local `return` now propagates correctly through helper methods when a proc is passed as `&proc`, proc-object `break` via direct `call` raises `LocalJumpError`, and escaped proc objects passed back through `&proc` now preserve proc identity so invalid `break`/`return` raise `LocalJumpError` instead of being misbound or silently collapsing; `Symbol#to_proc` now returns lambda-like proc objects and `&:symbol` block-pass works in calls; `&proc` block-pass; `*arr` splat args in calls; `block_given?`; `Object#itself`
- Time: minimal native `Time` objects from `File.mtime`, with `to_i`, `==`, `!=`, and `<=>`
- Integer: `gcd`, `lcm`, `pow` (with optional modulus), `divmod`, `digits`, `chr`, `succ`/`pred`, `ceil`/`floor`/`round`/`truncate`, `positive?`/`negative?`/`nonzero?`/`integer?`, `abs2`, `between?`, `clamp`, `step`, `to_s(base)`
- Float: `nan?`, `infinite?`, `finite?`, `divmod`, `ceil`/`floor`/`round`/`truncate` with optional ndigits, `positive?`/`negative?`/`nonzero?`/`integer?`, `abs2`, `between?`, `clamp`, `step`, `to_r`, `rationalize`; `0.0/0.0` now returns `NaN` (IEEE 754)
- Numeric extras: `Rational` and `Complex` prelude classes now subclass `Numeric`; rational/imaginary literals, `Rational()`/`Complex()` kernel constructors, and basic arithmetic/coercion paths are covered
- Regexp: `Regexp.new(string)`, regexp literals `/.../` with `i`/`m`/`x`, `Regexp#match`, `String#match`, `=~` operator, `MatchData` with `to_s`, `[]` by integer index, `begin`, `end`, `pre_match`, `post_match`; `Regexp#source`, `Regexp#inspect`; regex-backed `sub`, `gsub`, and `scan`; `RegexpError` on invalid patterns. Backed by reginold (Onigmo under a stable opaque API).
- String: UTF-8 strings with a lightweight ASCII-8BIT tag for binary data; codepoint-aware `length`/`size`, `chars`, `split("")`, `each_char`, `reverse`, `ord`, `index`, `rindex`, `[]`/`slice` (integer index, two-arg, and range forms), `chop`, `strip`, `delete_prefix`, `delete_suffix`; `start_with?` and `end_with?` accept multiple arguments (any-match semantics); `insert`, `slice!`; encoding surface: `encoding` returns `Encoding` objects, `valid_encoding?` (currently always true), `ascii_only?`, `bytesize`, `byteslice`, `b`, `force_encoding`, `encode` (identity); `lstrip`, `rstrip`, `ljust`, `rjust`, `center`, `upcase`, `downcase`, `capitalize`, `swapcase`, `succ`, `tr`, `count`, `delete`, `squeeze`; plus `chomp`, `hex`, `oct`, `bytes`, `<<`, `lines`, `each_line`, `scan`, `sub`, `gsub`, and sprintf-style `String#%` with basic `%s`/`%d`/`%i`/`%f`, width, precision, and `%%`; `inspect`. Current Unicode semantics are codepoint-based with simple case mapping rather than full locale- or grapheme-aware behavior.
- Kernel: `puts`, `print`, `p`, `pp`, `warn`, `Integer()`, `Float()`, `String()`, `Array()`, `format`, `sprintf`, `raise`, `lambda`, `rand`, `exit`, `__method__` (returns the current method name as a symbol, or `nil` at top level); `eql?` (strict type-and-value equality: `1.eql?(1.0)` → `false`); `freeze`/`frozen?` now work on strings, arrays, and hashes in addition to objects
- IO: `$stdout`, `$stderr`, `$stdin`, `STDOUT`, `STDERR`, `STDIN` as IO objects with `puts`, `print`, `write`, `<<`, `flush`, `sync`, `sync=`, `fileno`, `isatty` / `tty?`, `close`, `closed?`, `tell`, `pos`, `seek`, `rewind`, `eof?`, `gets`, `readline`, `readlines`, `read`, `getc`, `readchar`, `getbyte`, `readbyte`, `each_byte`, `each_char`, `each_line`, `IO.pipe`, and `IO.new(fd, mode)` wrappers with mode enforcement; `IO.new(fd)` / `IO.new(fd, nil)` now infer the descriptor access mode, and explicit `+` modes like `r+` are treated as read-write; `sync` / `sync=` state now persists on `IO`/`File`, and sync-enabled writes now flush immediately; kernel `puts` / `print` / `p` / `pp` now honor `$stdout`/`STDOUT` close state and write-capable `$stdout` redirection; invalid `$stdout`/`$stderr` assignment raises `TypeError`; uncaught runtime errors honor redirected `$stderr`, including custom write-capable objects; mode violations raise `IOError`, closed-stream access raises `IOError`
- File: `File.read`, `File.write`, `File.open` (block and non-block), `File.delete`, `File.exist?`, `File.realpath`, `File.directory?`, `File.file?`, `File.readable?`, `File.writable?`, `File.executable?`, `File.mtime`; path utilities `File.basename`, `File.dirname`, `File.extname`, `File.join`, `File.split`, `File.expand_path`, `File.absolute_path`; file objects with stateful native handles for `read`, `gets`, `readline`, `readlines`, `getc`, `readchar`, `getbyte`, `readbyte`, `each_byte`, `each_char`, `each_line`, `write`, `<<`, `print`, `puts`, `flush`, `sync`, `sync=`, `fileno`, `isatty` / `tty?`, `path`, `mode`, `tell`, `pos`, `seek`, `rewind`, `eof?`, `close`, `closed?`; text modes `r`, `w`, `a` and binary modes `rb`, `wb`, `ab` — binary mode skips UTF-8 validation and tags read strings as ASCII-8BIT so non-UTF-8 byte sequences read without error; file-not-found raises `Errno::ENOENT`
- Dir: `Dir.pwd`, `Dir.chdir` (block and non-block), `Dir.mkdir`
- Packaging/bootstrap: minimal `require "rubygems"` support with `Gem::Version`, `Gem::Requirement`, `Gem::Specification`, `Gem::Platform`, `Gem::NameTuple`, `Gem.loaded_specs`, `gem(name)` activation, and compatibility sub-requires like `rubygems/specification` and `rubygems/command`; `exe/gem` and `exe/bundle` provide early bringup command surfaces for install/build/probe workflows, including local gem build/install/manage flows, installed gemspec metadata preservation, plus Bundler lockfile, grouped dependency, and `bundle exec` probes
- Globals and constants

Known limitations:

- This is not Ruby-compatible enough for real-world code yet
- Text is UTF-8-only across source loading and runtime strings; invalid UTF-8 is rejected in source, `require`, `File.read`, and stdin text reads
- File/IO coverage is now stateful enough for real cursors, descriptor wrappers, line/char/byte reads, and several MRI-style cursor/query methods, but it is still well short of MRI: no socket IO and still-limited encoding/mode fidelity outside the covered text/binary and shared stream-read behavior
- RubyGems/Bundler support is still bringup-grade: enough for shim loading and targeted local experiments, but full dependency resolution, activation semantics, native extensions, and real-world gem installation still need work
- Exceptions work, but they still need broader standard exception coverage and fuller Ruby rescue semantics beyond the current typed clauses, lists, variable binding, and `retry`
- Proc/lambda semantics exist, but there are still edge cases around proc-vs-lambda control flow beyond the current top-level/lambda-return and direct/escaped-proc `break`/`return` cases, and around argument handling, that are not Ruby-complete
- Compatibility around edge-case parsing and method semantics is still being tightened, especially outside the now-covered command-call spacing, unary-arg, grouped-call, and delayed `do`/`end` attachment cases
- File loading is much more canonicalized than before, but platform-specific resolution details and broader MRI search behavior still need work

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
