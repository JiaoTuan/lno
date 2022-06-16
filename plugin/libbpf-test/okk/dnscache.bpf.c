// +build ignore
#include "vmlinux.h"
#include <iproute2/bpf_elf.h>
#include <string.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#define A_RECORD_TYPE 0x0001
#define DNS_CLASS_IN 0x0001
//RFC1034: the total number of octets that represent a domain name is limited to 255.
//We need to be aligned so the struct does not include padding bytes. We'll set the length to 256.
//Otherwise padding bytes will generate problems with the verifier, as it ?could contain arbitrary data from memory?
#define MAX_DNS_NAME_LENGTH 256

struct dns_hdr
{
    uint16_t transaction_id;
    uint8_t rd : 1;      //Recursion desired
    uint8_t tc : 1;      //Truncated
    uint8_t aa : 1;      //Authoritive answer
    uint8_t opcode : 4;  //Opcode
    uint8_t qr : 1;      //Query/response flag
    uint8_t rcode : 4;   //Response code
    uint8_t cd : 1;      //Checking disabled
    uint8_t ad : 1;      //Authenticated data
    uint8_t z : 1;       //Z reserved bit
    uint8_t ra : 1;      //Recursion available
    uint16_t q_count;    //Number of questions
    uint16_t ans_count;  //Number of answer RRs
    uint16_t auth_count; //Number of authority RRs
    uint16_t add_count;  //Number of resource RRs
};

#ifdef EDNS
struct ar_hdr {
    uint8_t name;
    uint16_t type;
    uint16_t size;
    uint32_t ex_rcode;
    uint16_t rcode_len;
} __attribute__((packed));
#endif

//Used as key in our hashmap
struct dns_query {
    uint16_t record_type;
    uint16_t class;
    char name[MAX_DNS_NAME_LENGTH];
};

//Used as a generic DNS response
struct dns_response {
   uint16_t query_pointer;
   uint16_t record_type;
   uint16_t class;
   uint32_t ttl;
   uint16_t data_length;
} __attribute__((packed));

//Used as value of our A record hashmap
struct a_record {
    struct in_addr ip_addr;
    uint32_t ttl;
};

struct{
    __uint(type, BPF_MAP_TYPE_HASH);
	__type(key, sizeof(struct dns_query));
	__type(value, sizeof(struct a_record));
	__uint(max_entries, 65536);
}xdns_a_records SEC("maps");

static char dns_buffer[512];

static int parse_query(struct xdp_md *ctx,void *query_start,struct dns_query *q){
    void *data_end = (void *)(long)ctx->data_end;
    #ifdef DEBUG
    bpf_printk("Parsing query");
    #endif
    uint16_t i;
    void *cursor = query_start;
    int namepos = 0;
    
    /* 缓冲区初始化为0 */
    //Fill dns_query.name with zero bytes
    //Not doing so will make the verifier complain when dns_query is used as a key in bpf_map_lookup
    memset(&q->name[0], 0, sizeof(q->name));
    //Fill record_type and class with default values to satisfy verifier
    q->record_type = 0;
    q->class = 0;

    //We create a bounded loop of MAX_DNS_NAME_LENGTH (maximum allowed dns name size).
    //We'll loop through the packet byte by byte until we reach '0' in order to get the dns query name
    for (i = 0; i < MAX_DNS_NAME_LENGTH; i++)
    {
        //Boundary check of cursor. Verifier requires a +1 here. 
        //Probably because we are advancing the pointer at the end of the loop
        if (cursor + 1 > data_end)
        {
            #ifdef DEBUG
            bpf_printk("Error: boundary exceeded while parsing DNS query name");
            #endif
            break;
        }

        /*
        #ifdef DEBUG
        bpf_printk("Cursor contents is %u\n", *(char *)cursor);
        #endif
        */

        //If separator is zero we've reached the end of the domain query
        if (*(char *)(cursor) == 0)
        {

            //We've reached the end of the query name.
            //This will be followed by 2x 2 bytes: the dns type and dns class.
            if (cursor + 5 > data_end)
            {
                #ifdef DEBUG
                bpf_printk("Error: boundary exceeded while retrieving DNS record type and class");
                #endif
            }
            else
            {
                q->record_type = bpf_htons(*(uint16_t *)(cursor + 1));
                q->class = bpf_htons(*(uint16_t *)(cursor + 3));
            }

            //Return the bytecount of (namepos + current '0' byte + dns type + dns class) as the query length.
            return namepos + 1 + 2 + 2;
        }

        //Read and fill data into struct
        q->name[namepos] = *(char *)(cursor);
        namepos++;
        cursor++;
    }

    return -1;
}


static int match_a_records(struct xdp_md *ctx,struct dns_query *q,struct a_record *a){
    #ifdef DEBUG
    bpf_printk("DNS record type: %i", q->record_type);
    bpf_printk("DNS class: %i", q->class);
    bpf_printk("DNS name: %s", q->name);
    #endif

    struct a_record *record;
    #ifdef BCC_SEC
    record = xdns_a_records.lookup(q);
    #else
    record = bpf_map_lookup_elem(&xdns_a_records, q);
    #endif

    if(record > 0){
        #ifdef DEBUG
        bpf_printk("DNS query matched");
        #endif
        a->ip_addr = record->ip_addr;
        a->ttl = record->ttl;

        return 0;
    }

    return -1;

}
// ?？？？ 将dns包修改为响应错误
static inline void modify_dns_header_response(struct dns_hdr *dns_hdr)
{
    //Set query response
    dns_hdr->qr = 1;
    //Set truncated to 0
    //dns_hdr->tc = 0;
    //Set authorative to zero
    //dns_hdr->aa = 0;
    //Recursion available
    dns_hdr->ra = 1;
    //One answer
    dns_hdr->ans_count = bpf_htons(1);
}

// create dns buffer as dns_buffer
static void create_query_response(struct a_record *a, char *dns_buffer, size_t *buf_size)
{
    //Formulate a DNS response. Currently defaults to hardcoded query pointer + type a + class in + ttl + 4 bytes as reply.
    struct dns_response *response = (struct dns_response *) &dns_buffer[0];
    response->query_pointer = bpf_htons(0xc00c);
    response->record_type = bpf_htons(0x0001);
    response->class = bpf_htons(0x0001);
    response->ttl = bpf_htonl(a->ttl);
    response->data_length = bpf_htons((uint16_t)sizeof(a->ip_addr));
    *buf_size += sizeof(struct dns_response);
    //Copy IP address
    __builtin_memcpy(&dns_buffer[*buf_size], &a->ip_addr, sizeof(struct in_addr));
    *buf_size += sizeof(struct in_addr);
}

//__builtin_memcpy only supports static size_t
//The following function is a memcpy wrapper that uses __builtin_memcpy when size_t n is known.
//Otherwise it uses our own naive & slow memcpy routine
static inline void copy_to_pkt_buf(struct xdp_md *ctx, void *dst, void *src, size_t n)
{
    //Boundary check
    if((void *)(long)ctx->data_end >= dst + n){
        int i;
        char *cdst = dst;
        char *csrc = src;

        //For A records, src is either 16 or 27 bytes, depending if OPT record is requested.
        //Use __builtin_memcpy for this. Otherwise, use our own slow, naive memcpy implementation.
        switch(n)
        {
            case 16:
                __builtin_memcpy(cdst, csrc, 16);
                break;
            
            case 27:
                __builtin_memcpy(cdst, csrc, 27);
                break;

            default:
                for(i = 0; i < n; i+=1)
                {
                    cdst[i] = csrc[i];
                }
        }
    }
}

static inline void swap_mac(uint8_t *src_mac, uint8_t *dst_mac)
{
    int i;
    for (i = 0; i < 6; i++)
    {
        uint8_t tmp_src;
        tmp_src = *(src_mac + i);
        *(src_mac + i) = *(dst_mac + i);
        *(dst_mac + i) = tmp_src;
    }
}

//Update IP checksum for IP header, as specified in RFC 1071
//The checksum_location is passed as a pointer. At this location 16 bits need to be set to 0.
static inline void update_ip_checksum(void *data, int len, uint16_t *checksum_location)
{
    uint32_t accumulator = 0;
    int i;
    for (i = 0; i < len; i += 2)
    {
        uint16_t val;
        //If we are currently at the checksum_location, set to zero
        if (data + i == checksum_location)
        {
            val = 0;
        }
        else
        {
            //Else we load two bytes of data into val
            val = *(uint16_t *)(data + i);
        }
        accumulator += val;
    }

    //Add 16 bits overflow back to accumulator (if necessary)
    uint16_t overflow = accumulator >> 16;
    accumulator &= 0x00FFFF;
    accumulator += overflow;

    //If this resulted in an overflow again, do the same (if necessary)
    accumulator += (accumulator >> 16);
    accumulator &= 0x00FFFF;

    //Invert bits and set the checksum at checksum_location
    uint16_t chk = accumulator ^ 0xFFFF;

    #ifdef DEBUG
    bpf_printk("Checksum: %u", chk);
    #endif

    *checksum_location = chk;
}

SEC("prog")
int xdp_dns(struct xdp_md *ctx)
{
    #ifdef DEBUG
    uint64_t start = bpf_ktime_get_ns();
    #endif

    void *data_end = (void *)(unsigned long)ctx->data_end;
    void *data = (void *)(unsigned long)ctx->data;
    
    //Boundary check: check if packet is larger than a full ethernet + ip header
    if (data + sizeof(struct ethhdr) + sizeof(struct iphdr) > data_end)
    {
        return XDP_PASS;
    }

    struct ethhdr *eth = data;
    //Ignore packet if ethernet protocol is not IP-based
    if (eth->h_proto != bpf_htons(0x0800))
    {
        return XDP_PASS;
    }
    struct iphdr *ip = data + sizeof(*eth);

    if (ip->protocol == IPPROTO_UDP)
    {
        struct udphdr *udp;
        //Boundary check for UDP
        if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp) > data_end)
        {
            return XDP_PASS;
        }

        udp = data + sizeof(*eth) + sizeof(*ip);
        //Check if dest port equals 53
        if (udp->dest == bpf_htons(53))
        {
            #ifdef DEBUG
            bpf_printk("Packet dest port 53");
            bpf_printk("Data pointer starts at %u", data);
            #endif

            //Boundary check for minimal DNS header
            if (data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp) + sizeof(struct dns_hdr) > data_end)
            {
                return XDP_PASS;
            }
            
            struct dns_hdr *dns_hdr = data + sizeof(*eth) + sizeof(*ip) + sizeof(*udp);
            //Check if header contains a standard query:标准请求查询
            if (dns_hdr->qr == 0 && dns_hdr->opcode == 0)
            {
                #ifdef DEBUG
                bpf_printk("DNS query transaction id %u", bpf_ntohs(dns_hdr->transaction_id));
                #endif

                //Get a pointer to the start of the DNS query
                void *query_start = (void *)dns_hdr + sizeof(struct dns_hdr);

                //We will only be parsing a single query for now
                struct dns_query q;
                int query_length = 0;
                query_length = parse_query(ctx, query_start, &q);
                if (query_length < 1)
                {
                    return XDP_PASS;
                }

                //Check if query matches a record in our hash table
                struct a_record a_record;
                int res = match_a_records(ctx, &q, &a_record);

                //If query matches... 如果有附加记录
                if (res == 0)
                {
                    size_t buf_size = 0;

                    //Change DNS header to a valid response header
                    modify_dns_header_response(dns_hdr);

                    //Create DNS response and add to temporary buffer.
                    create_query_response(&a_record, &dns_buffer[buf_size], &buf_size);
    
                    #ifdef EDNS
                    //If an additional record is present 如果请求包中有附加记录
                    if(dns_hdr->add_count > 0)
                    {
                        //Parse AR record
                        struct ar_hdr ar;
                        if(parse_ar(ctx, dns_hdr, query_length, &ar) != -1)
                        {     
                            //Create AR response and add to temporary buffer
                            create_ar_response(&ar, &dns_buffer[buf_size], &buf_size);
                        }
                    }
                    #endif

                    //Start our response [query_length] bytes beyond the header
                    void *answer_start = (void *)dns_hdr + sizeof(struct dns_hdr) + query_length;
                    //Determine increment of packet buffer
                    int tailadjust = answer_start + buf_size - data_end;

                    //Adjust packet length accordingly
                    if (bpf_xdp_adjust_tail(ctx, tailadjust))
                    {
                        #ifdef DEBUG
                        bpf_printk("Adjust tail fail");
                        #endif
                    }
                    else
                    {
                        //Because we adjusted packet length, mem addresses might be changed.
                        //Reinit pointers, as verifier will complain otherwise.
                        data = (void *)(unsigned long)ctx->data;
                        data_end = (void *)(unsigned long)ctx->data_end;

                        //Copy bytes from our temporary buffer to packet buffer
                        copy_to_pkt_buf(ctx, data + sizeof(struct ethhdr) +
                                sizeof(struct iphdr) +
                                sizeof(struct udphdr) +
                                sizeof(struct dns_hdr) +
                                query_length,
                            &dns_buffer[0], buf_size);

                        eth = data;
                        ip = data + sizeof(struct ethhdr);
                        udp = data + sizeof(struct ethhdr) + sizeof(struct iphdr);

                        //Do a new boundary check
                        if (data + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr) > data_end)
                        {
                            #ifdef DEBUG
                            bpf_printk("Error: Boundary exceeded");
                            #endif
                            return XDP_PASS;
                        }

                        //Adjust UDP length and IP length
                        uint16_t iplen = (data_end - data) - sizeof(struct ethhdr);
                        uint16_t udplen = (data_end - data) - sizeof(struct ethhdr) - sizeof(struct iphdr);
                        ip->tot_len = bpf_htons(iplen);
                        udp->len = bpf_htons(udplen);

                        //Swap eth macs
                        swap_mac((uint8_t *)eth->h_source, (uint8_t *)eth->h_dest);

                        //Swap src/dst IP
                        uint32_t src_ip = ip->saddr;
                        ip->saddr = ip->daddr;
                        ip->daddr = src_ip;

                        //Set UDP checksum to zero
                        udp->check = 0;

                        //Swap udp src/dst ports
                        uint16_t tmp_src = udp->source;
                        udp->source = udp->dest;
                        udp->dest = tmp_src;

                        //Recalculate IP checksum
                        update_ip_checksum(ip, sizeof(struct iphdr), &ip->check);

                        #ifdef DEBUG
                        bpf_printk("XDP_TX");
                        #endif

            
                        #ifdef DEBUG
                        uint64_t end = bpf_ktime_get_ns();
                        uint64_t elapsed = end-start;
                        bpf_printk("Time elapsed: %d", elapsed);
                        #endif

                        //Emit modified packet
                        return XDP_TX;
                    }
                }
            }
        }
    }
    return XDP_PASS;
}