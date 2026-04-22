# Exception Test Matrix

This is the next evaluator-semantic target. The goal is to add exception support in a controlled order, starting from the current `raise` behavior and expanding toward Ruby-style `begin` / `rescue` / `ensure`.

## Current baseline

These behaviors already exist and should stay green while exception work lands:

| Case | Current status | Executable coverage |
|---|---|---|
| `raise` with no args | exits non-zero with `RuntimeError: RuntimeError` | `raise_default_runtime_error` |
| `raise "boom"` | exits non-zero with `RuntimeError: boom` | `raise_explicit_runtime_error` |
| error location points at the `raise` site | yes | `raise_explicit_runtime_error` |
| missing method is a terminal runtime error | yes | `runtime_error` |

## Phase 1: exception signal plumbing

Target: `raise` stops being just `ev->errored` and becomes a real unwindable signal.

Planned cases:

- uncaught `raise "boom"` exits non-zero and prints the raised message
- raise inside a method unwinds to the caller
- raise inside a block unwinds out of the block and method call
- nested calls preserve the original raise site in the surfaced error
- evaluator does not confuse exception unwind with `return`, `break`, or `next`

## Phase 2: parser and AST for `begin` / `rescue` / `ensure`

Target: parser accepts explicit exception-handling forms.

Planned cases:

- `begin ... rescue ... end`
- `begin ... ensure ... end`
- `begin ... rescue ... ensure ... end`
- nested `begin` blocks
- syntax errors for incomplete forms like `begin rescue end` with missing bodies where required

## Phase 3: rescue semantics

Target: rescue catches exception signals and converts them back into ordinary values.

Planned cases:

- rescued raise returns the rescue body value
- code after `raise` in the protected body does not run
- first matching rescue arm wins
- bare `rescue` catches default runtime exceptions
- non-matching rescue class lets the exception continue unwinding
- nested rescue catches inner exceptions without swallowing outer ones accidentally

Current coverage:

- bare `rescue`
- typed `rescue ArgumentError`
- non-matching typed rescue
- multiple rescue arms
- rescuing `NoMethodError`

## Phase 4: ensure semantics

Target: ensure runs exactly once on both success and failure paths.

Planned cases:

- ensure runs when no exception occurs
- ensure runs when an exception is raised
- ensure runs when a method returns early
- ensure runs when a block exits via `break` or `next`
- ensure body ordering matches Ruby expectations: protected body, rescue if any, then ensure

## Phase 5: typed exceptions

Target: runtime failures become real exception classes instead of just formatted strings.

Planned cases:

- `raise RuntimeError`
- `raise ArgumentError, "bad arg"`
- missing method produces `NoMethodError`
- wrong arity produces `ArgumentError`
- obvious type misuse produces `TypeError`
- `rescue ArgumentError` only catches matching exception classes

## Phase 6: polish

Nice-to-have after the basics work:

- exception variable binding in rescue clauses
- re-raise
- exception object message / class inspection
- backtrace support
