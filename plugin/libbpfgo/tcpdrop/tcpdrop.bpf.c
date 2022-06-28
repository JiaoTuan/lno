//go:build ignore
//+build ignore
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include <asm-generic/errno.h>
#include <bpf/bpf_endian.h>
#define PERF_MAX_STACK_DEPTH         127
typedef __u64 stack_trace_t[PERF_MAX_STACK_DEPTH];
struct {
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(max_entries, 16);
	__uint(key_size, sizeof(uint32_t));
	__uint(value_size, sizeof(stack_trace_t));
} stack_traces SEC(".maps");
// separate data structs for ipv4 and ipv6
struct ipv4_data_t {
    u32 pid;
    u64 ip;
    u32 saddr;
    u32 daddr;
    u16 sport;
    u16 dport;
    u8 state;
    u8 tcpflags;
    u32 stack_id;
};
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} ipv4_events SEC(".maps");

struct ipv6_data_t {
    u32 pid;
    u64 ip;
    unsigned __int128 saddr;
    unsigned __int128 daddr;
    u16 sport;
    u16 dport;
    u8 state;
    u8 tcpflags;
    u32 stack_id;
};
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} ipv6_events SEC(".maps");

// static struct tcphdr *skb_to_tcphdr(const struct sk_buff *skb)
// {
//     // unstable API. verify logic in tcp_hdr() -> skb_transport_header().
//     //return (struct tcphdr *)(skb->head + skb->transport_header);
//     struct tcphdr *tcp = skb->head + skb->transport_header;
//     if (tcp == NULL)
//         return 0;
//     return tcp;
// }
// static inline struct iphdr *skb_to_iphdr(const struct sk_buff *skb)
// {
//     // unstable API. verify logic in ip_hdr() -> skb_network_header().
//     return (struct iphdr *)(skb->head + skb->network_header);
// }

SEC("kprobe/tcp_drop")
int kprobe__tcp_drop(struct pt_regs *ctx)
{
    struct sock *sk = (struct sock *)ctx->di;
    struct sk_buff *skb =(struct sk_buff *)ctx->si;
    // unsigned char *head =(unsigned char *) skb->head;
    if (sk == NULL && skb == NULL &&skb->head == NULL)
        return 0;
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    // pull in details from the packet headers and the sock struct
    u16 family = 0;
    unsigned char state = 0;
    u16 sport = 0, dport = 0;
    unsigned char *head = 0;
    unsigned short tp;

    //BPF_CORE_READ_INTO(&family,sk,__sk_common.skc_family);
    bpf_probe_read_kernel(&family,sizeof(family),&sk->__sk_common.skc_family);
    if(family != 2&&family!=30)
         return 0;
    BPF_CORE_READ_INTO(&state,sk,__sk_common.skc_state);
   //struct tcphdr *tcp =(struct tcphdr *)(skb->head + skb->transport_header);
    bpf_probe_read_kernel(&family,sizeof(family),&sk->__sk_common.skc_family);
    BPF_CORE_READ_INTO(&head, skb, head); 
    
    BPF_CORE_READ_INTO(&tp, skb, transport_header);
    struct tcphdr *tcp = (struct tcphdr *)(head + tp);
    if(tcp ==NULL)
        return 0;
    // struct iphdr *ip = skb_to_iphdr(skb);
    //struct iphdr *ip =(struct iphdr *)(skb->head + skb->network_header);
    // u8 tcpflags = ((u_int8_t *)tcp)[13];
    //sport = tcp->source;
    bpf_probe_read_kernel(&sport,sizeof(sport),&tcp->source);
    //dport = tcp->dest;
    bpf_probe_read_kernel(&dport,sizeof(dport),&tcp->dest);
    sport = bpf_ntohs(sport);
    dport = bpf_ntohs(dport);
    
    //BPF_CORE_READ_INTO(&family,sk,__sk_common.skc_family);
    //family = sk->__sk_common.skc_family;
    
    // BPF_CORE_READ_INTO(&family,sk,__sk_common.skc_family);

    //
        struct ipv4_data_t data4 = {};
        data4.pid = pid;
        data4.ip = 4;
    //    data4.saddr = ip->saddr;
      //  data4.daddr = ip->daddr;
        data4.dport = dport;
        data4.sport = sport;
        data4.state = state;
        //data4.tcpflags = tcpflags;
        data4.stack_id = bpf_get_stackid(ctx,&stack_traces,0);
        bpf_ringbuf_output(&ipv4_events,&data4,sizeof(data4),0);
  //  } else if (family == 30) {
    //     struct ipv6_data_t data6 = {};
    //     data6.pid = pid;
    //     data6.ip = 6;
    //     // The remote address (skc_v6_daddr) was the source
    //     bpf_probe_read_kernel(&data6.saddr, sizeof(data6.saddr),
    //         sk->__sk_common.skc_v6_daddr.in6_u.u6_addr32);
    //     // The local address (skc_v6_rcv_saddr) was the destination
    //     bpf_probe_read_kernel(&data6.daddr, sizeof(data6.daddr),
    //         sk->__sk_common.skc_v6_rcv_saddr.in6_u.u6_addr32);
    //     data6.dport = dport;
    //     data6.sport = sport;
    //     data6.state = state;
    //     data6.tcpflags = tcpflags;
    //     data6.stack_id = bpf_get_stackid(ctx,&stack_traces,0);
    //     bpf_ringbuf_output(&ipv6_events,&data6,sizeof(data6),0);
    // }
    // else drop
    return 0;
}

char LICENSE[] SEC("license") = "GPL";