/*	$OpenBSD: arlalib.h,v 1.1.1.1 1998/09/14 21:52:52 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

/* $KTH: arlalib.h,v 1.8 1998/07/24 19:55:48 lha Exp $ */

#ifndef ARLALIB_H
#define ARLALIB_H 1

#ifdef KERBEROS
int get_cred(char *host, CREDENTIALS *c);
#endif /* KERBEROS */

struct rx_connection *
arlalib_getconnbyaddr(int32_t addr, const char *host,
		      int32_t port, int32_t servid, int noauth);

struct rx_connection *
arlalib_getconnbyname(const char *host, int32_t port, int32_t servid,
		      int noauth);

int arlalib_destroyconn(struct rx_connection *conn);
int arlalib_getservername(u_int32_t serverNumber, char **servername);
int arlalib_getsyncsite(const char *cell, const char *host, int32_t port, 
		    u_int32_t *synchost, int notauth);



/*
 * Wrappers around pioctl calls
 */

void fserr(char *progname, int error, char *realpath);

int fs_getfid (char *path, VenusFid *fid);
int fs_getfilecellname (char *path, char *cell, size_t len);
int fs_nop(void);

/* arla extensions */

const char *fslib_version(void);

int fs_setcrypt (u_int32_t level);
int fs_getcrypt (u_int32_t *level);

int fs_connect (int32_t type, int32_t *flags);

int fs_setfprio(VenusFid fid, int16_t prio);
int fs_getfprio(VenusFid fid, int16_t *prio);
int fs_setmaxfprio(int16_t maxprio);
int fs_getmaxfprio(int16_t *maxprio);



#endif /* ARLALIB_H */
