package main

import "C"

import (
	"fmt"
	"net/http"
	"os"
	"syscall"
	"time"

	bpf "github.com/aquasecurity/libbpfgo"
)

func main() {
	/*	assert(setsockopt(sock, SOL_SOCKET, SO_ATTACH_BPF, &prog_fd,
		sizeof(prog_fd)) == 0);
		在套接字级别设置选项指level为SOL_SOCKET
	*/
	syscall.SetsockoptInt(fd,syscall.SOL_SOCKET,syscall.SO_ATTACH_BPF,)
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