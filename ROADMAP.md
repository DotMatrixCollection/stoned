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
- continue RubyGems/Bundler bringup as a Stage 5 load-path workload:
  - keep tightening `Gem::Specification.find_by_name` and `gem(name)` activation against on-disk gem homes
  - expand command-surface coverage for `exe/gem` / `exe/bundle` with local fixture-driven tests before broader networked workflows
  - keep `Gemfile.lock` behavior reproducible as grouped dependencies and `bundle exec` semantics get closer to Bundler
  - keep shim coverage focused on real compatibility callers rather than cloning all of RubyGems wholesale
  - document interpreter-driven Bundler gaps explicitly instead of hiding them by rewriting every regression into a `stoned`-specific DSL subset

Bundler-facing Ruby edges recently fixed:

- `class_eval`/`module_eval` with a block now correctly registers `def` in the class instead of the block frame; `def` inside any block inside a class body also hoists correctly (both paths fixed by correcting the `__singleton_target__ = nil` fallthrough in `NODE_DEF`)
- `instance_exec` added (like `instance_eval` but passes args to the block)
- `exit` and `abort` now raise rescuable `SystemExit` exceptions instead of calling C's `exit()` directly; `exit!` still bypasses rescue; `SystemExit#status` and `SystemExit#success?` added; `SystemExit` and `IndexError` added to the builtin class hierarchy with correct superclass wiring (`SystemExit < Exception`, `IndexError < StandardError`, `KeyError < IndexError`)
- Custom exception `initialize` with multiple parameters now works: the multi-arg check was rejecting all `new` calls with `argc > 1` before the user-defined `initialize` could run; fix hoists the user-init detection before the strict arity gate
- `abort` added as a kernel method (prints msg to stderr, raises `SystemExit` with status 1)

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
- multiline regexp literal lexing now accepts embedded newlines, which unblocks extended-mode `/.../x` patterns spread across lines
- Bundler bringup now covers local install/check/list/exec flows, `bundle show` / `bundle info` / `bundle open` path introspection for both path and installed gems, lockfile drift failures, missing-command validation for `bundle exec`, missing-lockfile handling for `bundle exec`, `bundle check`, `bundle list`, `bundle show`, and `bundle info`, required-gem validation for `bundle info`, `gemspec` runtime-dependency path overrides, path-gem runtime dependency closure against the local gem home, grouped dependency filtering via `BUNDLE_WITH` / `BUNDLE_WITHOUT`, installed- and path-gem `bundle exec` command lookup for gem executables, reproducible lockfiles that retain excluded path-group entries, local gem build/install preserving installed gemspec runtime dependency and executable metadata, local `gem search`, filtered `gem list`, `gem contents`, `gem specification`, `gem open`, `gem which`, `gem unpack`, and `gem uninstall` against installed gems, including version-selectable lookup, uninstall-time executable wrapper cleanup, and missing-argument validation, version-aware `Gem::Specification.find_by_name` / `gem(name, requirement)` activation against on-disk gem homes, installed-gem `Gem.available?` probing, version-aware `Gem.bin_path` / `Gem.activate_bin_path` resolution for installed executables, missing-version error reporting via `Gem::MissingSpecVersionError`, and RubyGems reloading those installed runtime dependencies through `Gem::Specification.find_by_name`
- Bundler local workflow complete: `bundle config` (read/write `.bundle/config`; list/get/set/unset subcommands; local vs global scopes; legacy `KEY VALUE` form); `bundle lock` (write lockfile without installing; missing-Gemfile error path); `bundle env` (Bundler version header plus optional Configuration and Gemfile sections); `bundle binstubs GEM` (writes executable wrappers to `bin/`, reports no-executables gems, missing-gem and missing-lockfile errors); `bundle install --frozen` / `--deployment` (silent resolve, lockfile diff, exit 1 on drift or missing lockfile); config-driven group filtering: `bundle_without_groups` / `bundle_with_groups` now merge `.bundle/config` via `merged_bundle_config()` so `bundle config set WITHOUT development` correctly filters list/check/exec/show without requiring env vars; `BUNDLE_GEMFILE` env override verified; version and config missing-arg error paths covered
- `Array#flatten` fixed to recurse fully by default; optional depth argument (`flatten(n)`) limits recursion depth
- `__method__` kernel method returning current method name as a symbol, or `nil` at top level; also fixed the `call_method:` dispatch path which was not setting `__method__` in the frame env
- Enumerable: `filter_map`, `each_slice(n)`, `each_cons(n)`, `take_while`, `drop_while`; `each_slice` and `each_cons` return an array when called without a block
- `String#[]` range form fixed: `"abcde"[1..3]` now returns `"bcd"` (was always returning the first character due to range not being handled in the integer-index branch); negative indices and exclusive ranges also work
- `Array#rotate` and `rotate!` added
- `Hash#transform_values`/`transform_values!`, `Hash#transform_keys`/`transform_keys!`, `Hash#filter_map`, `Hash#count` with block; `Hash#count` no-block fast path split from `length`/`size` so the block form falls through correctly
- Array set operators `Array#-` (difference), `Array#&` (intersection), `Array#|` (union), `Array#*` (repeat integer or join-with-string); `Array#combination(k)`, `Array#permutation(k)`, `Array#product(*arrays)` with optional block form
- `Array#index`/`find_index` (value or block), `Array#rindex`, `Array#sample`, `Array#shuffle`/`shuffle!`, `Array#delete` (with block fallback), `Array#pop(n)`, `Array#clear`, `Array#cycle(n)`, `Array#min(n)`, `Array#max(n)`, `Array#with_index`, `Array#to_h`, blockless `Array#map`/`select`/`reject` returns self for chaining
- `Array#any?`/`all?`/`none?` without block: test element truthiness (was raising LocalJumpError)
- `Hash#merge` multi-hash with optional conflict block, `Hash#merge!` multi-hash; `Hash#values_at`, `Hash#to_h` with block
- `nil` conversions: `to_i`=0, `to_f`=0.0, `to_a`=[], `to_h`={}; `nil.to_s`="" (was returning nil); `nil.nil?`=true (was returning nil — critical bug)
- `instance_variable_get`, `instance_variable_set`, `instance_variable_defined?`, `instance_variables`; strips/adds `@` prefix since ivars stored without it internally
- `String#to_i(base)`, `String#insert`, `String#slice!`, encoding stubs (`encoding`, `valid_encoding?`, `ascii_only?`, `bytesize`, `b`, `force_encoding`, `encode`)
- `Enumerable#partition`, `Enumerable#each_with_index` (Enumerable prelude fallback for non-array/hash types)
- `is_a?`/`kind_of?` now checks included modules; `Comparable` true for Integer/Float/String/Symbol, `Enumerable` true for Array/Hash/Range; `Class#include?(mod)` added
- Custom `inspect`/`to_s` dispatch: `p`, `puts`, `print` now call Ruby methods on VAL_OBJECT, with recursive array/hash dispatch in inspect
- `p` inspect bug: `val_inspect` in value.c had raw memcpy without escape handling
- Array bang mutators: `map!`/`collect!`, `select!`/`filter!`/`keep_if`, `reject!`/`delete_if`, `sort!`, `uniq!`, `compact!`, `flatten!` (all return `nil` when no change was made, per MRI)
- Hash: `slice(*keys)`, `except(*keys)`, `invert`, `to_a`, `key(v)`/`index(v)`, `assoc(k)`, `rassoc(v)`; `any?`/`none?` without block check emptiness
- String: `delete_prefix`, `delete_suffix`; `start_with?` and `end_with?` upgraded to accept multiple arguments (any-match)
- `freeze`/`frozen?` extended to strings (`frozen` bit on Value), arrays, and hashes (`frozen` field on RubyArray/RubyHash); integers/floats/symbols/nil/bool remain always-frozen
- `eql?` added to universal dispatch: same-type strict equality (`1.eql?(1.0)` → false)
- Enumerable: `minmax` and `minmax_by` added to prelude; `Hash#none?` with block fixed (was handled for any?/all? but missed none?)
- `reduce`/`inject` symbol form: `[1,2,3].reduce(:+)`, `reduce(init, :sym)`, `(1..5).reduce(:+)` now work; symbol dispatches via `dispatch_method` so any defined operator works; Hash reduce still requires a block
- `String#+` added to `dispatch_string` so it's reachable via `send`, `reduce(:+)`, and similar method dispatch paths
- `p` inspect bug fixed: `val_inspect` in value.c was doing raw `memcpy` without escaping special characters; now escapes `\n`, `\t`, `\r`, `\"`, `\\`, and control chars to `\xNN`
- Splat in array literals: `[*a]`, `[0, *a, 4]`, `[*1..5]` now work; parser's `prefix_bp(TOK_STAR)` returns 2 (low, so it captures full expression including ranges); eval expands splat elements and calls `to_a` on non-Array iterables
- `Range#step` without block now returns an array instead of raising, enabling `(1..10).step(2).to_a`, `(1..10).step(2).map {}` etc.
- Value struct frozen field: all inline Value constructors now zero `frozen` to prevent garbage-stack false-positive `frozen?` results on freshly created strings
- `Array#index`/`find_index` (value or block), `Array#rindex` (value or block), `Array#sample`, `Array#shuffle`/`shuffle!`
- `String#to_i(base)`: now accepts base 2–36 via `strtoll`; `"ff".to_i(16)` = 255, `"1010".to_i(2)` = 10
- Regex match globals: `$~` (MatchData), `$1`–`$9` (capture groups) now set after every `=~`, `String#match`, `Regexp#match`, and `Regexp#=~`; cleared to nil on no-match. Bug was stack-allocated key strings passed to `global_set` which stores the pointer — fixed with static key table
- Blockless iterators now return `Enumerator` objects instead of raising `LocalJumpError`: `Integer#times`, `upto`, `downto`, `step`; `Array#each`, `each_with_index`; `Hash#each`/`each_pair`, `each_key`, `each_value`; `Range#each`, `each_with_index` — plus enumerator adapter methods like `with_index`, `with_object`, `peek_values`, and `next_values`
- Namespaced constant paths (`Foo::Bar`) in parser, eval, and const lookup; `Module.constants`, `Module#constants`; nested constant assignment (`Outer::K = v`); `private_constant`/`public_constant`/`deprecate_constant` no-ops
- ENV hash populated from host environment; ARGV, RUBY_ENGINE/VERSION/PLATFORM/DESCRIPTION/PATCHLEVEL/REVISION/RELEASE_DATE/COPYRIGHT; Marshal::MAJOR_VERSION/MINOR_VERSION; Thread::Mutex stub with synchronize; Process.pid; rbconfig.rb shim
- Lexer: 0o/0O octal notation, %r{} percent-regexp literals, ?x char literals, regexp starting with `=` or `=~` now lex correctly
- Parser: beginless/endless ranges (`..expr`, `expr..`, standalone `..`); multiple-assignment in modifier position (`a, b = x if cond`); for-loop destructuring (`for a, b in coll`); `def Const::path` method definitions; hashrocket hash args in command-call position (`f key => val`); `do` block correctly bound after parenthesized call; yield/proc keyword bare params; yield in modifier position
- Binding: `local_variable_get`/`set`, `eval` with env injection, `source_location`, `dup`
- `Binding#eval` now seeds semantic local-name resolution from the full binding env chain instead of a source-prelude hack, fixing `eval("x + y", binding.dup)` and similar parent-scope local lookups
- `define_method` with block and symbol name; `method_defined?` guard pattern; `UnboundMethod#bind_call`
- `using` no-op (refinement block silently accepted, returns nil)
- IO/console compatibility: `IO.open` with option hash (`external_encoding`, `internal_encoding`); `IO.console_size`; `IO#winsize`, `external_encoding`, `internal_encoding`, `to_i`, `to_int`, `wait_readable` (no-op), `ungetc`; `raw`/`cooked`/`raw!`/`cooked!` wrappers; `require "io/console"` and `require "io/console/size"` load guards
- Reline, Prism, Singleton module stubs (sub-modules and classes); Pathname stub; Struct.new member access
- Singleton methods on Array and Hash objects; `attr_reader`/`attr_writer` in singleton env; singleton class expression (`class << obj`)
- `alias` now resolves builtin methods by probe dispatch, not env-only lookup; nested class body no longer inherits singleton-class def-target from an enclosing `class << self`
- `Regexp.union` with strings and arrays of strings/regexps; `Regexp#inspect` for union results
- `parse_args` now skips terminators at entry and after last arg, so `)` on its own line in a paren call works correctly
- Heredoc rest-of-line (`hd_pending`): tokens on the same line as `<<~HEREDOC` (e.g. the closing `)`) are buffered and emitted after `INTERP_END` instead of being silently discarded
- Heredoc inner string fix (`hd_imode_depth`): `"..."` inside `#{}` inside a heredoc body no longer incorrectly triggers the heredoc content scanner
- `.[]()` / `&.[]()` parsed as explicit method-name calls after dot
- Stabby lambda accepts unparenthesized multi-param: `-> x, y { x + y }`
- Method destructured params `def f((a, b))` now registers nested locals so `/` after a destructured var is division, not regexp
- `Complex()` and `Rational()` added to parser's kernel-const-call list
- Leading-dot newline continuation: a line starting with `.method` chains onto the previous expression
- `NODE_CLASS` / `NODE_MODULE` reopen check now scans `target_env->vars` directly (not `env_get` chain), fixing the bug where `class Foo < Base` inside `module Inner` reopened `Outer::Foo` instead of creating `Inner::Foo`
- `alias` inside a module now synthesizes a forwarding `def` for known Kernel functions (load, require, puts, …) instead of raising `NameError`
- `__FILE__` and `__LINE__` keywords implemented via `builtin_kernel`; backtick command strings `` `cmd` `` execute via `popen`
- `Shellwords.split`, `StringIO` class stub, `Open3` module stub, `Encoding` class with UTF_8/ASCII_8BIT/EUC_JP/… constants and `Encoding.find`
- `open3`, `tmpdir`, `tempfile` require stubs
- **birb milestone**: `irb.rb` (the full IRB entry point) now loads cleanly — exit 0, no parse or runtime errors
- `MatchData#[]` accepts symbol/string keys for named capture groups; names extracted from regexp source at match time
- `$:` global alias for `$LOAD_PATH`; `$"` alias for `$LOADED_FEATURES`
- `and`/`or` keywords now short-circuit correctly (same code path as `&&`/`||`)
- Class variables `@@x`: `NODE_CVAR` read/write; compound assignment `@@x += 1` handled via `assign_target` fallback
- `class << self` now saves and restores `__singleton_target__` so instance methods defined after the block go to the instance method env, not the singleton env
- `Thread.current` returns a singleton main-thread stub
- `Kernel.method` dispatch: calls on the Kernel module forward to `builtin_kernel`
- Module constant-lookup fix: when a class method `self.X` exists, `Module.X(args)` calls the method rather than returning the constant
- `NoMethodError` messages now show the real receiver class name (was always "Object")
- `catch`/`throw`: implemented via `VAL_THROW` signal with `throw_sig.{tag,value}` struct fields
- `trap`, `at_exit`, `sleep`, `catch`, `throw` added to `builtin_kernel` and `kernel_names` dispatch list
- `Array#reverse_each`; `PP` class stub; `pp` and `color_printer` require stubs
- Block `def_file` captured at creation time, restored in `call_block` so `require_relative` inside procs resolves relative to the block's defining file
- `def Foo::bar` syntax: lowercase IDENT now accepted as the final method-name segment after `::`
- Predicate/bang identifiers (`method?`, `method!`) always dispatch as method calls, never fall through to `NODE_LVAR`
- **birb session-5**: `IRB.start` now reaches the readline/prompt-generation code deep in the REPL loop
- `String#gsub`/`#sub` with block now sets `$1`..$9` from capture groups before calling the block (fixes `format_prompt` pattern)
- `String#[]` with Regexp argument: performs regex_search and returns matched substring; `String#[regex, n]` returns nth capture group
- `String#[]` with String argument: substring match
- **birb complete**: `IRB.start` runs the full REPL loop through prompt generation and exits 0 on EOF
- `NODE_LVAR` now returns nil (not NameError) when a variable was seen by the parser but not yet assigned — matches Ruby semantics for conditional branches
- `class << self` now saves/restores `__visibility__` in addition to `__singleton_target__` so `private` inside singleton blocks doesn't bleed into the outer class body
- `String#<<` and `String#concat` mutation propagation: for LVAR/IVAR/GVAR receivers, update the binding after the operation via `env_set`/`val_object_set_ivar`
- `String#clear`, `String#setbyte` stubs; `String#[]` with Regexp, String, and capture-group index arguments
- `String#gsub`/`#sub` with block: sets `$1`..$9` capture globals before calling the replacement block
- `NilClass`: `match?`, `byteslice`, `[]`, `empty?`, `size`, `length`, `lines`, `chomp`, `strip`, `clear` all return safe values
- `Proc#parameters` returns `[[:type, :name], ...]` pairs from the block's param list
- `Class#to_s`, `Class#inspect`, `Class#name` all return the class name string
- `Array#concat`: append elements from argument arrays in place
- `Reline::Unicode.calculate_width`, `split_by_width` stubs for IRB pager width calculations
- `Reline` module class-level dispatch: `get_screen_size`, `ambiguous_width`, etc.
- `Binding#local_variables`: scans env chain and collects local variable symbol names
- Prism shim (`require "prism"`): `ParseLexResult`, `ParseResult`, `StubNode`, `Source`, `Visitor`; `Prism.parse` vs `Prism.parse_lex` return different result types; Source handles empty code
- PP shim (`require "pp"`): minimal `PP` class with `initialize`, `guard_inspect_key`, `pp`, `flush`, `self.pp`
- `$>` global alias for `$stdout`
- **birb session-6 milestone**: the REPL evaluates expressions and echoes results — `1+1` returns `2`, `puts 2+2` prints `4`
- `Float::INFINITY` and `Float::NAN` constants bootstrapped in prelude
- RubyGems bringup: `require "rubygems"` now loads a larger compatibility shim with `Gem::Version`, `Gem::Requirement`, `Gem::Dependency`, `Gem::Specification`, `Gem::Platform`, `Gem::NameTuple`, `Gem::Command`, and key `rubygems/*` sub-requires routed back to the shim; inline source assembly now uses chunked C string parts to avoid overlength string-literal warnings
- `Enumerator::Lazy`: `(1..Float::INFINITY).lazy.select{...}.first(n)` and chained lazy ops on infinite ranges; `to_a`/`force` and `take` now correct — shared `_collect` helper fixes `to_a` returning a scalar and `:take` not limiting the outer loop
- `Range#each` now accepts `Float::INFINITY` end for lazy/break-able iteration
- `Array#chunk`, `#chunk_while`, `#slice_when`, `#slice_before` enumerable partitioning
- Symbol methods: `:sym.upcase/downcase/capitalize/length/size/<=>` delegate to String or return Symbols
- `Integer/Float#clamp(range)` single-argument Range form
- Class comparison operators: `A < B`, `A <= B`, `A > B`, `A >= B`, `A <=> B` (subclass hierarchy)
- `Class#class_variables`, `#const_defined?`, `#const_get`
- `Object#singleton_class` (returns class), `#singleton_methods`, `#define_singleton_method`
- `Kernel.instance_method(:name)` returns a working `UnboundMethod`; `.bind(obj).call` dispatches correctly
- StringIO shim: `write/puts/print/<<`, `string`, `read`, `truncate`, `flush`, `tty?` — enables IRB commands
- `execute_as_command?` bug fixed via post-load hook in `eval_require_path` after `irb.rb` fully loads
- IRB commands (`ls`, `help`, `exit`, etc.) now route correctly through the command dispatch system
- **birb session-7 milestone**: `ls` command executes without crashing; multi-line input, lazy enumerators, and class hierarchy work in the REPL

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
- 2026-05-16: `receiver.method :sym, val` command-call with symbol args now works — lexer colon case extended to allow symbols in `LEX_EXPR_END+had_space` state (covers post-dot-method position); `require "irb"` now loads clean with no parse errors
- 2026-05-16: `define_singleton_method` block now actually executes — `call_method_value` was reading `NODE_BLOCK` fields through the `def` union member (wrong offsets); fixed by dispatching on `node->kind` to use `block.params`/`block.body` vs `def.params`/`def.body`
- 2026-05-16: RubyGems bringup expanded — `require "rubygems"` now exposes a much broader compatibility shim, direct `rubygems/specification`/`platform`/`name_tuple`/`command` sub-requires resolve through it, and fixture coverage now exercises the shim explicitly
- 2026-05-16: multiline extended-mode regexp literals now lex across embedded newlines, with a regression covering `/.../x` patterns split across lines and comments
- 2026-05-17: Bundler local workflow complete — `bundle config`, `bundle lock`, `bundle env`, `bundle binstubs`, `bundle install --frozen`, config-driven group filtering, `BUNDLE_GEMFILE` override, and all key error paths covered; 421 regressions green
- 2026-05-17: Networking bring-up — `STONED_GEM_SERVER` env override enables fake file:// server for tests; `install_gem_from_server` now reads real gemspec from gem data (with runtime deps) and falls back to metadata.gz parsing; `resolve_and_install` refactored to BFS queue with transitive dep discovery after each install; `force_update: true` path bypasses local cache for `bundle update`; `bundle update` and `gem update` commands added; four new regressions: transitive dep chain install via fake server, `gem install` from fake server with bin wrapper, `gem update` old→new with already-at-latest path, `bundle update` version bump; 425 regressions green
- 2026-05-17: Gem/bundle CLI completion — `gem cleanup` (multi-version pruning with bin wrapper refresh), `gem fetch` (download without install), `bundle add GEM` (Gemfile append + install, duplicate guard, --version/--group/--path), `bundle remove GEM` (Gemfile line removal + lockfile update), `bundle outdated` (per-gem latest-vs-locked comparison), `bundle exec` end-to-end network test, `bundle install --without/--with` flags persisted to .bundle/config, `bundle platform` (Ruby/Bundler/platform header + active config display); `Gem::Requirement ~>` fixed (was only checking lower bound, now also applies upper bound by incrementing the second-to-last version segment); `fetch_version` now consults `/api/v1/versions/GEM.json` to select the highest version satisfying Gemfile constraints; `resolve_and_install` now verifies locally-installed versions against requirements before accepting them; 434 regressions green
- 2026-05-17: Final networking polish — `gem install` BFS refactored with `force:`, `no_deps:` keyword options; `gem install --no-deps` skips transitive dep fetch; `gem install --force` re-downloads over an existing install; `gem pristine GEM` re-downloads and re-installs from server (force+no_deps); `bundle pristine` re-downloads all locked remote gems and restores files; `gem install -v "~> X"` now resolves version constraints via versions API (`resolve_gem_version` helper); `bundle install` prints "Bundle up to date!" on second run when lockfile unchanged and nothing newly installed; 439 regressions green
- 2026-05-17–18: Stage 4/6 compatibility sweep (round 1) — `Proc#curry`/`>>`/`<<`; `Integer#[]`; `Hash.[]`; string bang methods; strict `Integer()`/`Float()` ArgumentError; frozen Array/Hash mutation; `at_exit` LIFO; custom exception `initialize`; `Object#equal?` pointer identity; `puts` trailing-newline fix; `String#each_char`/`upto` blockless; modulo Ruby sign semantics; `MatchData#named_captures`/`names`/Range `[]`; `srand`; `Array#zip` block/`transpose`/`assoc`/`rassoc`/`repeated_combination`/`repeated_permutation`/`shift(n)`; `gsub`/`sub` hash replacement; `Class#methods` class method inclusion; `File.size`/`zero?`, `IO.read`/`foreach`; `String#prepend`; `Numeric#coerce`; `Proc#>>`/`<<` composition; `Array#repeated_combination`/`repeated_permutation`; `Struct#inspect` fixed (`#<struct ClassName k=v>`); constant assignment renames anonymous Struct/class names; env_get_own fixes spurious method lookup through class_env parent chain; 463 regressions green
- 2026-05-19: method/binding reflection sweep — method-backed procs now preserve real arity (`Method#to_proc` + `&method` + `Proc#curry` interaction), `Method#curry` and `Method#===` now work directly, core `respond_to?` tables were tightened for `dup`/`clone`/`__id__`/`itself`, `Binding#eval` now resolves locals from the full captured env chain, `Rational`/`Complex` now subclass `Numeric`, and `IO.pipe` returns working reader/writer IO pairs; 568 regressions green
- 2026-05-19: methods list + &method top-level fix — `Object#methods` / `public_methods` now include primitive-type method lists (Integer operators, Float, String, Array, Hash, Symbol); `&method(:name)` for top-level `def`s now dispatches via bare-call path instead of `nil.method(args)` (fixed by checking nil receiver in `make_bound_method_proc`); 564 regressions green
- 2026-05-19: Fiber coroutines + Struct#inspect fix — `Fiber` class backed by POSIX `ucontext`/`makecontext`/`swapcontext`; `Fiber.new { |x| ... }`, `fiber.resume(val)`, `Fiber.yield(val)`, `fiber.alive?`, `Fiber.current`; `FiberError` (subclass of `StandardError`) raised on dead-fiber resume or main-fiber yield; fiber stack is 512 KiB malloc'd per fiber; `Struct#inspect` now emits MRI-correct `, ` comma-space separator between members (`#<struct Foo x=1, y=2>` was `#<struct Foo x=1 y=2>`); 562 regressions green
- 2026-05-19: Numeric literals and conversions — `3r` rational literal, `2i`/`1.5i` imaginary literal, `String#to_r`/`#to_c`, `Float#to_r` (approximates via 1e7 denominator), `Integer + Rational`/`Integer + Complex` coercion in both `eval_binop` and `dispatch_integer`, `Rational#frozen?`/`Complex#frozen?` return true (value-object semantics), Complex built-in unary `-@`; 561 regressions green
- 2026-05-19: Numeric and parity sweep — `Rational` class (full arithmetic, comparison, rounding, Comparable; auto-reduces via GCD; `Rational()` kernel function); `Complex` class (arithmetic, abs, conjugate, polar/rectangular; `Complex()` kernel function); `Integer#to_r` returns real Rational instead of stub; `Integer#to_c` returns real Complex; `def -@`/`def +@`/`def !@` unary operator definitions now parse correctly (parser detects bare `@` after operator token); unary `-`/`+`/`~` now dispatch to user-defined `-@`/`+@`/`~` methods on non-primitive receivers; `def /` inside class bodies now parses (lexer suppresses regex scan when `after_def` is set); bare `respond_to?` added to `kernel_names` with top-level-aware handler (checks kernel methods and user-defined top-level defs when `self` is nil); bare `method(:sym)` in `builtin_kernel` now searches `top_env` for user-defined top-level methods when `self` is nil; `Rational`/`Complex` kernel functions routed through `builtin_kernel` (fixes segfault from `__kern__` dispatch overwriting class constant with method); `for` loop with arbitrary iterables (falls back to `to_a` then iterates, covering Hash and Enumerable objects); 559 regressions green
- 2026-05-18: Bundler/gem CLI round 4 — `bundler/inline` (gemfile DSL via block parameter, installs deps into temp dir, sets up load path, quiet: true suppresses output; uses BUNDLER_INLINE_STONED_ROOT constant to anchor paths at load time since stoned's __FILE__ doesn't track definition-site in methods), `bundle install --quiet`/`-q` (suppresses Using/Installing/Bundle complete messages), deduplication of DEPENDENCIES section in Gemfile.lock (gemspec runtime deps no longer produce duplicate entries when also explicitly listed in Gemfile); 484 regressions green
- 2026-05-18: Bundler shim expansion — `Bundler.require` fixed after path-helper regression; top-level `Bundler` now exposes path helpers, filtered env snapshots, `definition`, `runtime`, and `settings`; new compatibility sub-requires include `bundler/version`, `bundler/errors`, `bundler/gem_helper`, `bundler/lockfile_parser`, `bundler/settings`, `bundler/shared_helpers`, `bundler/current_ruby`, `bundler/definition`, `bundler/runtime`, `bundler/rubygems_ext`, `bundler/spec_set`, `bundler/ui/shell`, `bundler/friendly_errors`, `bundler/dependency`, `bundler/dsl`, `bundler/source_list`, `bundler/source/rubygems`, `bundler/source/path`, `bundler/source/git`, `bundler/lazy_specification`, `bundler/remote_specification`, `bundler/stub_specification`, `bundler/cli`, and `bundler/cli/common`; full suite green at 508 regressions
- 2026-05-18: Bundler shim deepening — `SpecSet`/`Index`/`Definition` query surfaces are broader, Gemfile DSL parsing now handles nested `eval_gemfile`, `path ... do`, `git ... do`, and `ruby`, `Bundler::Settings` now reads local/global `.bundle/config` with precedence overlaid by env/temporary/command options, `bundler/inline` now supports `ruby` and nested `eval_gemfile`, and docs now call out the remaining interpreter edges that still block MRI-style Bundler DSL parity; full suite green at 523 regressions
- 2026-05-18: Ruby parity for Bundler DSLs — block execution now carries defining `self` correctly through `instance_eval`-style paths, direct receiverless Bundler-style DSL calls are covered by a new regression, and `bundler/inline` has been switched back to the MRI-style no-arg block form; broader DSL-wrapper parity and rescue-wrapper semantics remain open; full suite still green at 523 regressions
- 2026-05-18: Bundler/gem CLI round 3 — `bundle list --paths` flag (prints absolute paths one per line without header), `require "bundler/setup"` shim (parses Gemfile.lock PATH/GEM sections, activates gem lib dirs in $LOAD_PATH), `require "bundler"` module stub (VERSION, setup, with_unbundled_env/clean_env, GemfileNotFound/GemNotFound/BundlerError exceptions), `require "bundler/gem_tasks"` stub (allows Rakefile includes without error); 483 regressions green
- 2026-05-19: Stage 4/6 compatibility sweep (round 2) — `loop` now calls `eval_clear_exception()` when swallowing `StopIteration` so the full exception state (including `exception_msg`) is cleared, preventing subsequent `rescue` clauses from missing exceptions after a `loop`; symbol `object_id` now stable (FNV-1a hash of name string; lexer intern() does not deduplicate); `caller_locations` now returns `Thread::Backtrace::Location` objects (with `.label`, `.lineno`, `.path`) instead of plain strings; `Thread::Backtrace::Location` class added to prelude; `IO.select` implemented via POSIX `select(2)`; `Integer ** negative_integer` now returns `Rational` per Ruby 3+ semantics; `Rational#to_s` now outputs `1/2` (no parens; was `(1/2)`); `Rational#inspect` unchanged `(1/2)`; `Random` class added to prelude (LCG PRNG, seeded instances, `Random::DEFAULT`); 587 regressions green
- 2026-05-18: Bundler/gem CLI round 2 — `bundle install --local` (path-only bundles succeed without vendor/cache; remote-gem bundles require vendor/cache and install from .gem files there), `eval_gemfile` in Gemfile DSL (includes another gemfile inline), `install_if` stub (evaluates block, git-source-compatible), `optional:` on `group`, `github:`/`bitbucket:` gem options mapped to `git:` URL (treated as unresolvable git source, warns cleanly), `ref:`/`tag:` aliases for `branch:` option, `gem exec EXEC_NAME` (searches all installed specs for named executable, loads in-process, missing-exec error path), `gem info` now persists description/homepage/license via updated `build_gemspec_content`; 480 regressions green
- 2026-05-18: Bundler/gem CLI completion sweep — `bundle init` (create Gemfile), `bundle gem NAME` (gem scaffold with versioned module, gemspec, Gemfile, spec_helper), `bundle licenses` (per-dep license lookup using path-gem gemspec or installed spec), `bundle doctor` (lockfile satisfaction check), `bundle cache`/`bundle clean` (vendor/cache packing and pruning), `bundle install --path` (persist BUNDLE_PATH to config, apply immediately), BUNDLE_PATH early-setup from config/env on every bundle invocation, `bundle exec` PATH fallback (system commands including `ruby SCRIPT` detected by basename/RbConfig comparison), platform gem filtering via `platforms:`/`platform:` DSL keys (incompatible platform tags excluded at parse time), Gemfile.lock format upgraded to full Bundler-compatible layout (GEM/PATH/PLATFORMS/DEPENDENCIES/BUNDLED WITH sections; PATH section per local gem with real version; DEPENDENCIES lists top-level deps with `!` suffix for path gems; Lockfile.read parses section headers to store "path" for PATH/GIT entries so drift detection stays correct), `build_gemspec_content` now persists description/homepage/license fields, `gem info` (version, summary, homepage, license, gem_dir, runtime deps), `gem dependency` (list runtime deps for installed gem); 476 regressions green

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

- ~~no real Rational type yet~~ done — `Rational` class in prelude with full arithmetic, comparison, rounding, and Comparable; `Rational()` kernel function; `Integer#to_r` returns a real Rational
- ~~no Complex type~~ done — `Complex` class in prelude with arithmetic, abs, conjugate, polar/rectangular; `Complex()` kernel function
- some float edge cases still need MRI-grade behavior
- `Float#to_r` still returns self rather than a true Rational approximation

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

- `class_eval`/`module_eval` with a block now correctly registers `def` in the class as an instance method (not the block frame); `def` inside any block inside a class body also hoists to the class — root cause was `__singleton_target__ = nil` in the class env making the `if (env_get(...singleton_target...))` branch fire but fall through to `env_define(env, ...)` instead of the class env
- `instance_exec` added: like `instance_eval` but passes the caller's args to the block instead of `recv`
- `exit` / `abort` raise rescuable `SystemExit` instead of calling C `exit()` directly; `exit!` keeps the bypass behavior; `SystemExit#status` and `#success?` added; `SystemExit` and `IndexError` registered in the builtin class table with correct hierarchy (`SystemExit < Exception`, `IndexError < StandardError`, `KeyError < IndexError`)
- Custom exception `initialize` with more than one parameter now works: the strict `argc > 1` guard in exception `new` was rejecting calls before the user `initialize` could run; detection of user-defined init now hoists before the arity gate and uses a relaxed message fallback
- `abort` added as a kernel method: prints message to stderr and raises `SystemExit` with status 1
- Class instance variables (`@foo` on self when self is VAL_CLASS): read, write (assign_target), compound assign (NODE_OP_ASSIGN), and instance_variable_get/set/defined? all now correct; fixes `@attr ||= val` memoization in module/class methods
- `Enumerator` class with `next`/`peek`/`rewind`/`each`/`size`/`with_index`/`with_object`; `Array#each` blockless returns an Enumerator; `Enumerator::Lazy` nested inside the class
- `FrozenError` on `instance_variable_set` for frozen objects; and on all String bang methods (`gsub!`, `sub!`, `upcase!`, `downcase!`, etc.)
- `module_function` no-arg form now creates module-level methods AND marks private; NODE_DEF in both code paths updated to detect the mode and auto-add `self.method_name`
- `Object#method_missing` default defined in prelude: raises `NoMethodError` with the original method name; super chains from user method_missing impls now produce correct error messages
- `Object#!=` for user objects now calls `==` and negates instead of using pointer identity; fixes Comparable-based `!=`
- `Object#clone` now copies the singleton_env (singleton methods); `Object#dup` still doesn't
- `Proc#===` calls the proc with the argument and checks truthiness; enables `case/when ->(x) { x > 0 }` patterns
- `alias_method` for built-in class methods: stores a VAL_SYMBOL forwarding entry; primitive dispatch checks class_env for aliases after failing C dispatch
- `Class#include?` checks transitively included modules (was one-level-only)
- `Array[start, length]` subarray form fixed (was returning single element at start)
- `Array[start, length]=` subarray assignment added
- `Array#reverse!` added
- `Array#join` now dispatches Ruby `to_s` on user objects (was using C val_to_s)
- `Float` Infinity/NaN now renders as `"Infinity"`, `"-Infinity"`, `"NaN"` (not platform C output)
- `Integer(nil)` now raises TypeError
- `format`/`sprintf` `%b` with width/padding now works correctly
- `Method#name`, `Method#owner`, `Method#receiver`, `Method#arity`, `Method#parameters`, `Method#source_location`, `Method#super_method`, `Method#unbind`, method-object equality/hash semantics; `UnboundMethod#name`, `#owner`, `#arity`, `#parameters`, `#source_location`, `#super_method`, `#bind`, `#bind_call`
- `Comparable#==` added to prelude (uses `<=>`)
- `Struct.new(keyword_init: true)` strips the option hash from member names; keyword_init structs accept keyword args at construction
- `Integer(str, base)` accepts optional base argument
- `Hash()` kernel function added (nil→{}, hash→identity, else TypeError); `Hash` added to `kernel_const_call_name` so `Hash(expr)` parses as a call
- `String#%` named format references `%{key}` implemented
- `bundler/dsl.rb` rewritten from line-by-line regex to real `instance_eval`; `eval_gemfile` uses a dir stack for relative path resolution
- `rand(range)` correctly returns random integer in the range
- `respond_to?` tables enriched for String, nil, Hash
- `Hash#sort` dispatches via the pair-array path (same as sort_by)
- `String+` tries `to_str` coercion before raising TypeError
- `Forwardable#def_delegator` handles `@ivar` accessor names
- `defined?` can appear as a command argument (parser `TOK_DEFINED` in `can_be_arg`)
- `String#oct` defaults to base 8 for unprefixed strings
- `Kernel#caller` and `#caller_locations` return the frame stack
- `IO.write`, `File.binread`/`IO.binread` class methods added
- `require "timeout"` stub; `Timeout.timeout(sec, &blk)` passes through
- `throw`/`catch` now propagates through iterator block boundaries: VAL_THROW added to `flow_signal_out` so throw inside times/each/map etc. unwinds correctly
- Struct member writers (`member=`) generated alongside readers; uses synthetic AST for the def body
- `Array#flatten` calls `to_ary` on user objects for implicit array coercion
- `Array#sum` with non-numeric initial value (array/string concatenation) works
- `Hash#each`/`Range#each` blockless return an Enumerator
- `Hash#compact`/`Hash#compact!`, `Hash#fetch_values`, `Hash#any?`/`none?` without block check emptiness
- `Integer#ceil/floor/round/truncate` with negative precision (e.g. 1234.ceil(-2) = 1300)
- `Array#intersection`, `#union`, `#difference` (Ruby 2.7+ named aliases)
- `String#partition`, `String#rpartition`
- `String#tr`, `#count`, `#delete` with `^` negation prefix now work correctly
- `String#dump` as inspect alias
- `String#match(regexp, pos)` start position argument
- `Integer#div` (floor division), `Integer#modulo` (floor modulo)
- `Kernel#Array()` tries `to_ary` before `to_a`
- `Range#sum` with block iterates and accumulates block values
- `Proc#===` calls the proc for `case/when` pattern matching
- `Object#!=` delegates to `==` instead of pointer identity
- `Object#clone` copies singleton class/methods
- `Object#method_missing` default defined in prelude with correct error message
- `Float#to_s` → `"Infinity"`, `"-Infinity"`, `"NaN"` (MRI-compatible)
- `Integer(nil)` raises TypeError; `format %b` with width/padding fixed
- String bang methods raise FrozenError on frozen strings
- `Class#include?` checks transitively included modules
- `module_function` (no-arg) creates module-level methods and marks private
- `alias_method` works for built-in class methods via VAL_SYMBOL forwarding
- `Array[start,length]=` subarray assignment; `Array#reverse!`
- `Array#join` dispatches Ruby `to_s` on user objects
- `Comparable#==` in prelude; `Struct keyword_init:`; `Integer(str, base)`; `Hash()` kernel function
- `String#%` named format references `%{key}`; `bundler/dsl.rb` uses real `instance_eval`
- `Enumerable#first(n)`: n-argument form returns first n elements; also fixed `break` from user-defined `yield`-based iterators escaping the method call instead of terminating it
- `Struct#each`, `#each_pair`, `#[]`, `#[]=`; Struct subclasses now include Enumerable so `map`/`select`/`min`/`sum`/etc. work directly on struct instances
- `String#[]=` (index, [index,len], range, and substring forms); `StringIO` expanded with read-position tracking (`pos`, `seek`, `rewind`, `eof?`, `gets`, `readline`, `readlines`, `each_line`, `read(n)`, `getc`); `$/` global initialized to `"\n"`; `$0`/`$PROGRAM_NAME` initialized from the script filename
- `Enumerator::Yielder` and `Enumerator.new { |y| ... }` block form (eager collection); `Object#to_enum`/`#enum_for` added to Kernel prelude
- Single-quoted string `'\\'` and `'\''` now correctly unescape (was storing raw backslash bytes)
- Bare method calls in `case/when then` bodies now work: NODE_LVAR on a name not in the env now falls through to a self method dispatch rather than returning nil; same fix covers any context where the parser produces NODE_LVAR for an unrecognized bare name
- `require 'json'` now provides a full JSON module with `parse`, `generate`, `dump`, `load`, and `pretty_generate`
- `require 'set'` now provides a full `Set` class backed by Hash with all standard operations (`&`, `|`, `-`, `+`, subset/superset predicates, Enumerable)
- `Exception#cause` tracks the exception that was active when a new one is raised inside a rescue block
- `instance_eval` block form now correctly routes `def` to the receiver's singleton class (was storing in block frame)
- `require 'date'` now provides a full `Date` class with Julian Day Number arithmetic, `parse`, `strftime`, Comparable, `today`, `leap?`, `yday`, `wday`, `next_day`/`prev_day`
- `Time.now`, `Time.new(y,m,d,h,min,sec)`, `Time.at`, `Time.local`/`utc`/`gm` added; Time instances now expose `year`, `month`, `day`, `hour`, `min`, `sec`, `wday`, `yday`, `strftime`
- `IO.popen(cmd) { |f| ... }` added using `popen(3)` with `is_pipe` flag on `NativeFile` for `pclose` on close
- `require 'tempfile'` now provides `Tempfile.new`/`Tempfile.create` with `write`/`close`/`unlink` lifecycle
- `File.unlink` added as alias for `File.delete`
- `Array#uniq` with block now uses block return value as the uniqueness key; `uniq!` added
- `Mutex` and `Queue`/`SizedQueue` added to core prelude (no require needed); `require 'thread'` also provides them; `Mutex` exposed at top level (was only `Thread::Mutex`)
- `File.readlines(path, chomp: true)` now strips newlines (the `chomp:` kwarg was previously ignored)
- `Binding#eval(src)` and `Binding#receiver` added
- `Kernel#fail` added as alias for `raise`
- `Module#const_get` falls back to top-level env (fixes `Kernel.const_get("Array")` etc.)
