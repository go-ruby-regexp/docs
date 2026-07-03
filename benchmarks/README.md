<!-- SPDX-License-Identifier: BSD-3-Clause -->
# `go-ruby-regexp` library-level benchmark harness

Reproducible, cross-runtime benchmark of the **pure-Go `go-ruby-regexp` library**
against the reference Ruby runtimes (MRI, MRI + YJIT, JRuby, TruffleRuby) **and
the C Onigmo it reimplements**. It measures the regexp primitive through the Go
API, isolated from the rbgo interpreter, so the numbers answer: *is the pure-Go
engine as fast as the reference runtimes — and as C Onigmo?*

## Layout

- `go/`            — self-contained Go driver; `go.mod` pins the published library.
- `ruby/regexp.rb` — the equivalent workload; `ruby/_harness.rb` is the shared timer.
- `c/onig_bench.c` — the C Onigmo 6.2.0 oracle (Ruby syntax + UTF-8, `onig_search`).
- `run.sh`         — builds the C oracle, verifies output equality across every
  driver, runs each available runtime, and prints one Markdown table per
  sub-benchmark (ns/op + ratio vs MRI).

## Run

```sh
bash benchmarks/run.sh
```

Environment knobs: `OUTER` (timed passes, default 25), `WARM` (untimed warm-up
passes, default 3), `RUBY`/`JRUBY`/`TRUFFLERUBY` to select runtime binaries,
`ONIG_PREFIX` to point at a prebuilt Onigmo install (otherwise it is built from
source, cached under `.work/`), and `SKIP_ONIG=1` to drop the C column.

## Operations

- **compile** — pattern → matcher, for five representative shapes: literal,
  character class, alternation, a backtracking-prone bounded repetition, and a
  Unicode property.
- **scan-word** — `String#scan(/\w+/)` over the corpus (full pass).
- **search-email** — `=~` leftmost search for an email address.
- **match-ipv4** — `Regexp#match` of an IPv4 pattern (early hit, with captures).
- **gsub-space** — `gsub(/\s+/, "_")` over the corpus (full-pass replace).

The go-ruby-regexp library exposes match primitives (`Compile`/`Match`/
`MatchData`), not Ruby's `scan`/`gsub` sugar, so those two ops are built on
`Match` in the Go driver exactly as a caller would — that cost is part of the
measured operation.

The **StringScanner-style tokenizer ops** run over a separate lexer-shaped corpus
(`"foo123 + bar456 - baz789 * qux000 / quux ; "` × 64) — the many-short-matches
workload the strscan parity suite flagged as trailing MRI + YJIT:

- **scan-tokenize** — anchored `scan` per token from an advancing cursor.
- **skip** — `skip` alternating whitespace / non-whitespace runs.
- **match?** — anchored, non-advancing `match?` at every position.
- **scan_until** — forward `scan_until` hopping to and past each operator.

The Ruby column drives these through its own `StringScanner`; the Go column
through the bounds-only `MatchBoundsAt` / `MatchBounds` primitives (no
`MatchData`); the C column through `onig_match` / `onig_search`.

## Method

Each process runs `WARM` untimed passes (to let the JVM/GraalVM JITs warm up),
then `OUTER` timed passes of a fixed inner loop, timed with a monotonic clock;
the **best** pass is reported as **ns/op**. Interpreter start-up is outside the
timed region. All drivers build **identical inputs** (the same deterministic
~13 KB corpus and patterns) and their outputs are cross-checked **byte-identical**
(corpus + every op, via an FNV-1a hash) before timing — a mismatch aborts the
run. Results are published, dated, in [`../docs/performance.md`](../docs/performance.md).

A deeper, dedicated **go-ruby-regexp vs C Onigmo vs RE2** parity study (ReDoS,
structured scans, the lazy-DFA lever history) lives in the library repo at
[`BENCHMARKS.md`](https://github.com/go-ruby-regexp/regexp/blob/main/BENCHMARKS.md).
