//go:build ignore
//+build ignore
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>


#define MAX_SLOTS	20

const volatile bool targ_dist = false;
const volatile bool targ_ns = false;

struct hist {
	__u32 slots[MAX_SLOTS];
};
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, u64);
} start SEC(".maps");

__u64 counts[NR_SOFTIRQS] = {};
struct hist hists[NR_SOFTIRQS] = {};

static __always_inline u64 log2(u32 v)
{
	u32 shift, r;

	r = (v > 0xFFFF) << 4; v >>= r;
	shift = (v > 0xFF) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	r |= (v >> 1);

	return r;
}

static __always_inline u64 log2l(u64 v)
{
	u32 hi = v >> 32;

	if (hi)
		return log2(hi) + 32;
	else
		return log2(v);
}

SEC("tp_btf/softirq_entry")
int btf_raw_tracepoint__softirq_entry(u64 *ctx)
{
    u64 ts = bpf_ktime_get_ns();
    u32 key = 0;

    bpf_map_update_elem(&start,&key,&ts,0);
    return 0;
}

SEC("tp_btf/softirq_exit")
int btf_raw_tracepoint__softirq_exit(u64 vec_nr)
{
    u32 key = 0;
	u64 delta;
	u64 *tsp;
    if (vec_nr >= NR_SOFTIRQS)
		return 0;
    tsp = bpf_map_lookup_elem(&start, &key);
    if (!tsp || !*tsp)
		return 0;
	delta = bpf_ktime_get_ns() - *tsp;
	if (delta < 0)
		return 0;
	if (!targ_ns)
		delta /= 1000U;
    char fmt[] = "the vec_nr is :%lld\n";
    bpf_trace_printk(fmt,sizeof(fmt),vec_nr);
	// if (!targ_dist) {
	// 	__sync_fetch_and_add(&counts[0], delta);
	// } //else {
	// 	struct hist *hist;
	// 	u64 slot;

	// 	hist = &hists[vec_nr];
	// 	slot = log2(delta);
	// 	if (slot >= MAX_SLOTS)
	// 		slot = MAX_SLOTS - 1;
	// 	__sync_fetch_and_add(&hist->slots[slot], 1);
	// }
	return 0;
}
char _license[] SEC("license") = "GPL";