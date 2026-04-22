# Roadmap

This reflects the current checked-in interpreter state, not the original prototype plan. Items within a section are roughly prioritized.

## Near term

### Semantics hardening
The current priority is making the implemented subset more Ruby-like and less surprising:

- tighten command-style call parsing in more edge cases
- expand regression coverage for arrays, hashes, and method dispatch
- reduce remaining differences between parenthesized and unparenthesized call forms
- keep exception behavior coherent as more Ruby-like forms land

### Type introspection consistency
Core reflection exists in part, but it should be made more consistent across value kinds:

- `is_a?`, `kind_of?`, `instance_of?`, `class`, `nil?`, `respond_to?`
- align behavior across primitives, objects, classes, arrays, and hashes

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

## Longer term

### `Comparable` and `Enumerable` polish
The core built-in mixins now exist, but the next gaps are:

- broader method coverage
- closer Ruby compatibility on edge cases
- more complete interaction with non-array/hash receiver kinds

### File loading polish
The core loading path now exists, but it still needs:

- stronger path canonicalization / platform handling
- cleaner load-time error reporting beyond the current basic `LoadError` path

### String: regex-dependent methods
`match`, `=~`, `gsub`/`sub`/`scan` with regex patterns. Planned as a 3-tier hybrid with Onigmo fallback for complex backtracking cases. The non-regex surface is now complete.

### Dispatch hook polish
The core hook path now exists, but it still needs:

- closer Ruby compatibility around `respond_to_missing?` edge cases
- integration with future metaprogramming features

### Exception polish
The core exception path now exists, but it still needs:

- richer exception objects beyond class/message/origin
- more rescue syntax (`rescue => e` is in; typed lists and fuller Ruby forms are not)
- better typed runtime errors throughout the evaluator
- `StopIteration` and broader standard exception coverage

### Frozen objects and string mutability
`freeze`, `frozen?`, `dup`, `clone`. Strings in Ruby are mutable by default; frozen strings raise `FrozenError` on mutation. Requires a `frozen` flag on string/object values.

### Numeric completeness
`Integer`: `gcd`, `lcm`, `digits`, `pow`, `divmod`, `to_r`, `chr`.  
`Float`: `nan?`, `infinite?`, `finite?`, `divmod`.  
`Numeric`: `between?`, `clamp`, `abs2`.

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
- re-raise and exception object basics
- uncaught exception backtraces
- broader typed runtime errors (`ArgumentError`, `TypeError`, `NoMethodError`, `ZeroDivisionError`, `LocalJumpError`, `KeyError`)
- multiple assignment, splat capture, and destructuring
- `Proc.new`, `lambda`, and `->` literals with callable proc/lambda values
- `module`, `include`, `prepend`, `extend`, and `super` through module ancestors
- `send`, `__send__`, `public_send`, and method visibility (`public`, `private`, `protected`)
- class-method visibility helpers (`private_class_method`, `public_class_method`, `protected_class_method`)
- `require_relative`, `require`, `$LOAD_PATH`, load guards, and `LoadError`
- `method_missing` / `respond_to_missing?` for objects, classes, and primitive-backed reopened classes
- built-in `Comparable` / `Enumerable` plus operator method defs like `def <=>`
- regression test suite wired into `make test`
- evaluator split into smaller files
- parser split into expression/statement files
- `File.read`, `File.write`, `File.open` (block and non-block forms), `File.delete`, `File.exist?`
- file objects: `read`, `write`, `print`, `puts`, `path`, `mode`, `close`, `closed?`; modes `r` / `w` / `a` enforced; `w` truncates on open
- `IO` class with `$stdout`, `$stderr`, `$stdin` / `STDOUT`, `STDERR`, `STDIN`; instance methods `puts`, `print`, `write`, `<<`, `flush`, `sync`, `sync=`, `fileno`, `tty?`; `$stdin.gets` / `$stdin.read`
- String non-regex completeness: `chomp`, `chop`, `lstrip`, `rstrip`, `capitalize`, `swapcase`, `ljust`, `rjust`, `center`, `ord`, `hex`, `oct`, `bytes`, `<<`, `index`, `rindex`, `[]`/`slice`, `lines`, `each_line`, `tr` (with range expansion), `count`, `delete`, `squeeze`, `scan`, `sub`, `gsub` (string and block forms), `inspect`
