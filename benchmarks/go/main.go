// SPDX-License-Identifier: BSD-3-Clause
//
// Go side of the go-ruby-regexp library-level benchmark. Exercises the same
// representative regexp operations as the Ruby driver (benchmarks/ruby), over a
// byte-identical corpus, through the pure-Go go-ruby-regexp library's public
// API. The library exposes match primitives (Compile / Match / MatchData), not
// Ruby's String#scan / gsub sugar, so scan and replace are built here from
// Match exactly as a caller would — that cost is part of the measured operation.
package main

import (
	"fmt"
	"os"
	"strings"

	regexp "github.com/go-ruby-regexp/regexp"
)

// block MUST be byte-identical to BLOCK in ../ruby/regexp.rb and the C driver.
const block = "The quick brown fox jumps over 12 lazy dogs near 192.168.1.1 today. " +
	"Please contact john.doe@example.com or admin_user@mail.test.org for details. " +
	"Visit https://www.example.org/path?q=1 and http://a.io soon. " +
	"Lorem ipsum dolor sit amet 007 042 3 14 159 265 primes and words here. "

var corpus = strings.Repeat(block, 48) + "ZZZ_END_NEEDLE_ZZZ"

var (
	reWord  = mustCompile(`\w+`)
	reEmail = mustCompile(`[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}`)
	reIPv4  = mustCompile(`([0-9]{1,3}\.){3}[0-9]{1,3}`)
	reSpace = mustCompile(`\s+`)
)

// lexer is the lexer-shaped corpus for the StringScanner-style tokenizer ops
// below — the many-short-matches workload where the engine's per-call cost, not
// the match itself, dominates. It is byte-identical to LEXER in ../ruby/regexp.rb
// and ../c/onig_bench.c.
var lexer = strings.Repeat("foo123 + bar456 - baz789 * qux000 / quux ; ", 64)

var (
	reIdent = mustCompile(`[A-Za-z_][A-Za-z0-9_]*`)
	reNum   = mustCompile(`[0-9]+`)
	reWS    = mustCompile(`\s+`)
	reOp    = mustCompile(`[-+*/;]`)
	reTWord = mustCompile(`[A-Za-z0-9_]+`)
	reNonWS = mustCompile(`\S+`)
)

// opScanTokenize is the classic StringScanner lexer loop: anchored match per token
// from an advancing cursor until end of input; result = tokens consumed. It uses
// the bounds-only MatchBoundsAt primitive (no MatchData), the fast path for a
// tokenizer that only needs each token's span.
func opScanTokenize() int {
	pats := []*regexp.Regexp{reIdent, reNum, reWS, reOp}
	pos, n := 0, 0
	for pos < len(lexer) {
		matched := false
		for _, re := range pats {
			if _, e, ok := re.MatchBoundsAt(lexer, pos); ok && e > pos {
				pos = e
				n++
				matched = true
				break
			}
		}
		if !matched {
			pos++
		}
	}
	return n
}

// opSkip alternates whitespace / non-whitespace runs (StringScanner#skip);
// result = total bytes skipped (== len(lexer)).
func opSkip() int {
	pos, total := 0, 0
	for pos < len(lexer) {
		if _, e, ok := reWS.MatchBoundsAt(lexer, pos); ok && e > pos {
			total += e - pos
			pos = e
		} else if _, e, ok := reNonWS.MatchBoundsAt(lexer, pos); ok && e > pos {
			total += e - pos
			pos = e
		} else {
			pos++
		}
	}
	return total
}

// opMatchQ is anchored, non-advancing match? at every character position
// (StringScanner#match?); result = sum of matched lengths.
func opMatchQ() int {
	n := 0
	for pos := 0; pos < len(lexer); pos++ {
		if b, e, ok := reTWord.MatchBoundsAt(lexer, pos); ok {
			n += e - b
		}
	}
	return n
}

// opScanUntil hops forward to and past each operator (StringScanner#scan_until);
// result = total bytes returned across all hops.
func opScanUntil() int {
	pos, total := 0, 0
	for pos < len(lexer) {
		_, e, ok := reOp.MatchBounds(lexer[pos:])
		if !ok {
			break
		}
		total += e
		pos += e
	}
	return total
}

func mustCompile(p string) *regexp.Regexp {
	r, err := regexp.Compile(p)
	if err != nil {
		panic(err)
	}
	return r
}

// fnv1a is the 32-bit FNV-1a hash, identical arithmetic to the Ruby and C
// drivers; used only to cross-check output equality, never in a timed region.
func fnv1a(s string) uint32 {
	var h uint32 = 2166136261
	for i := 0; i < len(s); i++ {
		h = (h ^ uint32(s[i])) * 16777619
	}
	return h
}

// scanAll returns every leftmost, non-overlapping match of r in s — the
// String#scan primitive built on Match. Match scans forward from the start of
// the string it is given, so advancing the window is a cheap re-slice (no copy).
func scanAll(r *regexp.Regexp, s string) []string {
	var out []string
	for pos := 0; pos <= len(s); {
		m := r.Match(s[pos:])
		if m == nil {
			break
		}
		b, e := pos+m.Begin(0), pos+m.End(0)
		out = append(out, s[b:e])
		if e == b { // defensive: none of the benchmark patterns match empty
			pos = e + 1
		} else {
			pos = e
		}
	}
	return out
}

// gsub replaces every leftmost, non-overlapping match of r in s with repl — the
// gsub primitive built on Match. The benchmark patterns never match empty.
func gsub(r *regexp.Regexp, s, repl string) string {
	var sb strings.Builder
	pos, last := 0, 0
	for pos <= len(s) {
		m := r.Match(s[pos:])
		if m == nil {
			break
		}
		b, e := pos+m.Begin(0), pos+m.End(0)
		sb.WriteString(s[last:b])
		sb.WriteString(repl)
		last = e
		if e == b {
			pos = e + 1
		} else {
			pos = e
		}
	}
	sb.WriteString(s[last:])
	return sb.String()
}

func main() {
	if len(os.Args) > 1 && os.Args[1] == "verify" {
		words := scanAll(reWord, corpus)
		me := reEmail.Match(corpus)
		mi := reIPv4.Match(corpus)
		fmt.Printf("VERIFY\tcorpus\t%d:%d\n", len(corpus), fnv1a(corpus))
		fmt.Printf("VERIFY\tscan-word\t%d:%d\n", len(words), fnv1a(strings.Join(words, "\x00")))
		fmt.Printf("VERIFY\tsearch-email\t%d\n", me.Begin(0))
		fmt.Printf("VERIFY\tmatch-ipv4\t%d:%s\n", mi.Begin(0), mi.Str(0))
		fmt.Printf("VERIFY\tgsub-space\t%d\n", fnv1a(gsub(reSpace, corpus, "_")))
		fmt.Printf("VERIFY\tscan-tokenize\t%d\n", opScanTokenize())
		fmt.Printf("VERIFY\tskip\t%d\n", opSkip())
		fmt.Printf("VERIFY\tmatch?\t%d\n", opMatchQ())
		fmt.Printf("VERIFY\tscan_until\t%d\n", opScanUntil())
		return
	}

	// compile: pattern -> matcher, five representative pattern shapes.
	bench("compile-literal", 5000, func() { r, _ := regexp.Compile("needle"); sink = r })
	bench("compile-class", 5000, func() { r, _ := regexp.Compile("[A-Za-z0-9_]+"); sink = r })
	bench("compile-alt", 5000, func() { r, _ := regexp.Compile("cat|dog|fox|bird|fish|wolf"); sink = r })
	bench("compile-backtrack", 5000, func() { r, _ := regexp.Compile(`([0-9]{1,3}\.){3}[0-9]{1,3}`); sink = r })
	bench("compile-unicode", 5000, func() { r, _ := regexp.Compile(`\p{L}+`); sink = r })

	// match / scan over the fixed corpus.
	bench("scan-word", 20, func() { sink = scanAll(reWord, corpus) })
	bench("search-email", 2000, func() { sink = reEmail.Match(corpus).Begin(0) })
	bench("match-ipv4", 2000, func() { sink = reIPv4.Match(corpus) })
	bench("gsub-space", 20, func() { sink = gsub(reSpace, corpus, "_") })

	// StringScanner-style tokenizer ops: the many-short-matches workload where the
	// engine's per-call setup dominates (the previously-losing scan ops).
	bench("scan-tokenize", 200, func() { sink = opScanTokenize() })
	bench("skip", 300, func() { sink = opSkip() })
	bench("match?", 200, func() { sink = opMatchQ() })
	bench("scan_until", 2000, func() { sink = opScanUntil() })
}
