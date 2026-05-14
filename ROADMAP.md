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

- ~~**basic `Regexp` / `MatchData`**: `Regexp.new`, `match`, `=~`, and core `MatchData` methods (`to_s`, `[]`, `begin`, `end`, `pre_match`, `post_match`) now work via reginold~~ done
- ~~add regexp literals `/.../`~~ done (with `i`/`m`/`x` flags; `case/when` dispatch fixed too)
- ~~Phase 7 core~~: `match?`, `Regexp#options`, `Regexp.escape`/`quote`, block form of `match`, `String#split` with regexp (captures + limit), captures in `MatchData#[]`/`captures`/`begin`/`end`; lexer now tracks local vars for correct `/` disambiguation
- ~~add regex-backed forms of `sub`, `gsub`, and `scan`~~ done
- improve current Unicode handling beyond simple codepoint behavior where MRI semantics matter
- decide how, or whether, binary-string behavior should coexist with the current UTF-8-only model

#### Numerics

- add proper `Rational` support instead of current `to_r` placeholders
- tighten float edge cases around negative zero, subnormals, and MRI-specific numeric behavior

Exit gate:

- a meaningful slice of everyday Ruby core-library code runs without needing ad hoc interpreter-specific rewrites

### Stage 5: Loading, IO, and execution environment parity
This is the bridge from language/runtime correctness to running larger real programs.

- keep tightening `require` / `require_relative` path canonicalization and platform handling
- keep improving load-time error reporting and feature-resolution behavior
- expand IO behavior:
  - ~~seek / tell / rewind~~ done
  - ~~binary mode (`rb`/`wb`/`ab`)~~ done — UTF-8 validation skipped in binary mode; mode helpers replace all strcmp checks
  - ~~`IO.new` from raw file descriptors~~ done
  - ~~closer stdin behavior and line-separator handling~~ done — shared `gets` path now supports custom separators, `nil`, paragraph mode (`""`), and long lines without fixed-buffer truncation across both `IO` and `File`
- keep file/object lifecycle semantics closer to MRI under block and non-block forms

Recent Stage 5 progress already landed:

- normalized `require` / `require_relative` path identity across relative, absolute, and mixed spellings
- better `$LOAD_PATH` resolution for nested entries
- friendly user-facing load errors while keeping canonical internal cache keys
- stateful `File.open` handles instead of path-reopen behavior
- `File.open` block cleanup now stays coherent across normal return, `next`, `break`, exceptions, early method `return`, and manual early `close`
- `File#tell`, `File#pos`, `File#seek`, `File#rewind`, `File#eof?`
- `IO.new(fd, mode)` wrappers plus mode enforcement on read/write entry points
- shared stream-read surface across `IO` and `File`: `gets`, `readline`, `readlines`, `getc`, `readchar`, `getbyte`, `readbyte`, `each_byte`, `each_char`, `each_line`, default newline, custom separator strings, `nil` for “read rest”, paragraph mode (`””`), long-line reads without the old 4KB cap, and byte-vs-character/line read distinctions
- IO/File error class cleanup: mode violations raise `IOError` (not `LoadError`), closed-stream access raises `IOError` with MRI-standard `"closed stream"` message
- `SystemCallError` base class, `Errno` module, and `Errno::ENOENT` / `EACCES` / `EEXIST` / `EBADF` / `EPERM` with correct inheritance chain (`Errno::ENOENT → SystemCallError → StandardError`); file-not-found operations raise `Errno::ENOENT` with MRI-style messages
- `File` path utilities: `basename` (with optional ext stripping and `".*"` wildcard), `dirname`, `extname`, `join`, `split`, `expand_path` (with `~` and `..`/`.` normalization), `absolute_path`
- `Dir.pwd`, `Dir.chdir` (including block restore semantics), `Dir.mkdir`, `File.realpath`, and `__dir__`
- file predicates and metadata: `File.directory?`, `File.file?`, `File.readable?`, `File.writable?`, `File.executable?`, and `File.mtime` with minimal native `Time` objects for comparisons and `to_i`
- `for var in array ... end` loop with correct MRI scope semantics (loop variable leaks into the enclosing scope); currently array-only — range and arbitrary-iterable `for` is a known open gap
- `begin ... end while cond` and `begin ... end until cond` post-test loops (do-while style), including `next` and `break` inside the body
- modifier `rescue` in assignment position (`lhs = expr rescue fallback`) and bare-expression position (`expr rescue fallback`); inner expression is wrapped in an implicit `begin/rescue` so only that expression is rescued
- parenthesized statement groups: `(stmt; stmt)` now parsed as a multi-statement body when the opening `(` is immediately followed by a newline or semicolon, evaluating to the last statement; single-statement form continues to work as before
- lexer operator state hardening: all binary and compound-assignment operators now set `LEX_EXPR_BEG` after the operator token, fixing `/` regex-vs-division disambiguation and multiline expression continuation after operators
- `Array#flatten` fixed to recurse fully by default; optional depth argument (`flatten(n)`) limits recursion depth
- `__method__` kernel method returning current method name as a symbol, or `nil` at top level; also fixed the `call_method:` dispatch path which was not setting `__method__` in the frame env
- Enumerable: `filter_map`, `each_slice(n)`, `each_cons(n)`, `take_while`, `drop_while`; `each_slice` and `each_cons` return an array when called without a block
- `String#[]` range form fixed: `"abcde"[1..3]` now returns `"bcd"` (was always returning the first character due to range not being handled in the integer-index branch); negative indices and exclusive ranges also work
- `Array#rotate` and `rotate!` added
- `Hash#transform_values`/`transform_values!`, `Hash#transform_keys`/`transform_keys!`, `Hash#filter_map`, `Hash#count` with block; `Hash#count` no-block fast path split from `length`/`size` so the block form falls through correctly

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
6. continue Stage 5 (loading / IO parity) now that the earlier Stage 1-3 survey items are mostly closed
7. then resume Stage 4 and remaining runtime polish from compatibility probes

Latest compatibility probe:

- 2026-05-13: probing `birb` with `birb/lib` on `$LOAD_PATH` now gets past `class << self`, safe navigation `&.`, ternary parsing after predicate methods, and block-body `rescue`; the next parser targets in `birb/lib/irb.rb` are remaining implicit-body rescue nesting, interpolation edge cases, and other uncovered expression forms later in the file

## Stage gap survey (2026-04-22)

A systematic test-probe of all three completed stages revealed the following confirmed gaps. Items are grouped by implementation effort.

### Group 1 — Prelude expansion (pure Ruby, no C changes)

- ~~**Comparable**: missing `<`, `>`, `<=`, `>=` operator methods — user classes that define `<=>` and `include Comparable` cannot use these operators. `between?`/`clamp` exist but `<`/`>` etc. do not.~~ done
- ~~**Enumerable**: current prelude only has `find`/`detect`/`entries`/`first`/`take`/`drop`/`count(block)`. Missing: `to_a`, `min`, `max`, `sort`, `include?`/`member?`, `sum`, `map`, `select`, `reject`, `reduce`/`inject`, `any?`, `all?`, `none?`, `count` (no-block form), `flat_map`, `each_with_object`, `min_by`, `max_by`, `sort_by`, `zip`, `group_by`, `tally`~~ done
- ~~**`alias` / `alias_method`**: not parsed or evaluated; `alias hi hello` silently fails~~ done

### Group 2 — Runtime additions (C changes, self-contained)

- ~~**`Hash.new(default)`** and **`Hash.new { |h,k| ... }`**: `Hash.new` with a default value or block not implemented; `h[missing_key]` should return default instead of nil~~ done
- ~~**`Array.new(n, val)`** and **`Array.new(n) { |i| ... }`**: `Array.new` only creates empty arrays; n-with-value and n-with-block forms not implemented~~ done
- ~~**`tap`**, **`then`**/**`yield_self`**: not defined on any receiver~~ done
- ~~**`pp` kernel method**: not defined (separate from `p`)~~ done
- ~~**`Integer(s)`** / **`Float(s)`** / **`String(v)`** / **`Array(v)`** kernel conversion functions: currently resolve to the class constant, not the conversion function~~ done
- ~~**`String#%`** (sprintf-style format): `"%.2f" % 3.14` raises NoMethodError~~ done
- ~~**`format`** / **`sprintf`** kernel methods: not defined~~ done
- ~~Protected method external-call error message says `"undefined method"` instead of `"protected method 'x' called for an instance of Foo"`~~ done

### Group 3 — Parser additions (require lexer/parser changes)

- ~~**`%w[...]`** word-array literals and **`%i[...]`** symbol-array literals: parse error~~ done
- ~~**`.()` call syntax**: `fn.(args)` (shorthand for `fn.call(args)`) raises parse error at the `.(`~~ done
- ~~**`defined?`** operator: treated as undefined method~~ done
- ~~**`rescue else`** clause: `begin ... rescue ... else ... end` and method-level `rescue ... else` not parsed~~ done
- ~~**`return 1, 2, 3`** (bare multi-value return): parse error; must use `return [1,2,3]` workaround~~ done
- ~~**`*head, last = array`** (leading splat on LHS of multi-assign): parse error; `first, *rest =` works but `*rest, last =` does not~~ done

### Group 4 — Complex / large features

- ~~**Heredocs** (`<<~HEREDOC` / `<<HEREDOC`): not implemented in lexer~~ done (`<<IDENT`, `<<~IDENT`, `<<"IDENT"`, `<<'IDENT'`, `<<~'IDENT'`); rest-of-line after marker not yet supported
- ~~**Keyword arguments** (`def foo(x:, y: 1)` / `foo(name: val)` / `**opts`): not implemented in parser or runtime~~ done; required and optional keyword params, `**opts` splat, `**hash` at call sites, call-order kwarg merging, missing required keyword raises `ArgumentError`, unknown keywords raise unless `**opts` is present

## Open problem buckets

These remain real compatibility gaps and should be pulled into the staged route above as concrete bugs are found:

### Strings and Unicode

- current string model is UTF-8-only
- some behavior is still codepoint-based where MRI semantics are more nuanced
- `sub`, `gsub`, and `scan` with regexp now work including captures

### Numeric model

- no real Rational type yet
- some float edge cases still need MRI-grade behavior

### IO and loading

- path canonicalization is materially stronger now, but not yet MRI-complete across all platform and feature-resolution cases
- IO surface is still narrower than MRI
- stateful file handles, cursor methods, and `IO.new(fd, mode)` now exist
- ~~`Errno::ENOENT` is wired for file-not-found; other `Errno::` classes registered but not raised from real `errno` values~~ done — `errno_class_name()` helper maps the C `errno` value to the correct `Errno::` class at every file/IO failure site; `EACCES`, `EBADF`, `EPERM`, `EEXIST` now dispatched correctly
- binary/encoding mode support is not there yet

## Already landed

These were previously roadmap items and are now implemented in the current tree:

- hash values and hash built-ins
- `Hash.new(default)` and `Hash.new { |h, k| ... }`, including missing-key lookup through fixed defaults or default procs and preserving hash defaults across `dup`/`clone`
- `Array.new(n, val)` and `Array.new(n) { |i| ... }`, including negative-size `ArgumentError` and Ruby-like shared-object behavior for the fill-value form
- `tap`, `then`, and `yield_self` on all receivers, with `LocalJumpError` when called without a block
- kernel `pp`, with `p`-like inspect output and return-value behavior
- kernel conversion functions `Integer()`, `Float()`, `String()`, and `Array()`, including parser support for bare capitalized conversion calls
- sprintf-style formatting via `String#%`, `format`, and `sprintf` with basic `%s`/`%d`/`%i`/`%f`, width, precision, and `%%`
- protected method external-call `NoMethodError` messages now report protected-call failures instead of generic undefined-method text
- `.()` call shorthand parsing for `recv.call(...)`
- `defined?` operator support for locals, instance variables, constants, literals, and method-existence checks on `self` and simple receivers
- `%w[...]` word-array literals and `%i[...]` symbol-array literals with paired delimiters, whitespace splitting, and backslash escapes
- `begin ... rescue ... else ... ensure` and method-level `rescue ... else`, with `else` running only on the non-exception path before `ensure`
- bare comma-separated multi-value `return` syntax, lowered to array-valued returns like `return [a, b]`
- leading-splat assignment targets like `*head = ary` and `*head, last = ary`, reusing existing destructuring assignment semantics
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
- stronger load canonicalization across normalized, absolute, nested-load-path, and mixed-identity require cases, while keeping friendly displayed error paths
- stateful `File.open` handles with cursor-sensitive `read` / `write` / `print` / `puts`
- `File#tell`, `File#seek`, `File#rewind`
- `IO.new(fd, mode)` wrappers with mode-enforced read/write behavior
- `method_missing` / `respond_to_missing?` for objects, classes, and primitive-backed reopened classes
- built-in `Comparable` / `Enumerable` plus operator method defs like `def <=>`
- `Comparable` prelude operators (`<`, `<=`, `>`, `>=`) for custom `<=>` implementations
- broader `Enumerable` prelude coverage for custom `each`-based classes: `to_a`, `map`, `select`, `reject`, `reduce`, predicates, ordering helpers, grouping, tallying, and related adapters
- endless method definitions: `def foo = expr`, `def foo(x) = expr`, `def foo(key:) = expr`, `def self.foo = expr` inside classes
- keyword arguments: `key:` and `key: default` params, `**opts` double-splat, `**hash` at call sites, call-order kwarg merging with later sources winning, missing required kwarg raises `ArgumentError`, and unknown keywords raise unless `**opts` is present; kwargs are assembled as a trailing hash at call time so `foo(a: 1)` and `def foo(a:)` compose naturally
- heredoc literals: `<<IDENT`, `<<~IDENT` (squiggly), `<<"IDENT"`, `<<'IDENT'`, `<<~'IDENT'`; squiggly stripping; interpolation via `#{}` using the existing rope path; body scan reads from original source so interpolated expressions have correct positions; rest-of-line after the marker is not supported
- `alias` statements and `alias_method` for method aliasing, including operator aliases and inherited instance methods; `alias_method` works both inside class bodies (bare call) and as an explicit class-method call
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
- basic `Regexp` / `MatchData`: `Regexp.new(pattern)`, `Regexp#match`, `String#match`, `=~` (both orders), `MatchData#to_s`/`[]`/`begin`/`end`/`pre_match`/`post_match`, `Regexp#source`/`#inspect`, `RegexpError` on compile failure; backed by reginold (Onigmo via a stable opaque C API, no Ruby VM dependency)
- `StopIteration` (subclass of `StandardError`); `loop{}` catches `StopIteration` silently and returns nil; `break value` inside `loop{}` returns that value
- `val_equal` extended to handle `VAL_CLASS` (pointer identity) and `VAL_OBJECT` (pointer identity), enabling `ancestors.include?(SomeClass)` and similar checks to work correctly
