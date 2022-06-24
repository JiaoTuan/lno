APPS = tcprtt
bpftool = $(shell which bpftool || ../tools/bpftool)
LIBBPF_SRC := $(abspath ../libbpf/src)
LIBBPF_OBJ := /usr/lib64/libbpf.a
INCLUDES := -Ilibbpf/usr/include -Ilibbpf/include/uapi -I/usr/include/x86_64-linux-gnu -I.

.PHONY: all
all: $(APPS)

$(APPS): %: %.bpf.c $(LIBBPF_OBJ) $(wildcard %.h)
	clang -g -O2 -target bpf -D__TARGET_ARCH_x86 $(INCLUDES) -c $@.bpf.c -o $@.bpf.o
	$(bpftool) gen skeleton $@.bpf.o > $@.skel.h
	clang -g -O2 -Wall $(INCLUDES) -c $@.c -o $@.o
	clang -Wall -O2 -g $@.o -static -lbpf -lelf -lz -o $@

vmlinux:
	$(bpftool) btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

libbpf: $(LIBBPF_OBJ)

format:
	VERSION_CONTROL=none indent -linux *.h *.c

clean:
	rm -rf $(APPS)*.o $(APPS)