# Roadmap

This reflects the current checked-in interpreter state, not the original prototype plan. Items within a section are roughly prioritized.

## Near term

### Semantics hardening
The current priority is making the implemented subset more Ruby-like and less surprising:

- tighten command-style call parsing in the remaining edge cases beyond current spaced-`(` grouping, unary-leading args, multiline-arg, nested-comma precedence, and covered delayed-`do`/`end` attachment cases
- expand regression coverage for arrays, hashes, and method dispatch
- reduce the remaining differences between parenthesized and unparenthesized call forms
- keep exception behavior coherent as more Ruby-like forms land

### Type introspection consistency
Core reflection exists in part, but it should be made more consistent across value kinds:

- keep tightening `is_a?`, `kind_of?`, `instance_of?`, `class`, `nil?`, `respond_to?`
- keep behavior coherent across primitives, objects, classes, modules, arrays, and hashes

## Medium term

### Proc / lambda
The core callable forms now exist, but the remaining work is semantic cleanup:

- Ruby-complete proc/lambda control-flow edge cases
- fuller arity behavior and argument coercion
- better integration with method/block conversion patterns

### Modules
The core module path now exists, but the next semantic gaps are:

- tighter ancestor ordering compatibility
- class-side mixins beyond the current `extend` behavior

### Method visibility polish
The core visibility model now exists, but it still needs:

- fuller Ruby compatibility around visibility edge cases
- any remaining `respond_to?` / dispatch inconsistencies as more metaprogramming features land

### `super` polish
The core `super` path now exists, but it still needs:

- closer Ruby compatibility on the remaining forwarding and visibility edge cases

## Longer term

### `Comparable` and `Enumerable` polish
The core built-in mixins now exist, but the next gaps are:

- broader `Enumerable` method coverage in the prelude (`map`, `select`, `reject`, etc. are only defined on `Array`, `Hash`, and `Range` natively, not via the shared prelude)
- closer Ruby compatibility on edge cases

### File loading polish
The core loading path now exists, but it still needs:

- stronger path canonicalization / platform handling
- cleaner load-time error reporting beyond the current basic `LoadError` path

### String: regex-dependent methods
`match`, `=~`, `gsub`/`sub`/`scan` with regex patterns. Planned as a 3-tier hybrid with Onigmo fallback for complex backtracking cases. The non-regex surface is now complete.

### UTF-8 polish
The runtime now treats source text and runtime strings as UTF-8-only. Remaining gaps are:

- audit any remaining byte-oriented string behavior for closer Ruby compatibility
- improve current simple codepoint-based Unicode handling toward fuller Ruby semantics where needed
- decide whether any future raw-binary APIs should coexist alongside the UTF-8-only text model

### Dispatch hook polish
The core hook path now exists, but it still needs:

- closer Ruby compatibility around `respond_to_missing?` edge cases
- integration with future metaprogramming features

### Exception polish
The core exception path now exists, but it still needs:

- fuller Ruby rescue syntax beyond the current `rescue => e`, typed clauses, typed lists, and `retry`
- `StopIteration` and broader standard exception coverage

### Frozen objects and string mutability
`freeze`, `frozen?`, `dup`, `clone`. Strings in Ruby are mutable by default; frozen strings raise `FrozenError` on mutation. Requires a `frozen` flag on string/object values.

### Numeric polish
Core numeric surface is now complete. Remaining gaps:

- `Integer#to_r` / `Float#to_r` return self (no Rational type yet)
- `Float` arithmetic edge cases around negative-zero, subnormals

### I/O polish
The core IO path has landed. Remaining gaps:

- no seek/tell/rewind on file objects
- no binary mode or encoding flags
- `$stdin.gets` reads from C stdin only; no line-separator configuration
- `IO.new` with a raw file descriptor not yet supported

## Already landed

These were previously roadmap items and are now implemented in the current tree:

- hash values and hash built-ins
- `attr_reader`, `attr_writer`, `attr_accessor`
- class methods via `def self.foo`
- bare `puts` / `print` / `p` and command-style calls
- block calls and `yield`
- exception signal plumbing
- `begin` / `rescue` / `ensure`
- typed rescue clauses and rescue variable binding
- typed rescue lists and `retry`
- re-raise and exception object basics
- uncaught exception backtraces
- exception instance methods `new(message)`, `exception`, `backtrace`, and `set_backtrace`
- broader typed runtime errors (`ArgumentError`, `TypeError`, `NameError`, `NoMethodError`, `ZeroDivisionError`, `LocalJumpError`, `KeyError`, `LoadError`, `SystemStackError`, `IOError`)
- UTF-8-only source and runtime string validation, with invalid UTF-8 rejected in source loading, `require`, `File.read`, and stdin text reads
- core UTF-8-aware string ops for `length`, `chars`, `split(\"\")`, `each_char`, `reverse`, `ord`, `[]`/`slice`, `index`/`rindex`, `chop`, and width-sensitive padding
- codepoint-based Unicode behavior for `upcase`, `downcase`, `capitalize`, `swapcase`, `succ`, `tr`, `count`, `delete`, and `squeeze`
- multiple assignment, splat capture, and destructuring
- `Proc.new`, `lambda`, and `->` literals with callable proc/lambda values
- `module`, `include`, `prepend`, `extend`, and `super` through module ancestors
- `send`, `__send__`, `public_send`, and method visibility (`public`, `private`, `protected`)
- class-method visibility helpers (`private_class_method`, `public_class_method`, `protected_class_method`)
- `require_relative`, `require`, `$LOAD_PATH`, load guards, and `LoadError`
- `method_missing` / `respond_to_missing?` for objects, classes, and primitive-backed reopened classes
- built-in `Comparable` / `Enumerable` plus operator method defs like `def <=>`
- `Range`: `..` / `...` literals, `begin`/`end`/`first`/`last` (with n-arg forms), `exclude_end?`, `include?`/`member?`/`cover?`/`===`, `each`, `each_with_index`, `to_a`, `size`/`count`/`length`, `min`, `max`, `sum`, `step`, `map`, `select`, `reject`, `reduce`, `any?`/`all?`/`none?`; `Range` includes `Enumerable`; integer and string ranges supported; `String#<=>` added as a dispatch method
- `case`/`when`: value equality, range membership, class membership (`===`), multi-pattern `when`, optional `else`, caseless form, `then` keyword; `case` is an expression; `Class#===` added
- `Symbol#to_proc`, `&:symbol` and `&proc` block-pass in calls, `*arr` splat args in calls, `proc {}` kernel method, `block_given?`, `Object#itself`; arithmetic/comparison operators (`+`, `-`, `*`, `/`, `%`, `**`, `<`, `<=`, `>`, `>=`, `<=>`, `<<`, `>>`, `&`, `|`, `^`) now dispatchable as methods; operator symbols (`:+`, `:-`, etc.) now valid symbol literals
- regression test suite wired into `make test`
- evaluator split into smaller files
- parser split into expression/statement files
- `File.read`, `File.write`, `File.open` (block and non-block forms), `File.delete`, `File.exist?`
- file objects: `read`, `write`, `print`, `puts`, `path`, `mode`, `close`, `closed?`; modes `r` / `w` / `a` enforced; `w` truncates on open
- `IO` class with `$stdout`, `$stderr`, `$stdin` / `STDOUT`, `STDERR`, `STDIN`; instance methods `puts`, `print`, `write`, `<<`, `flush`, `sync`, `sync=`, `fileno`, `tty?`; `$stdin.gets` / `$stdin.read`
- Numeric completeness: Integer `gcd`, `lcm`, `pow`+modulus, `divmod`, `digits`, `chr`, `succ`/`pred`, rounding methods, `positive?`/`negative?`/`nonzero?`/`integer?`, `abs2`, `between?`, `clamp`, `step`, `to_s(base)`; Float `nan?`, `infinite?`, `finite?`, `divmod`, precision rounding, same Numeric methods; `0.0/0.0` now IEEE 754 NaN; float `to_s` always includes decimal point
- String non-regex completeness: `chomp`, `chop`, `lstrip`, `rstrip`, `capitalize`, `swapcase`, `ljust`, `rjust`, `center`, `ord`, `hex`, `oct`, `bytes`, `<<`, `index`, `rindex`, `[]`/`slice`, `lines`, `each_line`, `tr` (with range expansion), `count`, `delete`, `squeeze`, `scan`, `sub`, `gsub` (string and block forms), `inspect`
