//go:build ignore
//+build ignore
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define MAC_HEADER_SIZE 14;
#define member_address(source_struct, source_member)
    ({
        void* __ret;
        __ret = (void*) (((char*)source_struct) + offsetof(typeof(*source_struct), source_member));
        __ret;
    })
#define member_read(destination, source_struct, source_member)
  do{
    bpf_probe_read(
      destination,
      sizeof(source_struct->source_member),
      member_address(source_struct, source_member)
    );
  } while(0)

struct ipt_do_table_args
{
    struct sk_buff *skb;
    const struct nf_hook_state *state;
    struct xt_table *table;
    u64 start_ns;
};

struct {
	__uint(type,BPF_MAP_TYPE_HASH),
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct ipt_do_table_args);
} cur_ipt_do_table_args SEC(".maps");

int kprobe__ipt_do_table(struct pt_regs *ctx, struct sk_buff *skb, const struct nf_hook_state *state, struct xt_table *table)
{
    u32 pid = bpf_get_current_pid_tgid();

    struct ipt_do_table_args args = {
        .skb = skb,
        .state = state,
        .table = table,
    };

    args.start_ns = bpf_ktime_get_ns();
    cur_ipt_do_table_args.update(&pid, &args);

    return 0;
};

struct event_data_t {
    void  *skb;
    u32 pid;
    u32 hook;
    u32 verdict;
    u8  pf;
    u8  reserv[3];
    char table[XT_TABLE_MAXNAMELEN];
};

BPF_PERF_OUTPUT(open_events);

int kretprobe__ipt_do_table(struct pt_regs *ctx)
{
    struct ipt_do_table_args *args;
    u32 pid = bpf_get_current_pid_tgid();
    struct event_data_t evt = {};

    args = cur_ipt_do_table_args.lookup(&pid);
    if (args == 0)
        return 0;

    cur_ipt_do_table_args.delete(&pid);

    evt.pid = pid;
    evt.skb = args->skb;
    member_read(&evt.hook, args->state, hook);
    member_read(&evt.pf, args->state, pf);
    member_read(&evt.table, args->table, name);
    evt.verdict = PT_REGS_RC(ctx);

    open_events.perf_submit(ctx, &evt, sizeof(evt));
    return 0;
}