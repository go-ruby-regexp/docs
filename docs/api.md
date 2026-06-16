# API

The public API lives in `regexp.go` at the module root
(`github.com/go-onigmo/regexp`). It is **Ruby-shaped but Go-idiomatic**: the
concepts map onto Ruby's `Regexp`/`MatchData`, but the surface follows Go
conventions (an explicit `error`, byte offsets, value types).

!!! success "Status: implemented"
    The engine is built — the standalone roadmap (Phases 0–4) is complete and the
    module is importable as `github.com/go-onigmo/regexp`. The shapes below are the
    public surface; the replacement DSL and the full Ruby `Regexp`/`MatchData`
    object surface (Phase 5) live downstream in the go-embedded-ruby adapter. See
    the [Roadmap](roadmap.md).

## Shape

- **`Compile(pattern string, opts Options) (*Regexp, error)`** — parse and
  compile a pattern once; reuse the `*Regexp` for many matches.
- **`(*Regexp).Match(input string) *MatchData`** — run the program against the
  input; returns `*MatchData` on success or `nil` on no match.
- **`MatchData`** — the result of a successful match: the whole-match span and
  every group's span, by **index and by name**, plus pre-match and post-match
  text. All spans are **byte offsets** into the input, so callers can map them
  back onto their own string representation.
    - `Group(name string) string` / group by index — captured text.
    - `Begin(i)` / `End(i)` — byte offsets of group `i`.
- **`(*Regexp).Replace(src, repl string) string`** — substitution with a
  replacement DSL that understands `\1` (numbered) and `\k<name>` (named)
  backreferences (and, later, `\&` and block forms).

## Example

```go
re, err := onigmo.Compile(`(?<year>\d{4})-(?<mon>\d{2})`, onigmo.None)
m := re.Match("2026-06")          // *MatchData or nil
m.Group("year")                   // "2026"
m.Begin(0); m.End(0)              // byte offsets
re.Replace(src, `\k<mon>/\k<year>`)
```

## Relationship to Ruby

A thin adapter in `go-embedded-ruby/ruby/internal/regexp` maps Ruby's `Regexp`
and `MatchData` objects onto this API. Because `MatchData` already reports byte
offsets and exposes captures by both index and name, the adapter is a mechanical
mapping rather than a reimplementation — the engine does the work, the adapter
just presents it in Ruby's object shapes.
