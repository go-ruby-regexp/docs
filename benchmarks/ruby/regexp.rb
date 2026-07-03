# frozen_string_literal: true
# SPDX-License-Identifier: BSD-3-Clause
#
# Ruby side of the go-ruby-regexp library-level benchmark. Exercises the same
# representative regexp operations as the Go driver (benchmarks/go), over a
# byte-identical corpus, so the reported ns/op are like-for-like: MRI's own
# Onigmo through `Regexp` vs the pure-Go go-ruby-regexp library.
require_relative "_harness"

# ---- shared corpus (byte-identical to the Go and C drivers) ----------------
# One deterministic ASCII block, repeated; a sentinel needle is appended so the
# tail is distinct. The verify step cross-checks every driver builds the SAME
# bytes (corpus length + FNV-1a hash) before any timing is trusted.
BLOCK = "The quick brown fox jumps over 12 lazy dogs near 192.168.1.1 today. " \
        "Please contact john.doe@example.com or admin_user@mail.test.org for details. " \
        "Visit https://www.example.org/path?q=1 and http://a.io soon. " \
        "Lorem ipsum dolor sit amet 007 042 3 14 159 265 primes and words here. "
CORPUS = (BLOCK * 48) + "ZZZ_END_NEEDLE_ZZZ"

# Precompiled literals for the match/scan loops (compiled once, as a program
# would); the compile benchmarks below build fresh Regexps to time compilation.
RE_WORD  = /\w+/
RE_EMAIL = /[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}/
RE_IPV4  = /([0-9]{1,3}\.){3}[0-9]{1,3}/
RE_SPACE = /\s+/

# 32-bit FNV-1a — identical arithmetic in the Go and C drivers; used only to
# cross-check output equality (never inside a timed region).
def fnv1a(str)
  h = 2166136261
  str.each_byte { |b| h = ((h ^ b) * 16777619) & 0xffffffff }
  h
end

if ARGV[0] == "verify"
  words = CORPUS.scan(RE_WORD)
  m = RE_IPV4.match(CORPUS)
  printf("VERIFY\tcorpus\t%d:%d\n", CORPUS.bytesize, fnv1a(CORPUS))
  printf("VERIFY\tscan-word\t%d:%d\n", words.size, fnv1a(words.join("\x00")))
  printf("VERIFY\tsearch-email\t%d\n", (CORPUS =~ RE_EMAIL))
  printf("VERIFY\tmatch-ipv4\t%d:%s\n", m.begin(0), m[0])
  printf("VERIFY\tgsub-space\t%d\n", fnv1a(CORPUS.gsub(RE_SPACE, "_")))
  exit
end

# ---- compile: pattern -> matcher, for five representative pattern shapes ----
bench("compile-literal",   5000) { Regexp.new("needle") }
bench("compile-class",     5000) { Regexp.new("[A-Za-z0-9_]+") }
bench("compile-alt",       5000) { Regexp.new("cat|dog|fox|bird|fish|wolf") }
bench("compile-backtrack", 5000) { Regexp.new("([0-9]{1,3}\\.){3}[0-9]{1,3}") }
bench("compile-unicode",   5000) { Regexp.new("\\p{L}+") }

# ---- match / scan over the fixed corpus -------------------------------------
bench("scan-word",     20)   { CORPUS.scan(RE_WORD) }          # String#scan (full pass)
bench("search-email",  2000) { CORPUS =~ RE_EMAIL }            # =~ leftmost search
bench("match-ipv4",    2000) { RE_IPV4.match(CORPUS) }         # Regexp#match (captures)
bench("gsub-space",    20)   { CORPUS.gsub(RE_SPACE, "_") }    # gsub-style replace (full pass)
