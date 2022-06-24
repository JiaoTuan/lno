all: vmlinux.h bpf_target go_target

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

bpf_target: execve.bpf.c
	clang -g -O2 -c -target bpf -o execve.bpf.o execve.bpf.c

go_target: simple.bpf.o main.go
	CC=gcc CGO_CFLAGS="-I /usr/include" CGO_LDFLAGS="/usr/lib64/libbpf.a" go build -o execve

clean:
	rm execve.bpf.o libbpfgo-prog vmlinux.h
