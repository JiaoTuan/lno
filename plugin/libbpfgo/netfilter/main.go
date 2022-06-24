package main

import "C"

import (
	"fmt"
	"os"
	"time"

	bpf "github.com/aquasecurity/libbpfgo"
)

func main() {

	bpfModule, err := bpf.NewModuleFromFile("softirq.bpf.o")
	if err != nil {
		os.Exit(-1)
	}
	defer bpfModule.Close()

	bpfModule.BPFLoadObject()
	prog1, err1 := bpfModule.GetProgram("btf_raw_tracepoint__softirq_entry")
	if err1 != nil {
		os.Exit(-1)
	}
	_,err3 := prog1.AttachGeneric()
	if err3 != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(-1)
	}

	prog2, err2 := bpfModule.GetProgram("btf_raw_tracepoint__softirq_exit")
	if err2 != nil {
		os.Exit(-1)
	}
	_,err4 := prog2.AttachGeneric()
	if err4 != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(-1)
	}

	for {
		fmt.Println("Waiting...")
		time.Sleep(10 * time.Second)
	}
}