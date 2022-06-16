#include <errno.h>
static const char *a_records_map_path = "/sys/fs/bpf/xdp/globals/xdns_a_records";

int get_map_fd(const char *map_path)
{
    int fd = bpf_obj_get(map_path);
    if (fd < 0)
    {
        if (errno == EACCES)
        {
            printf("ERROR: Permission denied while trying to access %s\n", map_path);
        }
        else if (errno == ENOENT)
        {
            printf("ERROR: Could not find BPF maps. Load XDP program with iproute2 first.\n");
        }
        else
        {
            printf("ERROR: BPF map error: %d (%s)\n", errno, strerror(errno));
        }
    }
    return fd;
}

int main(int argc,char **argv)
{
    int ret = EINVAL;

    int a_records_fd;
    a_records_fd = get_map_fd(a_records_map_path);
    if(a_records_fd < 0){
        return EXIT_FAILURE;
    }

    if (argc == 2)
    {
        if (strcmp(argv[1], "list") == 0)
        {
            struct dns_query key, next_key;
            struct a_record value;
            int res = -1;
            while (bpf_map_get_next_key(a_records_fd, &key, &next_key) == 0)
            {
                res = bpf_map_lookup_elem(a_records_fd, &next_key, &value);
                if (res > -1)
                {
                    char new_dns_name[strnlen(next_key.name, MAX_DNS_NAME_LENGTH)];
                    replace_length_octets_with_dots(next_key.name, new_dns_name);
                    printf("A %s %s %i\n", new_dns_name, inet_ntoa(value.ip_addr), value.ttl);
                }
                key = next_key;
            }
            ret = 0;
        }
    }
    else if (argc == 5 || argc == 6)
    {
        if (strcmp(argv[1], "add") == 0 || strcmp(argv[1], "remove") == 0)
        {
            struct in_addr ip_addr;

            //Check for 'A' record
            if (strcmp(argv[2], "a") == 0 || strcmp(argv[2], "A") == 0)
            {
                if (inet_aton(argv[4], &ip_addr) == 0)
                {
                    printf("ERROR: Invalid IP address\n");
                    ret = EINVAL;
                    return ret;
                }

            } else {
                printf("ERROR: %s is not a DNS record type.\n", argv[2]);
                ret = EINVAL;   
                return ret;
            }

            //Create a new dns_name char array
            char new_dns_name[MAX_DNS_NAME_LENGTH];
            //Zero fill the new_dns_name
            memset(&new_dns_name, 0, sizeof(new_dns_name));
            replace_dots_with_length_octets(argv[3], new_dns_name);

            struct dns_query dns;
            dns.class = DNS_CLASS_IN;
            dns.record_type = A_RECORD_TYPE;
            memcpy(dns.name, new_dns_name, sizeof(new_dns_name));

            if (strcmp(argv[1], "add") == 0)
            {
                struct a_record a;
                a.ip_addr = ip_addr;
                if(argc == 5){
                    a.ttl = 0;
                } else {
                    a.ttl = (uint32_t)atoi(argv[5]);
                }
                bpf_map_update_elem(a_records_fd, &dns, &a, BPF_ANY);
                printf("DNS record added\n");
                ret = 0;
            }
            else if (strcmp(argv[1], "remove") == 0)
            {
                if (bpf_map_delete_elem(a_records_fd, &dns) == 0)
                {
                    printf("DNS record removed\n");
                    ret = 0;
                }
                else
                {
                    printf("DNS record not found\n");
                    ret = ENOENT;
                }
            }
        }
    }
    if(ret != 0)
        usage(argv[0]);

    return ret;
}