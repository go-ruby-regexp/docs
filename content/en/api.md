---
title: "API"
weight: 70
---

# API {#api}

The public API lives in `regexp.go` at the module root
(`github.com/go-ruby-regexp/regexp`). It is **Ruby-shaped but Go-idiomatic**: the
concepts map onto Ruby's `Regexp`/`MatchData`, but the surface follows Go
conventions (an explicit `error`, byte offsets, value types).

{{< callout type="info" >}}
**Status: implemented**

The engine is built — the standalone roadmap (Phases 0–4) is complete and the
module is importable as `github.com/go-ruby-regexp/regexp`. The shapes below are the
public surface; the replacement DSL and the full Ruby `Regexp`/`MatchData`
object surface (Phase 5) live downstream in the go-embedded-ruby adapter. See
the [Roadmap]({{< relref "roadmap.md" >}}).
{{< /callout >}}

## Shape {#shape}

### Compiling {#compiling}

- **`Compile(pattern string) (*Regexp, error)`** — parse and compile a pattern
  once in the default UTF-8 encoding; reuse the `*Regexp` for many matches. A
  `*Regexp` is immutable and safe for concurrent use.
- **`CompileEnc(pattern string, enc Encoding) (*Regexp, error)`** — `Compile`
  with an explicit input encoding. `Encoding` is `UTF8` (the default — the dot
  and byte-oriented classes advance by a whole code point) or `ASCII8BIT`
  (Ruby's binary `/n` — every atom advances one byte).
- **`(*Regexp).WithTimeout(d time.Duration) *Regexp`** — return a *copy* that
  aborts any single match exceeding `d` of wall-clock time (Ruby's
  `Regexp.timeout` equivalent), the real-time backstop to the deterministic step
  budget. The receiver is left unchanged, so a shared `*Regexp` stays
  concurrency-safe. `(*Regexp).Timeout()` reads the limit back.
- **`(*Regexp).Encoding() Encoding`** and **`(*Regexp).String() string`** —
  report the input encoding (Ruby's `Regexp#encoding`) and the source pattern.

### Matching {#matching}

- **`(*Regexp).Match(s string) *MatchData`** — search for the leftmost match;
  returns `*MatchData` on success or `nil` on no match (or when the step budget
  or a configured timeout is exceeded).
- **`(*Regexp).MatchString(s string) bool`** — report whether `s` matches.

### Reading a result {#reading-a-result}

`MatchData` holds the byte spans of the whole match (group 0) and of each
capturing group, by **index and by name**. All offsets are **byte offsets** into
the input, so callers can map them back onto their own string representation.

- `Str(i int) string` / `StrName(name string) string` — captured text by index
  or by name.
- `Begin(i int) int` / `End(i int) int` — byte offsets of group `i` (`-1` if the
  group did not participate).
- `IndexOfName(name string) int` — the 1-based group index for a named capture
  (`-1` if no group has that name).
- `NGroups() int` — the number of capturing groups (not counting group 0).
- `Pre() string` / `Post() string` — the input before and after the whole match.

## Example {#example}

```go
re, err := onigmo.Compile(`(?<year>\d{4})-(?<mon>\d{2})`)
m := re.Match("2026-06")          // *MatchData or nil
m.StrName("year")                 // "2026"
m.Begin(0); m.End(0)              // byte offsets of the whole match

// An explicit encoding and a wall-clock timeout:
bin, _ := onigmo.CompileEnc(`\xC3\xA9`, onigmo.ASCII8BIT)
safe := re.WithTimeout(100 * time.Millisecond)
```

## Relationship to Ruby {#relationship-to-ruby}

A thin adapter in `go-embedded-ruby/ruby/internal/regexp` maps Ruby's `Regexp`
and `MatchData` objects onto this API. Because `MatchData` already reports byte
offsets and exposes captures by both index and name, the adapter is a mechanical
mapping rather than a reimplementation — the engine does the work, the adapter
just presents it in Ruby's object shapes. The replacement DSL (`\1`, `\k<name>`,
`\&`, block forms) and the rest of the full Ruby `Regexp`/`MatchData` surface
(Phase 5) live **in that adapter, not in this engine module**.
