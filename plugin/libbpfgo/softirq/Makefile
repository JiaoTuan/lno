all: vmlinux.h bpf_target go_target

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

bpf_target: softirq.bpf.c
	clang -g -O2 -c -target bpf -o softirq.bpf.o softirq.bpf.c

go_target: softirq.bpf.o main.go
	CC=gcc CGO_CFLAGS="-I/home/codenovic/Downloads/libbpf/src/root/usr/include" CGO_LDFLAGS="/home/codenovic/Downloads/libbpf/src/root/usr/lib64/libbpf.a" go build -o libbpfgo-prog

clean:
	rm softirq.bpf.o libbpfgo-prog vmlinux.h 
