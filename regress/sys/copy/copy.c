/*	$OpenBSD: copy.c,v 1.2 2005/04/17 07:37:10 tedu Exp $	*/

/* Written by Ted Unangst 2004 Public Domain */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <strings.h>
#include <errno.h>

#include <stdio.h>
#include <err.h>
#include <unistd.h>

int failure;

static void
fail(const char *str)
{
	fprintf(stderr, "%s\n", str);
	failure++;
}

int
main(int argc, char **argv)
{
 	char buf[4096];
 	void *goodbuf;
 	void *badbuf;
 	int mib[6];
 	struct kinfo_proc2 kinfo;
 	size_t kinfosize = sizeof(kinfo);
 	int s;
 	struct ifreq ifrdesc;


 	s = socket(AF_INET, SOCK_DGRAM, 0);
 	if (s == -1)
 		err(1, "socket");

 	mib[0] = CTL_KERN;
 	mib[1] = KERN_PROC2;
 	mib[2] = KERN_PROC_PID;
 	mib[3] = getpid();
 	mib[4] = sizeof(struct kinfo_proc2);
 	mib[5] = 1;

 	if (sysctl(mib, 6, &kinfo, &kinfosize, 0, 0))
 		err(1, "sysctl");


 	goodbuf = buf;
 	badbuf = (void*)(long)kinfo.p_paddr;

 	/* printf("goodbuf %p badbuf %p\n", goodbuf, badbuf); */

 	/* copyin */
 	if (!syscall(202, 0, 6, &kinfo, &kinfosize, 0, 0))
 		fail("copyin did not fail on 0 buf\n");
 	if (!syscall(202, badbuf, 6, &kinfo, &kinfosize, 0, 0))
 		fail("copyin did not fail on bad buf\n");

 	/* copyout */
 	if (statfs("/", goodbuf))
 		fail("copyout failed on a good buf\n");
 	if (!statfs("/", 0) || errno != EFAULT)
 		fail("copyout didn't fail on 0 buf\n");
 	if (!statfs("/", badbuf) || errno != EFAULT)
 		fail("copyout didn't fail on bad buf\n");

 	/* copyoutstr */
 	memset(&ifrdesc, 0, sizeof(ifrdesc));
 	strlcpy(ifrdesc.ifr_name, "lo0", sizeof(ifrdesc.ifr_name));
 	ifrdesc.ifr_data = goodbuf;
 	if (ioctl(s, SIOCGIFDESCR, &ifrdesc))
 		fail("SIOCIFDESCR ioctl failed\n");
 	memset(&ifrdesc, 0, sizeof(ifrdesc));
 	strlcpy(ifrdesc.ifr_name, "lo0", sizeof(ifrdesc.ifr_name));
 	ifrdesc.ifr_data = 0;
 	if (!ioctl(s, SIOCGIFDESCR, &ifrdesc))
 		fail("copyoutstr didn't fail on 0 buf\n");
 	memset(&ifrdesc, 0, sizeof(ifrdesc));
 	strlcpy(ifrdesc.ifr_name, "lo0", sizeof(ifrdesc.ifr_name));
 	ifrdesc.ifr_data = badbuf;
 	if (!ioctl(s, SIOCGIFDESCR, &ifrdesc))
 		fail("copyoutstr didn't fail on badbuf\n");

 	/* copyinstr */
 	if (statfs("/", goodbuf))
 		fail("copyinstr failed on a good buf\n");
 	if (!statfs(0, goodbuf) || errno != EFAULT)
 		fail("copyinstr didn't fail on 0 buf\n");
 	if (!statfs(badbuf, goodbuf) || errno != EFAULT)
 		fail("copyinstr didn't fail on bad buf\n");

	if (failure)
		errx(1, "%d failures", failure);
 	return 0;
}
