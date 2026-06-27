# go-ruby-regexp documentation

**A pure-Go reimplementation of Onigmo** — the regular-expression engine that
Ruby uses — built as a **backtracking VM** with **zero cgo**.

`go-ruby-regexp/regexp` takes a pattern through a scanner/parser into an AST, lowers
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
module path is `github.com/go-ruby-regexp/regexp`.

!!! success "Status: Phases 0–4 complete — the engine roadmap is done"
    The standalone engine roadmap (Phases 0–4) is **complete**: a backtracking VM
    with leftmost-first semantics covering literals/escapes, `.`, character
    classes (incl. POSIX `[[:alpha:]]` and multibyte members `[é]` / `[à-ï]`),
    anchors, **every quantifier mode** (greedy, lazy, possessive `*+ ++ ?+`,
    atomic groups `(?>…)`), capturing/non-capturing/named groups, alternation,
    **backreferences `\1` / `\k<name>`**, **lookahead and fixed/bounded-width
    lookbehind**, `\G`, **recursive subexpression calls `\g<…>`**, inline flags
    `(?imx)`, **`\p{…}` Unicode properties**, rune-level `/i` case folding, `\h` /
    `\H`, `\R`, and **UTF-8 / ASCII-8BIT multi-encoding** with a char-advancing
    `.`. ReDoS-hardened (memoization + step budget + recursion cap +
    `WithTimeout`) with a **start-position / interior-literal optimizer**
    (up to ~210× faster) and a benchmark suite — differential-tested against MRI,
    100% coverage, CI green across 6 arches. The [Roadmap](roadmap.md) records the
    documented out-of-scope boundaries; Phase 5 (the Ruby surface) is downstream
    in the go-embedded-ruby adapter, not in this engine module.

## Performance

The engine is measured against the bar it reimplements — **C Onigmo** — and
against Go's stdlib `regexp` (**RE2**). Honest wins and losses (Apple M4 Max,
single core; full numbers in
[BENCHMARKS.md](https://github.com/go-ruby-regexp/regexp/blob/main/BENCHMARKS.md)):

- **Beats C Onigmo on the headline cases.** Literal scans run **1.7× faster**
  than C (`strings.Index` prefilter vs byte-by-byte `onig_search`); an
  alternation miss (`zoo|quux|kite`) **1.2× C and 5.8× RE2**; a structured
  numeric scan (`([0-9]{1,3}\.){3}…`) **6.5× C and 33× RE2**; the `email` scan
  lands **≈ RE2**.
- **ReDoS safety (the headline).** On `\A(a|aa)+b` the C Onigmo we reimplement
  **blows up past 70 s**; our `(pc, sp)` memo holds it to **~2 µs** — we are
  algorithmically safer than the engine we clone.
- **Inner loops narrowed to 1.6–5× of C** (was far worse) after a **lazy-NFA +
  cached-DFA** search path (an RE2-style on-the-fly simulation with a memoized
  `(frontier, byte-class) → frontier` transition table) now serves the
  capture/backref/lookaround-free subset; the backtracking VM remains the source
  of truth for the feature-rich patterns and submatch extraction. Multi-byte-heavy
  UTF-8 input is gated back to the per-step simulation, so it is never slower than
  the bare NFA. The residual gap is early-hit micro-cases, where C's per-call
  setup is cheaper than warming the table.

## Repositories

| Repo | What it is |
| --- | --- |
| [`regexp`](https://github.com/go-ruby-regexp/regexp) | the engine — scanner/parser, compiler, backtracking VM, and the public `regexp.go` API |
| [`docs`](https://github.com/go-ruby-regexp/docs) | this documentation site (MkDocs Material, versioned with mike) |
| [`go-ruby-regexp.github.io`](https://github.com/go-ruby-regexp/go-ruby-regexp.github.io) | the organization landing page (Hugo) |
| [`brand`](https://github.com/go-ruby-regexp/brand) | logo and brand assets |

## What it is

- A **backtracking VM** faithful to Onigmo, so Ruby-compatible features and
  semantics are expressible.
- **Pure Go, `CGO_ENABLED=0`** — trivial cross-compilation, a single static
  binary, no C toolchain.
- A **standalone module** with a Ruby-shaped but Go-idiomatic public API.
- **ReDoS-aware**: memoization plus a deterministic timeout/step budget, in the
  spirit of Ruby ≥3.2.

## What it is not

- **Not** an RE2 replacement. Its source-of-truth matcher is a backtracking VM,
  so it does not guarantee linear time; instead it bounds pathological matching
  with memoization plus a step/time budget. (A lazy-NFA + cached-DFA fast path
  *is* layered in for the capture/backref/lookaround-free subset — see
  [Performance](#performance) — but the backtracker remains authoritative for the
  feature-rich patterns and for submatch extraction.)
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
[github.com/go-ruby-regexp/regexp](https://github.com/go-ruby-regexp/regexp).
