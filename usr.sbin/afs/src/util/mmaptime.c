OBSOLETE /*
OBSOLETE  * Copyright (c) 1998 Kungliga Tekniska Högskolan
OBSOLETE  * (Royal Institute of Technology, Stockholm, Sweden).
OBSOLETE  * All rights reserved.
OBSOLETE  * 
OBSOLETE  * Redistribution and use in source and binary forms, with or without
OBSOLETE  * modification, are permitted provided that the following conditions
OBSOLETE  * are met:
OBSOLETE  * 
OBSOLETE  * 1. Redistributions of source code must retain the above copyright
OBSOLETE  *    notice, this list of conditions and the following disclaimer.
OBSOLETE  * 
OBSOLETE  * 2. Redistributions in binary form must reproduce the above copyright
OBSOLETE  *    notice, this list of conditions and the following disclaimer in the
OBSOLETE  *    documentation and/or other materials provided with the distribution.
OBSOLETE  * 
OBSOLETE  * 3. Neither the name of the Institute nor the names of its contributors
OBSOLETE  *    may be used to endorse or promote products derived from this software
OBSOLETE  *    without specific prior written permission.
OBSOLETE  * 
OBSOLETE  * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
OBSOLETE  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
OBSOLETE  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
OBSOLETE  * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
OBSOLETE  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
OBSOLETE  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OBSOLETE  * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
OBSOLETE  * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
OBSOLETE  * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OBSOLETE  * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
OBSOLETE  * SUCH DAMAGE.
OBSOLETE  */
OBSOLETE 
OBSOLETE #ifdef HAVE_CONFIG_H
OBSOLETE #include <config.h>
OBSOLETE #endif
OBSOLETE 
OBSOLETE RCSID("$arla: mmaptime.c,v 1.8 2002/12/20 12:57:07 lha Exp $");
OBSOLETE 
OBSOLETE #include <stdio.h>
OBSOLETE 
OBSOLETE /*
OBSOLETE  * Speed hack
OBSOLETE  *
OBSOLETE  *   Here we try to mmap time from the kernelspace.
OBSOLETE  *   There are people claiming that this is about 100 times faster
OBSOLETE  *   then gettimeofday()
OBSOLETE  *
OBSOLETE  *   You need to have USE_MMAPTIME defined to get it.
OBSOLETE  *
OBSOLETE  *   Other thing that you need is mmap(), getpagesize(), kvm_open() & co
OBSOLETE  *
OBSOLETE  *   If not initalized with mmaptime_probe gettimeofday will be called
OBSOLETE  *
OBSOLETE  */
OBSOLETE 
OBSOLETE #if defined(USE_MMAPTIME)
OBSOLETE 
OBSOLETE #if defined(HAVE_MMAP) && defined(HAVE_KVM_OPEN)  && defined(HAVE_KVM_NLIST) && defined(HAVE_GETPAGESIZE)
OBSOLETE 
OBSOLETE #include "mmaptime.h"
OBSOLETE 
OBSOLETE #ifdef HAVE_UNISTD_H
OBSOLETE #include <unistd.h>
OBSOLETE #endif
OBSOLETE #ifdef HAVE_SYS_MMAN_H
OBSOLETE #include <sys/mman.h>
OBSOLETE #endif
OBSOLETE #ifdef HAVE_FCNTL_H
OBSOLETE #include <fcntl.h>
OBSOLETE #endif
OBSOLETE #ifdef HAVE_NLIST_H
OBSOLETE #include <nlist.h>
OBSOLETE #endif
OBSOLETE #ifdef HAVE_ELFLIB_NLIST_H
OBSOLETE #include <elflib/nlist.h>
OBSOLETE #endif
OBSOLETE #ifdef HAVE_KVM_H
OBSOLETE #include <kvm.h>
OBSOLETE #endif
OBSOLETE #include <errno.h>
OBSOLETE 
OBSOLETE 
OBSOLETE static unsigned long ps;
OBSOLETE static unsigned long begpage;
OBSOLETE static void *mem;
OBSOLETE static int kmemfd = -1;
OBSOLETE static struct timeval *tp = NULL;
OBSOLETE 
OBSOLETE 
OBSOLETE int mmaptime_probe(void)
OBSOLETE {
OBSOLETE     kvm_t *kvm;
OBSOLETE     unsigned long value;
OBSOLETE     struct nlist nl[2];
OBSOLETE     int i, saved_errno;
OBSOLETE     
OBSOLETE     if (tp)
OBSOLETE 	return 0;
OBSOLETE     
OBSOLETE     if (geteuid()) {
OBSOLETE         fprintf(stderr, "mmaptime needs to be run as root, falling back on gettimeofday\n");
OBSOLETE         return EPERM;
OBSOLETE     }	
OBSOLETE 
OBSOLETE     kvm = kvm_open(NULL, NULL, NULL, O_RDONLY, "util/mmaptime");
OBSOLETE     nl[0].n_name = "_time";
OBSOLETE     nl[1].n_name = NULL;
OBSOLETE     i = kvm_nlist(kvm, nl);
OBSOLETE 
OBSOLETE     /*
OBSOLETE      * SunOS 5.6 is broken and returns a zero even when it fails.
OBSOLETE      */
OBSOLETE 
OBSOLETE     if (nl[0].n_value == 0) {
OBSOLETE 	nl[0].n_name = "time";
OBSOLETE 	nl[1].n_name = NULL;
OBSOLETE 	i = kvm_nlist(kvm, nl);
OBSOLETE     }
OBSOLETE 
OBSOLETE     kvm_close(kvm);
OBSOLETE 
OBSOLETE     if (i != 0)
OBSOLETE 	return ENOENT;
OBSOLETE 
OBSOLETE     kmemfd = open("/dev/kmem", O_RDONLY);
OBSOLETE     if (kmemfd < 0) 
OBSOLETE 	return errno;
OBSOLETE 
OBSOLETE     value = nl[0].n_value;
OBSOLETE 
OBSOLETE     ps = getpagesize();
OBSOLETE 
OBSOLETE     begpage = value - value % ps ;
OBSOLETE     mem = mmap(NULL, ps, PROT_READ, MAP_SHARED, kmemfd, begpage);
OBSOLETE 
OBSOLETE     if (mem == (void *) -1) {
OBSOLETE 	saved_errno = errno;
OBSOLETE 	close(kmemfd);
OBSOLETE 	return saved_errno;
OBSOLETE     }
OBSOLETE 
OBSOLETE     tp = (struct timeval *) (mem + value % ps);
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE int 
OBSOLETE mmaptime_close(void)
OBSOLETE {
OBSOLETE     if (tp)
OBSOLETE 	munmap(mem, ps);
OBSOLETE 
OBSOLETE     tp = NULL;
OBSOLETE 
OBSOLETE     if (kmemfd >= 0)
OBSOLETE 	close(kmemfd);
OBSOLETE 
OBSOLETE     
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE int
OBSOLETE mmaptime_gettimeofday(struct timeval *t, void *tzp)
OBSOLETE {
OBSOLETE     if (t && tp && !tzp) {
OBSOLETE 	*t = *tp;
OBSOLETE 	return 0;
OBSOLETE     } else 
OBSOLETE 	return gettimeofday(tp, tzp);
OBSOLETE 
OBSOLETE     /* NOT REACHED */
OBSOLETE     return -1;
OBSOLETE }
OBSOLETE 
OBSOLETE 
OBSOLETE #else /* USE_MMAPTIME not useable */
OBSOLETE 
OBSOLETE #include <errno.h>
OBSOLETE 
OBSOLETE int 
OBSOLETE mmaptime_probe(void) 
OBSOLETE {
OBSOLETE     return EOPNOTSUPP;
OBSOLETE }
OBSOLETE 
OBSOLETE int 
OBSOLETE mmaptime_gettimeofday(struct timeval *tp, void *tzp)
OBSOLETE {
OBSOLETE     return gettimeofday(tp, tzp);
OBSOLETE }
OBSOLETE 
OBSOLETE int 
OBSOLETE mmaptime_close(void)
OBSOLETE {
OBSOLETE     return 0;
OBSOLETE }
OBSOLETE 
OBSOLETE #endif /* USE_MMAPTIME not useable */
OBSOLETE #endif /* USE_MMAPTIME */
OBSOLETE 
