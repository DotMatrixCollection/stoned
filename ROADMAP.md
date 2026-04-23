# Roadmap

This reflects the current checked-in interpreter state, not the original prototype plan.

The goal is no longer just “more features”; it is a staged route toward high MRI compatibility, with Ruby 4 as the long-range target. That should be treated as a compatibility program, not a grab-bag backlog:

- prefer semantic correctness and conformance over adding isolated built-ins
- land broad parser/runtime changes behind focused regression matrices
- use CRuby behavior as the oracle whenever semantics are ambiguous
- avoid calling the project “Ruby 4 compatible” until parser, control flow, object model, and core library behavior are all good enough to run a meaningful compatibility slice end to end

## Compatibility route

### Stage 0: Conformance infrastructure
This is the permanent foundation for every later stage.

- keep expanding fixture coverage around parser ambiguity, dispatch, reflection, block semantics, and exception unwind
- add more direct CRuby comparison cases for semantics that are easy to get subtly wrong
- classify gaps by compatibility level:
  - parser mismatch
  - runtime semantic mismatch
  - missing core API
  - missing stdlib / load-path behavior
- track “known intentional differences” separately from accidental incompatibilities

Exit gate:

- new compatibility work normally lands with a regression that would fail before the patch
- roadmap sections below are driven by failing conformance slices instead of intuition alone

### Stage 1: Parser and call semantics parity
This is the shortest path to making the language feel Ruby-like in ordinary code.

- finish remaining command-style call edge cases
- keep reducing differences between parenthesized and unparenthesized forms
- tighten block binding around `do` vs `{}` in deeper nesting and chaining cases
- keep keyword-hash and grouped-argument parsing aligned with CRuby
- make `yield`, `super`, and block-pass forms behave like ordinary calls wherever Ruby does

Exit gate:

- common Ruby call forms parse the same way users expect from MRI
- command-call and grouped/block-binding regressions stop being a recurring source of surprises

### Stage 2: Core runtime semantics parity
This is the current high-value lane. It determines whether code that parses also behaves correctly.

#### Proc / lambda

- finish proc-vs-lambda control-flow edge cases beyond the current direct-call and escaped-`&proc` `break`/`return` behavior plus top-level lambda/proc `return`
- complete the remaining argument coercion and arity edge cases beyond current defaults, optional/splat `arity`, and non-lambda single-array autosplat
- tighten block/method conversion patterns beyond the current `&proc`, iterator forwarding, and lambda-like `Symbol#to_proc` behavior

#### Dispatch / reflection / visibility

- keep tightening `is_a?`, `kind_of?`, `instance_of?`, `class`, `nil?`, `respond_to?`, and `respond_to_missing?`
- keep visibility coherent across primitives, objects, classes, modules, arrays, hashes, and proc objects
- close the remaining `method_missing`, `public_send`, and hidden-method edge cases

#### `super` / modules / ancestors

- tighten module ancestor ordering to closer MRI behavior
- close remaining `super` forwarding and visibility edge cases
- extend class-side mixin behavior beyond the current `extend` path where MRI allows it

Exit gate:

- proc/lambda, reflection, method visibility, and ancestor dispatch behave predictably enough to stop blocking ordinary Ruby metaprogramming patterns

### Stage 3: Exception and object-model completeness
This is where the interpreter becomes much less toy-like for real code.

- fill out broader rescue syntax and remaining exception semantics beyond current typed clauses, typed lists, variable binding, and `retry`
- add broader standard exception coverage, including iterator-facing cases like `StopIteration`
- implement frozen state and object/string mutability semantics:
  - `freeze`
  - `frozen?`
  - `dup`
  - `clone`
  - `FrozenError`
- keep unwind behavior coherent when exceptions interact with `return`, `break`, `next`, and `ensure`

Exit gate:

- exception handling and object mutability are compatible enough that ordinary library code is not constantly tripping interpreter-only differences

### Stage 4: Core library parity
This is where “Ruby-like language core” becomes “usable Ruby runtime”.

#### Collections and mixins

- broaden `Enumerable` coverage in the shared prelude instead of only on selected concrete classes
- tighten `Comparable` / `Enumerable` corner-case behavior

#### Strings and regex

- add regex-backed `match`, `=~`, and regex forms of `sub` / `gsub` / `scan`
- improve current Unicode handling beyond simple codepoint behavior where MRI semantics matter
- decide how, or whether, binary-string behavior should coexist with the current UTF-8-only model

#### Numerics

- add proper `Rational` support instead of current `to_r` placeholders
- tighten float edge cases around negative zero, subnormals, and MRI-specific numeric behavior

Exit gate:

- a meaningful slice of everyday Ruby core-library code runs without needing ad hoc interpreter-specific rewrites

### Stage 5: Loading, IO, and execution environment parity
This is the bridge from language/runtime correctness to running larger real programs.

- strengthen `require` / `require_relative` path canonicalization and platform handling
- improve load-time error reporting and feature-resolution behavior
- expand IO behavior:
  - seek / tell / rewind
  - binary mode and encoding flags
  - `IO.new` from raw file descriptors
  - closer stdin behavior and line-separator handling
- keep file/object lifecycle semantics closer to MRI under block and non-block forms

Exit gate:

- multi-file Ruby programs with realistic file and load-path behavior can run with limited surprises

### Stage 6: MRI compatibility push
This is the final campaign toward any serious “Ruby 4 MRI compatibility” claim.

- build a compatibility matrix against selected MRI behavior slices, not just project-local tests
- run representative Ruby programs and small gems to find systemic gaps
- separate “missing implementation” from “architectural mismatch” and fix the latter first
- decide which MRI features are in scope for the claim:
  - parser/runtime/core language
  - core classes and modules
  - file/load-path behavior
  - enough stdlib to run targeted workloads
- only then consider performance, packaging, and host-integration polish as major priorities

Exit gate:

- the project can state a bounded compatibility claim that is backed by a repeatable test matrix, not aspiration

## Immediate agenda

If work starts today, the next highest-value sequence is:

1. ~~finish proc/lambda argument and control-flow leftovers~~ — done
2. ~~reflection / visibility / dispatch hardening~~ — done (see Already landed)
3. ~~tighten module ancestor ordering and `super`~~ — done
4. ~~expand exception completeness~~ — done (see Already landed)
5. **gap fill: Stages 1–3 known holes** — surveyed 2026-04-22, see below
6. then resume Stage 4 (core library parity)

## Stage gap survey (2026-04-22)

A systematic test-probe of all three completed stages revealed the following confirmed gaps. Items are grouped by implementation effort.

### Group 1 — Prelude expansion (pure Ruby, no C changes)

- ~~**Comparable**: missing `<`, `>`, `<=`, `>=` operator methods — user classes that define `<=>` and `include Comparable` cannot use these operators. `between?`/`clamp` exist but `<`/`>` etc. do not.~~ done
- ~~**Enumerable**: current prelude only has `find`/`detect`/`entries`/`first`/`take`/`drop`/`count(block)`. Missing: `to_a`, `min`, `max`, `sort`, `include?`/`member?`, `sum`, `map`, `select`, `reject`, `reduce`/`inject`, `any?`, `all?`, `none?`, `count` (no-block form), `flat_map`, `each_with_object`, `min_by`, `max_by`, `sort_by`, `zip`, `group_by`, `tally`~~ done
- ~~**`alias` / `alias_method`**: not parsed or evaluated; `alias hi hello` silently fails~~ `alias` done; `alias_method` still pending

### Group 2 — Runtime additions (C changes, self-contained)

- ~~**`Hash.new(default)`** and **`Hash.new { |h,k| ... }`**: `Hash.new` with a default value or block not implemented; `h[missing_key]` should return default instead of nil~~ done
- ~~**`Array.new(n, val)`** and **`Array.new(n) { |i| ... }`**: `Array.new` only creates empty arrays; n-with-value and n-with-block forms not implemented~~ done
- ~~**`tap`**, **`then`**/**`yield_self`**: not defined on any receiver~~ done
- **`pp` kernel method**: not defined (separate from `p`)
- **`Integer(s)`** / **`Float(s)`** / **`String(v)`** / **`Array(v)`** kernel conversion functions: currently resolve to the class constant, not the conversion function
- **`String#%`** (sprintf-style format): `"%.2f" % 3.14` raises NoMethodError
- **`format`** / **`sprintf`** kernel methods: not defined
- Protected method external-call error message says `"undefined method"` instead of `"protected method 'x' called for an instance of Foo"`

### Group 3 — Parser additions (require lexer/parser changes)

- **`%w[...]`** word-array literals and **`%i[...]`** symbol-array literals: parse error
- **`.()` call syntax**: `fn.(args)` (shorthand for `fn.call(args)`) raises parse error at the `.(`
- **`defined?`** operator: treated as undefined method
- **`rescue else`** clause: `begin ... rescue ... else ... end` and method-level `rescue ... else` not parsed
- **`return 1, 2, 3`** (bare multi-value return): parse error; must use `return [1,2,3]` workaround
- **`*head, last = array`** (leading splat on LHS of multi-assign): parse error; `first, *rest =` works but `*rest, last =` does not

### Group 4 — Complex / large features

- **Heredocs** (`<<~HEREDOC` / `<<HEREDOC`): not implemented in lexer
- **Keyword arguments** (`def foo(x:, y: 1)` / `foo(name: val)` / `**opts`): not implemented in parser or runtime — the single largest conformance gap at Stage 1

## Open problem buckets

These remain real compatibility gaps and should be pulled into the staged route above as concrete bugs are found:

### Strings and Unicode

- current string model is UTF-8-only
- some behavior is still codepoint-based where MRI semantics are more nuanced
- regex-backed string APIs are still missing

### Numeric model

- no real Rational type yet
- some float edge cases still need MRI-grade behavior

### IO and loading

- path canonicalization is still basic
- IO surface is still narrower than MRI
- binary/encoding mode support is not there yet

## Already landed

These were previously roadmap items and are now implemented in the current tree:

- hash values and hash built-ins
- `Hash.new(default)` and `Hash.new { |h, k| ... }`, including missing-key lookup through fixed defaults or default procs and preserving hash defaults across `dup`/`clone`
- `Array.new(n, val)` and `Array.new(n) { |i| ... }`, including negative-size `ArgumentError` and Ruby-like shared-object behavior for the fill-value form
- `tap`, `then`, and `yield_self` on all receivers, with `LocalJumpError` when called without a block
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
- proc/lambda defaults and `arity` polish: arrow lambdas and block literals parse defaulted params; arrow lambdas honor omitted default args; non-lambda procs/blocks autosplat a single array argument across multi-slot parameter lists; proc/lambda `arity` reports Ruby-like negative values for optional/splat forms
- proc/lambda control-flow polish: lambda `break` returns from the lambda call; lambda `return` works in top-level and block-passed lambda literals; top-level proc literals may carry `return`; direct-call proc `break` and escaped proc-object `break`/`return` through `&proc` now raise `LocalJumpError`
- `module`, `include`, `prepend`, `extend`, and `super` through module ancestors
- `send`, `__send__`, `public_send`, and method visibility (`public`, `private`, `protected`)
- class-method visibility helpers (`private_class_method`, `public_class_method`, `protected_class_method`)
- `require_relative`, `require`, `$LOAD_PATH`, load guards, and `LoadError`
- `method_missing` / `respond_to_missing?` for objects, classes, and primitive-backed reopened classes
- built-in `Comparable` / `Enumerable` plus operator method defs like `def <=>`
- `Comparable` prelude operators (`<`, `<=`, `>`, `>=`) for custom `<=>` implementations
- broader `Enumerable` prelude coverage for custom `each`-based classes: `to_a`, `map`, `select`, `reject`, `reduce`, predicates, ordering helpers, grouping, tallying, and related adapters
- `alias` statements for method aliasing, including operator aliases and inherited instance methods
- `Range`: `..` / `...` literals, `begin`/`end`/`first`/`last` (with n-arg forms), `exclude_end?`, `include?`/`member?`/`cover?`/`===`, `each`, `each_with_index`, `to_a`, `size`/`count`/`length`, `min`, `max`, `sum`, `step`, `map`, `select`, `reject`, `reduce`, `any?`/`all?`/`none?`; `Range` includes `Enumerable`; integer and string ranges supported; `String#<=>` added as a dispatch method
- `case`/`when`: value equality, range membership, class membership (`===`), multi-pattern `when`, optional `else`, caseless form, `then` keyword; `case` is an expression; `Class#===` added
- `Symbol#to_proc`, lambda-like `Symbol#to_proc#lambda?`, `&:symbol` and `&proc` block-pass in calls, `*arr` splat args in calls, `proc {}` kernel method, `block_given?`, `Object#itself`; arithmetic/comparison operators (`+`, `-`, `*`, `/`, `%`, `**`, `<`, `<=`, `>`, `>=`, `<=>`, `<<`, `>>`, `&`, `|`, `^`) now dispatchable as methods and listed in `respond_to?`; operator symbols (`:+`, `:-`, etc.) now valid symbol literals
- `Symbol#to_s` returns the bare name (no colon); string interpolation `"#{:sym}"` now correct; `Symbol#inspect` still returns `:sym`; `Array#sort` now works for symbol arrays
- `Class#superclass`, `Class#ancestors` (full MRI traversal order including modules); classes without explicit `< Foo` now implicitly inherit from `Object`
- `Class#name`; `Class#instance_methods` / `public_instance_methods` / `private_instance_methods` / `protected_instance_methods` (with `true`/`false` inherited flag)
- `Class#method_defined?` / `public_method_defined?` / `private_method_defined?` / `protected_method_defined?`
- `Class#instance_method` returning an `UnboundMethod`; `UnboundMethod#bind` returning a bound `Method`
- `Object#methods` / `public_methods` / `private_methods` / `protected_methods` (with optional `false` to restrict to own class); built-in Object methods appear in inherited-mode output
- `Object#method` — returns a bound `Method` object for any callable (user-defined or native); raises `NameError` for unknown names; `Method#call` bypasses visibility; `Method#arity`; `Method#to_proc` and `&method` block-pass
- `method_missing` inheriting through class chains and modules; `super` from `method_missing` correctly falls through to `NoMethodError`
- regression test suite wired into `make test`
- evaluator split into smaller files
- parser split into expression/statement files
- `File.read`, `File.write`, `File.open` (block and non-block forms), `File.delete`, `File.exist?`
- file objects: `read`, `write`, `print`, `puts`, `path`, `mode`, `close`, `closed?`; modes `r` / `w` / `a` enforced; `w` truncates on open
- `IO` class with `$stdout`, `$stderr`, `$stdin` / `STDOUT`, `STDERR`, `STDIN`; instance methods `puts`, `print`, `write`, `<<`, `flush`, `sync`, `sync=`, `fileno`, `tty?`; `$stdin.gets` / `$stdin.read`
- Numeric completeness: Integer `gcd`, `lcm`, `pow`+modulus, `divmod`, `digits`, `chr`, `succ`/`pred`, rounding methods, `positive?`/`negative?`/`nonzero?`/`integer?`, `abs2`, `between?`, `clamp`, `step`, `to_s(base)`; Float `nan?`, `infinite?`, `finite?`, `divmod`, precision rounding, same Numeric methods; `0.0/0.0` now IEEE 754 NaN; float `to_s` always includes decimal point
- String non-regex completeness: `chomp`, `chop`, `lstrip`, `rstrip`, `capitalize`, `swapcase`, `ljust`, `rjust`, `center`, `ord`, `hex`, `oct`, `bytes`, `<<`, `index`, `rindex`, `[]`/`slice`, `lines`, `each_line`, `tr` (with range expansion), `count`, `delete`, `squeeze`, `scan`, `sub`, `gsub` (string and block forms), `inspect`
- Inline method-level `rescue` / `ensure` without explicit `begin..end`; multiple rescue clauses at method level
- `freeze`, `frozen?`, `dup`, `clone`, `FrozenError`; `frozen?` always true for Integer/Symbol/nil/bool; `FrozenError < RuntimeError`; ivar assignment raises `FrozenError` on frozen objects; `dup` on Hash/Array now makes independent copies
- `StopIteration` (subclass of `StandardError`); `loop{}` catches `StopIteration` silently and returns nil; `break value` inside `loop{}` returns that value
- `val_equal` extended to handle `VAL_CLASS` (pointer identity) and `VAL_OBJECT` (pointer identity), enabling `ancestors.include?(SomeClass)` and similar checks to work correctly
