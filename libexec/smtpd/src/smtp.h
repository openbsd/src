/*
 * Obtuse smtp store/forward daemon include file
 *
 * $Id: smtp.h,v 1.2 1998/06/03 08:57:07 beck Exp $ 
 *
 * Copyright (c) 1996, 1997 Obtuse Systems Corporation. All rights
 * reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Obtuse Systems 
 *      Corporation and its contributors.
 * 4. Neither the name of the Obtuse Systems Corporation nor the names
 *    of its contributors may be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY OBTUSE SYSTEMS CORPORATION AND
 * CONTRIBUTORS ``AS IS''AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL OBTUSE SYSTEMS CORPORATION OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */


#include<arpa/nameser.h>
#include<sys/time.h>
#include<sys/types.h>
#include<unistd.h>


#ifndef USE_LOCKF
 #ifndef USE_FLOCK
 #define USE_FLOCK
 #endif
#endif

#ifndef SPOOLDIR
#define SPOOLDIR "/usr/spool/smtpd"
#endif

#ifndef SMTP_USER
#define SMTP_USER "uucp"
#endif

#ifndef SMTP_GROUP
#define SMTP_GROUP "uucp"
#endif

#ifndef EX_CONFIG
#define EX_CONFIG 78
#endif

/* How big can a fully qualified hostname be? */
#define SMTP_MAXFQNAME (MAXHOSTNAMELEN + MAXDNAME + 1) /* leave room for . */

/* According to rfc 821, the maxiumum length of a command line including
 * crlf is 512 characters. 
 */
#define SMTP_MAX_CMD_LINE (512+1)

/* according to rfc 821, the maxiumum length of a mail path is
 * is 256 characters. Ick. We'll take a fully qualified hostname + 80
 * for the user name. any more and we complain.
 */

#define SMTP_MAX_MAILPATH (SMTP_MAXFQNAME + 80)

struct smtp_victim {
  char *name; /* mailname of recipient */
  long location;  /* start of RCPT line in spoolfile */
  struct smtp_victim * next;
};

extern int accumlog(int level, const char *fmt, ...);
