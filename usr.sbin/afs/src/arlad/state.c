/*
 * Copyright (c) 2001 - 2002 Kungliga Tekniska Högskolan
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

/*
 * The interface for the on disk storage of data when arla is down
 */

#include "arla_local.h"
RCSID("$arla: state.c,v 1.5 2002/09/30 19:13:39 lha Exp $");

/*
 * Save the fcache inforation in `fn', call `func' until `func' return
 * a none zero value. A negative value is an errno, and a positive is
 * a signal to terminate the loop (success). `ptr' is passed to
 * `func'.
 */

int
state_store_fcache(const char *fn, store_fcache_fn func, void *ptr)
{
    int ret, fd, save_errno;
    struct fcache_store st;
    uint32_t u1, u2;
    char file[MAXPATHLEN];

    snprintf(file, sizeof(file), "%s.new", fn);

    fd = open (file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0)
	return errno;
    u1 = FCACHE_MAGIC_COOKIE;
    u2 = FCACHE_VERSION;

    if (write (fd, &u1, sizeof(u1)) != sizeof(u1)
	|| write (fd, &u2, sizeof(u2)) != sizeof(u2)) {
	save_errno = errno;
	
	close (fd);
	return save_errno;
    }

    while (1) {

	memset(&st, 0, sizeof(st));

	ret = (*func)(&st, ptr);
	if (ret < 0) { /* errors */
	    close(fd);
	    return -ret;
	} else if (ret == STORE_SKIP) {
	    continue;
	} else if (ret == STORE_NEXT) {
	    if (write(fd, &st, sizeof(st)) != sizeof(st)) {
		save_errno = errno;
		close(fd);
		return save_errno;
	    }
	} else if (ret == STORE_DONE) {
	    break;
	} else
	    errx(-1, "state_store_fcache: unknown return %d\n", ret);
    }
    
    if(close (fd))
	return errno;
    if (rename (file, fn))
	return errno;

    return 0;
}		   

int
state_recover_fcache(const char *file, store_fcache_fn func, void *ptr)
{
    int fd;
    struct fcache_store st;
    uint32_t u1, u2;

    fd = open (file, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	return errno;
    if (read (fd, &u1, sizeof(u1)) != sizeof(u1)
	|| read (fd, &u2, sizeof(u2)) != sizeof(u2)) {
	close (fd);
	return EINVAL;
    }
    if (u1 != FCACHE_MAGIC_COOKIE) {
#if 0
	arla_warnx (ADEBFCACHE, "dump file not recognized, ignoring");
#endif
	close (fd);
	return EINVAL;
    }
    if (u2 != FCACHE_VERSION) {
#if 0
	arla_warnx (ADEBFCACHE, "unknown dump file version number %u", u2);
#endif
	close (fd);
	return EINVAL;
    }
    
    while (read (fd, &st, sizeof(st)) == sizeof(st)) {

	st.cell[sizeof(st.cell)-1] = '\0';
	st.parentcell[sizeof(st.parentcell)-1] = '\0';

	if ((*func)(&st, ptr)) {
	    close(fd);
	    return 1;
	}
    }
    close (fd);

    return 0;
}

/*
 * Save the volcache inforation in `fn', call `func' until `func' return
 * a none zero value. A negative value is an errno, and a positive is
 * a signal to terminate the loop (success). `ptr' is passed to
 * `func'.
 */

int
state_store_volcache(const char *fn, store_volcache_fn func, void *ptr)
{
    int ret, fd, save_errno;
    struct volcache_store st;
    uint32_t u1, u2;
    char file[MAXPATHLEN];

    snprintf(file, sizeof(file), "%s.new", fn);

    fd = open (file, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
    if (fd < 0) 
	return errno;
    u1 = VOLCACHE_MAGIC_COOKIE;
    u2 = VOLCACHE_VERSION;

    if (write (fd, &u1, sizeof(u1)) != sizeof(u1)
	|| write (fd, &u2, sizeof(u2)) != sizeof(u2)) {
	save_errno = errno;
	
	close (fd);
	return save_errno;
    }

    while (1) {

	memset(&st, 0, sizeof(st));

	ret = (*func)(&st, ptr);
	if (ret < 0) { /* errors */
	    close(fd);
	    return -ret;
	} else if (ret == STORE_SKIP) {
	    continue;
	} else if (ret == STORE_NEXT) {
	    if (write(fd, &st, sizeof(st)) != sizeof(st)) {
		save_errno = errno;
		close(fd);
		return save_errno;
	    }
	} else if (ret == STORE_DONE) {
	    break;
	} else
	    errx(-1, "state_store_volcache: unknown return %d\n", ret);
    }
    
    if(close (fd))
	return errno;
    if (rename (file, fn))
	return errno;

    return 0;
}		   

int
state_recover_volcache(const char *file, store_volcache_fn func, void *ptr)
{
    int fd;
    struct volcache_store st;
    uint32_t u1, u2;

    fd = open (file, O_RDONLY | O_BINARY, 0);
    if (fd < 0)
	return errno;
    if (read (fd, &u1, sizeof(u1)) != sizeof(u1)
	|| read (fd, &u2, sizeof(u2)) != sizeof(u2)) {
	close (fd);
	return EINVAL;
    }
    if (u1 != VOLCACHE_MAGIC_COOKIE) {
#if 0
	arla_warnx (ADEBVOLCACHE, "dump file not recognized, ignoring");
#endif
	close (fd);
	return EINVAL;
    }
    if (u2 != VOLCACHE_VERSION) {
#if 0
	arla_warnx (ADEBVOLCACHE, "unknown dump file version number %u", u2);
#endif
	close (fd);
	return EINVAL;
    }
    
    while (read (fd, &st, sizeof(st)) == sizeof(st)) {

	st.cell[sizeof(st.cell)-1] = '\0';

	if ((*func)(&st, ptr)) {
	    close(fd);
	    return 1;
	}
    }
    close (fd);

    return 0;
}

