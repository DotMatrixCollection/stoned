# Roadmap

Roughly priority order. Items within a section are not strictly ordered.

## Near term

### Hash
`{}` literals currently evaluate to `nil`. Needs: parser already handles `NODE_HASH`/`NODE_PAIR`, evaluator needs to build a hash value, and a `VAL_HASH` type (or use sorted arrays of pairs). Built-in methods: `[]`, `[]=`, `each`, `map`, `select`, `reject`, `keys`, `values`, `merge`, `has_key?`, `fetch`, `to_a`.

### `attr_reader` / `attr_writer` / `attr_accessor`
Kernel-level macros that define getter/setter methods at class-definition time. Straightforward once classes are solid: intercept these calls in the class body eval and synthesize VAL_METHOD nodes (or define them as Ruby methods backed by ivar access).

### Class methods (`def self.foo`)
The parser already stores `Node.def.recv`. The evaluator needs to: detect `recv == self` in a class body, and store the method in the class's own env under a distinguished namespace (or directly on the class object) rather than in the instance method table.

### Type introspection
`is_a?`, `kind_of?`, `instance_of?`, `class`, `nil?`, `respond_to?` on all value types. Currently `nil?` exists on nil and bool only.

## Medium term

### `begin` / `rescue` / `ensure`
Exception handling. Requires: a new `VAL_EXCEPTION` signal type (parallel to VAL_RETURN), `raise` producing it, `begin`/`rescue` catching it by class match, `ensure` always running. The parser will need `NODE_BEGIN` with rescue/ensure clauses.

### Multiple assignment
`a, b = 1, 2` and `a, b = b, a`. Splat on the left: `first, *rest = arr`. Parser already tokenises `NODE_ASSIGN`; needs a multi-target LHS node or a convention for NodeList targets.

### Proc / lambda
`Proc.new { |x| x }`, `lambda { |x| x }`, `-> (x) { x }`. Mostly reuses the existing `VAL_BLOCK` machinery; the main delta is: lambdas check arity strictly and `return` from a lambda exits the lambda (not the enclosing method).

### `puts` / `print` / `p` without parentheses
The parser currently drops arguments to bare method calls without parentheses. Requires teaching the parser to greedily consume comma-separated argument expressions after a bare identifier when not followed by an operator or terminator.

## Longer term

### Modules and `include` / `extend`
`module Foo ... end`, `include Foo` in a class body. Method lookup needs to walk the full ancestor chain (class → included modules in reverse order → superclass → ...).

### `Comparable` and `Enumerable`
Implement as built-in modules once module infrastructure exists. `Comparable` needs `<=>` defined; `Enumerable` needs `each` defined.

### `require` / `require_relative`
File loading. `require_relative` is straightforward — resolve path relative to the current file and feed it through the same pipeline. `require` needs a load path and a loaded-files registry.

### String: more completeness
`gsub`, `sub`, `match`, `=~`, `scan`, `tr`, `squeeze`, `center`, `ljust`, `rjust`, `bytes`, `encode`. Regex support needs an external library (PCRE2 or similar) or a hand-rolled NFA.

### `method_missing` / `respond_to_missing?`
Hook called when method lookup fails on an object. Enables metaprogramming patterns like `OpenStruct`, `Proxy`, `DelegateClass`.

### `send` / `public_send`
Dynamic method dispatch by name. Needed for metaprogramming; straightforward once method lookup is centralised.

### Exception hierarchy
`RuntimeError`, `ArgumentError`, `TypeError`, `NoMethodError`, `StopIteration` etc. as actual class values so `rescue TypeError` works correctly.

### Frozen objects and string mutability
`freeze`, `frozen?`, `dup`, `clone`. Strings in Ruby are mutable by default; frozen strings raise `FrozenError` on mutation. Requires a `frozen` flag on string/object values.

### Numeric completeness
`Integer`: `gcd`, `lcm`, `digits`, `pow`, `divmod`, `to_r`, `chr`.  
`Float`: `nan?`, `infinite?`, `finite?`, `divmod`.  
`Numeric`: `between?`, `clamp`, `abs2`.

### I/O
`File.read`, `File.write`, `File.open`, `IO` basics. Required for any non-trivial program.
