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

- `private_class_method`, `public_class_method`, `protected_class_method`
- fuller Ruby compatibility around visibility edge cases
- any remaining `respond_to?` / dispatch inconsistencies as more metaprogramming features land

## Longer term

### `Comparable` and `Enumerable`
Implement as built-in modules once module infrastructure exists. `Comparable` needs `<=>` defined; `Enumerable` needs `each` defined.

### File loading polish
The core loading path now exists, but it still needs:

- a real `$LOAD_PATH`
- stronger path canonicalization / platform handling
- cleaner load-time error reporting beyond the current basic `LoadError` path

### String: more completeness
`gsub`, `sub`, `match`, `=~`, `scan`, `tr`, `squeeze`, `center`, `ljust`, `rjust`, `bytes`, `encode`. Regex support needs an external library (PCRE2 or similar) or a hand-rolled NFA.

### `method_missing` / `respond_to_missing?`
Hook called when method lookup fails on an object. Enables metaprogramming patterns like `OpenStruct`, `Proxy`, `DelegateClass`.

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

### I/O
`File.read`, `File.write`, `File.open`, `IO` basics. Required for any non-trivial program.

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
- `require_relative`, minimal `require`, load guards, and `LoadError`
- regression test suite wired into `make test`
- evaluator split into smaller files
- parser split into expression/statement files
