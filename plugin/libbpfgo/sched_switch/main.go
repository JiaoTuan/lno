package main

import "C"

import (
	"fmt"
	"os"
	"time"

	bpf "github.com/aquasecurity/libbpfgo"
)

func main() {

	bpfModule, err := bpf.NewModuleFromFile("simple.bpf.o")
	if err != nil {
		os.Exit(-1)
	}
	defer bpfModule.Close()

	bpfModule.BPFLoadObject()
	prog, err := bpfModule.GetProgram("btf_raw_tracepoint__sched_switch")
	if err != nil {
		os.Exit(-1)
	}

	_,err1 := prog.AttachGeneric()
	if err1 != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(-1)
	}
	
	if prog.GetFd() == 0 {
		os.Exit(-1)
	}

	for {
		fmt.Println("Waiting...")
		time.Sleep(10 * time.Second)
	}
}