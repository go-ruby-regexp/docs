#!/usr/bin/env bash
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Library-level cross-runtime benchmark runner for go-ruby-regexp.
#
# Runs the SAME regexp workload through (a) the pure-Go go-ruby-regexp library
# (go/), (b) each available reference Ruby runtime (ruby/regexp.rb), and (c) the
# C Onigmo the library reimplements (c/onig_bench.c, built from source), then
# prints one Markdown table per sub-benchmark: ns/op and the ratio vs MRI.
#
# Every driver is checked to emit byte-identical output (VERIFY rows) before any
# timing is trusted; a mismatch aborts.
#
# Usage:  bash benchmarks/run.sh
# Env:    OUTER (timed passes, default 25), WARM (untimed passes, default 3),
#         RUBY / JRUBY / TRUFFLERUBY (override runtime binaries),
#         ONIG_PREFIX (prebuilt Onigmo install prefix; else built from source),
#         ONIGMO_TAG (default Onigmo-6.2.0), SKIP_ONIG=1 (drop the C column).
set -u
cd "$(dirname "$0")"

RUBY=${RUBY:-ruby}
JRUBY=${JRUBY:-jruby}
TRUFFLERUBY=${TRUFFLERUBY:-truffleruby}
export GOWORK=off

TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

# ---- Optionally build the C Onigmo oracle from source (cached in .work). -----
ONIG_BIN=""
if [ "${SKIP_ONIG:-0}" != "1" ]; then
  WORK="${WORK:-$PWD/.work}"; mkdir -p "$WORK"
  PREFIX="${ONIG_PREFIX:-$WORK/onigmo-install}"
  if [ ! -f "$PREFIX/lib/libonigmo.a" ] && [ -z "${ONIG_PREFIX:-}" ]; then
    echo ">> building Onigmo C (${ONIGMO_TAG:-Onigmo-6.2.0}) from source ..." >&2
    mkdir -p "$WORK"; rm -rf "$WORK/Onigmo"
    git clone --depth 1 --branch "${ONIGMO_TAG:-Onigmo-6.2.0}" \
      https://github.com/k-takata/Onigmo.git "$WORK/Onigmo" 2>/dev/null || true
    if [ -d "$WORK/Onigmo" ]; then
      ( cd "$WORK/Onigmo"
        CF="-O2 -std=gnu17 -Wno-incompatible-function-pointer-types -Wno-deprecated-non-prototype"
        if command -v autoreconf >/dev/null 2>&1; then autoreconf -i
        else pkgx +gnu.org/autoconf +gnu.org/automake +gnu.org/libtool +gnu.org/m4 -- autoreconf -i; fi
        ./configure --prefix="$PREFIX" CFLAGS="$CF" && make -j4 && make install ) >/dev/null 2>&1
    fi
  fi
  if [ -f "$PREFIX/lib/libonigmo.a" ]; then
    if cc -O2 -I"$PREFIX/include" c/onig_bench.c -L"$PREFIX/lib" -lonigmo -o "$WORK/onig_bench" 2>/dev/null; then
      ONIG_BIN="$WORK/onig_bench"
    fi
  fi
  [ -z "$ONIG_BIN" ] && echo "  (onig-c: Onigmo not available — C column skipped)" >&2
fi

# ---- Verify every driver produces byte-identical output. --------------------
echo ">> verifying output equality ..." >&2
GO_V=$(cd go && go run . verify 2>/dev/null)
MRI_V=$($RUBY ruby/regexp.rb verify 2>/dev/null)
if [ "$GO_V" != "$MRI_V" ]; then
  echo "!! go vs MRI output mismatch — aborting" >&2
  diff <(echo "$GO_V") <(echo "$MRI_V") >&2; exit 1
fi
if [ -n "$ONIG_BIN" ]; then
  ONIG_V=$("$ONIG_BIN" verify 2>/dev/null)
  if [ "$ONIG_V" != "$MRI_V" ]; then
    echo "!! onig-c vs MRI output mismatch — aborting" >&2
    diff <(echo "$ONIG_V") <(echo "$MRI_V") >&2; exit 1
  fi
fi
echo "   outputs identical across go / MRI${ONIG_BIN:+ / onig-c}." >&2

run() { # <runtime-label> <cmd...>
  local label=$1; shift
  command -v "$1" >/dev/null 2>&1 || { echo "  ($label: $1 not found — skipped)" >&2; return; }
  echo "  $label ..." >&2
  "$@" 2>/dev/null | awk -v r="$label" '$1=="RESULT"{printf "%s\t%s\t%s\n", r, $2, $3}' >> "$TMP"
}

echo "== go-ruby-regexp library-level benchmark ==" >&2
echo "  go ..." >&2
( cd go && command -v go >/dev/null 2>&1 && go run . 2>/dev/null ) \
  | awk '$1=="RESULT"{printf "go\t%s\t%s\n", $2, $3}' >> "$TMP"
run "mri"         "$RUBY"                "ruby/regexp.rb"
run "mri-yjit"    "$RUBY" --yjit         "ruby/regexp.rb"
run "jruby"       "$JRUBY"               "ruby/regexp.rb"
run "truffleruby" "$TRUFFLERUBY"         "ruby/regexp.rb"
if [ -n "$ONIG_BIN" ]; then
  echo "  onig-c ..." >&2
  "$ONIG_BIN" 2>/dev/null | awk '$1=="RESULT"{printf "onig-c\t%s\t%s\n", $2, $3}' >> "$TMP"
fi

echo >&2
# Emit one Markdown table per sub-benchmark (label), runtimes as rows.
awk -F'\t' '
  { key=$2; rt=$1; ns=$3; labels[key]=1; val[rt SUBSEP key]=ns; rts[rt]=1 }
  END {
    order="go onig-c mri mri-yjit jruby truffleruby"
    n=split(order, ord, " ")
    ln=0; for (k in labels) lab[++ln]=k
    for (i=1;i<=ln;i++) for (j=i+1;j<=ln;j++) if (lab[j]<lab[i]){t=lab[i];lab[i]=lab[j];lab[j]=t}
    for (i=1;i<=ln;i++){
      k=lab[i]
      printf "\n#### %s\n\n", k
      print  "| Runtime | ns/op | vs MRI |"
      print  "| --- | ---: | ---: |"
      base=val["mri" SUBSEP k]
      for (o=1;o<=n;o++){
        rt=ord[o]; v=val[rt SUBSEP k]
        if (v=="") continue
        ratio=(base!=""&&base+0>0)? sprintf("%.2f×", v/base) : "—"
        name=rt
        if (rt=="go") name="**go-ruby-regexp (pure Go)**"
        else if (rt=="onig-c") name="C Onigmo 6.2.0"
        else if (rt=="mri") name="MRI"
        else if (rt=="mri-yjit") name="MRI + YJIT"
        else if (rt=="jruby") name="JRuby"
        else if (rt=="truffleruby") name="TruffleRuby"
        printf "| %s | %s | %s |\n", name, v, ratio
      }
    }
  }
' "$TMP"
