/* $OpenBSD: nm1.C,v 1.2 2017/02/07 12:57:12 bluhm Exp $ */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define D(T)	void witness_##T(T) {}

D(cpuid_t)
D(dev_t)
D(fixpt_t)
D(fsblkcnt_t)
D(gid_t)
D(id_t)
D(in_addr_t)
D(in_port_t)
D(ino_t)
D(key_t)
D(mode_t)
D(nlink_t)
D(pid_t)
D(rlim_t)
D(sa_family_t)
D(segsz_t)
D(socklen_t)
D(suseconds_t)
D(swblk_t)
D(uid_t)
D(uint64_t)
D(uint32_t)
D(size_t)
D(off_t)
D(useconds_t)
