OBSOLETE /*
OBSOLETE  * Copyright (c) 1998 Kungliga Tekniska Högskolan
OBSOLETE  * (Royal Institute of Technology, Stockholm, Sweden).
OBSOLETE  * All rights reserved.
OBSOLETE  * 
OBSOLETE  * Redistribution and use in source and binary forms, with or without
OBSOLETE  * modification, are permitted provided that the following conditions
OBSOLETE  * are met:
OBSOLETE  * 
OBSOLETE  * 1. Redistributions of source code must retain the above copyright
OBSOLETE  *    notice, this list of conditions and the following disclaimer.
OBSOLETE  * 
OBSOLETE  * 2. Redistributions in binary form must reproduce the above copyright
OBSOLETE  *    notice, this list of conditions and the following disclaimer in the
OBSOLETE  *    documentation and/or other materials provided with the distribution.
OBSOLETE  * 
OBSOLETE  * 3. Neither the name of the Institute nor the names of its contributors
OBSOLETE  *    may be used to endorse or promote products derived from this software
OBSOLETE  *    without specific prior written permission.
OBSOLETE  * 
OBSOLETE  * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
OBSOLETE  * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
OBSOLETE  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
OBSOLETE  * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
OBSOLETE  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
OBSOLETE  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OBSOLETE  * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
OBSOLETE  * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
OBSOLETE  * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OBSOLETE  * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
OBSOLETE  * SUCH DAMAGE.
OBSOLETE  */
OBSOLETE 
OBSOLETE /* $arla: mmaptime.h,v 1.3 2002/12/20 13:11:51 lha Exp $ */
OBSOLETE 
OBSOLETE #ifndef _UTIL_MMAPTIME_H
OBSOLETE #define _UTIL_MMAPTIME_H 1
OBSOLETE 
OBSOLETE #include <sys/time.h>
OBSOLETE 
OBSOLETE int mmaptime_probe(void);
OBSOLETE int mmaptime_gettimeofday(struct timeval *tp, void *tzp);
OBSOLETE int mmaptime_close(void);
OBSOLETE 
OBSOLETE #endif
OBSOLETE 
OBSOLETE 
