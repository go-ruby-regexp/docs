# Why a backtracking engine

Go already ships a regular-expression engine in the standard library, so the
first question is why `go-ruby-regexp/regexp` does not simply use it. The answer is
that the standard library is **RE2**, and RE2 makes design choices that are
incompatible with Ruby's regexps — both in features and in semantics.

## What RE2 (Go's `regexp`) is

RE2 compiles a pattern to a finite automaton and simulates it. That buys a strong
guarantee: matching runs in **time linear** in the length of the input, with no
catastrophic backtracking. To keep that guarantee, RE2 **deliberately omits**
anything that cannot be expressed by an automaton:

- **No backreferences.** A pattern cannot refer back to text a previous group
  captured.
- **No lookahead or lookbehind.** No zero-width assertions about what surrounds
  the current position.
- **Leftmost-longest** match semantics (POSIX-style) by default, rather than the
  leftmost-first semantics Perl-family engines use.

For most workloads those trade-offs are exactly right. For *Ruby compatibility*
they are disqualifying.

## What Onigmo (Ruby) needs that RE2 cannot express

Ruby's engine is Onigmo, a backtracking engine in the Perl/Oniguruma family.
Real Ruby programs rely on constructs that have no automaton encoding:

- **Backreferences** — match the same text a group already captured:

    ```ruby
    /(\w+)\s+\1/      # \1 must equal the first group
    ```

- **Lookbehind** (and lookahead) — zero-width assertions:

    ```ruby
    /(?<=\$)\d+/      # digits preceded by a literal $
    ```

- **Possessive quantifiers** — consume greedily and never give back:

    ```ruby
    /a++/             # like a+ but no backtracking into the run
    ```

- **Atomic groups** — match a subpattern and discard its internal backtrack
  points:

    ```ruby
    /(?>a|ab)c/       # once the group matches, it will not retry alternatives
    ```

- **Named groups and backreferences by name**:

    ```ruby
    /(?<y>\d{4})-\k<y>/   # capture y, then require the same four digits
    ```

- **Subexpression calls** (`\g<name>`), Ruby-specific anchors and escapes
  (`\A \z \Z \G \h \H \R`), `\p{…}` Unicode properties, and per-encoding
  behaviour.

## The consequence

Backreferences and atomic/possessive constructs are not expressible in a finite
automaton, and Ruby's **leftmost-first** (not leftmost-longest) semantics mean
the *order* in which alternatives and quantifiers are tried is observable in the
result. To produce byte-for-byte the same match span and captures as Ruby, the
engine must explore the search space the way Onigmo does.

That requires a **backtracking VM**: a program of opcodes executed against the
input with an explicit backtrack stack. The cost is that naive backtracking can
blow up exponentially on adversarial patterns — which is why ReDoS hardening is a
first-class concern; see [ReDoS hardening](architecture/redos.md).
