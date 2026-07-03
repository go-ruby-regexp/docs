/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * C-Onigmo side of the go-ruby-regexp library-level benchmark. This is the bar
 * go-ruby-regexp reimplements in pure Go: the actual C Onigmo library MRI links.
 * It runs the SAME representative operations as the Go and Ruby drivers, over a
 * byte-identical corpus, with the SAME timing protocol (WARM untimed passes,
 * then OUTER timed passes of `inner` ops each, best pass -> ns/op) so the column
 * is like-for-like. Patterns are compiled Ruby-syntax + UTF-8, as MRI compiles.
 *
 * Build (see run.sh):
 *   cc -O2 -I$PREFIX/include c/onig_bench.c -L$PREFIX/lib -lonigmo -o onig_bench
 * Usage:
 *   ./onig_bench          # emit RESULT rows (ns/op)
 *   ./onig_bench verify   # emit VERIFY rows (cross-check output equality)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "onigmo.h"

static int OUTER = 25;
static int WARM = 3;

static long long now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* block MUST be byte-identical to ../ruby/regexp.rb BLOCK and ../go/main.go. */
static const char *BLOCK =
    "The quick brown fox jumps over 12 lazy dogs near 192.168.1.1 today. "
    "Please contact john.doe@example.com or admin_user@mail.test.org for details. "
    "Visit https://www.example.org/path?q=1 and http://a.io soon. "
    "Lorem ipsum dolor sit amet 007 042 3 14 159 265 primes and words here. ";
static const char *NEEDLE = "ZZZ_END_NEEDLE_ZZZ";

static char *corpus;      /* built at startup */
static size_t corpus_len;

/* LEXBLOCK MUST be byte-identical to LEXER in ../ruby/regexp.rb and ../go/main.go. */
static const char *LEXBLOCK = "foo123 + bar456 - baz789 * qux000 / quux ; ";
static char *lexer;       /* the lexer-shaped corpus for the tokenizer ops */
static size_t lexer_len;

static void build_corpus(void) {
  size_t bl = strlen(BLOCK), nl = strlen(NEEDLE);
  corpus_len = bl * 48 + nl;
  corpus = malloc(corpus_len + 1);
  size_t o = 0;
  for (int i = 0; i < 48; i++) { memcpy(corpus + o, BLOCK, bl); o += bl; }
  memcpy(corpus + o, NEEDLE, nl);
  corpus[corpus_len] = '\0';

  size_t xl = strlen(LEXBLOCK);
  lexer_len = xl * 64;
  lexer = malloc(lexer_len + 1);
  o = 0;
  for (int i = 0; i < 64; i++) { memcpy(lexer + o, LEXBLOCK, xl); o += xl; }
  lexer[lexer_len] = '\0';
}

/* 32-bit FNV-1a — identical arithmetic to the Go and Ruby drivers. */
static uint32_t fnv1a(const unsigned char *s, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) h = (h ^ s[i]) * 16777619u;
  return h;
}

static regex_t *compile_once(const char *pat) {
  regex_t *reg = NULL;
  OnigErrorInfo einfo;
  const UChar *p = (const UChar *)pat;
  int r = onig_new(&reg, p, p + strlen(pat), ONIG_OPTION_DEFAULT,
                   ONIG_ENCODING_UTF8, ONIG_SYNTAX_RUBY, &einfo);
  if (r != ONIG_NORMAL) {
    OnigUChar s[ONIG_MAX_ERROR_MESSAGE_LEN];
    onig_error_code_to_str(s, r, &einfo);
    fprintf(stderr, "onig compile error for /%s/: %s\n", pat, s);
    exit(1);
  }
  return reg;
}

/* one leftmost search from byte offset pos; returns 1 and fills *b,*e on hit. */
static int search_at(regex_t *reg, OnigRegion *region, size_t pos,
                     long *b, long *e) {
  const UChar *str = (const UChar *)corpus;
  const UChar *end = str + corpus_len;
  int r = onig_search(reg, str, end, str + pos, end, region, ONIG_OPTION_NONE);
  if (r < 0) return 0;
  *b = region->beg[0];
  *e = region->end[0];
  return 1;
}

/* --- op implementations (each returns a scalar so the loop can't be elided) - */

static volatile long sink;

static long op_scan_word(regex_t *reg, OnigRegion *region) {
  long count = 0, b, e; size_t pos = 0;
  while (pos <= corpus_len && search_at(reg, region, pos, &b, &e)) {
    count++;
    pos = (e == b) ? (size_t)e + 1 : (size_t)e;
  }
  return count;
}

static long op_search_email(regex_t *reg, OnigRegion *region) {
  long b, e;
  return search_at(reg, region, 0, &b, &e) ? b : -1;
}

static long op_match_ipv4(regex_t *reg, OnigRegion *region) {
  long b, e;
  return search_at(reg, region, 0, &b, &e) ? b : -1;
}

/* gsub of /\s+/ -> "_", returns FNV-1a of the rewritten string. */
static uint32_t op_gsub_space(regex_t *reg, OnigRegion *region, char *out) {
  long b, e; size_t pos = 0, last = 0, o = 0;
  while (pos <= corpus_len && search_at(reg, region, pos, &b, &e)) {
    memcpy(out + o, corpus + last, (size_t)b - last); o += (size_t)b - last;
    out[o++] = '_';
    last = (size_t)e;
    pos = (e == b) ? (size_t)e + 1 : (size_t)e;
  }
  memcpy(out + o, corpus + last, corpus_len - last); o += corpus_len - last;
  return fnv1a((const unsigned char *)out, o);
}

/* --- StringScanner-style tokenizer ops over the lexer corpus -------------- */
/* Anchored match at pos (StringScanner's cursor primitive): onig_match matches
 * only at `at`, filling region; returns 1 and the match end on a hit. */
static regex_t *re_ident, *re_num, *re_ws, *re_op, *re_tword, *re_nonws;

static int lex_match_at(regex_t *reg, OnigRegion *region, size_t pos, long *e) {
  const UChar *s = (const UChar *)lexer, *end = s + lexer_len;
  int r = onig_match(reg, s, end, s + pos, region, ONIG_OPTION_NONE);
  if (r < 0) return 0;
  *e = region->end[0];
  return 1;
}

/* scan-tokenize: anchored match per token from an advancing cursor; count. */
static long op_scan_tokenize(OnigRegion *region) {
  regex_t *pats[4] = {re_ident, re_num, re_ws, re_op};
  long n = 0; size_t pos = 0;
  while (pos < lexer_len) {
    int matched = 0;
    for (int i = 0; i < 4; i++) {
      long e;
      if (lex_match_at(pats[i], region, pos, &e) && (size_t)e > pos) {
        pos = (size_t)e; n++; matched = 1; break;
      }
    }
    if (!matched) pos++;
  }
  return n;
}

/* skip: alternate whitespace / non-whitespace runs; total bytes skipped. */
static long op_skip(OnigRegion *region) {
  long total = 0; size_t pos = 0;
  while (pos < lexer_len) {
    long e;
    if (lex_match_at(re_ws, region, pos, &e) && (size_t)e > pos) {
      total += (long)((size_t)e - pos); pos = (size_t)e;
    } else if (lex_match_at(re_nonws, region, pos, &e) && (size_t)e > pos) {
      total += (long)((size_t)e - pos); pos = (size_t)e;
    } else {
      pos++;
    }
  }
  return total;
}

/* match?: anchored, non-advancing, at every position; sum of matched lengths. */
static long op_match_q(OnigRegion *region) {
  long n = 0;
  for (size_t pos = 0; pos < lexer_len; pos++) {
    long e;
    if (lex_match_at(re_tword, region, pos, &e)) n += (long)((size_t)e - pos);
  }
  return n;
}

/* scan_until: forward search to and past each operator; total bytes returned. */
static long op_scan_until(OnigRegion *region) {
  const UChar *s = (const UChar *)lexer, *end = s + lexer_len;
  long total = 0; size_t pos = 0;
  while (pos < lexer_len) {
    int r = onig_search(re_op, s, end, s + pos, end, region, ONIG_OPTION_NONE);
    if (r < 0) break;
    long e = region->end[0];
    total += (long)((size_t)e - pos);
    pos = (size_t)e;
  }
  return total;
}

/* timing harness mirroring _harness.rb / bench.go. */
#define BENCH(label, inner, CODE)                                        \
  do {                                                                   \
    int _n = (inner);                                                    \
    for (int w = 0; w < WARM; w++)                                       \
      for (int j = 0; j < _n; j++) { CODE }                              \
    long long best = 0;                                                  \
    for (int o = 0; o < OUTER; o++) {                                    \
      long long t0 = now_ns();                                           \
      for (int j = 0; j < _n; j++) { CODE }                             \
      long long dt = now_ns() - t0;                                      \
      if (o == 0 || dt < best) best = dt;                                \
    }                                                                    \
    printf("RESULT\t%s\t%.1f\n", label, (double)best / _n);             \
  } while (0)

int main(int argc, char **argv) {
  if (getenv("OUTER")) OUTER = atoi(getenv("OUTER"));
  if (getenv("WARM")) WARM = atoi(getenv("WARM"));
  build_corpus();

  regex_t *re_word = compile_once("\\w+");
  regex_t *re_email = compile_once("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}");
  regex_t *re_ipv4 = compile_once("([0-9]{1,3}\\.){3}[0-9]{1,3}");
  regex_t *re_space = compile_once("\\s+");
  re_ident = compile_once("[A-Za-z_][A-Za-z0-9_]*");
  re_num = compile_once("[0-9]+");
  re_ws = compile_once("\\s+");
  re_op = compile_once("[-+*/;]");
  re_tword = compile_once("[A-Za-z0-9_]+");
  re_nonws = compile_once("\\S+");
  OnigRegion *region = onig_region_new();
  char *gbuf = malloc(corpus_len * 2 + 16);

  if (argc > 1 && strcmp(argv[1], "verify") == 0) {
    /* scan-word: count + FNV-1a of matches joined by \0 (as the other drivers) */
    long b, e; size_t pos = 0, jn = 0; long count = 0;
    char *join = malloc(corpus_len + 1);
    while (pos <= corpus_len && search_at(re_word, region, pos, &b, &e)) {
      if (count > 0) join[jn++] = '\0';
      memcpy(join + jn, corpus + b, (size_t)(e - b)); jn += (size_t)(e - b);
      count++;
      pos = (e == b) ? (size_t)e + 1 : (size_t)e;
    }
    long eb, ee; search_at(re_ipv4, region, 0, &eb, &ee);
    long mb, me; search_at(re_email, region, 0, &mb, &me);
    printf("VERIFY\tcorpus\t%zu:%u\n", corpus_len, fnv1a((const unsigned char *)corpus, corpus_len));
    printf("VERIFY\tscan-word\t%ld:%u\n", count, fnv1a((const unsigned char *)join, jn));
    printf("VERIFY\tsearch-email\t%ld\n", mb);
    printf("VERIFY\tmatch-ipv4\t%ld:%.*s\n", eb, (int)(ee - eb), corpus + eb);
    printf("VERIFY\tgsub-space\t%u\n", op_gsub_space(re_space, region, gbuf));
    printf("VERIFY\tscan-tokenize\t%ld\n", op_scan_tokenize(region));
    printf("VERIFY\tskip\t%ld\n", op_skip(region));
    printf("VERIFY\tmatch?\t%ld\n", op_match_q(region));
    printf("VERIFY\tscan_until\t%ld\n", op_scan_until(region));
    free(join);
    return 0;
  }

  BENCH("compile-literal",   5000, { regex_t *r = compile_once("needle"); onig_free(r); });
  BENCH("compile-class",     5000, { regex_t *r = compile_once("[A-Za-z0-9_]+"); onig_free(r); });
  BENCH("compile-alt",       5000, { regex_t *r = compile_once("cat|dog|fox|bird|fish|wolf"); onig_free(r); });
  BENCH("compile-backtrack", 5000, { regex_t *r = compile_once("([0-9]{1,3}\\.){3}[0-9]{1,3}"); onig_free(r); });
  BENCH("compile-unicode",   5000, { regex_t *r = compile_once("\\p{L}+"); onig_free(r); });

  BENCH("scan-word",    20,   { sink = op_scan_word(re_word, region); });
  BENCH("search-email", 2000, { sink = op_search_email(re_email, region); });
  BENCH("match-ipv4",   2000, { sink = op_match_ipv4(re_ipv4, region); });
  BENCH("gsub-space",   20,   { sink = (long)op_gsub_space(re_space, region, gbuf); });

  BENCH("scan-tokenize", 200,  { sink = op_scan_tokenize(region); });
  BENCH("skip",          300,  { sink = op_skip(region); });
  BENCH("match?",        200,  { sink = op_match_q(region); });
  BENCH("scan_until",    2000, { sink = op_scan_until(region); });

  onig_region_free(region, 1);
  onig_free(re_word); onig_free(re_email); onig_free(re_ipv4); onig_free(re_space);
  onig_free(re_ident); onig_free(re_num); onig_free(re_ws); onig_free(re_op);
  onig_free(re_tword); onig_free(re_nonws);
  free(gbuf); free(corpus); free(lexer);
  onig_end();
  return 0;
}
