---
title: "Syntax & parser"
weight: 40
---

# Syntax & parser {#syntax-parser}

The `syntax` package is the front of the pipeline: a **scanner** turns the
pattern text into tokens, and a **parser** builds an abstract syntax tree in
Onigmo's grammar. The AST is what the [compiler]({{< relref "_index.md#packages" >}}) lowers to
bytecode, so the parser's job is to capture every construct Ruby's regexps can
express — faithfully, including the cases RE2 omits.

## Scanner {#scanner}

The scanner reads the pattern as bytes under a declared encoding (UTF-8 or
ASCII-8BIT) and emits tokens for literals, metacharacters, escape sequences,
group openers, quantifiers, and class brackets. Encoding matters here: a
character class or a `\p{…}` property is interpreted in terms of the pattern's
encoding, which the `Encoding`-keyed cursor in the
[`compile`]({{< relref "_index.md#packages" >}}) package selects.

## The Onigmo grammar surface {#the-onigmo-grammar-surface}

The parser accepts the Onigmo construct set, which is considerably larger than
RE2's. The main groups of syntax it must recognize:

### Escapes and anchors {#escapes-and-anchors}

- Standard escapes and shorthand classes: `\d \w \s` and negations.
- Ruby anchors and escapes: `\A` (start of string), `\z` (end of string), `\Z`
  (end, before a trailing newline), `\G` (match-start anchor), `\h`/`\H`
  (hex digit / non), `\R` (any line break).

### Groups {#groups}

- Named capture: `(?<name>…)`.
- Non-capturing: `(?:…)`.
- Atomic: `(?>…)` — match the subpattern, then discard its internal backtrack
  points.
- Inline flag scopes: `(?i:…)`, `(?m)…`, etc.

### Quantifiers {#quantifiers}

- **Greedy**: `* + ? {m,n}` — match as much as possible, give back on failure.
- **Lazy** (non-greedy): `*? +? ?? {m,n}?` — match as little as possible, take
  more on failure.
- **Possessive**: `*+ ++ ?+` — match greedily and **never** give back. (A
  trailing `+` on a `{m,n}` brace is *not* possessive but a stacked greedy repeat
  `(a{m,n})+`, matching Onigmo.)

### Character classes and properties {#character-classes-and-properties}

- Bracketed classes `[...]` with ranges and negation `[^...]`.
- POSIX classes inside brackets: `[[:alpha:]]`, `[[:digit:]]`, … (the 14
  standard classes, positive and negated).
- Unicode properties: `\p{L}`, `\p{Nd}`, `\P{…}` — resolved by the
  [`charset`]({{< relref "_index.md#packages" >}}) package. A **deliberate slice** of the
  general categories (`L N P S Z C`), the `Lu Ll Lt Lm Lo Nd` subcategories, and
  the Onigmo POSIX-style aliases (`Alpha Alnum Digit Space Upper Lower Word`) is
  supported; script and block names (`\p{Han}`, …) and the one-letter `\pL` form
  are out of scope (see the [Roadmap]({{< relref "../roadmap.md" >}})).

### Flags {#flags}

- Pattern-level options (case-insensitive, multiline, extended) and their inline
  scoped forms, threaded through the AST so the compiler can specialize matching.

## Output {#output}

The parser produces a typed AST: concatenation, alternation, repetition (with
greediness and possessiveness), captures (indexed and named), assertions
(lookaround, anchors), and character-set nodes. Anything malformed is reported as
a parse error rather than silently accepted — fuzzing the parser for crashes is
part of the test plan (see [Contributing]({{< relref "../contributing.md" >}})).
