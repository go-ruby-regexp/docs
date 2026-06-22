# ReDoS hardening

A backtracking VM is the price of Ruby compatibility, and the bill comes due as
**ReDoS** — regular-expression denial of service. Certain patterns
(`(a+)+$` against a long non-matching input is the textbook case) make a naive
backtracker explore an exponential number of paths. `go-ruby-regexp/regexp` treats
this as a first-class concern and mitigates it the way Ruby ≥3.2 does, rather
than relying on the caller to defend itself.

## Threat model

The danger is not malformed input crashing the engine; it is a **pattern and
input pair that runs for an unbounded time**. This matters most when either the
pattern or the input comes from an untrusted source. An attacker who can supply a
catastrophic pattern, or an input that triggers catastrophic backtracking in an
existing pattern, can hang a request thread. The engine must therefore guarantee
that *every* match terminates within a bounded amount of work.

## Mitigations

### Memoization where it is sound

The VM memoizes `(instruction, input-position)` pairs so that a path it has
already proven to fail is not re-explored. For a large class of patterns this
collapses the exponential search back to polynomial work. Memoization is applied
**only where it is safe** — specifically, where the outcome does not depend on
captured text. A subpattern that contains a [backreference](vm.md#how-backreferences-constrain-optimization)
cannot be memoized on position alone, because the same `(instruction, position)`
can succeed or fail depending on what was captured; those regions fall back to
the budget.

### A deterministic step budget

Independently of memoization, the VM carries a **backtrack-step budget**. Each
unit of backtracking work decrements it; when it is exhausted the match is
**aborted deterministically** (`Match` returns `nil`) rather than returning a
wrong answer or running forever. "Deterministic" is the key word: the same
pattern, input, and budget always abort at the same point, so behaviour is
reproducible across runs and platforms — not dependent on wall-clock jitter. A
**recursion-depth cap** bounds subexpression calls (`\g<…>`) the same way, so a
non-terminating recursive grammar fails locally instead of exhausting the stack.

### A wall-clock timeout

`re.WithTimeout(d)` returns a *copy* of the `Regexp` that aborts any single match
exceeding `d` of real time (Ruby's `Regexp.timeout` equivalent) — the real-time
backstop to the deterministic step budget, for the residual cases (notably
backreference-bearing patterns) memoization cannot prune. The receiver is left
unchanged, so a shared `*Regexp` stays concurrency-safe. The VM polls the
monotonic clock only once every 4096 steps, so a search with no deadline pays
nothing. A pathological pattern is bounded by whichever of the budget or the
deadline it reaches first.

## Never rely on a host watchdog alone

A common but fragile approach is to run the match on a goroutine and kill it from
the outside. That is **not** sufficient here: a goroutine cannot be forcibly
preempted mid-CPU-loop in a portable way, and an external watchdog leaks
resources and is racy. The budget and timeout are enforced **inside** the VM's
own loop, so termination is a property of the engine, not of how the caller
happens to invoke it. The watchdog, if present, is a backstop — never the primary
defence.
