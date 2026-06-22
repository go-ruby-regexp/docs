# Contributing

Contributions are welcome. `go-ruby-regexp/regexp` is built to a small set of
non-negotiable rules — they are what keep the engine pure-Go, correct, and
Ruby-compatible. Please read these before opening a pull request.

## Hard rules

- **Build from source — no vendoring.** Everything compiles from source. Do not
  reach for prebuilt binaries or vendored blobs as a shortcut; being able to
  compile from source is a guarantee of independence.
- **100% test coverage target, enforced in CI.** New code ships with tests, and
  coverage is a CI gate. Fill the error branches, not just the happy path.
- **All GitHub content in English.** Issues, pull requests, commits, comments,
  and discussions are English-only.
- **Differential testing against Onigmo/MRI.** Correctness is defined by
  reference Ruby. A corpus of `(pattern, input)` pairs is run through both Ruby
  and this engine, and the match span, captures, and named groups are compared
  **exactly** — not approximated from memory. Onigmo's own test corpus is ported
  as fixtures, and property/fuzz tests check the parser for crashes.
- **Pure Go, cgo disabled.** The whole point is a single static binary with no C
  toolchain. Code must build with `CGO_ENABLED=0`. If a feature seems to need C,
  it needs a pure-Go path instead.
- **The ReDoS budget always terminates.** Every match must finish within the
  deterministic timeout/step budget. A change that can make matching run
  unbounded is a bug, no matter how fast the common case is — see
  [ReDoS hardening](architecture/redos.md).

## Workflow

1. Pick or open an issue describing the change.
2. Work test-first: add the differential / unit tests, then make them pass.
3. Run the full suite with coverage and confirm the gate is green.
4. Open a PR in English, referencing the issue.

## Where things live

The engine — scanner/parser, compiler, backtracking VM, and the public API — is
in [`github.com/go-ruby-regexp/regexp`](https://github.com/go-ruby-regexp/regexp). This
documentation site is in
[`github.com/go-ruby-regexp/docs`](https://github.com/go-ruby-regexp/docs). Start from the
[Architecture overview](architecture/index.md) and the [Roadmap](roadmap.md) to
find the right place for your change.
