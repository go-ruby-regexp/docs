# go-onigmo documentation

**A pure-Go reimplementation of Onigmo** — the regular-expression engine that
Ruby uses — built as a **backtracking VM** with **zero cgo**.

`go-onigmo/regexp` takes a pattern through a scanner/parser into an AST, lowers
that AST to a bytecode program, and runs the program on a backtracking virtual
machine to produce `MatchData`. It deliberately follows Onigmo's model rather
than the RE2 automata model used by Go's standard `regexp`, because Ruby's
regexps rely on features automata cannot express — backreferences,
lookahead/lookbehind, possessive quantifiers, atomic groups, named groups, and
subexpression calls — and on **leftmost-first** match semantics, so results are
byte-for-byte what Ruby produces.

The module is **standalone and reusable**: any Go program can import it. It is
also the regexp backend for [go-embedded-ruby](https://github.com/go-embedded-ruby),
where a thin adapter maps Ruby's `Regexp`/`MatchData` onto this engine. The
module path is `github.com/go-onigmo/regexp`.

!!! success "Status: Phases 0 and 1 implemented"
    The engine runs: a greedy backtracking VM with leftmost-first semantics —
    literals/escapes, `.`, character classes, anchors, greedy quantifiers,
    groups, alternation, **named groups `(?<name>…)` and backreferences
    `\1` / `\k<name>`** — differential-tested against MRI, 100% coverage, CI
    green across 6 arches. The [Roadmap](roadmap.md) tracks the remaining phases.

## Repositories

| Repo | What it is |
| --- | --- |
| [`regexp`](https://github.com/go-onigmo/regexp) | the engine — scanner/parser, compiler, backtracking VM, and the public `regexp.go` API |
| [`docs`](https://github.com/go-onigmo/docs) | this documentation site (MkDocs Material, versioned with mike) |
| [`go-onigmo.github.io`](https://github.com/go-onigmo/go-onigmo.github.io) | the organization landing page (Hugo) |
| [`brand`](https://github.com/go-onigmo/brand) | logo and brand assets |

## What it is

- A **backtracking VM** faithful to Onigmo, so Ruby-compatible features and
  semantics are expressible.
- **Pure Go, `CGO_ENABLED=0`** — trivial cross-compilation, a single static
  binary, no C toolchain.
- A **standalone module** with a Ruby-shaped but Go-idiomatic public API.
- **ReDoS-aware**: memoization plus a deterministic timeout/step budget, in the
  spirit of Ruby ≥3.2.

## What it is not

- **Not** an NFA/DFA or RE2-style engine. It does not guarantee linear time;
  instead it bounds pathological matching with a budget.
- **Not** a drop-in for Go's standard `regexp` syntax — it implements Onigmo
  (Ruby) syntax and semantics.
- **Not** dependent on go-embedded-ruby; the dependency runs the other way.

## Where to go next

- [Why a backtracking engine](why.md) — what RE2 cannot express and why
  backtracking is required for byte-compatible Ruby regexps.
- [Architecture overview](architecture/index.md) — the pipeline and the packages
  that make it up.
- [Roadmap (phases)](roadmap.md) — the six phases from scanner to full Ruby
  surface.

Source lives at
[github.com/go-onigmo/regexp](https://github.com/go-onigmo/regexp).
