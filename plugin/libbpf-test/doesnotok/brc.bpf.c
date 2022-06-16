#include "vmlinux.h"	// 必须放在首位

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>

#include "bpf_helpers.h"
#include "brc_common.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";
// tcp头长度 表示tcp头包含多少个32位的字
#define tcp_hdrlen(tcp) (tcp->doff * 4)
#define ipv4_hdrlen(ip) (ip->ihl * 4)
// do while0 保证宏定义的使用者能无编译错误的使用宏
// #define ensure_header(skb, var_off, const_off, hdr) do{	\
// 	u32 len = const_off + sizeof(*hdr);			\
// 	void *data = (void *)(long)skb->data + var_off;		\
// 	void *data_end = (void *)(long)skb->data_end;		\
// 								\
// 	if (data + len > data_end)				\
// 		bpf_skb_pull_data(skb, var_off + len);		\
// 								\
// 	data = (void *)(long)skb->data + var_off;		\
// 	data_end = (void *)(long)skb->data_end;			\
// 	if (data + len > data_end)				\
// 		return 0;				\
// 								\
// 	hdr = (void *)(data + const_off);			\
// } while(0)


// 尾调用结构map
struct {
	__uint(type, BPF_MAP_TYPE_PROG_ARRAY);
	__uint(max_entries, RECURSION_UPPER_LIMIT);
	__type(key, u32);
	__type(value, u32);
} tc_progs SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, u32);
	__type(value, struct brc_cache_entry);
	__uint(max_entries, BRC_CACHE_ENTRY_COUNT);
} map_cache SEC(".maps");