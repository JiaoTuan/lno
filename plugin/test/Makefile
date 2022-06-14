all: bpf_target go_target

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

bpf_target: test.bpf.c
	clang -g -O2 -c -target bpf -o test.bpf.o test.bpf.c -I../headers/libbpf/usr/include

go_target: test.bpf.o main.go
	CC=gcc CGO_CFLAGS="-I /usr/include/bpf" CGO_LDFLAGS="/usr/lib64/libbpf.a" go build -o libbpfgo-prog

clean:
	rm simple.bpf.o libbpfgo-prog vmlinux.h