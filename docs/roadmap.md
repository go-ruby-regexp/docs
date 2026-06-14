# Roadmap (phases)

`go-onigmo/regexp` is grown **test-first**, one capability at a time, each phase
differential-tested against Onigmo/MRI rather than built in isolation. There are
six phases, 0 through 5. Phase 0 and the named-group/backreference half of
Phase 1 are implemented; the rest is planned.

| Phase | Name | Goal | Status |
| --- | --- | --- | --- |
| 0 | Scanner + parser + VM | Scanner/parser for the common subset (literals, classes, `. * + ? {m,n}`, groups, alternation, anchors `^ $ \A \z`), compiler, and a minimal backtracking VM. Exit: anchored/greedy matching with captures matches MRI on a starter corpus. | **Done** |
| 1 | Groups & backreferences | Named groups `(?<name>…)` and backreferences `\1` / `\k<name>` **done**; non-greedy and possessive quantifiers and atomic groups pending. | **Partial** |
| 2 | Lookaround & calls | Lookahead/lookbehind, the `\G` anchor, and subexpression calls `\g<…>`. | Planned |
| 3 | Unicode & encodings | Unicode properties `\p{…}`, POSIX classes, case-folding, and multi-encoding support. | Planned |
| 4 | ReDoS hardening & optimizer | Memoization plus a deterministic timeout/step budget; optimizer (first-byte sets, literal prefixes); benchmarks. | Planned |
| 5 | Ruby surface | The full Ruby `Regexp`/`MatchData` surface via the go-embedded-ruby adapter, and the replacement DSL (`\1`, `\k<>`, `\&`, blocks). | Planned |

See the [Architecture overview](architecture/index.md) for the pipeline these
phases build out, and [ReDoS hardening](architecture/redos.md) for the Phase 4
threat model.
