/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
 * The callback interface to the cache manager.
 */

#include "arla_local.h"
RCSID("$KTH: cmcb.c,v 1.29 2000/11/28 01:50:44 lha Exp $") ;

#include "cb.ss.h"

/*
 * Create an instance of the callback service and start a server.
 */

void
cmcb_init (void)
{
     static struct rx_securityClass *nullSecObjP;
     static struct rx_securityClass *(securityObjects[1]);

     nullSecObjP = rxnull_NewClientSecurityObject ();
     if (nullSecObjP == NULL)
	  arla_errx (1, ADEBWARN, "Cannot create null security object.");
     
     securityObjects[0] = nullSecObjP;

     if (rx_NewService (0, CM_SERVICE_ID, "cm", securityObjects,
			sizeof(securityObjects) / sizeof(*securityObjects),
			RXAFSCB_ExecuteRequest) == NULL)
	  arla_errx (1, ADEBWARN, "Cannot install callback service");
     rx_StartServer (0);
}

/*
 * Just tell the host that we're still alive.
 */

int
RXAFSCB_Probe (struct rx_call *a_rxCallP)
{
     u_long host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));
     struct in_addr in_addr;

     in_addr.s_addr = host;
     arla_warnx (ADEBCALLBACK, "probe (%s)", inet_ntoa(in_addr));
     return 0;
}

/*
 * Throw away all callbacks from the host in `a_rxCallP'.
 */

int
RXAFSCB_InitCallBackState (struct rx_call *a_rxCallP)
{
     u_long host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));
     struct in_addr in_addr;

     cm_check_consistency();

     in_addr.s_addr = host;
     arla_warnx (ADEBCALLBACK, "InitCallBackState (%s)", inet_ntoa(in_addr));
     fcache_purge_host (host);

     cm_check_consistency();

     return 0;
}

/*
 * Handle the callbacks in `a_fidArrayP' and `a_callBackArrayP' (this
 * array can be shorter).
 * There are two types of callbacks:
 * - (volume-id, 0, 0) is a volume callback.
 * - (volume-id, x, y) is a callback on a file.
 */

int
RXAFSCB_CallBack (struct rx_call *a_rxCallP,
		  const AFSCBFids *a_fidArrayP,
		  const AFSCBs *a_callBackArrayP)
{
     int i;
     long cell;
     u_long host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));
     struct in_addr in_addr;

     in_addr.s_addr = host;
     arla_warnx (ADEBCALLBACK, "callback (%s)", inet_ntoa(in_addr));
     cell = conn_host2cell(host, afsport, FS_SERVICE_ID);
     if (cell == -1)
	 arla_warnx (ADEBCALLBACK,
		     "callback from unknown host: %s",
		     inet_ntoa (in_addr));
     for (i = 0; i < a_fidArrayP->len; ++i) {
	  VenusFid fid;
	  AFSCallBack broken_callback = {0, 0, CBDROPPED};

	  fid.fid = a_fidArrayP->val[i];
	  fid.Cell = cell;
	  arla_warnx (ADEBCALLBACK, "%d: (%u, %u, %u)", fid.Cell,
		      fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);

	  /*
	   * Check if it's a volume callback.
	   */

	  if (fid.fid.Vnode == 0 && fid.fid.Unique == 0) {
	      fcache_purge_volume (fid);
	      volcache_invalidate (fid.fid.Volume, fid.Cell);
	  } else if (i < a_callBackArrayP->len)
	      fcache_stale_entry (fid, a_callBackArrayP->val[i]);
	  else
	      fcache_stale_entry (fid, broken_callback);
     }
     cm_check_consistency();

     return 0;
}


int
RXAFSCB_GetLock(struct rx_call *a_rxCallP,
		int32_t index,
		AFSDBLock *lock)
{
    return 1;
}

int
RXAFSCB_GetCE(struct rx_call *a_rxCallP,
	      int32_t index,
	      AFSDBCacheEntry *dbentry)
{
    return 1;
}

int
RXAFSCB_XStatsVersion(struct rx_call *a_rxCallP,
		      int32_t *version)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_GetXStats(struct rx_call *a_rxCallP,
		  int32_t client_version_num,
		  int32_t collection_number,
		  int32_t *server_version_number,
		  int32_t *time,
		  AFSCB_CollData *stats)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_InitCallBackState2(struct rx_call *a_rxCallP, interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_WhoAreYou(struct rx_call *a_rxCallP,
		  interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_InitCallBackState3(struct rx_call *a_rxCallP,
			   const struct afsUUID *serverUuid)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_ProbeUUID(struct rx_call *a_rxCallP,
		  const struct afsUUID *uuid)
{
    return RXGEN_OPCODE;
}
