/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska Högskolan
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
 *      This product includes software developed by Kungliga Tekniska 
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

/* $KTH: com_err.h,v 1.3 1998/05/02 20:13:28 assar Exp $ */

/* MIT compatible com_err library */

#ifndef __COM_ERR_H__
#define __COM_ERR_H__

#ifdef __STDC__
#include <stdarg.h>
#endif

#ifndef __P
#ifdef __STDC__
#define __P(X) X
#else
#define __P(X) ()
#endif
#endif


/*
 * For compatibility with MIT's com_err the com_right.h include
 * file is inserted here.
 */
/* $KTH: com_right.h,v 1.8 1998/02/17 21:19:43 bg Exp $ */

#ifndef __COM_RIGHT_H__
#define __COM_RIGHT_H__

struct error_table {
    char const * const * msgs;
    long base;
    int n_msgs;
};
struct et_list {
    struct et_list *next;
    struct error_table *table;
};
extern struct et_list *_et_list;

const char *com_right(struct et_list *list, long code);
void initialize_error_table_r(struct et_list **, const char **, int, long);
void free_error_table(struct et_list *);

#endif /* __COM_RIGHT_H__ */


typedef void (*errf)(const char *, long, const char *, va_list);

const char * error_message(long);
int init_error_table(const char**, long, int);

void com_err_va(const char *, long, const char *, va_list);
void com_err(const char *, long, const char *, ...);

errf set_com_err_hook(errf);
errf reset_com_err_hook(void);

const char *error_table_name(int num);

#endif /* __COM_ERR_H__ */
