/*
 * Copyright (c) 1999, 2000 Kungliga Tekniska Högskolan
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

#include "bos_locl.h"

RCSID("$Id: bosprocs.c,v 1.1 2000/09/11 14:41:12 art Exp $");

/*
 *
 */

int
BOZO_CreateBnode(struct rx_call *call,
		 const char *type,
		 const char *instance,
		 const char *p1,
		 const char *p2,
		 const char *p3,
		 const char *p4,
		 const char *p5,
		 const char *p6)
{
    bosdebug ("BOZO_CreateNode: %s %s\n", type, instance);

    if (!sec_is_superuser(call))
	return VL_PERM;

    return 0;
}

/*
 *
 */

int
BOZO_DeleteBnode(struct rx_call *call,
		 const char *instance)
{
    bosdebug ("BOZO_DeleteBnode: %s\n", instance);

    return 0;
}


/*
 *
 */

int
BOZO_GetStatus(struct rx_call *call,
	       const char *instance,
	       int32_t *inStat,
	       char *statdescr)
{
    bosdebug ("BOZO_GetStatus: %s\n", instance);

    strcpy (statdescr, "foo");
    *inStat = 0;
    return 0;
}


/*
 *
 */

int
BOZO_SetStatus(struct rx_call *call,
	       const char *instance)
{
    bosdebug ("BOZO_SetStatus: %s\n", instance);

    return 0;
}

/*
 *
 */

int
BOZO_EnumerateInstance(struct rx_call *call,
			   const int32_t instance,
			   char **iname)
{
    bosdebug ("BOZO_EnumerateInstance: %d\n", instance);

    return -1;
}

/*
 *
 */

int
BOZO_GetInstanceInfo(struct rx_call *call,
		     const char *instance,
		     char **type,
		     struct bozo_status *status)
{
    bosdebug ("BOZO_GetInstanceInfo: %s\n", instance);

#if 0
    strcpy (type, "simple");
#endif
    memset (status, 0, sizeof(*status));
    return 0;
}


/*
 *
 */

int
BOZO_GetInstanceParm(struct rx_call *call,
		     const char *instance,
		     const int32_t num,
		     char **param)
{
    bosdebug ("BOZO_GetInstanceParm: %s %d\n", instance, num);

#if 0
    strcpy (param, "foo");
#endif
    return 0;
}

