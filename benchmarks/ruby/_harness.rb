# frozen_string_literal: true
#
# Copyright (c) the go-ruby-* authors
# SPDX-License-Identifier: BSD-3-Clause
#
# Library-level micro-benchmark harness (Ruby side).
#
# bench(label, inner) { work } runs `WARM` untimed outer passes (to let YJIT /
# JRuby / TruffleRuby reach steady state), then `OUTER` timed passes of `inner`
# operations each, timed with a monotonic clock, and reports the BEST pass as
# nanoseconds per operation. Interpreter start-up is deliberately OUTSIDE the
# timed region: this isolates the operation itself, so the number is the library
# primitive's cost, not `ruby file.rb` process cost.
#
# Output protocol (one line per sub-benchmark), consumed by run.sh:
#   RESULT\t<label>\t<ns_per_op>

OUTER = Integer(ENV.fetch("OUTER", "25"))
WARM  = Integer(ENV.fetch("WARM", "3"))

def bench(label, inner)
  WARM.times { inner.times { yield } }
  best = nil
  OUTER.times do
    t0 = Process.clock_gettime(Process::CLOCK_MONOTONIC)
    inner.times { yield }
    dt = Process.clock_gettime(Process::CLOCK_MONOTONIC) - t0
    best = dt if best.nil? || dt < best
  end
  ns = (best / inner) * 1e9
  printf("RESULT\t%s\t%.1f\n", label, ns)
end

# Deterministic byte buffer shared with the Go driver: buf[i] = (i*31 + 7) & 0xFF.
def det_bytes(n)
  (0...n).map { |i| ((i * 31 + 7) & 0xFF).chr }.join.b
end
