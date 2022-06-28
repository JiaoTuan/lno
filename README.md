# lno
Linux Network Observability with eBPF


# Bug

## 网络字节序转主机字节序时：
bpf_ntohs

## GPL协议：

family = sk->__sk_common.skc_family; x
BPF_CORE_READ_INTO(&family,sk,__sk_common.skc_family); x
bpf_probe_read_kernel(&data6.saddr, sizeof(data6.saddr);x


#define bpf_core_read(dst, sz, src)					    \
	bpf_probe_read_kernel(dst, sz, (const void *)__builtin_preserve_access_index(src))

u64 __weak bpf_probe_read_kernel(void *dst, u32 size, const void *unsafe_ptr)
{
	memset(dst, 0, size);
	return -EFAULT;
}

## kprobe无法使用指针内容赋值
参见tcpdrop.bpf.c