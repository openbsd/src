/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <limits.h>
#include <sys/mount.h>

#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#endif

#include <fcntl.h>

#include <err.h>
#include <roken.h>
#include <agetarg.h>

#include <atypes.h>
#include <roken.h>
#include <kafs.h>

RCSID("$KTH: fhbench.c,v 1.6.2.1 2002/02/24 22:37:36 lha Exp $");

struct fhb_handle {
    char data[512];
};

static int help_flag;
static int num_files;
static int write_file = 0;
static int num_runs = 3;

static struct agetargs args[] = {
    {"num",	'n',	aarg_integer,	&num_files,	"number of files"},
    {"write",	'w',	aarg_integer,	&write_file,	"write num kb"},
    {"runs",	'r',	aarg_integer,	&num_runs,	"number of runs"},
    {"help",	0,	aarg_flag,	&help_flag,	NULL,		NULL},
    {NULL,	0,	aarg_end,	NULL,		NULL,		NULL}
};


static void
fhb_fhget (char *filename, struct fhb_handle *handle)
{
    int ret = 0;
#if defined(HAVE_GETFH) && defined(HAVE_FHOPEN)
    {
	fhandle_t fh;

	ret = getfh (filename, &fh);
	if (ret)
	    err (1, "getfh");
	memcpy (handle, &fh, sizeof(fh));
    }
#endif
#ifdef KERBEROS
    {
	struct ViceIoctl vice_ioctl;
	
	vice_ioctl.in      = NULL;
	vice_ioctl.in_size = 0;
	
	vice_ioctl.out      = (caddr_t)handle;
	vice_ioctl.out_size = sizeof(*handle);
	
	ret = k_pioctl (filename, VIOC_FHGET, &vice_ioctl, 0);
	if (ret)
	    errx (1, "k_pioctl");
    }
#endif
}


static int
fhb_fhopen (struct fhb_handle *handle, int flags)
{
    int ret;
#if defined(HAVE_GETFH) && defined(HAVE_FHOPEN)
    {
	fhandle_t fh;

	memcpy (&fh, handle, sizeof(fh));
	ret = fhopen (&fh, flags);
	if (ret >= 0)
	    return ret;
    }
#endif

#ifdef KERBEROS			/* really KAFS */
    {
	struct ViceIoctl vice_ioctl;
	
	vice_ioctl.in      = (caddr_t)handle;
	vice_ioctl.in_size = sizeof(*handle);
	
	vice_ioctl.out      = NULL;
	vice_ioctl.out_size = 0;
	
	ret = k_pioctl (NULL, VIOC_FHOPEN, &vice_ioctl, flags);
	if (ret >= 0)
	    return ret;
    }
#endif
    errx (1, "fhopen/k_pioctl");
}

static void
nop_call (void)
{
#ifdef KERBEROS			/* really KAFS */
    {
	struct ViceIoctl vice_ioctl;
	char c[8];
	int ret;
	
	vice_ioctl.in      = (caddr_t)&c;
	vice_ioctl.in_size = sizeof(c);
	
	vice_ioctl.out      = NULL;
	vice_ioctl.out_size = 0;
	
	ret = k_pioctl (NULL, VIOC_XFSDEBUG, &vice_ioctl, 0);
	if (ret < 0)
	    err (1, "k_pioctl");
    }
#else
    {
	static first = 1;
	if (first) {
	    warnx ("can't test this");
	    first = 0;
	}
    }
#endif
}

static void
create_file (int num, struct fhb_handle *handle)
{
    int fd;
    char filename[1024];

    snprintf (filename, sizeof(filename), "file-%d", num);

    fd = open (filename, O_CREAT|O_EXCL|O_RDWR, 0666);
    if (fd < 0)
	err (1, "open");

    close (fd);
    
    fhb_fhget(filename, handle);
}

char databuf[1024];

static void
write_to_file (int fd, int num)
{
    int ret;
    while (num > 0) {
	ret = write (fd, databuf, sizeof(databuf));
	if (ret != sizeof(databuf))
	    err (1, "write");
	num--;
    }
}

static void
fhopen_file (int num, struct fhb_handle *handle)
{
    int fd;

    fd = fhb_fhopen(handle, O_RDWR);
    if (fd < 0)
	err (1, "open");

    if (write_file)
	write_to_file(fd, write_file);
    close(fd);
}

static void
open_file (int num)
{
    int fd;
    char filename[1024];

    snprintf (filename, sizeof(filename), "file-%d", num);

    fd = open (filename, O_RDWR, 0666);
    if (fd < 0)
	err (1, "open");

    if (write_file)
	write_to_file(fd, write_file);

    close (fd);
}

static void
unlink_file (int num)
{
    int ret;
    char filename[1024];

    snprintf (filename, sizeof(filename), "file-%d", num);

    ret = unlink(filename);
    if (ret < 0)
	err (1, "unlink");
}

/*
 * Make `t1' consistent.
 */

static void
tvfix(struct timeval *t1)
{
    if (t1->tv_usec < 0) {
        t1->tv_sec--;
        t1->tv_usec += 1000000;
    }
    if (t1->tv_usec >= 1000000) {
        t1->tv_sec++;
        t1->tv_usec -= 1000000;
    }
}
 
/*
 * t1 -= t2
 */

static void
tvsub(struct timeval *t1, const struct timeval *t2)
{
    t1->tv_sec  -= t2->tv_sec;
    t1->tv_usec -= t2->tv_usec;
    tvfix(t1);
}


struct timeval time1, time2;

static void
starttesting(char *msg)
{
    printf("testing %s...\n", msg);
    fflush (stdout);
    gettimeofday(&time1, NULL);
}    

static void
endtesting(void)
{
    gettimeofday(&time2, NULL);
    tvsub(&time2, &time1);
    printf("timing: %ld.%06ld\n", (long)time2.tv_sec, (long)time2.tv_usec);
}

static void
usage (int exit_val)
{
    aarg_printusage (args, NULL, "number of files", AARG_GNUSTYLE);
    exit (exit_val);
}

static void
open_bench (int i, struct fhb_handle *handles)
{
    printf ("====== test run %d\n"
	    "==================\n",
	    i);

    starttesting ("fhopening files");
    for (i = 0; i < num_files; i++)
	fhopen_file (i, &handles[i]);
    endtesting ();
   
    starttesting ("opening files");
    for (i = 0; i < num_files; i++)
	open_file (i);
    endtesting ();
}

int
main (int argc, char **argv)
{
    int optind = 0;
    int i;
    struct fhb_handle *handles;

    set_progname (argv[0]);

    if (agetarg (args, argc, argv, &optind, AARG_GNUSTYLE))
	usage (1);

    if (help_flag)
	usage (0);

    if (num_files <= 0)
	usage (1);

    if (write_file < 0)
	usage (1);

    if (!k_hasafs())
	errx (1, "no afs kernel module");

    handles = emalloc (num_files * sizeof(*handles));

    starttesting ("creating files");
    for (i = 0; i < num_files; i++)
	create_file (i, &handles[i]);
    endtesting ();

    for (i = 0 ; i < num_runs; i++)
	open_bench (i, handles);
   
    printf ( "==================\n");
    starttesting ("unlink files");
    for (i = 0; i < num_files; i++)
	unlink_file (i);
    endtesting ();

    printf ( "==================\n");
    starttesting ("nop call");
    for (i = 0; i < num_files; i++)
	nop_call ();
    endtesting ();

    return 0;
}
