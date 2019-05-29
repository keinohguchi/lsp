// SPDX-License-Identifier: GPL-2.0
package lsp

import (
	"testing"

	"golang.org/x/sys/unix"
)

func TestSelect(t *testing.T) {
	tests := []struct {
		name string
		nfds int
		rfds *unix.FdSet
		wfds *unix.FdSet
		xfds *unix.FdSet
		tv   *unix.Timeval
		want int
		err  error
	}{
		{
			name: "read select on stdin",
			nfds: unix.Stdin + 1,
			rfds: &unix.FdSet{
				Bits: [16]int64{1 << unix.Stdin},
			},
			want: 1,
		},
		{
			name: "write select on stdout",
			nfds: unix.Stdout + 1,
			wfds: &unix.FdSet{
				Bits: [16]int64{1 << unix.Stdout},
			},
			want: 1,
		},
		{
			name: "read/write select on stdin and stdout",
			nfds: unix.Stdout + 1,
			rfds: &unix.FdSet{
				Bits: [16]int64{1 << unix.Stdin},
			},
			wfds: &unix.FdSet{
				Bits: [16]int64{1 << unix.Stdout},
			},
			want: 2,
		},
		{
			name: "1usec timeout with no descriptor ready",
			tv:   &unix.Timeval{Usec: 1},
			want: 0,
		},
	}

	for _, tc := range tests {
		tc := tc
		t.Run(tc.name, func(t *testing.T) {
			n, err := unix.Select(
				tc.nfds, tc.rfds, tc.wfds, tc.xfds, tc.tv,
			)
			if got, want := n, tc.want; got != want {
				t.Fatalf("unexpected number of file descriptors:\n- want: %d\n-  got: %d",
					want, got)
			}
			if got, want := err, tc.err; got != want {
				t.Fatalf("unexpected error:\n- want: %v\n-  got: %v",
					want, got)
			}
		})
	}
}
