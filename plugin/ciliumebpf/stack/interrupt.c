// +build ignore
#include "vmlinux.h"
#include "bpf_helpers.h"
#include "bpf_tracing.h"
#include "bpf_endian.h"
//#include <bpf/trace_helpers.c>
//#define MAX_STACK_RAWTP 100

#define PERF_MAX_STACK_DEPTH         10
struct intr_message
{
    unsigned long vector;
    u32 pid;
    // int kern_stack_size;
	// int user_stack_size;
    // char kern_stack[MAX_STACK_RAWTP];
	// u64 user_stack[MAX_STACK_RAWTP];
	// struct bpf_stack_build_id user_stack_buildid[MAX_STACK_RAWTP];
    u64 kernelstack[PERF_MAX_STACK_DEPTH];
};

//typedef __u64 stack_trace_t[PERF_MAX_STACK_DEPTH];
struct {
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(max_entries, 16);
	__uint(key_size, sizeof(uint32_t));
	__uint(value_size, PERF_MAX_STACK_DEPTH*sizeof(unsigned long));
} stack_traces SEC(".maps");

struct bpf_map_def SEC("maps") events = {
    .type = BPF_MAP_TYPE_RINGBUF,
	.max_entries = 1<<24,
};

SEC("kprobe/do_error_trap")
int kprobe__do_error_trap(struct pt_regs *ctx)
{
    struct intr_message *intr_mess;
    unsigned long trapnr = PT_REGS_PARM4(ctx);
    bpf_printk("wocao %d %d %d",trapnr,sizeof(u64),sizeof(u32));
    u32 pid = bpf_get_current_pid_tgid()>>32;
    //max_len = MAX_STACK_RAWTP * sizeof(__u64);

    intr_mess = bpf_ringbuf_reserve(&events, sizeof(struct intr_message), 0);
	if (!intr_mess) {
		return 0;
	}

    bpf_get_stack(ctx,intr_mess->kernelstack,PERF_MAX_STACK_DEPTH*sizeof(unsigned long),0);
    //bpf_get_current_comm(&intr_mess->comm, sizeof(intr_mess->comm));
    // intr_mess->kern_stack_size = bpf_get_stack(ctx, intr_mess->kern_stack,
	// 				      max_len, 0);
	// intr_mess->user_stack_size = bpf_get_stack(ctx, intr_mess->user_stack, max_len,
	// 				    BPF_F_USER_STACK);
    //bpf_get_stack(ctx,&stack_traces,PERF_MAX_STACK_DEPTH,BPF_F_SKIP_FIELD_MASK);
    // bpf_get_stackid(ctx,&stack_traces,0);
    bpf_printk("wocao %d",pid);
    // bpf_probe_read(&intr_mess->pid,sizeof(u32),&pid);
    intr_mess->pid = pid;
	intr_mess ->vector = trapnr;
	bpf_ringbuf_submit(intr_mess, 0);
    return 0;
}

char __license[] SEC("license") = "Dual MIT/GPL";