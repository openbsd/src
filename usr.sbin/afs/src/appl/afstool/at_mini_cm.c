/*
 * Copyright (c) 1999 - 2003 Kungliga Tekniska Högskolan
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

#include "at_locl.h"
#include "cb.ss.h"

RCSID("$arla: at_mini_cm.c,v 1.1 2003/03/05 11:41:24 lha Exp $");

/*
 * Each client need a callbackserver, here we go...
 */

int
SRXAFSCB_Probe (struct rx_call *a_rxCallP)
{
    return 0;
}

int
SRXAFSCB_InitCallBackState (struct rx_call *a_rxCallP)
{
    return 0;
}

int
SRXAFSCB_CallBack (struct rx_call *a_rxCallP,
		   const AFSCBFids *a_fidArrayP,
		   const AFSCBs *a_callBackArrayP)
{
    return 0;
}


int
SRXAFSCB_GetLock(struct rx_call *a_rxCallP,
		 int32_t index,
		 AFSDBLock *lock)
{
    return 1;
}

int
SRXAFSCB_GetCE(struct rx_call *a_rxCallP,
	       int32_t index,
	       AFSDBCacheEntry *dbentry)
{
    return 1;
}

int
SRXAFSCB_XStatsVersion(struct rx_call *a_rxCallP,
		       int32_t *version)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_GetXStats(struct rx_call *a_rxCallP,
		   int32_t client_version_num,
		   int32_t collection_number,
		   int32_t *server_version_number,
		   int32_t *time,
		   AFSCB_CollData *stats)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_InitCallBackState2(struct rx_call *a_rxCallP,
			    interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_WhoAreYou(struct rx_call *a_rxCallP,
		   interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_InitCallBackState3(struct rx_call *a_rxCallP,
			    const afsUUID *server_uuid)
{
    return 0;
}

int
SRXAFSCB_ProbeUUID(struct rx_call *a_rxCallP,
		   const afsUUID *uuid)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_GetCellServDB(struct rx_call *a_rxCallP,
		       const int32_t cellIndex,
		       char *cellName,
		       serverList *cellHosts)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_GetLocalCell(struct rx_call *a_rxCallP,
		      char *cellName)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_GetCacheConfig(struct rx_call *a_rxCallP,
			const uint32_t callerVersion,
			uint32_t *serverVersion,
			uint32_t *configCount,
			cacheConfig *config)
{
    *serverVersion = 0;
    *configCount = 0;
    config->len = 0;
    config->val = NULL;

    return RXGEN_OPCODE;
}

int
SRXAFSCB_GetCellByNum(struct rx_call *call,
		      const int32_t cellNumber,
		      char *cellName,
		      serverList *cellHosts)
{
    return RXGEN_OPCODE;
}

int
SRXAFSCB_TellMeAboutYourself(struct rx_call *call,
			     struct interfaceAddr *addr,
			     Capabilities *capabilities)
{
    capabilities->len = 0;
    capabilities->val = NULL;
    return RXGEN_OPCODE;
}

void
mini_cachemanager_init (void)
{
    static struct rx_securityClass *nullSecObjP;
    static struct rx_securityClass *(securityObjects[1]);
    
    nullSecObjP = rxnull_NewClientSecurityObject ();
    if (nullSecObjP == NULL) {
	printf("Cannot create null security object.\n");
	return;
    }
    
    securityObjects[0] = nullSecObjP;
    
    if (rx_NewService (0, CM_SERVICE_ID, "cm", securityObjects,
		       sizeof(securityObjects) / sizeof(*securityObjects),
		       RXAFSCB_ExecuteRequest) == NULL ) {
	printf("Cannot install service.\n");
	return;
    }
    rx_StartServer (0);
}
