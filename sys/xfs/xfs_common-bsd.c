/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

#include <xfs/xfs_locl.h>
#include <xfs/xfs_common.h>
#include <xfs/xfs_deb.h>

RCSID("$Id: xfs_common-bsd.c,v 1.4 2002/06/07 04:10:32 hin Exp $");

#ifdef MALLOC_DEFINE
MALLOC_DEFINE(M_XFS, "xfs", "xfs buffer");
#endif

#ifdef XFS_DEBUG
static u_int xfs_allocs;
static u_int xfs_frees;

void *
xfs_alloc(u_int size)
{
    void *ret;

    xfs_allocs++;
    XFSDEB(XDEBMEM, ("xfs_alloc: xfs_allocs - xfs_frees %d\n", 
		     xfs_allocs - xfs_frees));

    MALLOC(ret, void *, size, M_XFS, M_WAITOK);
    return ret;
}

void
xfs_free(void *ptr, u_int size)
{
    xfs_frees++;
    FREE(ptr, M_XFS);
}

#endif /* XFS_DEBUG */

int
xfs_suser(struct proc *p)
{
#ifdef HAVE_TWO_ARGUMENT_SUSER
    return suser (xfs_proc_to_cred(p), NULL);
#else
    return suser (p);
#endif
}

/*
 * Print a `dev_t' in some readable format
 */

#ifdef HAVE_KERNEL_DEVTONAME

const char *
xfs_devtoname_r (dev_t dev, char *buf, size_t sz)
{
    return devtoname (dev);
}

#else /* !HAVE_KERNEL_DEVTONAME */

const char *
xfs_devtoname_r (dev_t dev, char *buf, size_t sz)
{
#ifdef HAVE_KERNEL_SNPRINTF
    snprintf (buf, sz, "%u/%u", major(dev), minor(dev));
    return buf;
#else
    return "<unknown device>";
#endif
}

#endif /* HAVE_KERNEL_DEVTONAME */
