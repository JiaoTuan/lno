package tcpdrop


import (
	"C"
	_"bytes"
	_"encoding/binary"
	"fmt"
	"os"
	"time"

	bpf "github.com/aquasecurity/tracee/libbpfgo"
)

func tcpDrop() {

	bpfModule, err := bpf.NewModuleFromFile("tcpdrop.bpf.o")
	if err != nil {
		os.Exit(-1)
	}
	defer bpfModule.Close()

	bpfModule.BPFLoadObject()
	prog, err := bpfModule.GetProgram("kprobe__tcp_drop")
	if err != nil {
		os.Exit(-1)
	}

	_, err = prog.AttachKprobe("tcp_drop")
	if err != nil {
		os.Exit(-1)
	}
	for {
		fmt.Println("Waiting...")
		time.Sleep(10 * time.Second)
	}
}
