# Roadmap (phases)

`go-ruby-regexp/regexp` is grown **test-first**, one capability at a time, each phase
differential-tested against Onigmo/MRI rather than built in isolation. There are
six phases, 0 through 5. **The standalone engine roadmap (Phases 0–4) is
complete**; Phase 5 (the Ruby surface) is downstream in the go-embedded-ruby
adapter that consumes this engine, not part of this module.

| Phase | Name | Goal | Status |
| --- | --- | --- | --- |
| 0 | Scanner + parser + VM | Scanner/parser for the common subset (literals, classes, `. * + ? {m,n}`, groups, alternation, anchors `^ $ \A \z`), compiler, and a backtracking VM. Exit: anchored/greedy matching with captures matches MRI on a starter corpus. | **Done** |
| 1 | Groups & quantifier modes | Named groups `(?<name>…)`, backreferences `\1` / `\k<name>`, and every quantifier mode — greedy, lazy `*? +? ?? {m,n}?`, possessive `*+ ++ ?+`, and atomic groups `(?>…)`. | **Done** |
| 2 | Lookaround & calls | Lookahead `(?=…)` `(?!…)`, fixed/bounded-width lookbehind `(?<=…)` `(?<!…)`, the `\G` anchor, and recursive subexpression calls `\g<…>` (`\g<name>` / `\g<n>` / `\g<±n>` / `\g<0>`). | **Done** |
| 3 | Unicode & encodings | `\p{…}` Unicode properties, POSIX bracket classes `[[:alpha:]]`, `\h` / `\H`, `\R`, rune-level `/i` case folding, inline flags `(?imx)`, and UTF-8 / ASCII-8BIT multi-encoding with multibyte class members `[é]` / `[à-ï]`. | **Done** |
| 4 | ReDoS hardening & optimizer | `(pc, sp)` memoization, a deterministic step budget, a recursion-depth cap, and a wall-clock `WithTimeout`; a transparent start-position / required-interior-literal prefilter (up to ~210× faster); a benchmark suite. | **Done** |
| 5 | Ruby surface | The full Ruby `Regexp`/`MatchData` surface via the go-embedded-ruby adapter, and the replacement DSL (`\1`, `\k<>`, `\&`, blocks). **Downstream** — lives in the adapter, not this module. | Downstream |

## Documented out-of-scope boundaries

These are **deliberate** divergences from MRI/Onigmo, recorded so the engine's
surface is unambiguous:

- **Full/special case folding** — only *simple (1:1)* folding is done; multi-char
  expansions (`ß`→`ss`) and locale rules (Turkish dotless-ı/İ) are out of scope,
  and backreference folding is ASCII-only.
- **`\p{…}` is a deliberate slice** of categories/aliases (`L N P S Z C`, the
  `Lu Ll Lt Lm Lo Nd` subcategories, and `Alpha Alnum Digit Space Upper Lower
  Word`); script and block names (`\p{Han}`, …) and the one-letter `\pL` form are
  not accepted.
- **Encodings beyond UTF-8 / ASCII-8BIT** (UTF-16/32, EUC-JP, Shift_JIS, …) are
  out of scope; a caller transcodes legacy/wide text to UTF-8 at the boundary.
- **Match offsets are byte offsets**, whereas MRI reports character offsets — the
  two agree on matched *text* but not on the numeric span on multi-byte input.
- **Invalid UTF-8** decodes leniently (replacement rune, width 1) rather than
  raising as MRI does.
- **Variable-width lookbehind** is rejected (a constant or bounded per-alternative
  width is required), matching Onigmo.

See the [Architecture overview](architecture/index.md) for the pipeline these
phases build out, and [ReDoS hardening](architecture/redos.md) for the Phase 4
threat model.
