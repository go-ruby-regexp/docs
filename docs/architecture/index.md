# Architecture overview

`go-ruby-regexp/regexp` is a compiler **and** a virtual machine. A pattern is parsed
into an abstract syntax tree, the AST is lowered to a bytecode program, and that
program is executed by a backtracking VM against the input to produce
`MatchData`. The model is Onigmo's — a backtracking matcher — not an NFA/DFA
simulator; see [Why a backtracking engine](../why.md) for the reasoning.

## The pipeline

```text
pattern (string, encoding, flags)
   │  scanner / parser  → AST (Onigmo syntax)
   ▼
   │  compiler          → bytecode program (opcodes for the backtracking VM)
   ▼
   │  optimizer         → anchors, first-byte sets, literal prefixes, atomic cuts
   ▼
program  ──►  VM (backtracking, memoized, budgeted)  ──►  MatchData
```

Each stage has a single responsibility:

- **scanner / parser** turns the pattern text into an AST in Onigmo's grammar.
- **compiler** lowers the AST into a bytecode program plus capture/group
  metadata.
- **optimizer** annotates the program — anchors, first-byte sets, literal
  prefixes, atomic cuts — to skip impossible work.
- **VM** executes the program with a backtrack stack, memoization, and a step
  budget, producing `MatchData`.

## Packages

The engine (`github.com/go-ruby-regexp/regexp`) is organized as a chain of small
packages mirroring the pipeline, plus the public API:

| Package | Responsibility |
| --- | --- |
| `internal/syntax` | scanner + parser → AST; Onigmo grammar and escapes |
| `internal/ast` | the typed AST node set the parser produces and the compiler consumes |
| `internal/compile` | AST → VM program (instructions + capture/group metadata), and the `Encoding`-keyed cursor (UTF-8 / ASCII-8BIT) |
| `internal/vm` | backtracking matcher: thread state, backtrack stack, memo, step/recursion budget, wall-clock timeout, and the start-position / interior-literal prefilters |
| `internal/charset` | `\p{…}` Unicode property classification |
| `regexp.go` | public API: `Compile` / `CompileEnc`, `Match` / `MatchString`, `WithTimeout`, `Encoding`, and `MatchData` (spans by index and name) |

The detail pages cover the load-bearing pieces:
[Syntax & parser](syntax.md), the [Backtracking VM](vm.md), and
[ReDoS hardening](redos.md).

## Relationship to go-embedded-ruby

The engine is **standalone**: it has no dependency on the Ruby runtime, and any
Go program can import `github.com/go-ruby-regexp/regexp` directly. The dependency runs
one way only — [go-embedded-ruby](https://github.com/go-embedded-ruby) uses this
engine as its regexp backend. A thin adapter in
`go-embedded-ruby/ruby/internal/regexp` maps Ruby's `Regexp` and `MatchData`
objects onto the engine's API, so byte offsets, named captures, and replacement
semantics line up with what Ruby exposes.
