//go:build linux
// +build linux

// This program demonstrates attaching an eBPF program to a kernel symbol.
// The eBPF program will be attached to the start of the sys_execve
// kernel function and prints out the number of times it has been called
// every second.
package main

import (
	"bytes"
	"encoding/binary"
	"flag"
	_ "flag"
	"fmt"
	"log"
	"os"
	"time"
	_ "time"

	"github.com/cilium/ebpf/link"
	_ "github.com/cilium/ebpf/perf"
	"github.com/cilium/ebpf/ringbuf"
	"github.com/cilium/ebpf/rlimit"
	"github.com/vishvananda/netlink"
	"golang.org/x/sys/unix"
)

// $BPF_CLANG and $BPF_CFLAGS are set by the Makefile.
//go:generate go run github.com/cilium/ebpf/cmd/bpf2go -cc $BPF_CLANG -cflags $BPF_CFLAGS --target=amd64 bpf dnsExpress.c -- -I../headers
var iface string


func main() {
	flag.StringVar(&iface, "iface", "", "interface attached xdp program")
	flag.Parse()

	if iface == "" {
		fmt.Println("iface is not specified.")
		os.Exit(1)
	}
	link,err := netlink.LinkByName(iface)
	if err != nil {
		panic(err)
	}

}