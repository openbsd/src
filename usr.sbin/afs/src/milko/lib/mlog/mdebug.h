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

/*
 * $arla: mdebug.h,v 1.1 2001/07/25 22:57:49 ahltorp Exp $
 */

#ifndef _mdebug_h
#define _mdebug_h

#include <stdio.h>
#include <stdarg.h>
#include <log.h>

#include <roken.h>

/*
 * Remeber to add any new logging class to fs/fileserver.c
 */

/* masks */
#define MDEBANY		0xffffffff
#define MDEBMISC        0x00000001	/* misc debugging */
#define MDEBVOLDB       0x00000002	/* voldb debugging */
#define MDEBVLD         0x00000004	/* vld debugging */
#define MDEBSALVAGE     0x00000008	/* salvager debugging */
#define MDEBFS	        0x00000010	/* fs debugging */
#define MDEBROPA        0x00000020	/* ropa debugging */
#define MDEBPR		0x00000040	/* pr debugging */
#define MDEBPRDB	0x00000080	/* pr db debugging */
#define MDEBVL		0x00000100	/* vl debugging */

#define MDEBWARN	0x08000000      /* don't ignore warning */
#define MDEBERROR	0x10000000      /* don't ignore error */

#define MDEBALL (MDEBMISC|MDEBVOLDB|MDEBVLD|MDEBWARN|MDEBERROR|MDEBSALVAGE|MDEBFS|MDEBROPA|MDEBPR|MDEBPRDB|MDEBVL)

extern struct units milko_deb_units[];

#define MDEFAULT_LOG (MDEBERROR|MDEBWARN)


#endif				       /* _mdebug_h */
