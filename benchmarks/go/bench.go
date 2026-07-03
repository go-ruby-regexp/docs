// Copyright (c) the go-ruby-* authors
// SPDX-License-Identifier: BSD-3-Clause
//
// Library-level micro-benchmark harness (Go side). Mirrors _harness.rb exactly:
// WARM untimed outer passes, then OUTER timed passes of `inner` ops each, best
// pass reported as ns/op. Emits the same RESULT protocol run.sh consumes.
package main

import (
	"fmt"
	"os"
	"strconv"
	"time"
)

var (
	outerN = envInt("OUTER", 25)
	warmN  = envInt("WARM", 3)
	// sink defeats dead-code elimination of the timed work.
	sink any
)

func envInt(k string, def int) int {
	if v := os.Getenv(k); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return def
}

func bench(label string, inner int, fn func()) {
	for i := 0; i < warmN; i++ {
		for j := 0; j < inner; j++ {
			fn()
		}
	}
	var best time.Duration
	for o := 0; o < outerN; o++ {
		t0 := time.Now()
		for j := 0; j < inner; j++ {
			fn()
		}
		dt := time.Since(t0)
		if o == 0 || dt < best {
			best = dt
		}
	}
	ns := float64(best.Nanoseconds()) / float64(inner)
	fmt.Printf("RESULT\t%s\t%.1f\n", label, ns)
}

// detBytes matches det_bytes in _harness.rb: buf[i] = (i*31 + 7) & 0xFF.
func detBytes(n int) []byte {
	b := make([]byte, n)
	for i := range b {
		b[i] = byte((i*31 + 7) & 0xFF)
	}
	return b
}
