/*	$NetBSD: snmp.c,v 1.3 1995/12/10 10:07:16 mycroft Exp $	*/

/*
 * Copyright (c) 1992, 2001 Xerox Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither name of the Xerox, PARC, nor the names of its contributors may be used
 * to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE XEROX CORPORATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"
#include <netinet/in_var.h>
#include "snmp.h"
#include "snmplib/asn1.h"
#include "snmplib/party.h"
#include "snmplib/snmp_impl.h"
#define MROUTED
#include "snmpd/snmp_vars.h"

    in_port_t dest_port = 0;
    int sdlen = 0;

struct addrCache {
    u_long addr;
    int status;
#define UNUSED 0
#define USED   1
#define OLD 2
};

static struct addrCache addrCache[10];

/*
 * Initialize the SNMP part of mrouted
 */
int /* returns: 0 on success, true on error */
snmp_init(in_port_t dest_port)
{
   u_long myaddr;
   int ret;
   struct partyEntry *pp;
   struct sockaddr_in  me;
   int index, sd, portlist[32];

   init_snmp();
   /* init_mib(); why was this here? */
    if (read_party_database("/etc/party.conf") > 0){
   fprintf(stderr, "Couldn't read party database from /etc/party.conf\n");
   exit(0);
    }
    if (read_context_database("/etc/context.conf") > 0){
   fprintf(stderr, "Couldn't read context database from /etc/context.conf\n");
   exit(0);
    }
    if (read_acl_database("/etc/acl.conf") > 0){
   fprintf(stderr, "Couldn't read acl database from /etc/acl.conf\n");
   exit(0);
    }
    if (read_view_database("/etc/view.conf") > 0){
   fprintf(stderr, "Couldn't read view database from /etc/view.conf\n");
   exit(0);
    }

    myaddr = get_myaddr();
    if (ret = agent_party_init(myaddr, ".1.3.6.1")){
   if (ret == 1){
       fprintf(stderr, "Conflict found with initial noAuth/noPriv parties... continuing\n");
   } else if (ret == -1){
       fprintf(stderr, "Error installing initial noAuth/noPriv parties, exiting\n");
       exit(1);
   } else {
       fprintf(stderr, "Unknown error, exiting\n");
       exit(2);
   }
    }

    printf("Opening port(s): ");
    fflush(stdout);
    party_scanInit();
    for(pp = party_scanNext(); pp; pp = party_scanNext()){
   if ((pp->partyTDomain != DOMAINSNMPUDP)
       || bcmp((char *)&myaddr, pp->partyTAddress, 4))
       continue;  /* don't listen for non-local parties */

   dest_port = 0;
   bcopy(pp->partyTAddress + 4, &dest_port, 2);
   for(index = 0; index < sdlen; index++)
       if (dest_port == portlist[index])
      break;
   if (index < sdlen)  /* found a hit before the end of the list */
       continue;
   printf("%u ", dest_port);
   fflush(stdout);
   /* Set up connections */
   sd = socket(AF_INET, SOCK_DGRAM, 0);
   if (sd < 0){
       perror("socket");
       return 1;
   }
   memset(&me, 0, sizeof me);
   me.sin_family = AF_INET;
   me.sin_addr.s_addr = INADDR_ANY;
   /* already in network byte order (I think) */
   me.sin_port = dest_port;
   if (bind(sd, (struct sockaddr *)&me, sizeof(me)) != 0){
       perror("bind");
       return 2;
   }
   register_input_handler(sd, snmp_read_packet);
   portlist[sdlen] = dest_port;
   if (++sdlen == 32){
       printf("No more sockets... ignoring rest of file\n");
       break;
   }
    }
    printf("\n");
    bzero((char *)addrCache, sizeof(addrCache));
}

/*
 * Place an IP address into an OID starting at element n
 */
void
put_address(oid *name, u_long addr, int n)
{
   int i;

   for (i=n+3; i>=n+0; i--) {
      name[i] = addr & 0xFF;
      addr >>= 8;
   }
}

/* Get an IP address from an OID starting at element n */
int
get_address(oid *name, int length, u_long *addr, int n)
{
   int i;
   int ok = 1;

   (*addr) = 0;

   if (length < n+4)
      return 0;

   for (i=n; i<n+4; i++) {
      (*addr) <<= 8;
      if (i >= length)
          ok = 0;
      else
         (*addr) |= name[i];
   }
   return ok;
}

/*
 * Implements scalar objects from DVMRP and Multicast MIBs
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method:  OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_scalar(register struct variable *vp, register oid *name, register int *length,
    int exact, int *var_len, int (**write_method)() )
{
    int result;

    *write_method = 0;
    result = compare(name, *length, vp->name, (int)vp->namelen);
    if ((exact && (result != 0)) || (!exact && (result >= 0)))
   return NULL;

	bcopy((char *)vp->name, (char *)name,
     (int)vp->namelen * sizeof(oid));
	*length = vp->namelen;
	*var_len = sizeof(long);

    switch (vp->magic) {

    case ipMRouteEnable:
       long_return = 1;
       return (u_char *) &long_return;

    case dvmrpVersion: {
       static char buff[15];

       snprintf(buff, sizeof buff, "mrouted%d.%d",
	    PROTOCOL_VERSION, MROUTED_VERSION);
       *var_len = strlen(buff);
       return (u_char *)buff;
    }

    case dvmrpGenerationId:
       long_return = dvmrp_genid;
       return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/*
 * Find if a specific scoped boundary exists on a Vif
 */
struct vif_acl *
find_boundary(vifi_t vifi, u_long addr, u_long mask)
{
   struct vif_acl *n;

   for (n = uvifs[vifi].uv_acl; n != NULL; n = n->acl_next) {
      if (addr == n->acl_addr && mask==n->acl_mask)
         return n;
   }
   return NULL;
}

/*
 * Find the lowest boundary >= (V,A,M) spec
 */
struct vif_acl *
next_boundary(vifi_t *vifi, u_long addr, u_long mask)
{
   struct vif_acl *bestn, *n;
   int  i;

   for (i = *vifi; i < numvifs; i++) {
      bestn = NULL;
      for (n = uvifs[i].uv_acl; n; n=n->acl_next) {
         if ((i > *vifi || n->acl_addr > addr
           || (n->acl_addr == addr && n->acl_mask >= mask))
          && (!bestn || n->acl_addr < bestn->acl_addr
           || (n->acl_addr==bestn->acl_addr && n->acl_mask<bestn->acl_mask)))
            bestn = n;
      }
      if (bestn) {
         *vifi = i;
         return bestn;
      }
   }
   return NULL;
}

/*
 * Implements the Boundary Table portion of the DVMRP MIB
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_dvmrpBoundaryTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    vifi_t     vifi;
    u_long	   addr, mask;
    struct vif_acl *bound;
    oid        newname[MAX_NAME_LEN];
    int        len;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    if (exact) {
	    if (*length != vp->namelen + 9)
		return NULL;

      if ((vifi = name[vp->namelen]) >= numvifs)
      return NULL;

      if (!get_address(name, *length, &addr, vp->namelen+1)
       || !get_address(name, *length, &mask, vp->namelen+5))
		return NULL;

      if (!(bound = find_boundary(vifi, addr, mask)))
		return NULL;

       bcopy((char *)name, (char *)newname, ((int)*length) * sizeof(oid));
	 } else {
       len = *length;
       if (compare(name, *length, vp->name, vp->namelen) < 0)
          len = vp->namelen;

	    if (len < vp->namelen + 9) { /* get first entry */

	         if (len == vp->namelen) {
	            vifi = addr = mask = 0;
	         } else {
	            vifi = name[vp->namelen];
	            get_address(name, len, &addr, vp->namelen+1);
	            get_address(name, len, &mask, vp->namelen+5);
	         }

	           bound = next_boundary(&vifi,addr,mask);
	           if (!bound)
	              return NULL;

		   newname[vp->namelen] = vifi;
	           put_address(newname, bound->acl_addr, vp->namelen+1);
	           put_address(newname, bound->acl_mask, vp->namelen+5);
	    } else {  /* get next entry given previous */
		   vifi = name[vp->namelen];
		   get_address(name, *length, &addr, vp->namelen+1);
		   get_address(name, *length, &mask, vp->namelen+5);

		   if (!(bound = next_boundary(&vifi,addr,mask+1)))
	             return NULL;

		   newname[vp->namelen] = vifi;
		   put_address(newname, bound->acl_addr, vp->namelen+1);
		   put_address(newname, bound->acl_mask, vp->namelen+5);
	    }
    }

    /* Save new OID */
    *length = vp->namelen + 9;
    bcopy((char *)newname, (char *)name, ((int)*length) * sizeof(oid));
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic) {

   case dvmrpBoundaryVifIndex:
       long_return = vifi;
       return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/*
 * Find the lowest neighbor >= (V,A) spec
 */
struct listaddr *
next_neighbor(vifi_t *vifi, u_long addr)
{
   struct listaddr *bestn, *n;
   int  i;

   for (i = *vifi; i < numvifs; i++) {
      bestn = NULL;
      for (n = uvifs[i].uv_neighbors; n; n=n->al_next) {
         if ((i > *vifi || n->al_addr >= addr)
          && (!bestn || n->al_addr < bestn->al_addr))
            bestn = n;
      }
      if (bestn) {
         *vifi = i;
         return bestn;
      }
   }
   return NULL;
}

/*
 * Find a neighbor, if it exists off a given Vif
 */
struct listaddr *
find_neighbor(vifi_t vifi, u_long addr)
{
   struct listaddr *n;

   for (n = uvifs[vifi].uv_neighbors; n != NULL; n = n->al_next) {
      if (addr == n->al_addr)
         return n;
   }
   return NULL;
}

/*
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_dvmrpNeighborTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    vifi_t     vifi;
    u_long     addr, mask;
    struct listaddr *neighbor;
    oid        newname[MAX_NAME_LEN];
    int        len;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    if (exact) {
	    if (*length != vp->namelen + 5)
		return NULL;

      if ((vifi = name[vp->namelen]) >= numvifs)
      return NULL;

      if (!get_address(name, *length, &addr, vp->namelen+1))
		return NULL;

      if (!(neighbor = find_neighbor(vifi, addr)))
		return NULL;

       bcopy((char *)name, (char *)newname, ((int)*length) * sizeof(oid));
	 } else {
       len = *length;
       if (compare(name, *length, vp->name, vp->namelen) < 0)
          len = vp->namelen;

	    if (len < vp->namelen + 5) { /* get first entry */

         if (len == vp->namelen) {
            vifi = addr = 0;
         } else {
            vifi = name[vp->namelen];
            get_address(name, len, &addr, vp->namelen+1);
         }

         neighbor = next_neighbor(&vifi,addr);
         if (!neighbor)
            return NULL;

	    newname[vp->namelen] = vifi;
            put_address(newname, neighbor->al_addr, vp->namelen+1);
	    } else {  /* get next entry given previous */
		   vifi = name[vp->namelen];
         get_address(name, *length, &addr, vp->namelen+1);

         if (!(neighbor = next_neighbor(&vifi,addr+1)))
            return NULL;

		   newname[vp->namelen] = vifi;
         put_address(newname, neighbor->al_addr, vp->namelen+1);
	    }
    }

    /* Save new OID */
    *length = vp->namelen + 5;
    bcopy((char *)newname, (char *)name, ((int)*length) * sizeof(oid));
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic) {

   case dvmrpNeighborUpTime: {
       time_t currtime;
       time(&currtime);
       long_return = (currtime - neighbor->al_ctime)*100;
       return (u_char *) &long_return;
   }

   case dvmrpNeighborExpiryTime:
       long_return = (NEIGHBOR_EXPIRE_TIME - neighbor->al_timer
        + secs_remaining_offset()) * 100;
       return (u_char *) &long_return;

   case dvmrpNeighborVersion: {
       static char buff[15];

       snprintf(buff, sizeof buff, "%d.%d", neighbor->al_pv, neighbor->al_mv);
       *var_len = strlen(buff);
       return (u_char *)buff;
   }

   case dvmrpNeighborGenerationId:
       long_return = neighbor->al_genid;
       return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/* Look up ifIndex given uvifs[ifnum].uv_lcl_addr */
struct in_ifaddr *        /* returns: in_ifaddr structure, or null on error */
ipaddr_to_ifindex(u_long ipaddr, int *ifIndex)
{
    int interface;
static struct in_ifaddr in_ifaddr;

    Interface_Scan_Init();
    for (;;) {
       if (Interface_Scan_Next(&interface, (char *)0, NULL, &in_ifaddr) == 0)
          return NULL;

       if (((struct sockaddr_in *) &(in_ifaddr.ia_addr))->sin_addr.s_addr
        == ipaddr) {
          *ifIndex = interface;
          return &in_ifaddr;
       }
    }
}

/*
 * Find if a specific scoped boundary exists on a Vif
 */
struct listaddr *
find_cache(u_long grp, vifi_t vifi)
{
   struct listaddr *n;

   for (n = uvifs[vifi].uv_groups; n != NULL; n = n->al_next) {
      if (grp == n->al_addr)
         return n;
   }
   return NULL;
}

/*
 * Find the next group cache entry >= (A,V) spec
 */
struct listaddr *
next_cache(u_long addr, vifi_t *vifi)
{
   struct listaddr *bestn=NULL, *n;
   int  i, besti;

   /* Step through all entries looking for the next one */
   for (i = 0; i < numvifs; i++) {
      for (n = uvifs[i].uv_groups; n; n=n->al_next) {
         if ((n->al_addr > addr || (n->al_addr == addr && i >= *vifi))
          && (!bestn || n->al_addr < bestn->al_addr
           || (n->al_addr == bestn->al_addr && i < besti))) {
            bestn = n;
            besti = i;
         }
      }
   }

   if (bestn) {
      *vifi = besti;
      return bestn;
   }
   return NULL;
}

/*
 * Implements the IGMP Cache Table portion of the IGMP MIB
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_igmpCacheTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    vifi_t     vifi;
    u_long     grp;
    int	      ifIndex;
    struct listaddr *cache;
    oid        newname[MAX_NAME_LEN];
    int        len;
    struct in_ifaddr *in_ifaddr;
    struct in_multi   in_multi, *inm;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    if (exact) {
	    if (*length != vp->namelen + 5)
		return NULL;

      if ((vifi = name[vp->namelen+4]) >= numvifs)
      return NULL;

      if (!get_address(name, *length, &grp, vp->namelen))
		return NULL;

      if (!(cache = find_cache(grp, vifi)))
		return NULL;

       bcopy((char *)name, (char *)newname, ((int)*length) * sizeof(oid));
	 } else {
       len = *length;
       if (compare(name, *length, vp->name, vp->namelen) < 0)
          len = vp->namelen;

	  if (len < vp->namelen + 5) { /* get first entry */

	     if (len == vp->namelen) {
		vifi = grp = 0;
             } else {
                get_address(name, len, &grp, vp->namelen);
                vifi = name[vp->namelen+4];
             }

             cache = next_cache(grp,&vifi);
             if (!cache)
                return NULL;

             put_address(newname, cache->al_addr, vp->namelen);
	     newname[vp->namelen+4] = vifi;
	  } else {  /* get next entry given previous */
             get_address(name, *length, &grp, vp->namelen);
	     vifi = name[vp->namelen+4]+1;

             if (!(cache = next_cache(grp,&vifi)))
                return NULL;

             put_address(newname, cache->al_addr, vp->namelen);
	     newname[vp->namelen+4] = vifi;
	  }
    }

    /* Save new OID */
    *length = vp->namelen + 5;
    bcopy((char *)newname, (char *)name, ((int)*length) * sizeof(oid));
    *write_method = 0;
    *var_len = sizeof(long);

    /* Look up ifIndex given uvifs[vifi].uv_lcl_addr */
    in_ifaddr = ipaddr_to_ifindex(uvifs[vifi].uv_lcl_addr, &ifIndex);

    switch (vp->magic) {

   case igmpCacheSelf:
       inm = in_ifaddr->ia_multiaddrs;
       while (inm) {
          klookup( (int)inm, (char *)&in_multi, sizeof(in_multi));

          if (in_multi.inm_addr.s_addr == cache->al_addr) {
             long_return = 1; /* true */
             return (u_char *) &long_return;
          }

          inm = in_multi.inm_next;
       }
       long_return = 2; /* false */
       return (u_char *) &long_return;

   case igmpCacheLastReporter:
       return (u_char *) &cache->al_genid;

   case igmpCacheUpTime: {
      time_t currtime;
      time(&currtime);
      long_return = (currtime - cache->al_ctime)*100;
      return (u_char *) &long_return;
   }

   case igmpCacheExpiryTime:
       long_return = secs_remaining(cache->al_timerid)*100;
       return (u_char *) &long_return;

   case igmpCacheStatus:
       long_return = 1;
       return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/*
 * Implements the IGMP Interface Table portion of the IGMP MIB
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_igmpInterfaceTable(register struct variable *vp, register oid	*name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    oid			newname[MAX_NAME_LEN];
    register int	ifnum;
    int result;
static struct sioc_vif_req v_req;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    /* find "next" interface */
    for(ifnum = 0; ifnum < numvifs; ifnum++){
       if (!(uvifs[ifnum].uv_flags & VIFF_QUERIER))
           continue;
       newname[vp->namelen] = (oid)ifnum;
       result = compare(name, *length, newname, (int)vp->namelen + 1);
       if ((exact && (result == 0)) || (!exact && (result < 0)))
          break;
    }
    if (ifnum >= numvifs)
       return NULL;

    /* Save new OID */
    bcopy((char *)newname, (char *)name, ((int)vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic){

	case igmpInterfaceQueryInterval:
		long_return = GROUP_QUERY_INTERVAL;
      return (u_char *) &long_return;

	case igmpInterfaceStatus:
		long_return = 1; /* active */
      return (u_char *) &long_return;

	default:
	    ERROR("");
    }
    return NULL;
}

/*
 * Given a virtual interface number, make sure we have the current
 * kernel information for that Vif.
 */
refresh_vif(struct sioc_vif_req *v_req, int ifnum)
{
   static   int lastq = -1;

   if (quantum!=lastq || v_req->vifi != ifnum) {
       lastq = quantum;
       v_req->vifi = ifnum;
       if (ioctl(udp_socket, SIOCGETVIFCNT, (char *)v_req) < 0)
          v_req->icount = v_req->ocount = v_req->ibytes = v_req->obytes = 0;
   }
}

/*
 * Implements the Multicast Routing Interface Table portion of the Multicast MIB
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_ipMRouteInterfaceTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    oid			newname[MAX_NAME_LEN];
    register int	ifnum;
    int result;
static struct sioc_vif_req v_req;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    /* find "next" interface */
    for(ifnum = 0; ifnum < numvifs; ifnum++){
	newname[vp->namelen] = (oid)ifnum;
	result = compare(name, *length, newname, (int)vp->namelen + 1);
	if ((exact && (result == 0)) || (!exact && (result < 0)))
	    break;
    }
    if (ifnum >= numvifs)
	return NULL;

    /* Save new OID */
    bcopy((char *)newname, (char *)name, ((int)vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic){

   case ipMRouteInterfaceTtl:
       long_return = uvifs[ifnum].uv_threshold;
       return (u_char *) &long_return;

   case dvmrpVInterfaceType:
      if (uvifs[ifnum].uv_flags & VIFF_SRCRT)
         long_return = 2;
      else if (uvifs[ifnum].uv_flags & VIFF_TUNNEL)
         long_return = 1;
      else if (uvifs[ifnum].uv_flags & VIFF_QUERIER)
         long_return = 3;
      else                               /* SUBNET */
         long_return = 4;
      return (u_char *) &long_return;

   case dvmrpVInterfaceState:
      if (uvifs[ifnum].uv_flags & VIFF_DISABLED)
         long_return = 3;
      else if ((uvifs[ifnum].uv_flags & VIFF_DOWN)
       || ((uvifs[ifnum].uv_flags & VIFF_TUNNEL) && (uvifs[ifnum].uv_neighbors==NULL)))
         long_return = 2;
      else /* UP */
         long_return = 1;
      return (u_char *) &long_return;

   case dvmrpVInterfaceLocalAddress:
      return (u_char *) &uvifs[ifnum].uv_lcl_addr;

   case dvmrpVInterfaceRemoteAddress:
      return (u_char *) ((uvifs[ifnum].uv_flags & VIFF_TUNNEL) ?
         &uvifs[ifnum].uv_rmt_addr :
         &uvifs[ifnum].uv_subnet);

   case dvmrpVInterfaceRemoteSubnetMask:
      return (u_char *) &uvifs[ifnum].uv_subnetmask;

   case dvmrpVInterfaceMetric:
       long_return = uvifs[ifnum].uv_metric;
       return (u_char *) &long_return;

   case dvmrpVInterfaceRateLimit:
       long_return = uvifs[ifnum].uv_rate_limit;
       return (u_char *) &long_return;

   case dvmrpVInterfaceInPkts:
       refresh_vif(&v_req, ifnum);
       long_return = v_req.icount;
       return (u_char *) &long_return;

   case dvmrpVInterfaceOutPkts:
       refresh_vif(&v_req, ifnum);
       long_return = v_req.ocount;
       return (u_char *) &long_return;

   case dvmrpVInterfaceInOctets:
       refresh_vif(&v_req, ifnum);
       long_return = v_req.ibytes;
       return (u_char *) &long_return;

   case dvmrpVInterfaceOutOctets:
       refresh_vif(&v_req, ifnum);
       long_return = v_req.obytes;
       return (u_char *) &long_return;

	default:
	    ERROR("");
    }
    return NULL;
}

/*
 * Implements the DVMRP Route Table portion of the DVMRP MIB
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_dvmrpRouteTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    u_long src, mask;
    oid        newname[MAX_NAME_LEN];
    int        len;
    struct rtentry *rt = NULL;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    if (exact) {
	    if (*length != vp->namelen + 8)
		return NULL;

      if (!get_address(name, *length, &src, vp->namelen)
       || !get_address(name, *length, &mask, vp->namelen+4))
		return NULL;

      if (!(rt = snmp_find_route(src, mask)))
		return NULL;

       bcopy((char *)name, (char *)newname, ((int)*length) * sizeof(oid));
	 } else {
       len = *length;
       if (compare(name, *length, vp->name, vp->namelen) < 0)
          len = vp->namelen;

	    if (len < vp->namelen + 8) { /* get first entry */

         if (len == vp->namelen) {
            src = mask = 0;
         } else {
            get_address(name, len, &src, vp->namelen);
            get_address(name, len, &mask, vp->namelen+4);
         }

         if (!next_route(&rt,src,mask)) /* Get first entry */
            return NULL;

         put_address(newname, rt->rt_origin    , vp->namelen);
         put_address(newname, rt->rt_originmask, vp->namelen+4);
	    } else {  /* get next entry given previous */
         get_address(name, *length, &src,  vp->namelen);
         get_address(name, *length, &mask, vp->namelen+4);

         if (!next_route(&rt, src,mask))
            return NULL;

         put_address(newname, rt->rt_origin,     vp->namelen);
         put_address(newname, rt->rt_originmask, vp->namelen+4);
	    }
    }

    /* Save new OID */
    *length = vp->namelen + 8;
    bcopy((char *)newname, (char *)name, ((int)*length) * sizeof(oid));
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic) {

      case dvmrpRouteUpstreamNeighbor:
         return (u_char *) &rt->rt_gateway;

      case dvmrpRouteInVifIndex:
         long_return = rt->rt_parent;
         return (u_char *) &long_return;

      case dvmrpRouteMetric:
         long_return = rt->rt_metric;
         return (u_char *) &long_return;

      case dvmrpRouteExpiryTime:
         long_return = (ROUTE_EXPIRE_TIME - rt->rt_timer
          + secs_remaining_offset()) * 100;
         return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/*
 * Implements the DVMRP Routing Next Hop Table portion of the DVMRP MIB
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_dvmrpRouteNextHopTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    u_long     src, mask;
    vifi_t     vifi;
    struct rtentry *rt = NULL;
    oid        newname[MAX_NAME_LEN];
    int        len;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    if (exact) {
	    if (*length != vp->namelen + 9)
		return NULL;

      if (!get_address(name, *length, &src, vp->namelen)
       || !get_address(name, *length, &mask, vp->namelen+4)
       || (!(rt=snmp_find_route(src,mask))))
		return NULL;

      vifi = name[vp->namelen+8];
      if (!(VIFM_ISSET(vifi, rt->rt_children)))
      return NULL;

       bcopy((char *)name, (char *)newname, ((int)*length) * sizeof(oid));
	 } else {
       len = *length;
       if (compare(name, *length, vp->name, vp->namelen) < 0)
          len = vp->namelen;

	    if (len < vp->namelen + 9) { /* get first entry */

         get_address(name, len, &src,  vp->namelen);
         get_address(name, len, &mask, vp->namelen+4);

         /* Find first child vif */
         vifi=0;
         if (!next_route_child(&rt, src, mask, &vifi))
            return NULL;

         put_address(newname, rt->rt_origin,     vp->namelen);
         put_address(newname, rt->rt_originmask, vp->namelen+4);
	 newname[vp->namelen+8] = vifi;
	    } else {  /* get next entry given previous */
		   vifi = name[vp->namelen+8] + 1;
         if (!get_address(name, *length, &src,  vp->namelen)
          || !get_address(name, *length, &mask, vp->namelen+4)
          || !next_route_child(&rt, src, mask, &vifi))
            return NULL;

         put_address(newname, rt->rt_origin,     vp->namelen);
         put_address(newname, rt->rt_originmask, vp->namelen+4);
		   newname[vp->namelen+8] = vifi;
	    }
    }

    /* Save new OID */
    *length = vp->namelen + 9;
    bcopy((char *)newname, (char *)name, ((int)*length) * sizeof(oid));
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic) {

    case dvmrpRouteNextHopType:
       long_return = (VIFM_ISSET(vifi, rt->rt_leaves))? 1 : 2;
       return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/*
 * Implements the IP Multicast Route Table portion of the Multicast MIB
 * vp          : IN - pointer to variable entry that points here
 * name        : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_ipMRouteTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    u_long src, grp, mask;
    struct gtable *gt = NULL;
    struct stable *st = NULL;
static struct sioc_sg_req sg_req;
    oid        newname[MAX_NAME_LEN];
    int        len;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    if (exact) {
	    if (*length != vp->namelen + 12)
		return NULL;

      if (!get_address(name, *length, &grp,  vp->namelen)
       || !get_address(name, *length, &src,  vp->namelen+4)
       || !get_address(name, *length, &mask, vp->namelen+8)
       || (mask != 0xFFFFFFFF) /* we keep sources now, not subnets */
       || !(gt = find_grp(grp))
       || !(st = find_grp_src(gt,src)))
		return NULL;

       bcopy((char *)name, (char *)newname, ((int)*length) * sizeof(oid));
	 } else {
       len = *length;
       if (compare(name, *length, vp->name, vp->namelen) < 0)
          len = vp->namelen;

	    if (len < vp->namelen + 12) { /* get first entry */

         get_address(name, len, &grp,  vp->namelen);
         get_address(name, len, &src,  vp->namelen+4);
         get_address(name, len, &mask, vp->namelen+8);

         if (!next_grp_src_mask(&gt,&st,grp,src,mask)) /* Get first entry */
            return NULL;

         put_address(newname, gt->gt_mcastgrp, vp->namelen);
         put_address(newname, st->st_origin,   vp->namelen+4);
         put_address(newname, 0xFFFFFFFF,      vp->namelen+8);
	    } else {  /* get next entry given previous */
         get_address(name, *length, &grp , vp->namelen);
         get_address(name, *length, &src , vp->namelen+4);
         get_address(name, *length, &mask, vp->namelen+8);

         if (!next_grp_src_mask(&gt, &st, grp,src,mask))
            return NULL;

         put_address(newname, gt->gt_mcastgrp, vp->namelen);
         put_address(newname, st->st_origin,   vp->namelen+4);
         put_address(newname, 0xFFFFFFFF,      vp->namelen+8);
	    }
    }

    /* Save new OID */
    *length = vp->namelen + 12;
    bcopy((char *)newname, (char *)name, ((int)*length) * sizeof(oid));
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic) {

      case ipMRouteUpstreamNeighbor:
         return (u_char *) &gt->gt_route->rt_gateway;

      case ipMRouteInIfIndex:
         long_return = gt->gt_route->rt_parent;
         return (u_char *) &long_return;

      case ipMRouteUpTime: {
         time_t currtime;
         time(&currtime);
         long_return = (currtime - gt->gt_ctime)*100;
         return (u_char *) &long_return;
      }

      case ipMRouteExpiryTime:
         long_return = 5*((gt->gt_timer+4)/5); /* round up to nearest 5 */
         long_return = (long_return + secs_remaining_offset()) * 100;
         return (u_char *) &long_return;

      case ipMRoutePkts:
         refresh_sg(&sg_req, gt, st);
         long_return = sg_req.pktcnt;
         return (u_char *) &long_return;

      case ipMRouteOctets:
         refresh_sg(&sg_req, gt, st);
         long_return = sg_req.bytecnt;
         return (u_char *) &long_return;

      case ipMRouteDifferentInIfIndexes:
         refresh_sg(&sg_req, gt, st);
         long_return = sg_req.wrong_if;
         return (u_char *) &long_return;

      case ipMRouteProtocol:
         long_return = 4;
         return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/*
 * Implements the IP Multicast Routing Next Hop Table portion of the Multicast
 * MIB
 * vp          : IN - pointer to variable entry that points here
 * name	       : IN/OUT - input name requested, output name found
 * length      : IN/OUT - length of input and output oid's
 * exact       : IN - TRUE if an exact match was requested.
 * var_len     : OUT - length of variable or 0 if function returned.
 * write_method: OUT - pointer to function to set variable, otherwise 0
 */
u_char *
o_ipMRouteNextHopTable(register struct variable *vp, register oid *name,
    register int *length, int exact, int *var_len, int (**write_method)())
{
    u_long src, grp, mask, addr;
    vifi_t   vifi;
    struct gtable *gt;
    struct stable *st;
    oid        newname[MAX_NAME_LEN];
    int        len;

    /* Copy name OID to new OID */
    bcopy((char *)vp->name, (char *)newname, (int)vp->namelen * sizeof(oid));

    if (exact) {
	    if (*length != vp->namelen + 17)
		return NULL;

      if (!get_address(name, *length, &grp, vp->namelen)
       || !get_address(name, *length, &src, vp->namelen+4)
       || !get_address(name, *length, &mask, vp->namelen+8)
       || !get_address(name, *length, &addr, vp->namelen+13)
       || grp!=addr
       || mask!=0xFFFFFFFF
       || (!(gt=find_grp(grp)))
       || (!(st=find_grp_src(gt,src))))
		return NULL;

      vifi = name[vp->namelen+12];
      if (!(VIFM_ISSET(vifi, gt->gt_route->rt_children)))
      return NULL;

       bcopy((char *)name, (char *)newname, ((int)*length) * sizeof(oid));
	 } else {
       len = *length;
       if (compare(name, *length, vp->name, vp->namelen) < 0)
          len = vp->namelen;

	    if (len < vp->namelen + 17) { /* get first entry */

         get_address(name, len, &grp, vp->namelen);
         get_address(name, len, &src, vp->namelen+4);
         get_address(name, len, &mask, vp->namelen+8);

         /* Find first child vif */
         vifi=0;
         if (!next_child(&gt, &st, grp, src, mask, &vifi))
            return NULL;

         put_address(newname, gt->gt_mcastgrp, vp->namelen);
         put_address(newname, st->st_origin,   vp->namelen+4);
         put_address(newname, 0xFFFFFFFF,      vp->namelen+8);
	 newname[vp->namelen+12] = vifi;
         put_address(newname, gt->gt_mcastgrp, vp->namelen+13);

	    } else {  /* get next entry given previous */
		   vifi = name[vp->namelen+12]+1;
         if (!get_address(name, *length, &grp,  vp->namelen)
          || !get_address(name, *length, &src,  vp->namelen+4)
          || !get_address(name, *length, &mask, vp->namelen+8)
          || !next_child(&gt, &st, grp, src, mask, &vifi))
            return NULL;

         put_address(newname, gt->gt_mcastgrp, vp->namelen);
         put_address(newname, st->st_origin,   vp->namelen+4);
         put_address(newname, 0xFFFFFFFF,      vp->namelen+8);
		   newname[vp->namelen+12] = vifi;
         put_address(newname, gt->gt_mcastgrp, vp->namelen+13);
	    }
    }

    /* Save new OID */
    *length = vp->namelen + 17;
    bcopy((char *)newname, (char *)name, ((int)*length) * sizeof(oid));
    *write_method = 0;
    *var_len = sizeof(long);

    switch (vp->magic) {

      case ipMRouteNextHopState:
         long_return = (VIFM_ISSET(vifi, gt->gt_grpmems))? 2 : 1;
         return (u_char *) &long_return;

      /* Currently equal to ipMRouteUpTime */
      case ipMRouteNextHopUpTime: {
         time_t currtime;
         time(&currtime);
         long_return = (currtime - gt->gt_ctime)*100;
         return (u_char *) &long_return;
      }

      case ipMRouteNextHopExpiryTime:
         long_return = 5*((gt->gt_prsent_timer+4)/5); /* round up to nearest 5*/
         long_return = (long_return + secs_remaining_offset()) * 100;
         return (u_char *) &long_return;

      case ipMRouteNextHopClosestMemberHops:
         long_return = 0;
         return (u_char *) &long_return;

      case ipMRouteNextHopProtocol:
         long_return = 4;
         return (u_char *) &long_return;

    default:
       ERROR("");
    }
    return NULL;
}

/* sync_timer is called by timer() every TIMER_INTERVAL seconds.
 * Its job is to record this time so that we can compute on demand
 * the approx # seconds remaining until the next timer() call
 */
static time_t lasttimer;

void
sync_timer(void)
{
    time(&lasttimer);
}

int /* in range [-TIMER_INTERVAL..0] */
secs_remaining_offset(void)
{
   time_t tm;

   time(&tm);
   return lasttimer-tm;
}
