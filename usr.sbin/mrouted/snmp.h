/*	$NetBSD: snmp.h,v 1.2 1995/10/09 03:52:00 thorpej Exp $	*/

/*
 *    This file contains excepts from ISODE include files, and is 
 *    subject to the following notice:
 *
 * The ISODE is not proprietary, but it is not in the public domain.  This was
 * necessary to include a "hold harmless" clause in the release.  The upshot
 * of all this is that anyone can get a copy of the release and do anything
 * they want with it, but no one takes any responsibility whatsoever for any
 * (mis)use.
 */

typedef u_char    PElementClass;
typedef u_char    PElementForm;
typedef u_short   PElementID;   /* 0..16383 are meaningful (14 bits) */
typedef int       PElementLen;
typedef u_char   *PElementData;
typedef int     (*IFP) ();
#define      INTDEF  long
typedef INTDEF integer;
#undef   IP
typedef int *IP;
#define      NULLIP          ((IP) 0)
#define      NULLIFP         ((IFP) 0)
#define      NULLFD          ((fd_set *) 0)
#define      NULLCP          ((char *) 0)
#define      NULLVP          ((char **) 0)

#ifndef  SFD
#if !defined(SVR3) && !defined(SUNOS4) && !defined(BSD44) && !defined(ultrix)
#define  SFD   int
#define  SFP   IFP
#else
#define  SFD   void
#define  SFP   VFP
#endif
#endif

typedef struct {
   int   pe_type; /* Type of entry */
   integer  pe_ucode;   /* index to user's code if any */
   int   pe_tag;     /* Tag of this entry if any */
   int   pe_flags;   /* Flags */
}  tpe;

typedef struct {
   int   pe_type; /* Type of entry */
   integer  pe_ucode;   /* index to user's code if any */
   int   pe_tag;     /* Tag of this entry if any */
   int   pe_flags;   /* Flags */
   char **pe_typename; /* User defined name of variable */
}  ptpe;

typedef  struct   {
   char  *md_name;   /* Name of this module */
   int   md_nentries;   /* Number of entries */
   tpe   **md_etab;  /* Pointer to encoding tables */
   tpe   **md_dtab;  /* Pointer to decoding tables */
   ptpe    **md_ptab;   /* Pointer to printing tables */
   int   (*md_eucode)();   /* User code for encoding */
   int   (*md_ducode)();   /* User code for decoding */
   int   (*md_pucode)();   /* User code for printing */
   caddr_t  *md_ptrtab; /* pointer table */
}  modtyp;

#define  type_SNMP_ObjectSyntax  PElement
typedef struct PElement {
    int      pe_errno;     /* Error codes */
    int      pe_context;      /* indirect reference */
    PElementClass pe_class;
#define  PE_CLASS_UNIV  0x0   /*   Universal */
    PElementForm  pe_form;
#define  PE_FORM_PRIM   0x0   /*   PRIMitive */
    PElementID pe_id;      /* should be extensible, 14 bits for now */
#define  PE_PRIM_NULL   0x005 /*   Null */
    PElementLen   pe_len;
    PElementLen   pe_ilen;
    union {
   PElementData    un_pe_prim;   /* PRIMitive value */
   struct PElement *un_pe_cons;  /* CONStructor head */
    }                       pe_un1;
    union {
   int       un_pe_cardinal;  /* cardinality of list */
   int       un_pe_nbits;  /* number of bits in string */
    }           pe_un2;
    int      pe_inline;    /* for "ultra-efficient" PElements */
    char   *pe_realbase;   /*   .. */
    int      pe_offset;    /* offset of element in sequence */
    struct PElement *pe_next;
    int      pe_refcnt;    /* hack for ANYs in pepy */
}        *PE;
#define  NULLPE  ((PE) 0)

typedef struct OIDentifier {
    int      oid_nelem;    /* number of sub-identifiers */

    unsigned int *oid_elements;  /* the (ordered) list of sub-identifiers */
}                        OIDentifier, *OID;
#define  NULLOID  ((OID) 0)
#define  type_SNMP_ObjectName OIDentifier

typedef struct object_syntax {
    char   *os_name;       /* syntax name */
    IFP      os_encode;       /* data -> PE */
    IFP      os_decode;       /* PE -> data */
    IFP      os_free;         /* free data */
    IFP      os_parse;        /* str -> data */
    IFP      os_print;        /* data -> tty */
    char  **os_data1;         /* for moresyntax() in snmpi... */
    int      os_data2;        /*   .. */
}     *OS;

typedef struct object_type {
    char   *ot_text;       /* OBJECT DESCRIPTOR */
    char   *ot_id;         /* OBJECT IDENTIFIER */
    OID      ot_name;         /*   .. */
    OS       ot_syntax;       /* SYNTAX */
    int      ot_access;       /* ACCESS */
    u_long  ot_views;         /* for views */
    int      ot_status;       /* STATUS */
    caddr_t ot_info;       /* object information */
    IFP      ot_getfnx;       /* get/get-next method */
    IFP      ot_setfnx;       /* set method */
    caddr_t ot_save;       /* for set method */
    caddr_t ot_smux;       /* for SMUX */
    struct object_type *ot_chain;   /* hash-bucket for text2obj */
    struct object_type *ot_sibling; /* linked-list for name2obj */
    struct object_type *ot_children;   /*   .. */
    struct object_type *ot_next; /* linked-list for get-next */
}     *OT;
#define       NULLOT  ((OT) 0)

typedef struct object_instance {
    OID      oi_name;         /* instance OID */
    OT       oi_type;         /* prototype */
}     object_instance, *OI;
#define       NULLOI  ((OI) 0)

struct type_SNMP_VarBind {
    struct type_SNMP_ObjectName *name;
    struct type_SNMP_ObjectSyntax *value;
};

struct type_SNMP_VarBindList {
        struct type_SNMP_VarBind *VarBind;
        struct type_SNMP_VarBindList *next;
};

#define    type_SNMP_GetRequest__PDU       type_SNMP_PDU
#define    type_SNMP_GetResponse__PDU       type_SNMP_PDU
struct type_SNMP_PDU {
    integer     request__id;
    integer     error__status;
#define  int_SNMP_error__status_noError   0
#define  int_SNMP_error__status_noSuchName   2
#define  int_SNMP_error__status_genErr 5
    integer     error__index;
    struct type_SNMP_VarBindList *variable__bindings;
};

struct type_SNMP_PDUs {
    int         offset;
#define  type_SNMP_PDUs_get__request   1
#define  type_SNMP_PDUs_get__next__request   2
#define  type_SNMP_PDUs_get__response  3
#define	type_SNMP_PDUs_set__request	4
    union {
        struct type_SNMP_GetRequest__PDU *get__request;
        struct type_SNMP_GetNextRequest__PDU *get__next__request;
        struct type_SNMP_GetResponse__PDU *get__response;
        struct type_SNMP_SetRequest__PDU *set__request;
        struct type_SNMP_Trap__PDU *trap;
    }       un;
};

struct type_SNMP_Message {
    integer     version;
#define  int_SNMP_version_version__1   0
    struct qbuf *community;
    struct type_SNMP_PDUs *data;
};

struct type_SNMP_SMUX__PDUs {
    int         offset;
#define  type_SNMP_SMUX__PDUs_close 2
#define  type_SNMP_SMUX__PDUs_registerResponse  4
#define  type_SNMP_SMUX__PDUs_get__request   5
#define  type_SNMP_SMUX__PDUs_get__next__request   6
#define  type_SNMP_SMUX__PDUs_set__request   8
#define  type_SNMP_SMUX__PDUs_commitOrRollback  10
    union {
        struct type_SNMP_SimpleOpen *simple;
        struct type_SNMP_ClosePDU *close;
        struct type_SNMP_RReqPDU *registerRequest;
        struct type_SNMP_RRspPDU *registerResponse;
        struct type_SNMP_GetRequest__PDU *get__request;
        struct type_SNMP_GetNextRequest__PDU *get__next__request;
        struct type_SNMP_GetResponse__PDU *get__response;
        struct type_SNMP_SetRequest__PDU *set__request;
        struct type_SNMP_Trap__PDU *trap;
        struct type_SNMP_SOutPDU *commitOrRollback;
    }       un;
};

struct type_SNMP_RReqPDU {
    struct type_SNMP_ObjectName *subtree;
    integer     priority;
    integer     operation;
#define  int_SNMP_operation_readWrite  2
};

struct type_SNMP_ClosePDU {
    integer     parm;
#define  int_SNMP_ClosePDU_goingDown   0
#define  int_SNMP_ClosePDU_protocolError  3
};

struct type_SNMP_RRspPDU {
    integer     parm;
#define  int_SNMP_RRspPDU_failure   -1
};

struct type_SNMP_SOutPDU {
    integer     parm;
#define  int_SNMP_SOutPDU_commit 0
};

struct type_SNMP_Trap__PDU {
    OID     enterprise;
    struct type_SNMP_NetworkAddress *agent__addr;
    integer     generic__trap;
#define  int_SNMP_generic__trap_coldStart 0
    integer     specific__trap;
    struct type_SNMP_TimeTicks *time__stamp;
    struct type_SNMP_VarBindList *variable__bindings;
};

struct smuxEntry {
    char   *se_name;
    OIDentifier se_identity;
    char   *se_password;
    int      se_priority;
};

typedef struct {
    int	    ps_errno;		/* Error codes */
#define	PS_ERR_NONE	 0	/*   No error */
#define	PS_ERR_OVERID	 1	/*   Overflow in ID */
#define	PS_ERR_OVERLEN	 2	/*   Overflow in length */
#define	PS_ERR_NMEM	 3	/*   Out of memory */
#define	PS_ERR_EOF	 4	/*   End of file */
#define	PS_ERR_EOFID	 5	/*   End of file reading extended ID */
#define	PS_ERR_EOFLEN	 6	/*   End of file reading extended length */
#define	PS_ERR_LEN	 7	/*   Length mismatch */
#define	PS_ERR_TRNC	 8	/*   Truncated */
#define	PS_ERR_INDF	 9	/*   Indefinite length in primitive form */
#define	PS_ERR_IO	10	/*   I/O error */
#define	PS_ERR_EXTRA	11	/*   Extraneous octets */
#define	PS_ERR_XXX	12	/*   XXX */
    union {
	caddr_t un_ps_addr;
	struct {
	    char   *st_ps_base;
	    int	    st_ps_cnt;
	    char   *st_ps_ptr;
	    int	    st_ps_bufsiz;
	}			un_ps_st;
	struct {
	    struct udvec *uv_ps_head;
	    struct udvec *uv_ps_cur;
	    struct udvec *uv_ps_end;
	    int	    uv_ps_elems;
	    int	    uv_ps_slop;
	    int	    uv_ps_cc;
	}			un_ps_uv;
    }                       ps_un;
#define	ps_addr	ps_un.un_ps_addr
#define	ps_base	ps_un.un_ps_st.st_ps_base
#define	ps_cnt	ps_un.un_ps_st.st_ps_cnt
#define	ps_ptr	ps_un.un_ps_st.st_ps_ptr
#define	ps_bufsiz	ps_un.un_ps_st.st_ps_bufsiz
#define	ps_head	ps_un.un_ps_uv.uv_ps_head
#define	ps_cur	ps_un.un_ps_uv.uv_ps_cur
#define	ps_end	ps_un.un_ps_uv.uv_ps_end
#define	ps_elems	ps_un.un_ps_uv.uv_ps_elems
#define	ps_slop	ps_un.un_ps_uv.uv_ps_slop
#define	ps_cc	ps_un.un_ps_uv.uv_ps_cc
    caddr_t ps_extra;		/* for George's recursive PStreams */
    int	    ps_inline;		/* for "ultra-efficient" PStreams */
    int	    ps_scratch;		/* XXX */
    int	    ps_byteno;		/* byte position */
    IFP	    ps_primeP;
    IFP	    ps_readP;
    IFP	    ps_writeP;
    IFP	    ps_flushP;
    IFP	    ps_closeP;
}			PStream, *PS;
#define	NULLPS	((PS) 0)

struct NSAPaddr {		/* this structure shouldn't have holes in it */
    long     na_stack;			/* TS-stack */
#define	NA_TCP	1			/*   RFC1006/TCP */
    long    na_community;		/* internal community # */
    union {
	struct na_nsap {		/* real network service */
#define	NASIZE	64			/* 20 ought to do it */
	    char    na_nsap_address[NASIZE];
	    char    na_nsap_addrlen;
	}               un_na_nsap;
	struct na_tcp {			/* emulation via RFC1006 */
#define	NSAP_DOMAINLEN	63
	    char    na_tcp_domain[NSAP_DOMAINLEN + 1];
	    u_short na_tcp_port;	/* non-standard TCP port */
	    u_short na_tcp_tset;	/* transport set */
#define	NA_TSET_TCP	0x0001		/*   .. TCP */
#define	NA_TSET_UDP	0x0002	        /*   .. UDP */
	}               un_na_tcp;
	struct na_x25 {			/* X.25 (assume single subnet) */
#define	NSAP_DTELEN	36
	    char    na_x25_dte[NSAP_DTELEN + 1]; /* Numeric DTE + Link */
	    char    na_x25_dtelen;	/* number of digits used */

/* Conventionally, the PID sits at the first head bytes of user data and so
 * should probably not be mentioned specially. A macro might do it, if
 * necessary.
 */
#define	NPSIZE	4
	    char    na_x25_pid[NPSIZE];	/* X.25 protocol id */
	    char    na_x25_pidlen;	/*   .. */
#define	CUDFSIZE 16
	    char    na_x25_cudf[CUDFSIZE];/* call user data field */
	    char    na_x25_cudflen;	/* .. */
/*
 * X25 Facilities field. 
 */
#define	FACSIZE	6
	    char    na_x25_fac[FACSIZE];	/* X.25 facilities */
	    char    na_x25_faclen;		/*   .. */
	}               un_na_x25;
    }               na_un;
#define	na_address	na_un.un_na_nsap.na_nsap_address
#define	na_addrlen	na_un.un_na_nsap.na_nsap_addrlen
#define	na_domain	na_un.un_na_tcp.na_tcp_domain
#define	na_port		na_un.un_na_tcp.na_tcp_port
#define	na_tset		na_un.un_na_tcp.na_tcp_tset
#define	na_dte		na_un.un_na_x25.na_x25_dte
#define	na_dtelen	na_un.un_na_x25.na_x25_dtelen
#define	na_pid		na_un.un_na_x25.na_x25_pid
#define	na_pidlen	na_un.un_na_x25.na_x25_pidlen
#define	na_cudf		na_un.un_na_x25.na_x25_cudf
#define	na_cudflen	na_un.un_na_x25.na_x25_cudflen
#define	na_fac		na_un.un_na_x25.na_x25_fac
#define	na_faclen	na_un.un_na_x25.na_x25_faclen
/* for backwards compatibility... these two will be removed after ISODE 7.0 */
#define	na_type		na_stack
#define	na_subnet	na_community
};

struct TSAPaddr {
#define  NTADDR   8        /* according to NIST OIW */
    struct NSAPaddr ta_addrs[NTADDR];  /* choice of network addresses */
    int     ta_naddr;
#define  TSSIZE   64
    int      ta_selectlen;
    union un_ta_type {           /* TSAP selector */
   char    ta_un_selector[TSSIZE];
   u_short ta_un_port;
    }               un_ta;
#define  ta_selector un_ta.ta_un_selector
#define  ta_port     un_ta.ta_un_port
};

struct qbuf {
    struct qbuf *qb_forw;  /* doubly-linked list */
    struct qbuf *qb_back;  /*   .. */
    int      qb_len;    /* length of data */
    char   *qb_data;    /* current pointer into data */
    char    qb_base[1];    /* extensible... */
};

#define  start_udp_client        start_udp_server
#define  read_udp_socket      read_dgram_socket
#define  write_udp_socket  write_dgram_socket
#define  close_udp_socket  close_dgram_socket
#define  check_udp_socket        check_dgram_socket
#define  free_SNMP_ObjectName oid_free
#define  o_ipaddr(oi,v,value) o_specific ((oi), (v), (caddr_t) (value))
#define  o_integer(oi,v,value)   o_longword ((oi), (v), (integer) (value))
#define  oid2ode(i)  oid2ode_aux ((i), 1)
#define  ps2pe(ps)               ps2pe_aux ((ps), 1, 1)
#define  pe2ps(ps, pe)           pe2ps_aux ((ps), (pe), 1)
#define  str2vec(s,v)    str2vecX ((s), (v), 0, NULLIP, NULL, 1)
#define  free_SNMP_Message(parm)\
   (void) fre_obj((char *) parm, _ZSNMP_mod.md_dtab[_ZMessageSNMP], &_ZSNMP_mod, 1)
#define encode_SNMP_Message(pe, top, len, buffer, parm) \
    enc_f(_ZMessageSNMP, &_ZSNMP_mod, pe, top, len, buffer, (char *) parm)
#define print_SNMP_Message(pe, top, len, buffer, parm) \
    prnt_f(_ZMessageSNMP, &_ZSNMP_mod, pe, top, len, buffer)
#define decode_SNMP_Message(pe, top, len, buffer, parm) \
    dec_f(_ZMessageSNMP, &_ZSNMP_mod, pe, top, len, buffer, (char **) parm)
#define  inaddr_copy(hp,sin) \
    bcopy ((hp) -> h_addr, (char *) &((sin) -> sin_addr), (hp) -> h_length)
#define  join_udp_server(fd,sock) \
      join_dgram_aux ((fd), (struct sockaddr *) (sock), 0)

#define MAXDGRAM        8192
#define      NOTOK           (-1)
#define      OK              0
#define  NVEC  100
#define  invalidOperation        (-1)
#define  parameterMissing        (-2)
#define  systemError             (-3)
#define  youLoseBig              (-4)
#define  congestion              (-5)
#define  inProgress              (-6)
#define  protocolError     int_SNMP_ClosePDU_protocolError
#define  goingDown      int_SNMP_ClosePDU_goingDown
#define  readWrite   int_SNMP_operation_readWrite

OID   oid_extend(), text2oid (), oid_cpy ();
OT    text2obj ();
OI    name2inst (), next2inst (), text2inst ();
OS    text2syn ();
PS    ps_alloc ();
PE    pe_alloc (), ps2pe_aux ();
struct smuxEntry *getsmuxEntrybyname ();
struct hostent *gethostbystring ();
char   *getlocalhost (), *oid2ode_aux ();
struct TSAPaddr *str2taddr ();  /* string encoding to TSAPaddr */
int   dg_open (), read_dgram_socket (), write_dgram_socket ();
int   check_dgram_socket (), pe2ps_aux ();
struct qbuf *str2qb ();

integer     request__id;
extern char PY_pepy[];
extern int quantum;
extern int  ts_comm_tcp_default, ps_len_strategy;
extern modtyp  _ZSNMP_mod;
#define _ZMessageSNMP   0

#define  PS_LEN_LONG 2

/* Scalars */
#define ipMRouteEnable                   0

/* IP Multicast Route Table */
#define ipMRouteUpstreamNeighbor         0
#define ipMRouteInIfIndex                1
#define ipMRouteUpTime                   2
#define ipMRouteExpiryTime               3
#define ipMRoutePkts                     4
#define ipMRouteDifferentInIfIndexes     5
#define ipMRouteOctets                   6
#define ipMRouteProtocol                 7

/* IP Multicast Routing Next Hop Table */
#define ipMRouteNextHopState             0
#define ipMRouteNextHopUpTime            1
#define ipMRouteNextHopExpiryTime        2
#define ipMRouteNextHopClosestMemberHops 3
#define ipMRouteNextHopProtocol          4

/* Multicast Routing Interface Table */
#define ipMRouteInterfaceTtl             0

/* Scalars (cont.) */
#define dvmrpVersion                     1
#define dvmrpGenerationId                2

/* DVMRP Virtual Interface Table */
#define dvmrpVInterfaceType              1
#define dvmrpVInterfaceState             2
#define dvmrpVInterfaceLocalAddress      3
#define dvmrpVInterfaceRemoteAddress     4
#define dvmrpVInterfaceRemoteSubnetMask  5
#define dvmrpVInterfaceMetric            6
#define dvmrpVInterfaceRateLimit         7
#define dvmrpVInterfaceInPkts            8
#define dvmrpVInterfaceOutPkts           9
#define dvmrpVInterfaceInOctets         10
#define dvmrpVInterfaceOutOctets        11

/* DVMRP Neighbor Table */
#define dvmrpNeighborUpTime              0
#define dvmrpNeighborExpiryTime          1
#define dvmrpNeighborVersion             2
#define dvmrpNeighborGenerationId        3

/* DVMRP Route Table */
#define dvmrpRouteUpstreamNeighbor       0
#define dvmrpRouteInVifIndex             1
#define dvmrpRouteMetric                 2
#define dvmrpRouteExpiryTime             3

/* DVMRP Routing Next Hop Table */
#define dvmrpRouteNextHopType            0

/* Boundary Table */
#define dvmrpBoundaryVifIndex            0

#define SNMPD_RETRY_INTERVAL 300 /* periodic snmpd probe interval */
extern int smux_fd;
extern int rock_and_roll;
extern int dont_bother_anymore;
