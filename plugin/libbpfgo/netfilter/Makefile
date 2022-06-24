all: vmlinux.h bpf_target go_target

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

bpf_target: netfilter.bpf.c
	clang -g -O2 -c -target bpf -o netfilter.bpf.o netfilter.bpf.c

go_target: netfilter.bpf.o main.go
	CC=gcc CGO_CFLAGS="-I/home/codenovic/Downloads/libbpf/src/root/usr/include" CGO_LDFLAGS="/home/codenovic/Downloads/libbpf/src/root/usr/lib64/libbpf.a" go build -o netfilter

clean:
	rm netfilter.bpf.o netfilter vmlinux.h 
