/*
 * Copyright (c) 1995 - 2000, 2002 Kungliga Tekniska Högskolan
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
RCSID("$arla: cmcb.c,v 1.41 2003/03/06 00:38:47 lha Exp $") ;

#include "cb.ss.h"

/*
 * Create an instance of the callback service and start a server.
 */

static afsUUID arla_client_uuid;

void
cmcb_reinit (void)
{
    if (afsUUID_create(&arla_client_uuid))
	arla_errx(1, ADEBWARN, "Failed to create uuid for client");
}

void
cmcb_init (void)
{
     static struct rx_securityClass *nullSecObjP;
     static struct rx_securityClass *(securityObjects[1]);

     cmcb_reinit ();

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
SRXAFSCB_Probe (struct rx_call *a_rxCallP)
{
     u_long host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));
     struct in_addr in_addr;

     in_addr.s_addr = host;
     arla_warnx (ADEBCALLBACK, "probe (%s)", inet_ntoa(in_addr));
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
SRXAFSCB_CallBack (struct rx_call *a_rxCallP,
		   const AFSCBFids *a_fidArrayP,
		   const AFSCBs *a_callBackArrayP)
{
     int i;
     long cell;
     uint32_t host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));
     struct in_addr in_addr;

     in_addr.s_addr = host;
     arla_warnx (ADEBCALLBACK, "callback (%s)", inet_ntoa(in_addr));
     cell = poller_host2cell(host);
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


/*
 * Throw away all callbacks from the `host'
 */

static void
init_callback_state(uint32_t host)
{
    struct in_addr in_addr;
    
    cm_check_consistency();
    
    in_addr.s_addr = host;
    arla_warnx (ADEBCALLBACK, "InitCallBackState (%s)", inet_ntoa(in_addr));
    fcache_purge_host (host);
    
    cm_check_consistency();
}

/*
 * Init the CallBack address in `addr'. Returns 0 or RXGEN_OPCODE.
 */

static int
init_address(interfaceAddr *addr)
{
    struct ifaddrs *ifa, *ifa0;
    int num_addr;

    memset(addr, 0, sizeof(*addr));

    addr->uuid = arla_client_uuid;
    
    if (getifaddrs(&ifa0) != 0)
	return RXGEN_OPCODE;

    num_addr = 0;

    for (ifa = ifa0; ifa != NULL; ifa = ifa->ifa_next) {
	if (ifa->ifa_addr == NULL)
	    continue;

#if IFF_LOOPBACK
	if (ifa->ifa_flags & IFF_LOOPBACK)
	    continue;
#endif

	switch (ifa->ifa_addr->sa_family) {
	case AF_INET: {
	    struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
	    struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;

	    if (sin->sin_addr.s_addr == htonl(0x7f000001))
		continue;

	    addr->addr_in[num_addr] = sin->sin_addr.s_addr;
	    if (netmask) {
		addr->subnetmask[num_addr] = netmask->sin_addr.s_addr;
	    } else {
		/* dream up something */
		addr->subnetmask[num_addr] = htonl(0xffffff00); 
	    }

	    addr->mtu[num_addr] = 1500; /* XXX */
    
	    num_addr++;
	    break;
	}
	}

	if (num_addr >= AFS_MAX_INTERFACE_ADDR)
	    break;
    }

    freeifaddrs(ifa0);

#if 0
    /* fail if there was no good ipv4 addresses */
    if (num_addr == 0)
	return RXGEN_OPCODE;
#endif

    addr->numberOfInterfaces = num_addr;

    return 0;
}

int
SRXAFSCB_InitCallBackState (struct rx_call *a_rxCallP)
{
    u_long host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));

    init_callback_state(host);
     
    return 0;
}

int
SRXAFSCB_InitCallBackState2 (struct rx_call *a_rxCallP, interfaceAddr *addr)
{
    u_long host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));

    init_callback_state(host);

    return init_address(addr);
}

int
SRXAFSCB_InitCallBackState3 (struct rx_call *a_rxCallP,
			     const struct afsUUID *serverUuid)
{
    u_long host = rx_HostOf (rx_PeerOf (rx_ConnectionOf (a_rxCallP)));

    init_callback_state(host);
    
    return 0;
}

int
SRXAFSCB_WhoAreYou(struct rx_call *a_rxCallP,
		   interfaceAddr *addr)
{
    return init_address(addr);
}

int
SRXAFSCB_ProbeUUID(struct rx_call *a_rxCallP,
		  const struct afsUUID *uuid)
{
    /* the the uuids are equal, we are the host belive we is */

    if (afsUUID_equal(uuid, &arla_client_uuid))
	return 0;
    return 1;
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
    strlcpy(cellName, cell_getthiscell(), AFSNAMEMAX);
    return 0;
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
    int ret;

    memset(addr, 0, sizeof(*addr));

    capabilities->len = 1;
    capabilities->val = malloc(sizeof(capabilities->val[0]));
    if (capabilities->val == NULL)
	return ENOMEM;

    capabilities->val[0] = 0x1; /* UAE */

    ret = init_address(addr);
    if (ret)
	return ret;

    return 0;
}
