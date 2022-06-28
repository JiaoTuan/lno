all: vmlinux.h bpf_target go_target

vmlinux.h:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

bpf_target: tcpdrop.bpf.c
	clang -g -O2 -c -target bpf -o tcpdrop.bpf.o tcpdrop.bpf.c

go_target: tcpdrop.bpf.o main.go
	CC=gcc CGO_CFLAGS="-I /usr/include" CGO_LDFLAGS="/usr/lib64/libbpf.a" go build -o tcpdrop

clean:
	rm tcpdrop.bpf.o tcpdrop vmlinux.h