//go:build ignore
//+build ignore
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <net/sock.h>
struct Key {
	u32 src_ip;               //source ip
	u32 dst_ip;               //destination ip
	unsigned short src_port;  //source port
	unsigned short dst_port;  //destination port
};

struct Leaf {
	int timestamp;            //timestamp in ns
};

struct {
    __uint(type,BPF_MAP_TYPE_HASH),
    __uint(max_entries, 1);
	__type(key, struct Key);
	__type(value, struct Leaf);
 }sesions SEC(".maps");

#define cursor_advance(_cursor, _len) \
        ({ void *_tmp = _cursor; _cursor += _len; _tmp; })

/*BPF_PROG_TYPE_SOCKET_FILTER*/
SEC("socket")
int socket_filter__http_filter(struct __sk_buff *skb)
{
    u8 *cursor = 0;
    // struct ethhdr *ethernet = cursor_advance(cursor,sizeof(*ethernet));
    // struct ethhdr *ethernet = skb->data + sizeof(*ethhdr);
	struct ethhdr *ethernet = skb->data;
    if (!(ethernet->type == 0x0800)) {
		goto DROP;
	}

    //struct iphdr *ip = cursor_advance(cursor, sizeof(*ip));
	struct iphdr *ip = skb->data + sizeof(struct ethhdr);
	//filter TCP packets (ip next protocol = 0x06)
	if (ip->nextp != IP_TCP) {
		goto DROP;
	}

    u32  tcp_header_length = 0;
	u32  ip_header_length = 0;
	u32  payload_offset = 0;
	u32  payload_length = 0;
	struct Key 	key;
	struct Leaf zero = {0};

    //calculate ip header length
    //value to multiply * 4
    //e.g. ip->hlen = 5 ; IP Header Length = 5 x 4 byte = 20 byte
    ip_header_length = ip->ihl << 2;    //SHL 2 -> *4 multiply
	//check ip header length against minimum
    if (ip_header_length < sizeof(*ip)) {
        goto DROP;
    }

	struct tcphdr *tcp = skb->data + sizeof(struct ethhdr) + sizeof(struct iphhdr);
	//retrieve ip src/dest and port src/dest of current packet
	//and save it into struct Key
	key.dst_ip = ip->daddr;
	key.src_ip = ip->saddr;
	key.dst_port = tcp->dest;
	key.src_port = tcp->source;
	//calculate tcp header length
	//value to multiply *4
	//e.g. tcp->offset = 5 ; TCP Header Length = 5 x 4 byte = 20 byte
	tcp_header_length = tcp->doff << 2; //SHL 2 -> *4 multiply

	//calculate payload offset and package data length
	payload_offset = ETH_HLEN + ip_header_length + tcp_header_length;
	payload_length = ip->tot_len - ip_header_length - tcp_header_length;

	//http://stackoverflow.com/questions/25047905/http-request-minimum-size-in-bytes
	//minimum length of http request is always geater than 7 bytes
	//avoid invalid access memory
	//include empty payload
	if(payload_length < 7) {
		return 0;
	}
	unsigned long p[7];
	int i = 0;
	for (i = 0; i < 7; i++) {
		p[i] = load_byte(skb, payload_offset + i);
	}

	//find a match with an HTTP message
	//HTTP
	if ((p[0] == 'H') && (p[1] == 'T') && (p[2] == 'T') && (p[3] == 'P')) {
		goto HTTP_MATCH;
	}
	//GET
	if ((p[0] == 'G') && (p[1] == 'E') && (p[2] == 'T')) {
		goto HTTP_MATCH;
	}
	//POST
	if ((p[0] == 'P') && (p[1] == 'O') && (p[2] == 'S') && (p[3] == 'T')) {
		goto HTTP_MATCH;
	}
	//PUT
	if ((p[0] == 'P') && (p[1] == 'U') && (p[2] == 'T')) {
		goto HTTP_MATCH;
	}
	//DELETE
	if ((p[0] == 'D') && (p[1] == 'E') && (p[2] == 'L') && (p[3] == 'E') && (p[4] == 'T') && (p[5] == 'E')) {
		goto HTTP_MATCH;
	}
	//HEAD
	if ((p[0] == 'H') && (p[1] == 'E') && (p[2] == 'A') && (p[3] == 'D')) {
		goto HTTP_MATCH;
	}
	//keep the packet and send it to userspace returning -1
HTTP_MATCH:
	//if not already present, insert into map <Key, Leaf>
	sessions.lookup_or_try_init(&key,&zero);
	return 0;
}