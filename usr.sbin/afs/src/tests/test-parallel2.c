/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include <err.h>
#include <roken.h>

#include <atypes.h>
#include <kafs.h>

#ifdef RCSID
RCSID("$KTH: test-parallel2.c,v 1.2 2000/10/03 00:36:10 lha Exp $");
#endif

#define WORKER_TIMES 1000
#define NUM_WORKER 100

static int
getcwd_worker (int num)
{
    char name[17];
    int i;

    snprintf (name, sizeof(name), "%d", num);
    if (mkdir (name, 0777) < 0)
	err (1, "mkdir %s", name);
    if (chdir (name) < 0)
	err (1, "chdir %s", name);
    for (i = 0; i < WORKER_TIMES; ++i) {
	char buf[256];

	getcwd (buf, sizeof(buf));
    }
    return 0;
}

static int
mkdir_worker (int num)
{
    int i;

    for (i = 0; i < WORKER_TIMES; ++i){
	char name[256];

	snprintf (name, sizeof(name), "m%d-%d", num, i);
	mkdir (name, 0777);
    }
    return 0;
}

static int
mkdir_rmdir_worker (int num)
{
    int i;

    for (i = 0; i < WORKER_TIMES; ++i){
	char name[256];

	snprintf (name, sizeof(name), "rm%d-%d", num, i);
	mkdir (name, 0777);
    }
    for (i = 0; i < WORKER_TIMES; ++i){
	char name[256];

	snprintf (name, sizeof(name), "rm%d-%d", num, i);
	rmdir (name);
    }
    return 0;
}

static int
rename_worker (int num)
{
    int i;

    for (i = 0; i < WORKER_TIMES; ++i){
	char name[256];
	int fd;

	snprintf (name, sizeof(name), "rm%d-%d", num, i);
	fd = open (name, O_WRONLY | O_CREAT, 0777);
	close (fd);
    }
    for (i = 0; i < WORKER_TIMES; ++i){
	char name[256], name2[256];

	snprintf (name, sizeof(name), "rm%d-%d", num, i);
	snprintf (name2, sizeof(name2), "rn%d-%d", num, i);
	rename (name, name2);
    }
    return 0;
}

static int
stat_worker (int num)
{
    char name[17];
    int i;
    char buf[256];
    struct stat sb;

    snprintf (name, sizeof(name), "%d", num);
    if (mkdir (name, 0777) < 0)
	err (1, "mkdir %s", name);
    if (chdir (name) < 0)
	err (1, "chdir %s", name);
    for (i = 0; i < WORKER_TIMES; ++i) {
	getcwd (buf, sizeof(buf));
	stat (buf, &sb);
    }
    return 0;
}

static int (*workers[])(int) = {getcwd_worker, mkdir_worker,
				mkdir_rmdir_worker, rename_worker,
				stat_worker};

static int nworkers = sizeof(workers)/sizeof(*workers);

int
main(int argc, char **argv)
{
    int i, ret;
    
    set_progname (argv[0]);

    for (i = 0; i < NUM_WORKER ; i++) {
	int ret;
	
	ret = fork();
	switch (ret) {
	case 0:
	    return (*workers[i % nworkers])(i);
	case -1:
	    err (1, "fork");
	}
    }
    i = NUM_WORKER;
    while (i && wait (&ret)) {
	i--;
	if (ret)
	    err (1, "wait: %d", ret);
    }
    return 0;
}
