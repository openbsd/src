/*	$OpenBSD: ko.h,v 1.1.1.1 1998/09/14 21:53:00 art Exp $	*/
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

/* $KTH: ko.h,v 1.10 1998/07/25 15:18:48 map Exp $ */

#ifndef __KO_H
#define __KO_H 1

#include <atypes.h>

typedef int32_t koerr_t;

/*
 * Error messages
 */

const char    *koerr_gettext(koerr_t err);

/*
 * sysname
 */

const char *arla_getsysname(void);


/*
 * Cell manglening
 */

void           cell_init (int cellcachesize);

struct in_addr cell_finddbserver (int32_t cell);
u_long         cell_listdbserver (int32_t cell, int index);
const char    *cell_findnamedbbyname (const char *cell);
const char    *cell_getthiscell (void);
const char    *cell_getcellbyhost(const char *host);
int32_t        cell_name2num (const char *cell);
const char    *cell_num2name (int32_t cell);

/* NIY
int            cell_serverdown (struct in_addr addr);
int            cell_cellup_p (int32_t cell);
int            cell_probe(int32_t cell);
int            cell_probeserver(struct in_addr addr);
*/

#endif


