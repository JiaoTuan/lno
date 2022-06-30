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
	_ "flag"
	"fmt"
	"log"
	"os"
	_ "os"
	"time"
	_ "time"

	"github.com/aquasecurity/libbpfgo/helpers"
	_ "github.com/aquasecurity/libbpfgo/helpers"
	"github.com/cilium/ebpf/link"
	_ "github.com/cilium/ebpf/perf"
	"github.com/cilium/ebpf/ringbuf"
	"github.com/cilium/ebpf/rlimit"
	_ "golang.org/x/sys/unix"
)

// $BPF_CLANG and $BPF_CFLAGS are set by the Makefile.
//go:generate go run github.com/cilium/ebpf/cmd/bpf2go -cc $BPF_CLANG -cflags $BPF_CFLAGS --target=amd64 bpf interrupt.c -- -I../headers
type intr_message struct {
    Vector    uint64
	Pid	      uint32
	Stack_id  [10]uint64
}

func main() {

	fn := "do_error_trap"

	if err := rlimit.RemoveMemlock(); err != nil {
		log.Fatal(err)
	}
	
	
	objs := bpfObjects{}
	if err := loadBpfObjects(&objs, nil); err != nil {
		log.Fatalf("loading objects: %v", err)
	}
	defer objs.Close()

	kp, err := link.Kprobe(fn, objs.KprobeDoErrorTrap,nil)
	if err != nil {
		log.Fatalf("opening kprobe: %s", err)
	}
	defer kp.Close()

	ticker := time.NewTicker(1 * time.Second)
	log.Println("Waiting for events..")
	rd, err := ringbuf.NewReader(objs.Events)
	if err != nil {
		panic(err)
	}
	defer rd.Close()
	event := intr_message{}
	for range ticker.C {
		//var ip [127]uint64
		record, err := rd.Read()
		fmt.Println(record)
		if err != nil {
			panic(err)
		}
		// Data is padded with 0 for alignment
		//fmt.Println("Sample:", record.RawSample)
		err1 := binary.Read(bytes.NewBuffer(record.RawSample), binary.LittleEndian, &event)
		//copy(event,record.RawSample)
		if err1 != nil {
			fmt.Printf("failed to decode received data: %s\n", err)
			continue
		}

		//err2 := objs.bpfMaps.StackTraces.Lookup(event.Stack_id,&ip)
		//i//f err2!=nil{
		//	panic(err2)
		//}
		m,err := helpers.NewKernelSymbolsMap()
		if err != nil {
		 	fmt.Fprintln(os.Stderr, err)
		 	os.Exit(-1)
		}
		fmt.Printf("??????? %v\n",event.Stack_id)
		//fmt.Println("%v",ip)
		for i:=0;i<10;i++ {
			// if ip[i] == 0{
			// 	break
			// }
			
			sym, _ := m.GetSymbolByAddr(event.Stack_id[i])
			// if err != nil {
			// 	//fmt.Fprintln(os.Stderr, err)
			// 	fmt.Println(ip[i])
			// 	continue
			// }
		
		// 	if sym.Address == 0 || sym.Name == "" {
		// 		continue
		// 	}
		 	fmt.Printf("%v %v %v %v\n",sym.Name,sym.Address,sym.Owner,sym.Type)
		// }
	}
	
	}
	return
}