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

#include "bos_locl.h"

RCSID("$arla: bosprocs.c,v 1.6 2002/06/02 21:12:15 lha Exp $");

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

    snprintf(statdescr, BOZE_BSSIZE, "foo");
    *inStat = 0;
    return 0;
}


/*
 *
 */

int
BOZO_SetStatus(struct rx_call *call,
	       const char *instance,
	       const int32_t status)
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
			   char *iname)
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
		     char *type,
		     struct bozo_status *status)
{
    bosdebug ("BOZO_GetInstanceInfo: %s\n", instance);

    strlcpy (type, "simple", BOZO_BSSIZE);
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
		     char *param)
{
    bosdebug ("BOZO_GetInstanceParm: %s %d\n", instance, num);

    strlcpy (param, "foo", BOZO_BSSIZE);
    return 0;
}

/*
 *
 */

int
BOZO_AddSUser(struct rx_call *call, const char *name)
{
    char *n;
    int ret;

    if (strchr(name, '@'))
	n = strdup (name);
    else
	asprintf (&n, "%s@%s", name, cell_getthiscell());
    if (n == NULL)
	return BZIO;
    if (strlen(n) > BOZO_BSSIZE)
	n[BOZO_BSSIZE - 1] = '\0';
    ret = sec_add_superuser (n);
    free (n);
    if (ret)
	return BZIO;
    return 0;
}


/*
 *
 */

int
BOZO_DeleteSUser(struct rx_call *call, const char *name)
{
    char *n;
    int ret;

    if (strchr(name, '@'))
	asprintf (&n, "%s", name);
    else
	asprintf (&n, "%s@%s", name, cell_getthiscell());
    if (n == NULL)
	return BZIO;
    if (strlen(n) > BOZO_BSSIZE)
	n[BOZO_BSSIZE - 1] = '\0';
    ret = sec_del_superuser (n);
    free (n);
    if (ret)
	return BZIO;
    return 0;
}


/*
 *
 */

int
BOZO_ListSUsers(struct rx_call *call, const /*
 *
 */

int32_t an, char *name)
{
    return -1;
}


/*
 *
 */

int
BOZO_ListKeys(struct rx_call *call, const int32_t an, int32_t *kvno,
	      struct bozo_key *key, struct bozo_keyInfo *keinfo)
{
    return -1;
}


/*
 *
 */

int
BOZO_AddKey(struct rx_call *call, const int32_t an,
	    const struct bozo_key *key)
{
    return -1;
}


/*
 *
 */

int
BOZO_DeleteKey(struct rx_call *call, const /*
 *
 */

int32_t an)
{
    return -1;
}


/*
 *
 */

int
BOZO_SetCellName(struct rx_call *call, const char *name)
{
    return -1;
}


/*
 *
 */

int
BOZO_GetCellName(struct rx_call *call, char *name)
{
    return -1;
}


/*
 *
 */

int
BOZO_GetCellHost(struct rx_call *call, const int32_t awhich, char *name)
{
    return -1;
}


/*
 *
 */

int
BOZO_AddCellHost(struct rx_call *call, const char *name)
{
    return -1;
}


/*
 *
 */

int
BOZO_DeleteCellHost(struct rx_call *call, const char *name)
{
    return -1;
}


/*
 *
 */

int
BOZO_SetTStatus(struct rx_call *call, const char *instance,
		const /*
 *
 */

int32_t status)
{
    return -1;
}


/*
 *
 */

int
BOZO_ShutdownAll(struct rx_call *call)
{
    return -1;
}


/*
 *
 */

int
BOZO_RestartAll(struct rx_call *call)
{
    return -1;
}


/*
 *
 */

int
BOZO_StartupAll(struct rx_call *call)
{
    return -1;
}


/*
 *
 */

int
BOZO_SetNoAuthFlag(struct rx_call *call, const /*
 *
 */

int32_t flag)
{
    return -1;
}


/*
 *
 */

int
BOZO_ReBozo(struct rx_call *call)
{
    return -1;
}


/*
 *
 */

int
BOZO_Restart(struct rx_call *call, const char *instance)
{
    return -1;
}


/*
 *
 */

int
BOZO_Install(struct rx_call *call, const char *path, const int32_t size,
	     const int32_t flags, const int32_t date)
{
    return -1;
}


/*
 *
 */

int
BOZO_UnInstall(struct rx_call *call, const char *path)
{
    return -1;
}


/*
 *
 */

int
BOZO_GetDates(struct rx_call *call, const char *path, int32_t *newtime,
	      int32_t *baktime, int32_t *oldtime)
{
    return -1;
}


/*
 *
 */

int
BOZO_Exec(struct rx_call *call, const char *cmd)
{
    return -1;
}


/*
 *
 */

int
BOZO_Prune(struct rx_call *call, const int32_t flags)
{
    return -1;
}


/*
 *
 */

int
BOZO_SetRestartTime(struct rx_call *call, const int32_t type,
		    const struct bozo_netKTime *restartTime)
{
    return -1;
}


/*
 *
 */

int
BOZO_GetRestartTime(struct rx_call *call, const int32_t type,
		    struct bozo_netKTime *restartTime)
{
    return -1;
}


/*
 *
 */

int
BOZO_GetLog(struct rx_call *call, const char *name)
{
    return -1;
}


/*
 *
 */

int
BOZO_WaitAll(struct rx_call *call)
{
    return -1;
}


/*
 *
 */

int
BOZO_GetInstanceStrings(struct rx_call *call, const char *instance,
			char *errorname, char *spare1, char *spare2, char *spare3)
{
    return -1;
}

