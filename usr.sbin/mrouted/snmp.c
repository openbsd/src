/*	$NetBSD: snmp.c,v 1.2 1995/10/09 03:51:58 thorpej Exp $	*/

/*
 * snmp.c
 *
 * Code written by David Thaler <thalerd@eecs.umich.edu>
 * Moved to a seperate file by Bill Fenner <fenner@parc.xerox.com>
 */


#include "defs.h"
#include <string.h>
#include "snmp.h"

#define NUMMIBS 2
#define SNMPD_RETRY_INTERVAL 300 /* periodic snmpd probe interval */

char *mibs[]={ "ipMRouteMIB", "dvmrpMIB" };

extern int o_ipMRouteTable();
extern int o_ipMRouteNextHopTable();
extern int o_dvmrpRouteTable();
extern int o_dvmrpRouteNextHopTable();

       int smux_fd = NOTOK;
       int rock_and_roll = 0;
       int dont_bother_anymore = 0;
       int quantum = 0;
static OID   subtree[NUMMIBS] = NULLOID;
static struct smuxEntry *se = NULL;
extern int smux_errno;
extern char smux_info[BUFSIZ];

/*
 * Place an IP address into an OID starting at element n 
 */
void
put_address(oid, addr, n)
   OID oid;
   u_long addr;
   int n;
{
   int i;

   for (i=n+3; i>=n+0; i--) {
      oid->oid_elements[i] = addr & 0xFF;
      addr >>= 8;
   }
}

/* Get an IP address from an OID starting at element n */
int
get_address(oid, addr, n)
   OID oid;
   u_long *addr;
   int n;
{
   int i;
   int ok = 1;

   (*addr) = 0;

   if (oid -> oid_nelem < n+4)
      return 0;

   for (i=n; i<n+4; i++) {
      (*addr) <<= 8;
      if (i >= oid->oid_nelem)
          ok = 0;
      else
         (*addr) |= oid->oid_elements[i];
   }

   return ok;
}

/*
 *  Attempt to start up SMUX protocol
 */
void
try_smux_init()
{
    if (smux_fd != NOTOK || dont_bother_anymore) 
       return;
    if ((smux_fd = smux_init(debug)) == NOTOK) {
       log(LOG_WARNING, 0,"smux_init: %s [%s]", smux_error(smux_errno), 
        smux_info);
    } else 
       rock_and_roll = 0;
}

/* 
 * Implements scalar objects from both MIBs 
 */
static int 
o_scalar(oi, v, offset)
   OI oi;
   register struct type_SNMP_VarBind *v;
   int offset;
{
    int     ifvar;
    register OID    oid = oi -> oi_name;
    register OT     ot = oi -> oi_type;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
        case type_SNMP_SMUX__PDUs_get__request:
            if (oid -> oid_nelem !=
                        ot -> ot_name -> oid_nelem + 1
                    || oid -> oid_elements[oid -> oid_nelem - 1]
                            != 0)
                return int_SNMP_error__status_noSuchName;
            break;

        case type_SNMP_SMUX__PDUs_get__next__request:
            if (oid -> oid_nelem
                    == ot -> ot_name -> oid_nelem) {
                OID     new;

                if ((new = oid_extend (oid, 1)) == NULLOID)
                    return int_SNMP_error__status_genErr;
                new -> oid_elements[new -> oid_nelem - 1] = 0;

                if (v -> name)
                    free_SNMP_ObjectName (v -> name);
                v -> name = new;
            }
            else
                return NOTOK;
            break;

        default:
            return int_SNMP_error__status_genErr;
    }

    switch (ifvar) {
        case ipMRouteEnable:
            return o_integer (oi, v, 1);

        case dvmrpVersion: {
            static char buff[15];

            sprintf(buff, "mrouted%d.%d", PROTOCOL_VERSION, MROUTED_VERSION);
            return o_string (oi, v, buff, strlen (buff));
        }

        case dvmrpGenerationId:
            return o_integer (oi, v, dvmrp_genid);

        default:
            return int_SNMP_error__status_noSuchName;
    }
}

/* 
 * Find if a specific scoped boundary exists on a Vif
 */
struct vif_acl *
find_boundary(vifi, addr, mask)
   int vifi;
   int addr;
   int mask;
{
   struct vif_acl *n;

   for (n = uvifs[vifi].uv_acl; n != NULL; n = n->acl_next) {
      if (addr == n->acl_addr && mask==n->acl_mask)
         return n;
   }
   return NULL;
}

/*
 * Find the next scoped boundary in order after a given spec
 */
struct vif_acl *
next_boundary(vifi, addr, mask)
   int *vifi;
   int  addr;
   int  mask;
{
   struct vif_acl *bestn, *n;
   int  i;

   for (i = *vifi; i < numvifs; i++) {
      bestn = NULL;
      for (n = uvifs[i].uv_acl; n; n=n->acl_next) {
         if ((i > *vifi || n->acl_addr > addr 
           || (n->acl_addr==addr && n->acl_mask>mask)) 
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
 */
static int  
o_dvmrpBoundaryTable (oi, v, offset)
OI	oi;
register struct type_SNMP_VarBind *v;
{
    int	    ifvar, vifi,
	    addr, mask;
    register OID    oid = oi -> oi_name;
    register OT	   ot = oi -> oi_type;
    struct vif_acl *bound;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
	case type_SNMP_SMUX__PDUs_get__request:
	    if (oid->oid_nelem != ot->ot_name->oid_nelem + 9)
		return int_SNMP_error__status_noSuchName;

      if ((vifi = oid -> oid_elements[ot-> ot_name->oid_nelem]) >= numvifs)
      return int_SNMP_error__status_noSuchName;

      if (!get_address(oid, &addr, ot->ot_name->oid_nelem+1)
       || !get_address(oid, &mask, ot->ot_name->oid_nelem+5))
		return int_SNMP_error__status_noSuchName;

      if (!(bound = find_boundary(vifi, addr, mask)))
		return int_SNMP_error__status_noSuchName;
	    break;

	case type_SNMP_SMUX__PDUs_get__next__request:
	    if (oid->oid_nelem < ot->ot_name->oid_nelem + 9) {
		OID	new;

      if (oid->oid_nelem == ot->ot_name->oid_nelem) {
         vifi = addr = mask = 0;
      } else {
         vifi = oid->oid_elements[ot->ot_name->oid_nelem];
         get_address(oid, &addr, ot->ot_name->oid_nelem+1);
         get_address(oid, &mask, ot->ot_name->oid_nelem+5);
      }

      bound = next_boundary(&vifi,addr,mask);
      if (!bound)
         return NOTOK;

		new = oid_extend (oid, ot->ot_name->oid_nelem+9-oid->oid_nelem);
		if (new == NULLOID)
		    return NOTOK;
		new -> oid_elements[ot->ot_name->oid_nelem] = vifi;
      put_address(new, bound->acl_addr, ot->ot_name->oid_nelem+1);
      put_address(new, bound->acl_mask, ot->ot_name->oid_nelem+5);

		if (v -> name)
		    free_SNMP_ObjectName (v -> name);
		v -> name = new;
	    } else {  /* get next entry given previous */
		int	i = ot -> ot_name -> oid_nelem;

		   vifi = oid->oid_elements[i];
         get_address(oid, &addr, ot->ot_name->oid_nelem+1);
         get_address(oid, &mask, ot->ot_name->oid_nelem+5);
         if (!(bound = next_boundary(&vifi,addr,mask+1)))
            return NOTOK;

         put_address(oid, bound->acl_addr, ot->ot_name->oid_nelem+1);
         put_address(oid, bound->acl_mask, ot->ot_name->oid_nelem+5);
		   oid->oid_elements[i] = vifi;
		   oid->oid_nelem = i + 9;
	    }
	    break;

	default:
	    return int_SNMP_error__status_genErr;
    }

    switch (ifvar) {

   case dvmrpBoundaryVifIndex:
       return o_integer (oi, v, vifi);

	default:
	    return int_SNMP_error__status_noSuchName;
    }
}

/* 
 * Given a vif index and address, return the next greater neighbor entry 
 */
struct listaddr *
next_neighbor(vifi, addr)
   int *vifi;
   int  addr;
{
   struct listaddr *bestn, *n;
   int  i;

   for (i = *vifi; i < numvifs; i++) {
      bestn = NULL;
      for (n = uvifs[i].uv_neighbors; n; n=n->al_next) {
         if ((i > *vifi || n->al_addr > addr) 
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
find_neighbor(vifi, addr)
   int vifi;
   int addr;
{
   struct listaddr *n;

   for (n = uvifs[vifi].uv_neighbors; n != NULL; n = n->al_next) {
      if (addr == n->al_addr)
         return n;
   }
   return NULL;
}

/*
 * Implements the Neighbor Table portion of the DVMRP MIB
 */
static int  
o_dvmrpNeighborTable (oi, v, offset)
OI	oi;
register struct type_SNMP_VarBind *v;
{
    int	    ifvar, vifi,
	    addr;
    register OID    oid = oi -> oi_name;
    register OT	   ot = oi -> oi_type;
    struct listaddr *neighbor;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
	case type_SNMP_SMUX__PDUs_get__request:
	    if (oid->oid_nelem != ot->ot_name->oid_nelem + 5)
		return int_SNMP_error__status_noSuchName;

      if ((vifi = oid -> oid_elements[ot-> ot_name->oid_nelem]) >= numvifs)
      return int_SNMP_error__status_noSuchName;

      if (!get_address(oid, &addr, ot->ot_name->oid_nelem+1))
		return int_SNMP_error__status_noSuchName;

      if (!(neighbor = find_neighbor(vifi, addr)))
		return int_SNMP_error__status_noSuchName;
	    break;

	case type_SNMP_SMUX__PDUs_get__next__request:
	    if (oid->oid_nelem < ot->ot_name->oid_nelem + 5) { 
		OID	new;

      if (oid->oid_nelem == ot->ot_name->oid_nelem) {
         vifi = addr = 0;
      } else {
         vifi = oid->oid_elements[ot->ot_name->oid_nelem];
         get_address(oid, &addr, ot->ot_name->oid_nelem+1);
      }

      neighbor = next_neighbor(&vifi,addr); /* Get first entry */
      if (!neighbor)
         return NOTOK;

		new = oid_extend (oid, ot->ot_name->oid_nelem+5-oid->oid_nelem);
		if (new == NULLOID)
		    return NOTOK;
		new -> oid_elements[ot->ot_name->oid_nelem] = vifi;
      put_address(new, neighbor->al_addr, ot->ot_name->oid_nelem+1);

		if (v -> name)
		    free_SNMP_ObjectName (v -> name);
		v -> name = new;

	    } else {  /* get next entry given previous */
		int	i = ot -> ot_name -> oid_nelem;

		   vifi = oid->oid_elements[i];
         get_address(oid, &addr, ot->ot_name->oid_nelem+1);
         if (!(neighbor = next_neighbor(&vifi,addr+1)))
            return NOTOK;

         put_address(oid, neighbor->al_addr, ot->ot_name->oid_nelem+1);
		   oid->oid_elements[i] = vifi;
		   oid->oid_nelem = i + 5;
	    }
	    break;

	default:
	    return int_SNMP_error__status_genErr;
    }

    switch (ifvar) {

   case dvmrpNeighborUpTime: {
       time_t currtime;
       time(&currtime);
       return o_integer (oi, v, (currtime - neighbor->al_ctime)*100);
   }

   case dvmrpNeighborExpiryTime:
       return o_integer (oi, v, (NEIGHBOR_EXPIRE_TIME-neighbor->al_timer) * 100);

   case dvmrpNeighborVersion: {
       static char buff[15];

       sprintf(buff, "%d.%d", neighbor->al_pv, neighbor->al_mv);
       return o_string (oi, v, buff, strlen (buff));
   }

   case dvmrpNeighborGenerationId: 
       return o_integer (oi, v, neighbor->al_genid);

	default:
	    return int_SNMP_error__status_noSuchName;
    }
}

/*
 * Given a virtual interface number, make sure we have the current
 * kernel information for that Vif.
 */
refresh_vif(v_req, ifnum)
   struct sioc_vif_req *v_req;
   int ifnum;
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
 */
static int  
o_ipMRouteInterfaceTable (oi, v, offset)
OI	oi;
register struct type_SNMP_VarBind *v;
int	offset;
{
    int	    ifnum,
	    ifvar;
    register OID    oid = oi -> oi_name;
    register OT	   ot = oi -> oi_type;
static struct sioc_vif_req v_req;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
	case type_SNMP_SMUX__PDUs_get__request:
	    if (oid -> oid_nelem != ot -> ot_name -> oid_nelem + 1)
		return int_SNMP_error__status_noSuchName;
	    if ((ifnum = oid -> oid_elements[oid -> oid_nelem - 1]) >= numvifs)
		return int_SNMP_error__status_noSuchName;
	    break;

	case type_SNMP_SMUX__PDUs_get__next__request:
	    if (oid -> oid_nelem == ot -> ot_name -> oid_nelem) {
		OID	new;

		ifnum = 0;

		if ((new = oid_extend (oid, 1)) == NULLOID)
		    return NOTOK;
		new -> oid_elements[new -> oid_nelem - 1] = ifnum;

		if (v -> name)
		    free_SNMP_ObjectName (v -> name);
		v -> name = new;

	    } else {
		int	i = ot -> ot_name -> oid_nelem;

		if ((ifnum = oid -> oid_elements[i] + 1) >= numvifs)
		    return NOTOK;

		oid -> oid_elements[i] = ifnum;
		oid -> oid_nelem = i + 1;
	    }
	    break;

	default:
	    return int_SNMP_error__status_genErr;
    }

    switch (ifvar) {
	case ipMRouteInterfaceTtl:
	    return o_integer (oi, v, uvifs[ifnum].uv_threshold);

	case dvmrpVInterfaceType:
      if (uvifs[ifnum].uv_flags & VIFF_SRCRT)
         return o_integer (oi, v, 2); 
      else if (uvifs[ifnum].uv_flags & VIFF_TUNNEL)
         return o_integer (oi, v, 1); 
      else if (uvifs[ifnum].uv_flags & VIFF_QUERIER)
         return o_integer (oi, v, 3); 
      else                               /* SUBNET */
         return o_integer (oi, v, 4); 

	case dvmrpVInterfaceState: 
      if (uvifs[ifnum].uv_flags & VIFF_DISABLED)
         return o_integer (oi, v, 3);
      else if (uvifs[ifnum].uv_flags & VIFF_DOWN)
         return o_integer (oi, v, 2);
      else /* UP */
         return o_integer (oi, v, 1); 

   case dvmrpVInterfaceLocalAddress: {
      struct sockaddr_in tmp;
      tmp.sin_addr.s_addr = uvifs[ifnum].uv_lcl_addr;
      return o_ipaddr (oi, v, &tmp);
   }

   case dvmrpVInterfaceRemoteAddress: {
      struct sockaddr_in tmp;
      tmp.sin_addr.s_addr = (uvifs[ifnum].uv_flags & VIFF_TUNNEL) ?
         uvifs[ifnum].uv_rmt_addr :
         uvifs[ifnum].uv_subnet;
      return o_ipaddr (oi, v, &tmp);
   }

   case dvmrpVInterfaceRemoteSubnetMask: {
      struct sockaddr_in tmp;
      tmp.sin_addr.s_addr = uvifs[ifnum].uv_subnetmask;
      return o_ipaddr (oi, v, &tmp);
   }

	case dvmrpVInterfaceMetric:
	    return o_integer (oi, v, uvifs[ifnum].uv_metric);

	case dvmrpVInterfaceRateLimit:
	    return o_integer (oi, v, uvifs[ifnum].uv_rate_limit);

	case dvmrpVInterfaceInPkts:
       refresh_vif(&v_req, ifnum);
	    return o_integer(oi, v, v_req.icount);

	case dvmrpVInterfaceOutPkts:
       refresh_vif(&v_req, ifnum);
	    return o_integer(oi, v, v_req.ocount);

	case dvmrpVInterfaceInOctets:
       refresh_vif(&v_req, ifnum);
	    return o_integer(oi, v, v_req.ibytes);

	case dvmrpVInterfaceOutOctets:
       refresh_vif(&v_req, ifnum);
	    return o_integer(oi, v, v_req.obytes);

	default:
	    return int_SNMP_error__status_noSuchName;
    }
}

struct mib_variable {
   char     *name;        /* MIB variable name */
   int     (*function)(); /* Function to call */
   int       info;        /* Which variable */
} mib_vars[] = {
 "ipMRouteEnable",               o_scalar, ipMRouteEnable,
 "ipMRouteUpstreamNeighbor",     o_ipMRouteTable,  ipMRouteUpstreamNeighbor,
 "ipMRouteInIfIndex",            o_ipMRouteTable,  ipMRouteInIfIndex,
 "ipMRouteUpTime",               o_ipMRouteTable,  ipMRouteUpTime, 
 "ipMRouteExpiryTime",           o_ipMRouteTable,  ipMRouteExpiryTime,
 "ipMRoutePkts",                 o_ipMRouteTable,  ipMRoutePkts, 
 "ipMRouteDifferentInIfIndexes", o_ipMRouteTable,  ipMRouteDifferentInIfIndexes,
 "ipMRouteOctets",               o_ipMRouteTable,  ipMRouteOctets,
 "ipMRouteProtocol",             o_ipMRouteTable,  ipMRouteProtocol,
 "ipMRouteNextHopState",      o_ipMRouteNextHopTable, ipMRouteNextHopState,
 "ipMRouteNextHopUpTime",     o_ipMRouteNextHopTable, ipMRouteNextHopUpTime,
 "ipMRouteNextHopExpiryTime", o_ipMRouteNextHopTable, ipMRouteNextHopExpiryTime,
 "ipMRouteNextHopClosestMemberHops", o_ipMRouteNextHopTable, ipMRouteNextHopClosestMemberHops,
 "ipMRouteNextHopProtocol",   o_ipMRouteNextHopTable, ipMRouteNextHopProtocol,
 "ipMRouteInterfaceTtl",  o_ipMRouteInterfaceTable, ipMRouteInterfaceTtl,
 "dvmrpVersion",               o_scalar, dvmrpVersion,
 "dvmrpGenerationId",          o_scalar, dvmrpGenerationId,
 "dvmrpVInterfaceType",     o_ipMRouteInterfaceTable, dvmrpVInterfaceType,
 "dvmrpVInterfaceState",    o_ipMRouteInterfaceTable, dvmrpVInterfaceState,
 "dvmrpVInterfaceLocalAddress", o_ipMRouteInterfaceTable, dvmrpVInterfaceLocalAddress,
 "dvmrpVInterfaceRemoteAddress", o_ipMRouteInterfaceTable, dvmrpVInterfaceRemoteAddress,
 "dvmrpVInterfaceRemoteSubnetMask", o_ipMRouteInterfaceTable, dvmrpVInterfaceRemoteSubnetMask,
 "dvmrpVInterfaceMetric",    o_ipMRouteInterfaceTable, dvmrpVInterfaceMetric,
 "dvmrpVInterfaceRateLimit", o_ipMRouteInterfaceTable, dvmrpVInterfaceRateLimit,
 "dvmrpVInterfaceInPkts",    o_ipMRouteInterfaceTable, dvmrpVInterfaceInPkts,
 "dvmrpVInterfaceOutPkts",   o_ipMRouteInterfaceTable, dvmrpVInterfaceOutPkts,
 "dvmrpVInterfaceInOctets",  o_ipMRouteInterfaceTable, dvmrpVInterfaceInOctets,
 "dvmrpVInterfaceOutOctets", o_ipMRouteInterfaceTable, dvmrpVInterfaceOutOctets,
 "dvmrpNeighborUpTime",      o_dvmrpNeighborTable, dvmrpNeighborUpTime,
 "dvmrpNeighborExpiryTime",  o_dvmrpNeighborTable, dvmrpNeighborExpiryTime,
 "dvmrpNeighborVersion",     o_dvmrpNeighborTable, dvmrpNeighborVersion,
 "dvmrpNeighborGenerationId",o_dvmrpNeighborTable, dvmrpNeighborGenerationId,
 "dvmrpRouteUpstreamNeighbor", o_dvmrpRouteTable, dvmrpRouteUpstreamNeighbor,
 "dvmrpRouteInVifIndex",       o_dvmrpRouteTable, dvmrpRouteInVifIndex,
 "dvmrpRouteMetric",           o_dvmrpRouteTable, dvmrpRouteMetric,
 "dvmrpRouteExpiryTime",       o_dvmrpRouteTable, dvmrpRouteExpiryTime,
 "dvmrpRouteNextHopType",    o_dvmrpRouteNextHopTable, dvmrpRouteNextHopType,
 "dvmrpBoundaryVifIndex",    o_dvmrpBoundaryTable, dvmrpBoundaryVifIndex,
 0, 0, 0
};

/*
 * Register variables as part of the MIBs
 */
void
init_mib()
{
   register OT ot;
   int i;

   for (i=0; mib_vars[i].name; i++)
      if (ot=text2obj(mib_vars[i].name)) {
         ot->ot_getfnx = mib_vars[i].function;
         ot->ot_info = (caddr_t)mib_vars[i].info;
      }
}

/*
 * Initialize the SNMP part of mrouted
 */
void
snmp_init()
{
    OT ot;
    int i;

    if (readobjects("mrouted.defs") == NOTOK)
       log(LOG_ERR, 0, "readobjects: %s", PY_pepy);
    for (i=0; i < NUMMIBS; i++) {
       if ((ot = text2obj(mibs[i])) == NULL)
          log(LOG_ERR, 0, "object \"%s\" not in \"%s\"",
		  mibs[i], "mrouted.defs");
       subtree[i] = ot -> ot_name;
    }
    init_mib();
    try_smux_init();
}

/*
 * Process an SNMP "get" or "get-next" request
 */
static  
get_smux (pdu, offset)
   register struct type_SNMP_GetRequest__PDU *pdu;
   int     offset;
{
    int     idx,
            status;
    object_instance ois;
    register struct type_SNMP_VarBindList *vp;
    IFP method;

    quantum = pdu -> request__id;
    idx = 0;
    for (vp = pdu -> variable__bindings; vp; vp = vp -> next) {
        register OI     oi;
        register OT     ot;
        register struct type_SNMP_VarBind *v = vp -> VarBind;

        idx++;

        if (offset == type_SNMP_SMUX__PDUs_get__next__request) {
            if ((oi = name2inst (v -> name)) == NULLOI
                    && (oi = next2inst (v -> name)) == NULLOI)
                goto no_name;

            if ((ot = oi -> oi_type) -> ot_getfnx == NULLIFP)
                goto get_next;
        }
        else
            if ((oi = name2inst (v -> name)) == NULLOI
                    || (ot = oi -> oi_type) -> ot_getfnx
                            == NULLIFP) {
no_name: ;
                pdu -> error__status =
                        int_SNMP_error__status_noSuchName;
                goto out;
            }

try_again: ;
   switch (offset) {
       case type_SNMP_SMUX__PDUs_get__request:
      if (!(method = ot -> ot_getfnx))
          goto no_name;
      break;

       case type_SNMP_SMUX__PDUs_get__next__request:
    if (!(method = ot -> ot_getfnx))
          goto get_next;
      break;

       case type_SNMP_SMUX__PDUs_set__request:
      if (!(method = ot -> ot_setfnx))
          goto no_name;
      break;

       default:
      goto no_name;
   }

        switch (status = (*ot -> ot_getfnx) (oi, v, offset)) {
            case NOTOK:     /* get-next wants a bump */
get_next: ;
                oi = &ois;
                for (;;) {
                    if ((ot = ot -> ot_next) == NULLOT) {
                        pdu -> error__status =
                              int_SNMP_error__status_noSuchName;
                        goto out;
                    }
                    oi -> oi_name =
                                (oi -> oi_type = ot) -> ot_name;
                    if (ot -> ot_getfnx)
                        goto try_again;
                }

            case int_SNMP_error__status_noError:
                break;

            default:
                pdu -> error__status = status;
                goto out;
        }
    }
    idx = 0;

out: ;
    pdu -> error__index = idx;

    if (smux_response (pdu) == NOTOK) {
        log(LOG_WARNING,0,"smux_response: %s [%s]",
               smux_error (smux_errno), smux_info);
        smux_fd = NOTOK;
    }
}

/*
 * Handle SNMP "set" request by replying that it is illegal
 */
static  
set_smux(event)
   struct type_SNMP_SMUX__PDUs *event;
{
    switch (event -> offset) {
        case type_SNMP_SMUX__PDUs_set__request:
            {
                register struct type_SNMP_GetResponse__PDU *pdu =
                                    event -> un.get__response;

                pdu -> error__status = int_SNMP_error__status_noSuchName;
                pdu -> error__index = pdu -> variable__bindings ? 1 : 0;

                if (smux_response (pdu) == NOTOK) {
                    log(LOG_WARNING, 0,
                            "smux_response: %s [%s]",
                            smux_error (smux_errno),
                            smux_info);
                    smux_fd = NOTOK;
                }
            }
            break;

        case type_SNMP_SMUX__PDUs_commitOrRollback:
            {
                struct type_SNMP_SOutPDU *cor =
                                event -> un.commitOrRollback;

                if (cor -> parm == int_SNMP_SOutPDU_commit) {
                                    /* "should not happen" */
                    (void) smux_close (protocolError);
                    smux_fd = NOTOK;
                }
            }
            break;
    }
}

/* 
 *  Handle an incoming SNMP message
 */
void
doit_smux()
{
   struct type_SNMP_SMUX__PDUs *event;
 
   if (smux_wait(&event, NOTOK)==NOTOK) {
      if (smux_errno==inProgress)
         return;
      log(LOG_WARNING, 0, "smux_wait: %s [%s]", smux_error(smux_errno), 
       smux_info);
      smux_fd = NOTOK;
      return;
   }

   switch (event -> offset) {
    case type_SNMP_SMUX__PDUs_registerResponse:
        {
            struct type_SNMP_RRspPDU *rsp =
                        event -> un.registerResponse;

            if (rsp -> parm == int_SNMP_RRspPDU_failure) {
                log(LOG_WARNING,0,"SMUX registration of subtree failed");
                dont_bother_anymore = 1;
                (void) smux_close (goingDown);
                break;
            }
        }
        if (smux_trap(NULLOID, int_SNMP_generic__trap_coldStart, 0,
                       (struct type_SNMP_VarBindList *)0) == NOTOK) {
            log(LOG_WARNING,0,"smux_trap: %s [%s]", smux_error (smux_errno), 
             smux_info);
            break;
        }
        return;

    case type_SNMP_SMUX__PDUs_get__request:
    case type_SNMP_SMUX__PDUs_get__next__request:
        get_smux (event -> un.get__request, event -> offset);
        return;

    case type_SNMP_SMUX__PDUs_close:
        log(LOG_WARNING, 0, "SMUX close: %s", 
         smux_error (event -> un.close -> parm));
        break;

    case type_SNMP_SMUX__PDUs_set__request:
    case type_SNMP_SMUX__PDUs_commitOrRollback:
        set_smux (event);
        return;

    default:
        log(LOG_WARNING,0,"bad SMUX operation: %d", event -> offset);
        (void) smux_close (protocolError);
        break;
   }
   smux_fd = NOTOK;
}

/* 
 * Inform snmpd that we are here and handling our MIBs
 */
void
start_smux()
{
   int i;

   for (i=0; i<NUMMIBS; i++) {
      if ((se = getsmuxEntrybyname (mibs[i])) == NULL) {
         log(LOG_WARNING,0,"no SMUX entry for \"%s\"", mibs[i]);
         return;
      }
 
      /* Only open a new connection the first time through */
      if (!i) {
         if (smux_simple_open(&se->se_identity, mibs[i], 
          se->se_password, strlen(se->se_password))==NOTOK) {
            if (smux_errno == inProgress)
               return;

            log(LOG_WARNING, 0,"smux_simple_open: %s [%s]", 
             smux_error(smux_errno), smux_info);
            smux_fd = NOTOK;
            return;
         }
         log(LOG_NOTICE,0, "SMUX open: %s \"%s\"",
          oid2ode (&se->se_identity), se->se_name);
         rock_and_roll = 1;
      }

      if (smux_register(subtree[i], -1, readWrite)==NOTOK) {
         log(LOG_WARNING, 0,"smux_register: %s [%s]", smux_error(smux_errno), 
          smux_info);
         smux_fd = NOTOK;
         return;
      }
   }
   log(LOG_NOTICE, 0, "SMUX registered");
}

/*
 * Implements the DVMRP Route Table portion of the DVMRP MIB 
 */
int
o_dvmrpRouteTable (oi, v, offset)
OI oi;
register struct type_SNMP_VarBind *v;
int	offset;
{
    u_long   src, mask;
    int	    ifvar;
    register OID    oid = oi -> oi_name;
    register OT	    ot = oi -> oi_type;
    struct rtentry *rt = NULL;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
	case type_SNMP_SMUX__PDUs_get__request:
      if (!get_address(oid, &src, ot->ot_name->oid_nelem)
       || !get_address(oid, &mask, ot->ot_name->oid_nelem+4)
       || !(rt = snmp_find_route(src,mask)))
         return int_SNMP_error__status_noSuchName;
      break;

	case type_SNMP_SMUX__PDUs_get__next__request:

       /* Check if we're requesting the first row */
      if (oid->oid_nelem < ot->ot_name->oid_nelem+8) {
         OID	new;

         /* Get partial specification (if any) */
         get_address(oid, &src, ot->ot_name->oid_nelem);
         get_address(oid, &mask, ot->ot_name->oid_nelem+4);

         if (!next_route(&rt,src,mask)) /* Get first entry */
            return NOTOK;

         /* Extend by 8 more ints to hold index columns */
         new = oid_extend (oid, ot->ot_name->oid_nelem+8-oid->oid_nelem);
         if (new == NULLOID)
            return NOTOK;

         put_address(new, rt->rt_origin,     ot->ot_name->oid_nelem);
         put_address(new, rt->rt_originmask, ot->ot_name->oid_nelem+4); 

         if (v -> name)
            free_SNMP_ObjectName (v -> name);
         v -> name = new;

      /* Else we start from a previous row */
      } else {
         int	i = ot -> ot_name -> oid_nelem;

         /* Get the lowest entry in the table > the given grp/src/mask */
         get_address(oid, &src, ot->ot_name->oid_nelem);
         get_address(oid, &mask, ot->ot_name->oid_nelem+4);
         if (!next_route(&rt, src,mask))
            return NOTOK; 

         put_address(oid, rt->rt_origin, ot->ot_name->oid_nelem);
         put_address(oid, rt->rt_originmask, ot->ot_name->oid_nelem+4);
      }
      break;

	default:
	   return int_SNMP_error__status_genErr;
   }

   switch (ifvar) {
      case dvmrpRouteUpstreamNeighbor: {
         struct sockaddr_in tmp;
         tmp.sin_addr.s_addr = rt->rt_gateway;
         return o_ipaddr (oi, v, &tmp);
      }

      case dvmrpRouteInVifIndex:
         return o_integer (oi, v, rt->rt_parent);

      case dvmrpRouteMetric:
         return o_integer (oi, v, rt->rt_metric);

      case dvmrpRouteExpiryTime:
         return o_integer (oi, v, rt->rt_timer*100);

      default:
         return int_SNMP_error__status_noSuchName;
   }
}

/* 
 * Implements the DVMRP Routing Next Hop Table portion of the DVMRP MIB 
 */
int
o_dvmrpRouteNextHopTable (oi, v, offset)
OI oi;
register struct type_SNMP_VarBind *v;
int   offset;
{
    u_long   src, mask;
    vifi_t   vifi;
    int	    ifvar;
    register OID    oid = oi -> oi_name;
    register OT	    ot = oi -> oi_type;
    struct rtentry *rt = NULL;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
	case type_SNMP_SMUX__PDUs_get__request:
      if (oid->oid_nelem != ot->ot_name->oid_nelem+9)
         return int_SNMP_error__status_noSuchName;

      if (!get_address(oid, &src, ot->ot_name->oid_nelem)
       || !get_address(oid, &mask, ot->ot_name->oid_nelem+4)
       || (!(rt=snmp_find_route(src,mask))))
         return int_SNMP_error__status_noSuchName;

      vifi = oid->oid_elements[ot->ot_name->oid_nelem+8];
      if (!(VIFM_ISSET(vifi, rt->rt_children)))
         return int_SNMP_error__status_noSuchName;
      break;

	case type_SNMP_SMUX__PDUs_get__next__request:

      /* Check if we're requesting the first row */
      if (oid->oid_nelem < ot->ot_name->oid_nelem+9) {
         OID	new;

         get_address(oid, &src, ot->ot_name->oid_nelem);
         get_address(oid, &mask, ot->ot_name->oid_nelem+4);

         /* Find first child vif */
         vifi=0;
         if (!next_route_child(&rt, src, mask, &vifi))
            return NOTOK;

         /* Extend by 9 more ints to hold index columns */
         new = oid_extend (oid, ot->ot_name->oid_nelem+9-oid->oid_nelem);
         if (new == NULLOID)
            return NOTOK;

         put_address(new, rt->rt_origin, ot->ot_name->oid_nelem);
         put_address(new, rt->rt_originmask, ot->ot_name->oid_nelem+4);
         new->oid_elements[ot->ot_name->oid_nelem+8] = vifi;

         if (v -> name)
            free_SNMP_ObjectName (v -> name);
         v -> name = new;

      /* Else we start from a previous row */
      } else {
         int	i = ot -> ot_name -> oid_nelem;

         /* Get the lowest entry in the table > the given grp/src/mask */
         vifi = oid->oid_elements[oid->oid_nelem-1] + 1;
         if (!get_address(oid, &src, ot->ot_name->oid_nelem)
          || !get_address(oid, &mask, ot->ot_name->oid_nelem+4)
          || !next_route_child(&rt, src, mask, &vifi))
            return NOTOK;

         put_address(oid, rt->rt_origin, ot->ot_name->oid_nelem);
         put_address(oid, rt->rt_originmask, ot->ot_name->oid_nelem+4);
         oid->oid_elements[ot->ot_name->oid_nelem+8] = vifi;
      }
      break;

	default:
	   return int_SNMP_error__status_genErr;
   }

   switch (ifvar) {

      case dvmrpRouteNextHopType:
         return o_integer (oi, v, (VIFM_ISSET(vifi, rt->rt_leaves))? 1 : 2);

      default:
         return int_SNMP_error__status_noSuchName;
   }
}

/* 
 * Implements the IP Multicast Route Table portion of the Multicast MIB 
 */
int  
o_ipMRouteTable (oi, v, offset)
OI	oi;
register struct type_SNMP_VarBind *v;
int	offset;
{
    u_long src, grp, mask;
    int	    ifvar;
    register OID    oid = oi -> oi_name;
    register OT	    ot = oi -> oi_type;
    struct gtable *gt = NULL;
    struct stable *st = NULL;
static struct sioc_sg_req sg_req;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
	case type_SNMP_SMUX__PDUs_get__request:
      if (!get_address(oid, &grp, ot->ot_name->oid_nelem)
       || !get_address(oid, &src, ot->ot_name->oid_nelem+4)
       || !get_address(oid, &mask, ot->ot_name->oid_nelem+8)
       || (mask != 0xFFFFFFFF) /* we keep sources now, not subnets */
       || !(gt = find_grp(grp))
       || !(st = find_grp_src(gt,src)))
         return int_SNMP_error__status_noSuchName;
      break;

	case type_SNMP_SMUX__PDUs_get__next__request:

       /* Check if we're requesting the first row */
      if (oid->oid_nelem < ot->ot_name->oid_nelem+12) {
         OID	new;

         /* Get partial specification (if any) */
         get_address(oid, &grp, ot->ot_name->oid_nelem);
         get_address(oid, &src, ot->ot_name->oid_nelem+4);
         get_address(oid, &mask, ot->ot_name->oid_nelem+8);

         if (!next_grp_src_mask(&gt,&st,grp,src,mask)) /* Get first entry */
            return NOTOK;

         /* Extend by 12 more ints to hold index columns */
         new = oid_extend (oid, ot->ot_name->oid_nelem+12-oid->oid_nelem);
         if (new == NULLOID)
            return NOTOK;

         put_address(new, gt->gt_mcastgrp, ot->ot_name->oid_nelem);
         put_address(new, st->st_origin, ot->ot_name->oid_nelem+4);
         put_address(new, 0xFFFFFFFF, ot->ot_name->oid_nelem+8); 

         if (v -> name)
            free_SNMP_ObjectName (v -> name);
         v -> name = new;

      /* Else we start from a previous row */
      } else {
         int	i = ot -> ot_name -> oid_nelem;

         /* Get the lowest entry in the table > the given grp/src/mask */
         get_address(oid, &grp, ot->ot_name->oid_nelem);
         get_address(oid, &src, ot->ot_name->oid_nelem+4);
         get_address(oid, &mask, ot->ot_name->oid_nelem+8);
         if (!next_grp_src_mask(&gt, &st, grp,src,mask))
            return NOTOK; 

         put_address(oid, gt->gt_mcastgrp, ot->ot_name->oid_nelem);
         put_address(oid, st->st_origin, ot->ot_name->oid_nelem+4);
         put_address(oid, 0xFFFFFFFF, ot->ot_name->oid_nelem+8);
      }
      break;

	default:
	   return int_SNMP_error__status_genErr;
   }

   switch (ifvar) {
      case ipMRouteUpstreamNeighbor: {
         struct sockaddr_in tmp;
         tmp.sin_addr.s_addr = gt->gt_route->rt_gateway;
         return o_ipaddr (oi, v, &tmp);
      }

      case ipMRouteInIfIndex:
         return o_integer (oi, v, gt->gt_route->rt_parent);

      case ipMRouteUpTime: {
         time_t currtime;
         time(&currtime);
         return o_integer (oi, v, (currtime - gt->gt_ctime)*100);
      }

      case ipMRouteExpiryTime:
         return o_integer (oi, v, gt->gt_timer*100);

      case ipMRoutePkts:
         refresh_sg(&sg_req, gt, st);
         return o_integer (oi, v, sg_req.pktcnt);
    
      case ipMRouteOctets:
         refresh_sg(&sg_req, gt, st);
         return o_integer (oi, v, sg_req.bytecnt);

      case ipMRouteDifferentInIfIndexes:
         refresh_sg(&sg_req, gt, st);
         return o_integer (oi, v, sg_req.wrong_if);

      case ipMRouteProtocol:
         return o_integer (oi, v, 4);

      default:
         return int_SNMP_error__status_noSuchName;
   }
}

/* 
 * Implements the IP Multicast Routing Next Hop Table portion of the Multicast
 * MIB 
 */
int  
o_ipMRouteNextHopTable (oi, v, offset)
OI	oi;
register struct type_SNMP_VarBind *v;
int	offset;
{
    u_long src, grp, mask, addr;
    vifi_t   vifi;
    int	    ifvar;
    register OID    oid = oi -> oi_name;
    register OT	    ot = oi -> oi_type;
    struct gtable *gt;
    struct stable *st;

    ifvar = (int) ot -> ot_info;
    switch (offset) {
	case type_SNMP_SMUX__PDUs_get__request:
      if (oid->oid_nelem != ot->ot_name->oid_nelem+17)
         return int_SNMP_error__status_noSuchName;

      if (!get_address(oid, &grp, ot->ot_name->oid_nelem)
       || !get_address(oid, &src, ot->ot_name->oid_nelem+4)
       || !get_address(oid, &mask, ot->ot_name->oid_nelem+8)
       || !get_address(oid, &addr, ot->ot_name->oid_nelem+13)
       || grp!=addr
       || mask!=0xFFFFFFFF
       || (!(gt=find_grp(grp)))
       || (!(st=find_grp_src(gt,src))))
         return int_SNMP_error__status_noSuchName;

      vifi = oid->oid_elements[ot->ot_name->oid_nelem+12];
      if (!(VIFM_ISSET(vifi, gt->gt_route->rt_children)))
         return int_SNMP_error__status_noSuchName;
      break;

	case type_SNMP_SMUX__PDUs_get__next__request:

      /* Check if we're requesting the first row */
      if (oid->oid_nelem < ot->ot_name->oid_nelem+17) {
         OID	new;

         get_address(oid, &grp, ot->ot_name->oid_nelem);
         get_address(oid, &src, ot->ot_name->oid_nelem+4);
         get_address(oid, &mask, ot->ot_name->oid_nelem+8);

         /* Find first child vif */
         vifi=0;
         if (!next_child(&gt, &st, grp, src, mask, &vifi))
            return NOTOK;

         /* Extend by 17 more ints to hold index columns */
         new = oid_extend (oid, ot->ot_name->oid_nelem+17-oid->oid_nelem);
         if (new == NULLOID)
            return NOTOK;

         put_address(new, gt->gt_mcastgrp, ot->ot_name->oid_nelem);
         put_address(new, st->st_origin, ot->ot_name->oid_nelem+4);
         put_address(new, 0xFFFFFFFF, ot->ot_name->oid_nelem+8);
         new->oid_elements[ot->ot_name->oid_nelem+12] = vifi;
         put_address(new, gt->gt_mcastgrp, ot->ot_name->oid_nelem+13);

         if (v -> name)
            free_SNMP_ObjectName (v -> name);
         v -> name = new;

      /* Else we start from a previous row */
      } else {
         int	i = ot -> ot_name -> oid_nelem;

         /* Get the lowest entry in the table > the given grp/src/mask */
         vifi = oid->oid_elements[oid->oid_nelem-1] + 1;
         if (!get_address(oid, &grp, ot->ot_name->oid_nelem)
          || !get_address(oid, &src, ot->ot_name->oid_nelem+4)
          || !get_address(oid, &mask, ot->ot_name->oid_nelem+8)
          || !next_child(&gt, &st, grp, src, mask, &vifi))
            return NOTOK;

         put_address(oid, gt->gt_mcastgrp, ot->ot_name->oid_nelem);
         put_address(oid, st->st_origin, ot->ot_name->oid_nelem+4);
         put_address(oid, 0xFFFFFFFF, ot->ot_name->oid_nelem+8);
         oid->oid_elements[ot->ot_name->oid_nelem+12] = vifi;
         put_address(oid, gt->gt_mcastgrp, ot->ot_name->oid_nelem+13);
      }
      break;

	default:
	   return int_SNMP_error__status_genErr;
   }

   switch (ifvar) {

      case ipMRouteNextHopState:
         return o_integer (oi, v, (VIFM_ISSET(vifi, gt->gt_grpmems))? 2 : 1);

      /* Currently equal to ipMRouteUpTime */
      case ipMRouteNextHopUpTime: {
         time_t currtime;
         time(&currtime);
         return o_integer (oi, v, (currtime - gt->gt_ctime)*100);
      }

      case ipMRouteNextHopExpiryTime:
         return o_integer (oi, v, gt->gt_prsent_timer);

      case ipMRouteNextHopClosestMemberHops:
         return o_integer (oi, v, 0);

      case ipMRouteNextHopProtocol:
         return o_integer (oi, v, 4);

      default:
         return int_SNMP_error__status_noSuchName;
   }
}
