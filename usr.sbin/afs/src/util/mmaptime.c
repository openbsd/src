/*	$OpenBSD: mmaptime.c,v 1.1.1.1 1998/09/14 21:53:24 art Exp $	*/
/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$KTH: mmaptime.c,v 1.5 1998/05/24 16:07:18 lha Exp $");

#include <stdio.h>

/*
 * Speed hack
 *
 *   Here we try to mmap time from the kernelspace.
 *   There are people claiming that this is about 100 times faster
 *   then gettimeofday()
 *
 *   You need to have USE_MMAPTIME defined to get it.
 *
 *   Other thing that you need is mmap(), getpagesize(), kvm_open() & co
 *
 *   If not initalized with mmaptime_probe gettimeofday will be called
 *
 */

#include "mmaptime.h"

#if defined(USE_MMAPTIME) && defined(HAVE_MMAP) && defined(HAVE_KVM_OPEN)  && defined(HAVE_KVM_NLIST) && defined(HAVE_GETPAGESIZE)

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_NLIST_H
#include <nlist.h>
#endif
#ifdef HAVE_ELFLIB_NLIST_H
#include <elflib/nlist.h>
#endif
#ifdef HAVE_KVM_H
#include <kvm.h>
#endif
#include <errno.h>


static unsigned long ps;
static unsigned long begpage;
static void *mem;
static int kmemfd = -1;
static struct timeval *tp = NULL;


int mmaptime_probe(void)
{
    kvm_t *kvm;
    unsigned long value;
    struct nlist nl[2];
    int i, saved_errno;
    
    if (tp)
	return 0;
    
    if (geteuid()) {
        fprintf(stderr, "mmaptime needs to be run as root, falling back on gettimeofday\n");
        return EPERM;
    }	

    kvm = kvm_open(NULL, NULL, NULL, O_RDONLY, "util/mmaptime");
    nl[0].n_name = "_time";
    nl[1].n_name = NULL;
    i = kvm_nlist(kvm, nl);

    /*
     * SunOS 5.6 is broken and returns a zero even when it fails.
     */

    if (nl[0].n_value == 0) {
	nl[0].n_name = "time";
	nl[1].n_name = NULL;
	i = kvm_nlist(kvm, nl);
    }

    kvm_close(kvm);

    if (i != 0)
	return ENOENT;

    kmemfd = open("/dev/kmem", O_RDONLY);
    if (kmemfd < 0) 
	return errno;

    value = nl[0].n_value;

    ps = getpagesize();

    begpage = value - value % ps ;
    mem = mmap(NULL, ps, PROT_READ, MAP_SHARED, kmemfd, begpage);

    if (mem == (void *) -1) {
	saved_errno = errno;
	close(kmemfd);
	return saved_errno;
    }

    tp = (struct timeval *) (mem + value % ps);
    return 0;
}


int 
mmaptime_close(void)
{
    if (tp)
	munmap(mem, ps);

    tp = NULL;

    if (kmemfd >= 0)
	close(kmemfd);

    
    return 0;
}

int
mmaptime_gettimeofday(struct timeval *t, void *tzp)
{
    if (t && tp && !tzp) {
	*t = *tp;
	return 0;
    } else 
	return gettimeofday(tp, tzp);

    /* NOT REACHED */
    return -1;
}


#else /* USE_MMAPTIME */

#include <errno.h>

int 
mmaptime_probe(void) 
{
    return EOPNOTSUPP;
}

int 
mmaptime_gettimeofday(struct timeval *tp, void *tzp)
{
    return gettimeofday(tp, tzp);
}

int 
mmaptime_close(void)
{
    return 0;
}

#endif
