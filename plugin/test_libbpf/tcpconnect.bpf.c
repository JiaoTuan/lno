#include<vmlinux.h>

#include<bpf/bpf_helpers.h>
#include<bpf/bpf_core_read.h>
#include<bpf/bpf_tracing.h>

#include "maps.bpf.h"
#include "tcpconnect.h"

// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 Anton Protopopov
#ifndef __TCPCONNECT_H
#define __TCPCONNECT_H

/* The maximum number of items in maps */
#define MAX_ENTRIES 8192

/* The maximum number of ports to filter */
#define MAX_PORTS 64

#define TASK_COMM_LEN 16

struct ipv4_flow_key {
	__u32 saddr;
	__u32 daddr;
	__u16 dport;
};

struct ipv6_flow_key {
	__u8 saddr[16];
	__u8 daddr[16];
	__u16 dport;
};

struct event {
	union {
		__u32 saddr_v4;
		__u8 saddr_v6[16];
	};
	union {
		__u32 daddr_v4;
		__u8 daddr_v6[16];
	};
	char task[TASK_COMM_LEN];
	__u64 ts_us;
	__u32 af; // AF_INET or AF_INET6
	__u32 pid;
	__u32 uid;
	__u16 dport;
};

#endif /* __TCPCONNECT_H */

SEC(".rodata") int filter_ports[MAX_PORTS];
const volatile int filter_ports_len = 0;
const volatile uid_t filter_uid = -1;
const volatile pid_t filter_pid = 0;
const volatile bool do_count = 0;
/* Define here, because there are conflicts with include files */
#define AF_INET		2
#define AF_INET6	10

struct {
    __uint(type,BPF_MAP_TYPE_HASH);
    __uint(max_entries,MAX_ENTRIES);
    __type(key,u32);
    __type(value,struct sock *);
    __uint(map_flags,BPF_F_NO_PREALLOC);
}socket SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, struct ipv4_flow_key);
	__type(value, u64);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} ipv4_count SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, MAX_ENTRIES);
	__type(key, struct ipv6_flow_key);
	__type(value, u64);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} ipv6_count SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events SEC(".maps");

static __alwyays_inline bool filter_ports(__u16 port)
{
    int i;
    if(filter_ports_len == 0)
        return false;
    for(i=0;i<filter_ports_len;i++){
        if(port == filter_ports[i])
            return false;
    }
    return true;
}

static __alwyays_inline int enter_tcp_connect(struct pt_regs *ctx,struct sock *sk){
    __u64 pid_tgid = bpf_get_current_pid_tgid();
	__u32 pid = pid_tgid >> 32;
	__u32 tid = pid_tgid;
	__u32 uid;

    if (filter_pid && pid != filter_pid)
		return 0;

    uid = bpf_get_current_uid_gid();
	if (filter_uid != (uid_t) -1 && uid != filter_uid)
		return 0;

	bpf_map_update_elem(&sockets, &tid, &sk, 0);
	return 0;
}

static __alwyays_inline void count_v4(struct sock *sk,__u16 dport){
    struct ipv4_flow_key key = {};
    static __u64 zero;
    __u64 *val;

    
}