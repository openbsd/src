/* $openbsd: dsdt.c,v 1.47 2006/06/14 16:30:07 canacar Exp $ */
/*
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#endif

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

/* XXX: endian macros */
#define aml_letohost16(x) x
#define aml_letohost32(x) x
#define aml_letohost64(x) x

#define opsize(opcode) (((opcode) &  0xFF00) ? 2 : 1)

#define AML_CHECKSTACK()

#define AML_FIELD_RESERVED  0x00
#define AML_FIELD_ATTRIB    0x01

#define AML_REVISION	 0x01
#define AML_INTSTRLEN	 16
#define AML_NAMESEG_LEN	 4

#define AML_MAX_ARG	 8
#define AML_MAX_LOCAL	 8

#define AML_BYTE	 'b'
#define AML_WORD	 'w'
#define AML_DWORD	 'd'
#define AML_QWORD	 'q'
#define AML_ANYINT	 'i'

#define aml_valid(pv)	 ((pv) != NULL)

#define acpi_mutex_acquire(ctx,lock,iv)	 dnprintf(60,"ACQUIRE: %x" #lock "\n", (short)iv)
#define acpi_mutex_release(ctx,lock)	 dnprintf(60,"RELEASE: " #lock "\n")

#define aml_ipaddr(n) ((n)-aml_root.start)

struct aml_scope;

/* New jordan code */
int                     aml_cmpvalue(struct aml_value *, struct aml_value *, int);
void                    aml_copyvalue(struct aml_value *, struct aml_value *);

void                    aml_setvalue(struct aml_scope *, struct aml_value *, struct aml_value *, int64_t);
void                    aml_freevalue(struct aml_value *);
struct aml_value       *aml_allocvalue(int, int64_t, const void *);
struct aml_value       *_aml_setvalue(struct aml_value *, int, int64_t, const void *);

u_int64_t		aml_convradix(u_int64_t, int, int);
int64_t			aml_evalexpr(int64_t, int64_t, int);
int     		aml_lsb(u_int64_t);
int			aml_msb(u_int64_t);

void                    aml_dump(int, u_int8_t *);
int                     aml_tstbit(const u_int8_t *, int);
void                    aml_setbit(u_int8_t *, int, int);

void                    aml_bufcpy(void *, int, const void *, int, int);

void aml_delref(struct aml_value **);
void aml_addref(struct aml_value *);
int aml_pc(uint8_t *);

struct aml_value *aml_parseop(struct aml_scope *, struct aml_value *);
struct aml_value *aml_parsetarget(struct aml_scope *, struct aml_value *, struct aml_value **);
struct aml_value *aml_parseterm(struct aml_scope *, struct aml_value *);

struct aml_value *aml_evaltarget(struct aml_scope *scope, struct aml_value *res);
int aml_evalterm(struct aml_scope *scope, struct aml_value *raw, struct aml_value *dst);


void aml_gasio(struct acpi_softc *, int, uint64_t, uint64_t,
	       int, int, int, void *, int);


struct aml_opcode      *aml_findopcode(int);

void *_acpi_os_malloc(size_t, const char *, int);
void  _acpi_os_free(void *, const char *, int);

struct aml_value *aml_evalmethod(struct aml_node *, int, struct aml_value *, struct aml_value *);

/*
 * @@@: Global variables 
 */
int                     aml_intlen=64;
struct aml_node		aml_root;
struct aml_value       *aml_global_lock;
struct acpi_softc      *dsdt_softc;

/* Perfect hash function for valid AML bytecodes */
#define HASH_OFF         6904
#define HASH_SIZE        179
#define HASH_KEY(k)      (((k) ^ HASH_OFF) % HASH_SIZE)

#define HASH_MAGIC(v)   (0xC0DE0000L + (v))
#define HTE(v,f...)     [ HASH_KEY(v) ] { HASH_MAGIC(v), f }

struct aml_opcode aml_table[] = {
	/* Simple types */
	HTE(AMLOP_ZERO,		  "Zero",	     "c"  ),
	HTE(AMLOP_ONE,		  "One",	     "c"  ),
	HTE(AMLOP_ONES,		  "Ones",	     "c"  ),
	HTE(AMLOP_REVISION,	  "Revision",	     "R"  ),
	HTE(AMLOP_BYTEPREFIX,	  ".Byte",	     "b"  ),
	HTE(AMLOP_WORDPREFIX,	  ".Word",	     "w"  ),
	HTE(AMLOP_DWORDPREFIX,	  ".DWord",	     "d"  ),
	HTE(AMLOP_QWORDPREFIX,	  ".QWord",	     "q"  ),
	HTE(AMLOP_STRINGPREFIX,	  ".String",	     "a"  ),
	HTE(AMLOP_DEBUG,	  "DebugOp",	     "D", ),
	HTE(AMLOP_BUFFER,	  "Buffer",	     "piB" ),
	HTE(AMLOP_PACKAGE,	  "Package",	     "pbT" ),
	HTE(AMLOP_VARPACKAGE,	  "VarPackage",	     "piT" ),

	/* Simple objects */
	HTE(AMLOP_LOCAL0,	  "Local0",	     "L",    ),
	HTE(AMLOP_LOCAL1,	  "Local1",	     "L",    ),
	HTE(AMLOP_LOCAL2,	  "Local2",	     "L",    ),
	HTE(AMLOP_LOCAL3,	  "Local3",	     "L",    ),
	HTE(AMLOP_LOCAL4,	  "Local4",	     "L",    ),
	HTE(AMLOP_LOCAL5,	  "Local5",	     "L",    ),
	HTE(AMLOP_LOCAL6,	  "Local6",	     "L",    ),
	HTE(AMLOP_LOCAL7,	  "Local7",	     "L",    ),
	HTE(AMLOP_ARG0,		  "Arg0",	     "A",    ),
	HTE(AMLOP_ARG1,		  "Arg1",	     "A",    ),
	HTE(AMLOP_ARG2,		  "Arg2",	     "A",    ),
	HTE(AMLOP_ARG3,		  "Arg3",	     "A",    ),
	HTE(AMLOP_ARG4,		  "Arg4",	     "A",    ),
	HTE(AMLOP_ARG5,		  "Arg5",	     "A",    ),
	HTE(AMLOP_ARG6,		  "Arg6",	     "A",    ),

	/* Control flow */
	HTE(AMLOP_IF,		  "If",		     "pI",   ),
	HTE(AMLOP_ELSE,		  "Else",	     "pT",   ),
	HTE(AMLOP_WHILE,	  "While",	     "piT",  ),
	HTE(AMLOP_BREAK,	  "Break",	     "",     ),
	HTE(AMLOP_CONTINUE,	  "Continue",	     "",     ),
	HTE(AMLOP_RETURN,	  "Return",	     "t",     ),
	HTE(AMLOP_FATAL,	  "Fatal",	     "bdi", ),
	HTE(AMLOP_NOP,		  "Nop",	     "",    ),
	HTE(AMLOP_BREAKPOINT,	  "BreakPoint",	     "",    ),

	/* Arithmetic operations */
	HTE(AMLOP_INCREMENT,	  "Increment",	     "t",     ),
	HTE(AMLOP_DECREMENT,	  "Decrement",	     "t",     ),
	HTE(AMLOP_ADD,		  "Add",	     "iir",   ),
	HTE(AMLOP_SUBTRACT,	  "Subtract",	     "iir",   ),
	HTE(AMLOP_MULTIPLY,	  "Multiply",	     "iir",   ),
	HTE(AMLOP_DIVIDE,	  "Divide",	     "iirr",  ),
	HTE(AMLOP_SHL,		  "ShiftLeft",	     "iir",   ),
	HTE(AMLOP_SHR,		  "ShiftRight",	     "iir",   ),
	HTE(AMLOP_AND,		  "And",	     "iir",   ),
	HTE(AMLOP_NAND,		  "Nand",	     "iir",   ),
	HTE(AMLOP_OR,		  "Or",		     "iir",   ),
	HTE(AMLOP_NOR,		  "Nor",	     "iir",   ),
	HTE(AMLOP_XOR,		  "Xor",	     "iir",   ),
	HTE(AMLOP_NOT,		  "Not",	     "ir",    ),
	HTE(AMLOP_MOD,		  "Mod",	     "iir",   ),
	HTE(AMLOP_FINDSETLEFTBIT, "FindSetLeftBit",  "ir",    ),
	HTE(AMLOP_FINDSETRIGHTBIT,"FindSetRightBit", "ir",    ),

	/* Logical test operations */
	HTE(AMLOP_LAND,		  "LAnd",	     "ii",    ),
	HTE(AMLOP_LOR,		  "LOr",	     "ii",    ),
	HTE(AMLOP_LNOT,		  "LNot",	     "i",     ),
	HTE(AMLOP_LNOTEQUAL,	  "LNotEqual",	     "tt",    ),
	HTE(AMLOP_LLESSEQUAL,	  "LLessEqual",	     "tt",    ),
	HTE(AMLOP_LGREATEREQUAL,  "LGreaterEqual",   "tt",    ),
	HTE(AMLOP_LEQUAL,	  "LEqual",	     "tt",    ),
	HTE(AMLOP_LGREATER,	  "LGreater",	     "tt",    ),
	HTE(AMLOP_LLESS,	  "LLess",	     "tt",    ),

	/* Named objects */
	HTE(AMLOP_NAMECHAR,	  ".NameRef",	     "n" ),
	HTE(AMLOP_ALIAS,	  "Alias",	     "nN",  ),
	HTE(AMLOP_NAME,		  "Name",	     "Nt",  ),
	HTE(AMLOP_EVENT,	  "Event",	     "N",   ),
	HTE(AMLOP_MUTEX,	  "Mutex",	     "Nb",  ),
	HTE(AMLOP_DATAREGION,	  "DataRegion",	     "Nttt" ),
	HTE(AMLOP_OPREGION,	  "OpRegion",	     "Nbii" ),
	HTE(AMLOP_SCOPE,	  "Scope",	     "pNT"  ),
	HTE(AMLOP_DEVICE,	  "Device",	     "pNT"  ),
	HTE(AMLOP_POWERRSRC,	  "Power Resource",  "pNbwT" ),
	HTE(AMLOP_THERMALZONE,	  "ThermalZone",     "pNT" ),
	HTE(AMLOP_PROCESSOR,	  "Processor",	     "pNbdbT", ),
	HTE(AMLOP_METHOD,	  "Method",	     "pNfM",  ),

	/* Field operations */
	HTE(AMLOP_FIELD,	    "Field",	       "pnfF" ),
	HTE(AMLOP_INDEXFIELD,	    "IndexField",      "pntfF" ),
	HTE(AMLOP_BANKFIELD,	    "BankField",       "pnnifF" ),
	HTE(AMLOP_CREATEFIELD,	    "CreateField",     "tiiN",   ),
	HTE(AMLOP_CREATEQWORDFIELD, "CreateQWordField","tiN",    ),
	HTE(AMLOP_CREATEDWORDFIELD, "CreateDWordField","tiN",    ),
	HTE(AMLOP_CREATEWORDFIELD,  "CreateWordField", "tiN",    ),
	HTE(AMLOP_CREATEBYTEFIELD,  "CreateByteField", "tiN",    ),
	HTE(AMLOP_CREATEBITFIELD,   "CreateBitField",  "tiN",    ),

	/* Conversion operations */
	HTE(AMLOP_TOINTEGER,	  "ToInteger",	     "tr",     ),
	HTE(AMLOP_TOBUFFER,	  "ToBuffer",	     "tr",     ),
	HTE(AMLOP_TODECSTRING,	  "ToDecString",     "ir",     ),
	HTE(AMLOP_TOHEXSTRING,	  "ToHexString",     "ir",     ), 
	HTE(AMLOP_TOSTRING,	  "ToString",	     "t",      ),
	HTE(AMLOP_FROMBCD,	  "FromBCD",	     "ir",     ),
	HTE(AMLOP_TOBCD,	  "ToBCD",	     "ir",     ),
	HTE(AMLOP_MID,		  "Mid",	     "tiir",   ),

	/* Mutex/Signal operations */
	HTE(AMLOP_ACQUIRE,	  "Acquire",	     "tw",     ),
	HTE(AMLOP_RELEASE,	  "Release",	     "t",      ),
	HTE(AMLOP_SIGNAL,	  "Signal",	     "t",      ),
	HTE(AMLOP_WAIT,		  "Wait",	     "ti",     ),
	HTE(AMLOP_RESET,	  "Reset",	     "t",      ),
 
	HTE(AMLOP_INDEX,	  "Index",	     "tir",    ),
	HTE(AMLOP_DEREFOF,	  "DerefOf",	     "t",      ),
	HTE(AMLOP_REFOF,	  "RefOf",	     "t",      ),
	HTE(AMLOP_CONDREFOF,	  "CondRef",	     "nr",     ),

	HTE(AMLOP_LOADTABLE,	  "LoadTable",	     "tttttt" ),
	HTE(AMLOP_STALL,	  "Stall",	     "i",      ),
	HTE(AMLOP_SLEEP,	  "Sleep",	     "i",      ),
	HTE(AMLOP_LOAD,		  "Load",	     "nt" ),
	HTE(AMLOP_UNLOAD,	  "Unload",	     "t" ), 
	HTE(AMLOP_STORE,	  "Store",	     "tr",     ),
	HTE(AMLOP_CONCAT,	  "Concat",	     "ttr" ),
	HTE(AMLOP_CONCATRES,	  "ConcatRes",	     "ttt" ),
	HTE(AMLOP_NOTIFY,	  "Notify",	     "ti" ),
	HTE(AMLOP_SIZEOF,	  "Sizeof",	     "t",      ),
	HTE(AMLOP_MATCH,	  "Match",	     "tbibii", ),
	HTE(AMLOP_OBJECTTYPE,	  "ObjectType",	     "t" ),
	HTE(AMLOP_COPYOBJECT,	  "CopyObject",	     "tr" ),
};

void _aml_die(const char *fn, int line, const char *fmt, ...);

int aml_pc(uint8_t *src)
{
	return src - aml_root.start;
}

void _aml_die(const char *fn, int line, const char *fmt, ...)
{
	va_list ap;
	char tmpbuf[256];

	va_start(ap, fmt);
	snprintf(tmpbuf,sizeof(tmpbuf),"aml_die %s:%d ", fn, line);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);

	/* XXX: don't panic */
	panic(tmpbuf);
}
#define aml_die(x...) _aml_die(__FUNCTION__,__LINE__,x)

struct aml_opcode *
aml_findopcode(int opcode)
{
	struct aml_opcode *tab;

	tab = &aml_table[HASH_KEY(opcode)];
	if (tab->opcode == HASH_MAGIC(opcode))
		return tab;
	return NULL;
}

const char *
aml_mnem(int opcode)
{
	struct aml_opcode *tab;

	if ((tab = aml_findopcode(opcode)) != NULL)
		return tab->mnem;
	return ("xxx");
}

struct aml_notify_data
{
	struct aml_node		*node;
	char			pnpid[20];
	void			*cbarg;
	int			(*cbproc)(struct aml_node *, int, void *);

	SLIST_ENTRY(aml_notify_data) link;
};

SLIST_HEAD(aml_notify_head, aml_notify_data);
struct aml_notify_head		aml_notify_list =
SLIST_HEAD_INITIALIZER(&aml_notify_list);

/*
 *  @@@: Memory management functions
 */

#define acpi_os_malloc(sz) _acpi_os_malloc(sz, __FUNCTION__, __LINE__)
#define acpi_os_free(ptr)  _acpi_os_free(ptr, __FUNCTION__, __LINE__)

long acpi_nalloc;

struct acpi_memblock
{
	size_t size;
};
	
void *
_acpi_os_malloc(size_t size, const char *fn, int line)
{
	struct acpi_memblock *sptr;

	sptr = malloc(size+sizeof(*sptr), M_DEVBUF, M_WAITOK);
	dnprintf(99,"alloc: %x %s:%d\n", sptr, fn, line);
	if (sptr) {
		acpi_nalloc += size;
		sptr->size = size;
		memset(&sptr[1], 0, size);
		return &sptr[1];
	}
	return NULL;
}

void
_acpi_os_free(void *ptr, const char *fn, int line)
{
	struct acpi_memblock *sptr;

	if (ptr != NULL) {
		sptr = &(((struct acpi_memblock *)ptr)[-1]);
		acpi_nalloc -= sptr->size;

		dnprintf(99,"free: %x %s:%d\n", sptr, fn, line);
		free(sptr, M_DEVBUF);
	}
}

/*
 * @@@: Misc utility functions
 */

void
aml_dump(int len, u_int8_t *buf)
{
	int		idx;
	
	dnprintf(50, "{ ");
	for (idx = 0; idx < len; idx++) {
		dnprintf(50, "%s0x%.2x", idx ? ", " : "", buf[idx]);
	}
	dnprintf(50, " }\n");
}

/* Bit mangling code */
int
aml_tstbit(const u_int8_t *pb, int bit)
{
	pb += aml_bytepos(bit);

	return (*pb & aml_bitmask(bit));
}

void
aml_setbit(u_int8_t *pb, int bit, int val)
{
	pb += aml_bytepos(bit);

	if (val)
		*pb |= aml_bitmask(bit);
	else
		*pb &= ~aml_bitmask(bit);
}

#if 0
#define aml_gasio(sc,tt,bb,ll,bp,bl,sz,buf,mode) acpi_gasio(sc,mode,tt,(bb)+((bp)>>3),(sz)>>3,(bl)>>3,buf)
#else
void
aml_gasio(struct acpi_softc *sc, int type, uint64_t base, uint64_t length,
	  int bitpos, int bitlen, int size, void *buf, int mode)
{
	acpi_gasio(sc, mode, type, base+(bitpos>>3), (size>>3), (bitlen>>3), buf);
}
#endif

/*
 * @@@: Notify functions
 */

void
aml_register_notify(struct aml_node *node, const char *pnpid,
		    int (*proc)(struct aml_node *, int, void *), void *arg)
{
	struct aml_notify_data	*pdata;

	dnprintf(10, "aml_register_notify: %s %s %x\n",
		 node->name, pnpid ? pnpid : "", proc);

	pdata = acpi_os_malloc(sizeof(struct aml_notify_data));
	pdata->node = node;
	pdata->cbarg = arg;
	pdata->cbproc = proc;

	if (pnpid) 
		strlcpy(pdata->pnpid, pnpid, sizeof(pdata->pnpid));

	SLIST_INSERT_HEAD(&aml_notify_list, pdata, link);
}

void
aml_notify(struct aml_node *node, int notify_value)
{
	struct aml_notify_data	*pdata = NULL;

	if (node == NULL)
		return;

	SLIST_FOREACH(pdata, &aml_notify_list, link)
		if (pdata->node == node)
			pdata->cbproc(pdata->node, notify_value, pdata->cbarg);
}

void
aml_notify_dev(const char *pnpid, int notify_value)
{
	struct aml_notify_data	*pdata = NULL;

	if (pnpid == NULL)
		return;

	SLIST_FOREACH(pdata, &aml_notify_list, link)
		if (pdata->pnpid && !strcmp(pdata->pnpid, pnpid))
			pdata->cbproc(pdata->node, notify_value, pdata->cbarg);
}

/*
 * @@@: Namespace functions 
 */

struct aml_node *__aml_search(struct aml_node *, uint8_t *);
void aml_delchildren(struct aml_node *);
const char *aml_getname(const char *);
const char *aml_nodename(struct aml_node *);


/* Search for a name in children nodes */
struct aml_node *
__aml_search(struct aml_node *root, uint8_t *nameseg)
{
	if (root == NULL)
		return NULL;
	for (root=root->child; root; root=root->sibling) {
		if (!memcmp(root->name, nameseg, AML_NAMESEG_LEN))
			return root;
	}
	return NULL;
}

/* Get absolute pathname of AML node */
const char *
aml_nodename(struct aml_node *node)
{
	static char namebuf[128];

	namebuf[0] = 0;
	if (node) {
		aml_nodename(node->parent);
		if (node->parent != &aml_root)
			strlcat(namebuf, ".", sizeof(namebuf));
		strlcat(namebuf, node->name, sizeof(namebuf));
	}
	return namebuf+1;
}

const char *
aml_getname(const char *name)
{
	static char namebuf[128], *p;
	int count;

	p = namebuf;
	while (*name == AMLOP_ROOTCHAR || *name == AMLOP_PARENTPREFIX) {
		*(p++) = *(name++);
	}
	switch (*name) {
	case 0x00:
		count = 0;
		break;
	case AMLOP_MULTINAMEPREFIX:
		count = name[1];
		name += 2;
		break;
	case AMLOP_DUALNAMEPREFIX:
		count = 2;
		name += 1;
		break;
	default:
		count = 1;
	}
	while (count--) {
		memcpy(p, name, 4);
		p[4] = '.';
		p += 5;
		name += 4;
		if (*name == '.') name++;
	}
	*(--p) = 0;
	return namebuf;
}

/* Create name/value pair in namespace */
struct aml_node *
aml_createname(struct aml_node *root, const void *vname, struct aml_value *value)
{
	struct aml_node *node, **pp;
	uint8_t *name = (uint8_t *)vname;
	int count;

	if (*name == AMLOP_ROOTCHAR) {
		root = &aml_root;
		name++;
	}
	while (*name == AMLOP_PARENTPREFIX && root) {
		root = root->parent;
		name++;
	}
	switch (*name) {
	case 0x00:
		return root;
	case AMLOP_MULTINAMEPREFIX:
		count = name[1];
		name += 2;
		break;
	case AMLOP_DUALNAMEPREFIX:
		count = 2;
		name += 1;
		break;
	default:
		count = 1;
		break;
	}
	node = NULL;
	while (count-- && root) {
		/* Create new name if it does not exist */
		if ((node = __aml_search(root, name)) == NULL) {
			node = acpi_os_malloc(sizeof(struct aml_node));

			memcpy((void *)node->name, name, AML_NAMESEG_LEN);
			for (pp = &root->child; *pp; pp=&(*pp)->sibling)
				;
			node->parent = root;
			node->sibling = NULL;
			*pp = node;
		}
		root  = node;
		name += AML_NAMESEG_LEN;
	}
	/* If node created, set value pointer */
	if (node && value) {
		node->value = value;
		value->node = node;
	}
	return node;
}

/* Search namespace for a named node */
struct aml_node *
aml_searchname(struct aml_node *root, const void *vname)
{
	struct aml_node *node;
	uint8_t *name = (uint8_t *)vname;
	int count;

	if (*name == AMLOP_ROOTCHAR) {
		root = &aml_root;
		name++;
	}
	while (*name == AMLOP_PARENTPREFIX && root) {
		root = root->parent;
		name++;
	}
	if (strlen(name) < AML_NAMESEG_LEN) {
		aml_die("bad name");
	}
	switch (*name) {
	case 0x00:
		return root;
	case AMLOP_MULTINAMEPREFIX:
		count = name[1];
		name += 2;
		break;
	case AMLOP_DUALNAMEPREFIX:
		count = 2;
		name += 1;
		break;
	default:
		if (name[4] == '.') {
			/* Called from user code */
			while (*name && (root = __aml_search(root, name)) != NULL) {
				name += AML_NAMESEG_LEN+1;
			}
			return root;
		}
		/* Special case.. search relative for name */
		while (root && (node = __aml_search(root, name)) == NULL) {
			root = root->parent;
		}
		return node;
	}
	/* Search absolute for name*/
	while (count-- && (root = __aml_search(root, name)) != NULL) {
		name += AML_NAMESEG_LEN;
	}
	return root;
}

/* Free all children nodes/values */
void
aml_delchildren(struct aml_node *node)
{
	struct aml_node *onode;

	if (node == NULL)
		return;
	while ((onode = node->child) != NULL) {
		node->child = onode->sibling;

		aml_delchildren(onode);

		/* Decrease reference count */
		aml_delref(&onode->value);

		/* Delete node */
		acpi_os_free(onode);
	}
}

/*
 * @@@: Value functions
 */
struct aml_vallist
{
	struct aml_value *obj;
	int nobj;
	struct aml_vallist *next;
};

struct aml_scope
{
	struct acpi_softc  *sc;
	uint8_t            *pos;
	uint8_t            *end;
	struct aml_node    *node;
	struct aml_vallist *tmpvals;
	struct aml_scope   *parent;
	struct aml_value   *locals;
	struct aml_value   *args;
	int                 nargs;
};

struct aml_value *aml_alloctmp(struct aml_scope *, int);
struct aml_scope *aml_pushscope(struct aml_scope *, uint8_t *, uint8_t *, struct aml_node *);
struct aml_scope *aml_popscope(struct aml_scope *);
int aml_parsenode(struct aml_node *, uint8_t *, uint8_t **, struct aml_value *);

#define LHS  0
#define RHS  1
#define DST  2
#define DST2 3

/* Allocate temporary storage in this scope */
struct aml_value *
aml_alloctmp(struct aml_scope *scope, int narg)
{
	struct aml_vallist *tmp;

	/* Allocate array of temp values */
	tmp = (struct aml_vallist *)acpi_os_malloc(sizeof(struct aml_vallist) + 
						   narg * sizeof(struct aml_value));

	tmp->obj = (struct aml_value *)&tmp[1];
	tmp->nobj = narg;

	/* Link into scope */
	tmp->next = scope->tmpvals;
	scope->tmpvals = tmp;

	/* Return array of values */
	return tmp->obj;
}

/* Allocate+push parser scope */
struct aml_scope *
aml_pushscope(struct aml_scope *parent, uint8_t *start, uint8_t  *end, 
	      struct aml_node *node)
{
	struct aml_scope *scope;

	scope = acpi_os_malloc(sizeof(struct aml_scope));
	scope->pos = start;
	scope->end = end;
	scope->node = node;
	scope->parent = parent;
	scope->sc = dsdt_softc;

	return scope;
}

struct aml_scope *
aml_popscope(struct aml_scope *scope)
{
	struct aml_scope *nscope;
	struct aml_vallist *ol;
	int idx;

	if (scope == NULL)
		return NULL;
	nscope = scope->parent;

	/* Free temporary values */
	while ((ol = scope->tmpvals) != NULL) {
		scope->tmpvals = ol->next;
		for (idx=0; idx<ol->nobj; idx++) {
			aml_freevalue(&ol->obj[idx]);
		}
		acpi_os_free(ol);
	}
	acpi_os_free(scope);

	return nscope;
}

int
aml_parsenode(struct aml_node *node, uint8_t *start, uint8_t **end, 
	      struct aml_value *res)
{
	struct aml_scope *scope;

	/* Don't parse zero-length scope */
	if (start == *end) {
		return 0;
	}
	scope = aml_pushscope(NULL, start, *end, node);
	if (res == NULL) {
		res = aml_alloctmp(scope, 1);
	}
	while (scope != NULL) {
		while (scope->pos < scope->end)
			aml_parseop(scope, res);
		scope = aml_popscope(scope);
	}
	return 0;
}

/*
 * Field I/O code
 */
void aml_setbufint(struct aml_value *, int, int, struct aml_value *);
void aml_getbufint(struct aml_value *, int, int, struct aml_value *);
void aml_fieldio(struct aml_scope *, struct aml_value *, struct aml_value *, int);

/* Copy from a bufferfield to an integer/buffer */
void
aml_setbufint(struct aml_value *dst, int bitpos, int bitlen, 
	      struct aml_value *src)
{
	if (src->type != AML_OBJTYPE_BUFFER) {
		aml_die("wrong setbufint type\n");
	}
	if (bitlen < aml_intlen) {
		/* XXX: Endian issues?? */
		/* Return integer type */
		_aml_setvalue(dst, AML_OBJTYPE_INTEGER, 0, NULL);
		aml_bufcpy(&dst->v_integer, 0, src->v_buffer, bitpos, bitlen);
	}
	else {
		/* Return buffer type */
		_aml_setvalue(dst, AML_OBJTYPE_BUFFER, (bitlen+7)>>3, NULL);
		aml_bufcpy(dst->v_buffer, 0, src->v_buffer, bitpos, bitlen);
	}
}

/* Copy from a string/integer/buffer to a bufferfield */
void
aml_getbufint(struct aml_value *src, int bitpos, int bitlen, 
	      struct aml_value *dst)
{
	if (dst->type != AML_OBJTYPE_BUFFER) {
		aml_die("wrong getbufint type\n");
	}
	switch (src->type) {
	case AML_OBJTYPE_INTEGER:
		aml_bufcpy(dst->v_buffer, bitpos, &src->v_integer, 0, bitlen);
		break;
	case AML_OBJTYPE_BUFFER:
		aml_bufcpy(dst->v_buffer, bitpos, src->v_buffer, 0, bitlen);
		break;
	case AML_OBJTYPE_STRING:
		aml_bufcpy(dst->v_buffer, bitpos, src->v_string, 0, bitlen);
		break;
	}
}

/* 
 * Buffer/Region: read/write to bitfields
 */
void
aml_fieldio(struct aml_scope *scope, struct aml_value *field, 
	    struct aml_value *res, int mode)
{
	struct aml_value opr, *pop;
	int bpos, blen, aligned, mask;

	memset(&opr, 0x0, sizeof(opr));
	switch (field->v_field.type) {
	case AMLOP_INDEXFIELD:
		/* Set Index */
		aml_setvalue(scope, field->v_field.ref1, NULL, field->v_field.bitpos>>3);
		aml_fieldio(scope, field->v_field.ref2, res, mode);
		break;
	case AMLOP_BANKFIELD:
		/* Set Bank */
		aml_setvalue(scope, field->v_field.ref1, NULL, field->v_field.ref3);
		aml_fieldio(scope, field->v_field.ref2, res, mode);
		break;
	case AMLOP_FIELD:
		/* This is an I/O field */
		pop = field->v_field.ref1;
		if (pop->type != AML_OBJTYPE_OPREGION) {
			aml_die("Not an opregion!\n");
		}

		/* Get field access size */
		switch (AML_FIELD_ACCESS(field->v_field.flags)) {
		case AML_FIELD_ANYACC:
		case AML_FIELD_BYTEACC:
			mask = 7;
			break;
		case AML_FIELD_WORDACC:
			mask = 15;
			break;
		case AML_FIELD_DWORDACC:
			mask = 31;
			break;
		case AML_FIELD_QWORDACC:
			mask = 63;
			break;
		}

		/* Get aligned bitpos/bitlength */
		bpos = field->v_field.bitpos & ~mask;
		blen = ((field->v_field.bitpos & mask) +
			field->v_field.bitlen + mask) & ~mask;
		aligned = (bpos == field->v_field.bitpos && 
			   blen == field->v_field.bitlen);
		mask++;

		/* Verify that I/O is in range */
#if 0
		if ((bpos+blen) >= (pop->v_opregion.iolen * 8)) {
			aml_die("Out of bounds I/O!!! region:%x:%llx:%x %x\n",
				pop->v_opregion.iospace,
				pop->v_opregion.iobase,
				pop->v_opregion.iolen,
				bpos+blen);
		}
#endif

		_aml_setvalue(&opr, AML_OBJTYPE_BUFFER, blen>>3, NULL);
		if (mode == ACPI_IOREAD) {
			/* Read from GAS space */
			aml_gasio(scope->sc, pop->v_opregion.iospace,
				  pop->v_opregion.iobase,
				  pop->v_opregion.iolen,
				  bpos, blen, mask+1,
				  opr.v_buffer,
				  ACPI_IOREAD);
			aml_setbufint(res, field->v_field.bitpos & mask,
				      field->v_field.bitlen, &opr);
		}
		else {
			switch (AML_FIELD_UPDATE(field->v_field.flags)) {
			case AML_FIELD_WRITEASONES:
				if (!aligned) {
					dnprintf(50,"fpr:WriteOnes\n");
					memset(opr.v_buffer, 0xff, opr.length);
				}
				break;
			case AML_FIELD_PRESERVE:
				if (!aligned) {
					/* Non-aligned I/O: need to read current value */
					dnprintf(50,"fpr:Preserve\n");
					aml_gasio(scope->sc, pop->v_opregion.iospace,
						  pop->v_opregion.iobase,
						  pop->v_opregion.iolen,
						  bpos, blen, mask+1,
						  opr.v_buffer,
						  ACPI_IOREAD);
				}
				break;
			}
			/* Copy Bits into destination buffer */
			aml_getbufint(res, field->v_field.bitpos & mask,
				      field->v_field.bitlen,
				      &opr);
			aml_gasio(scope->sc, pop->v_opregion.iospace,
				  pop->v_opregion.iobase,
				  pop->v_opregion.iolen,
				  bpos, blen, mask+1,
				  opr.v_buffer,
				  ACPI_IOWRITE);
		}
		aml_freevalue(&opr);
		break;
	default:
		/* This is a buffer field */
		bpos = field->v_field.bitpos;
		blen = field->v_field.bitlen;
		pop = field->v_field.ref1;
		if (mode == ACPI_IOREAD) {
			aml_setbufint(res, bpos, blen, pop);
		}
		else {
			aml_getbufint(res, bpos, blen, pop);
		}
		break;
	}
}

/*
 * @@@: Value set/compare/alloc/free routines
 */
int64_t aml_str2int(const char *, int);
struct aml_value *aml_dereftarget(struct aml_scope *, struct aml_value *);
struct aml_value *aml_derefterm(struct aml_scope *, struct aml_value *, int);

void
aml_showvalue(struct aml_value *val)
{
	int idx;

	if (val == NULL)
		return;
  
	if (val->node) {
		dnprintf(0," [%s]", aml_nodename(val->node));
	}
	dnprintf(0," %p cnt:%.2x", val, val->refcnt);
	switch (val->type) {
	case AML_OBJTYPE_INTEGER:
		dnprintf(0," integer: %llx\n", val->v_integer);
		break;
	case AML_OBJTYPE_STRING:
		dnprintf(0," string: %s\n", val->v_string);
		break;
	case AML_OBJTYPE_METHOD:
		dnprintf(0," method: %.2x\n", val->v_method.flags);
		break;
	case AML_OBJTYPE_PACKAGE:
		dnprintf(0," package: %.2x\n", val->length);
		for (idx=0; idx<val->length; idx++) {
			aml_showvalue(val->v_package[idx]);
		}
		break;
	case AML_OBJTYPE_BUFFER:
		dnprintf(0," buffer: %.2x {", val->length);
		for (idx=0; idx<val->length; idx++) {
			dnprintf(0,"%s%.2x", idx ? ", " : "", val->v_buffer[idx]);
		}
		dnprintf(0,"}\n");
		break;
	case AML_OBJTYPE_FIELDUNIT:
	case AML_OBJTYPE_BUFFERFIELD:
		dnprintf(0," field: bitpos=%.4x bitlen=%.4x ref1:%x ref2:%x [%s]\n", 
			 val->v_field.bitpos, val->v_field.bitlen,
			 val->v_field.ref1, val->v_field.ref2, 
			 aml_mnem(val->v_field.type));
		aml_showvalue(val->v_field.ref1);
		aml_showvalue(val->v_field.ref2);
		break;
	case AML_OBJTYPE_MUTEX:
		dnprintf(0," mutex: %llx\n", val->v_integer);
		break;
	case AML_OBJTYPE_EVENT:
		dnprintf(0," event:\n");
		break;
	case AML_OBJTYPE_OPREGION:
		dnprintf(0," opregion: %.2x,%.8llx,%x\n",
			 val->v_opregion.iospace,
			 val->v_opregion.iobase,
			 val->v_opregion.iolen);
		break;
	case AML_OBJTYPE_NAMEREF:
		dnprintf(0," nameref: %s\n", aml_getname(val->v_nameref));
		break;
	case AML_OBJTYPE_DEVICE:
		dnprintf(0," device:\n");
		break;
	case AML_OBJTYPE_PROCESSOR:
		dnprintf(0," cpu: %.2x,%.4x,%.2x\n", 
			 val->v_processor.proc_id, val->v_processor.proc_addr,
			 val->v_processor.proc_len);
		break;
	case AML_OBJTYPE_THERMZONE:
		dnprintf(0," thermzone:\n");
		break;
	case AML_OBJTYPE_POWERRSRC:
		dnprintf(0," pwrrsrc: %.2x,%.2x\n", 
			 val->v_powerrsrc.pwr_level, 
			 val->v_powerrsrc.pwr_order);
		break;
	case AML_OBJTYPE_OBJREF:
		dnprintf(0," objref: %p index:%x\n", val->v_objref.ref, val->v_objref.index);
		aml_showvalue(val->v_objref.ref);
		break;
	default:
		dnprintf(0," !!type: %x\n", val->type);
	}
}

/* Returns dereferenced target value */
struct aml_value *
aml_dereftarget(struct aml_scope *scope, struct aml_value *ref)
{
	struct aml_node *node;
	int index;

	for(;;) {
		switch (ref->type) {
		case AML_OBJTYPE_NAMEREF:
			node = aml_searchname(scope->node, ref->v_nameref);
			if (node && node->value)
				ref = node->value;
			break;
		case AML_OBJTYPE_OBJREF:
			index = ref->v_objref.index;
			ref = aml_dereftarget(scope, ref->v_objref.ref);
			if (index != -1) {
				switch (ref->type) {
				case AML_OBJTYPE_PACKAGE:
					ref = ref->v_package[index];
					break;
				default:
					aml_die("Index");
					break;
				}
			}
			break;
		default:
			return ref;
		}
	}
}
/* Perform DeRef on terminal
 * Returns expected type if required
 */
struct aml_value *
aml_derefterm(struct aml_scope *scope, struct aml_value *ref, int expect)
{
	struct aml_value *tmp;

	ref = aml_dereftarget(scope, ref);
	switch (ref->type) {
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		/* Read I/O field into temporary storage */
		tmp = aml_alloctmp(scope, 1);
		aml_fieldio(scope, ref, tmp, ACPI_IOREAD);
		return tmp;
	}
	if (expect != ref->type) {
		dnprintf(50,"convert in derefterm\n");
	}
	return ref;
}

int64_t
aml_str2int(const char *str, int radix)
{
	/* XXX: fixme */
	return 0;
}

int64_t
aml_val2int(struct aml_value *rval)
{
	int64_t ival = 0;

	if (rval == NULL) {
		dnprintf(50,"null val2int\n");
		return (0);
	}
	switch (rval->type & ~AML_STATIC) {
	case AML_OBJTYPE_INTEGER:
		ival = rval->v_integer;
		break;
	case AML_OBJTYPE_BUFFER:
		aml_bufcpy(&ival, 0, rval->v_buffer, 0, 
			   min(aml_intlen*8, rval->length*8));
		break;
	case AML_OBJTYPE_STRING:
		ival = (strncmp(rval->v_string, "0x", 2) == 0) ?
			aml_str2int(rval->v_string+2, 16) :
			aml_str2int(rval->v_string, 10);
		break;
	}
	return (ival);
}

struct aml_value *
_aml_setvalue(struct aml_value *lhs, int type, int64_t ival, const void *bval)
{
	memset(&lhs->_, 0x0, sizeof(lhs->_));

	lhs->type = type;
	switch (lhs->type & ~AML_STATIC) {
	case AML_OBJTYPE_INTEGER:
		lhs->v_integer = ival;
		break;
	case AML_OBJTYPE_METHOD:
		lhs->v_integer = ival;
		break;
	case AML_OBJTYPE_NAMEREF:
		lhs->v_nameref = (uint8_t *)bval;
		break;
	case AML_OBJTYPE_OBJREF:
		lhs->v_objref.index = ival;
		lhs->v_objref.ref = (struct aml_value *)bval;
		break;
	case AML_OBJTYPE_BUFFER:
		lhs->length = ival;
		lhs->v_buffer = (uint8_t *)acpi_os_malloc(ival);
		if (bval)
			memcpy(lhs->v_buffer, bval, ival);
		break;
	case AML_OBJTYPE_STRING:
		if (ival == -1)
			ival = strlen((const char *)bval);
		lhs->length = ival;
		lhs->v_string = (char *)acpi_os_malloc(ival+1);
		if (bval)
			strncpy(lhs->v_string, (const char *)bval, ival);
		break;
	case AML_OBJTYPE_PACKAGE:
		lhs->length = ival;
		lhs->v_package = (struct aml_value **)acpi_os_malloc(ival * sizeof(struct aml_value *));
		for (ival=0; ival<lhs->length; ival++) {
			lhs->v_package[ival] = aml_allocvalue(AML_OBJTYPE_UNINITIALIZED, 0, NULL);
		}
		break;
	}
	return lhs;
}

void
aml_copyvalue(struct aml_value *lhs, struct aml_value *rhs)
{
	int idx;

	lhs->type = rhs->type & ~AML_STATIC;
	switch (lhs->type) {
	case AML_OBJTYPE_UNINITIALIZED:
		break;
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_MUTEX:
		lhs->v_integer = rhs->v_integer;
		break;
	case AML_OBJTYPE_BUFFER:
		_aml_setvalue(lhs, rhs->type, rhs->length, rhs->v_buffer);
		break;
	case AML_OBJTYPE_STRING:
		_aml_setvalue(lhs, rhs->type, rhs->length, rhs->v_string);
		break;
	case AML_OBJTYPE_OPREGION:
		lhs->v_opregion = rhs->v_opregion;
		break;
	case AML_OBJTYPE_NAMEREF:
		lhs->v_nameref = rhs->v_nameref;
		break;
	case AML_OBJTYPE_PACKAGE:
		_aml_setvalue(lhs, rhs->type, rhs->length, NULL);
		for (idx=0; idx<rhs->length; idx++) {
			aml_copyvalue(lhs->v_package[idx], rhs->v_package[idx]);
		}
		break;
	case AML_OBJTYPE_OBJREF:
		_aml_setvalue(lhs, rhs->type, rhs->v_objref.index, rhs->v_objref.ref);
		break;
	default:
		aml_die("copyvalue: %x", rhs->type);
		break;
	}
}

/* Guts of the code: Assign one value to another */
void
aml_setvalue(struct aml_scope *scope, struct aml_value *lhs, 
	     struct aml_value *rhs, int64_t ival)
{
	struct aml_value tmpint;
	struct aml_value *olhs;

	/* Use integer as result */
	if (rhs == NULL) {
		memset(&tmpint, 0, sizeof(tmpint));
		rhs = _aml_setvalue(&tmpint, AML_OBJTYPE_INTEGER, ival, NULL);
	}

	olhs = lhs;
	lhs = aml_dereftarget(scope, lhs);

	switch (lhs->type) {
	case AML_OBJTYPE_UNINITIALIZED:
		aml_copyvalue(lhs, rhs);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		aml_fieldio(scope, lhs, rhs, ACPI_IOWRITE);
		break;
	case AML_OBJTYPE_DEBUGOBJ:
	case AML_OBJTYPE_INTEGER+AML_STATIC:
		break;
	case AML_OBJTYPE_INTEGER:
		lhs->v_integer = aml_val2int(rhs);
		break;
	case AML_OBJTYPE_BUFFER:
		if (lhs->node) {
			dnprintf(40,"named.buffer\n");
		}
		aml_freevalue(lhs);
		if (rhs->type == AML_OBJTYPE_BUFFER) {
			_aml_setvalue(lhs, rhs->type, rhs->length, rhs->v_buffer);
		}
		else if (rhs->type == AML_OBJTYPE_INTEGER) {
			_aml_setvalue(lhs, AML_OBJTYPE_BUFFER, sizeof(rhs->v_integer),
				      &rhs->v_integer);
		}
		else if (rhs->type == AML_OBJTYPE_STRING) {
			_aml_setvalue(lhs, AML_OBJTYPE_BUFFER, rhs->length, rhs->v_string);
		}
		else {
			//aml_showvalue(rhs);
			aml_die("setvalue.buf");
		}
		break;
	case AML_OBJTYPE_STRING:
		if (lhs->node) {
			dnprintf(40,"named string\n");
		}
		aml_freevalue(lhs);
		if (rhs->type == AML_OBJTYPE_STRING) {
			_aml_setvalue(lhs, rhs->type, rhs->length, rhs->v_string);
		}
		else if (rhs->type == AML_OBJTYPE_BUFFER) {
			_aml_setvalue(lhs, AML_OBJTYPE_STRING, rhs->length, rhs->v_buffer);
		}
		else if (rhs->type == AML_OBJTYPE_INTEGER) {
			_aml_setvalue(lhs, AML_OBJTYPE_STRING, 10, NULL);
			snprintf(lhs->v_string, lhs->length, "%lld", rhs->v_integer);
		}
		else {
			//aml_showvalue(rhs);
			aml_die("setvalue.str");
		}
		break;
	default:
		/* XXX: */
		dnprintf(0,"setvalue.unknown: %x", lhs->type);
		break;
	}
}

/* Allocate dynamic AML value
 *   type : Type of object to allocate (AML_OBJTYPE_XXXX)
 *   ival : Integer value (action depends on type)
 *   bval : Buffer value (action depends on type)
 */
struct aml_value *
aml_allocvalue(int type, int64_t ival, const void *bval) 
{
	struct aml_value *rv;

	rv = (struct aml_value *)acpi_os_malloc(sizeof(struct aml_value));
	if (rv != NULL) {
		aml_addref(rv);
		return _aml_setvalue(rv, type, ival, bval);
	}
	return NULL;
}

void
aml_freevalue(struct aml_value *val)
{
	int idx;
	
	if (val == NULL)
		return;
	switch (val->type) {
	case AML_OBJTYPE_STRING:
		acpi_os_free(val->v_string);
		break;
	case AML_OBJTYPE_BUFFER:
		acpi_os_free(val->v_buffer);
		break;
	case AML_OBJTYPE_PACKAGE:
		for (idx=0; idx<val->length; idx++) {
			aml_freevalue(val->v_package[idx]);
			acpi_os_free(val->v_package[idx]);
		}
		acpi_os_free(val->v_package);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		aml_delref(&val->v_field.ref1);
		aml_delref(&val->v_field.ref2);
		break;
	}
	val->type = 0;
	memset(&val->_, 0, sizeof(val->_));
}

/* Increase reference count */
void
aml_addref(struct aml_value *val)
{
	if (val) 
		val->refcnt++;
}

/* Decrease reference count + delete value */
void
aml_delref(struct aml_value **val)
{
	if (val && *val && --(*val)->refcnt == 0) {
		aml_freevalue(*val);
		acpi_os_free(*val);
		*val = NULL;
	}
}

/*
 * @@@: Math eval routines 
 */

/* Convert number from one radix to another
 * Used in BCD conversion routines */
u_int64_t 
aml_convradix(u_int64_t val, int iradix, int oradix)
{
	u_int64_t rv = 0, pwr;

	rv = 0;
	pwr = 1;
	while (val) {
		rv += (val % iradix) * pwr;
		val /= iradix;
		pwr *= oradix;
	}
	return rv;
}
/* Calculate LSB */
int
aml_lsb(u_int64_t val)
{
	int		lsb;

	if (val == 0) 
		return (0);

	for (lsb = 1; !(val & 0x1); lsb++)
		val >>= 1;

	return (lsb);
}

/* Calculate MSB */
int
aml_msb(u_int64_t val)
{
	int		msb;

	if (val == 0) 
		return (0);

	for (msb = 1; val != 0x1; msb++)
		val >>= 1;

	return (msb);
}

/* Evaluate Math operands */
int64_t
aml_evalexpr(int64_t lhs, int64_t rhs, int opcode)
{
	dnprintf(50, "evalexpr: %s %lld %lld\n", aml_mnem(opcode), lhs, rhs);

	switch (opcode) {
		/* Math operations */
	case AMLOP_INCREMENT:
	case AMLOP_ADD:
		return (lhs + rhs);
	case AMLOP_DECREMENT:
	case AMLOP_SUBTRACT:
		return (lhs - rhs);
	case AMLOP_MULTIPLY:
		return (lhs * rhs);
	case AMLOP_DIVIDE:
		return (lhs / rhs);
	case AMLOP_MOD:
		return (lhs % rhs);
	case AMLOP_SHL:
		return (lhs << rhs);
	case AMLOP_SHR:
		return (lhs >> rhs);
	case AMLOP_AND:
		return (lhs & rhs);
	case AMLOP_NAND:
		return ~(lhs & rhs);
	case AMLOP_OR:
		return (lhs | rhs); 
	case AMLOP_NOR:
		return ~(lhs | rhs); 
	case AMLOP_XOR:
		return (lhs ^ rhs);
	case AMLOP_NOT:
		return ~(lhs);

		/* Conversion/misc */
	case AMLOP_FINDSETLEFTBIT:
		return aml_msb(lhs);
	case AMLOP_FINDSETRIGHTBIT:
		return aml_lsb(lhs);
	case AMLOP_TOINTEGER:
		return (lhs);
	case AMLOP_FROMBCD:
		return aml_convradix(lhs, 16, 10);
	case AMLOP_TOBCD:
		return aml_convradix(lhs, 10, 16);

		/* Logical/Comparison */
	case AMLOP_LAND:
		return (lhs && rhs);
	case AMLOP_LOR:
		return (lhs || rhs);
	case AMLOP_LNOT:
		return (!lhs);
	case AMLOP_LNOTEQUAL:
		return (lhs != rhs);
	case AMLOP_LLESSEQUAL:
		return (lhs <= rhs);
	case AMLOP_LGREATEREQUAL:
		return (lhs >= rhs);
	case AMLOP_LEQUAL:
		return (lhs == rhs);
	case AMLOP_LGREATER:
		return (lhs > rhs);
	case AMLOP_LLESS:
		return (lhs < rhs);
	}

	return (0);
}

int
aml_cmpvalue(struct aml_value *lhs, struct aml_value *rhs, int opcode)
{
	int rc;

	rc = 0;
	if (lhs->type == rhs->type) {
		switch (lhs->type) {
		case AML_OBJTYPE_INTEGER:
			rc = (lhs->v_integer - rhs->v_integer);
			break;
		case AML_OBJTYPE_STRING:
			rc = strncmp(lhs->v_string, rhs->v_string, min(lhs->length, rhs->length));
			if (rc == 0)
				rc = lhs->length - rhs->length;
			break;
		case AML_OBJTYPE_BUFFER:
			rc = memcmp(lhs->v_buffer, rhs->v_buffer, min(lhs->length, rhs->length));
			if (rc == 0)
				rc = lhs->length - rhs->length;
			break;
		}
	}
	else if (lhs->type == AML_OBJTYPE_INTEGER) {
		rc = lhs->v_integer - aml_val2int(rhs);
	}
	else if (rhs->type == AML_OBJTYPE_INTEGER) {
		rc = aml_val2int(lhs) - rhs->v_integer;
	}
	else {
		aml_die("mismatched compare\n");
	}
	return aml_evalexpr(rc, 0, opcode);
}

/*
 * aml_bufcpy copies/shifts buffer data, special case for aligned transfers
 * dstPos/srcPos are bit positions within destination/source buffers
 */
void
aml_bufcpy(void *pvDst, int dstPos, const void *pvSrc, int srcPos,
	   int len)
{
	const u_int8_t *pSrc = pvSrc;
	u_int8_t *pDst = pvDst;
	int		idx;

	if (aml_bytealigned(dstPos|srcPos|len)) {
		/* Aligned transfer: use memcpy */
		memcpy(pDst+aml_bytepos(dstPos), pSrc+aml_bytepos(srcPos), aml_bytelen(len));
		return;
	}

	/* Misaligned transfer: perform bitwise copy (slow) */
	for (idx = 0; idx < len; idx++)
		aml_setbit(pDst, idx + dstPos, aml_tstbit(pSrc, idx + srcPos));
}

/*
 * Evaluate an AML method
 *
 * Returns a copy of the result in res (must be freed by user)
 */
struct aml_value *
aml_evalmethod(struct aml_node *node,
	       int argc, struct aml_value *argv,
	       struct aml_value *res)
{
	struct aml_scope *scope;

	scope = aml_pushscope(NULL, node->value->v_method.start, 
			      node->value->v_method.end, node);
	scope->args = argv;
	scope->nargs = argc;
	scope->locals = aml_alloctmp(scope, AML_MAX_LOCAL);

	if (res == NULL)
		res = aml_alloctmp(scope, 1);
#ifdef ACPI_DEBUG
	dnprintf(10,"calling [%s] (%d args)\n",
		 aml_nodename(node),
		 scope->nargs);
	while (scope->pos < scope->end)
		aml_parseterm(scope, res);
	printf("[%s] returns: ", aml_nodename(node));
	aml_showvalue(res);
#else
	while (scope->pos < scope->end)
		aml_parseterm(scope, res);
#endif
	/* Free any temporary children nodes */
	aml_delchildren(node);
	aml_popscope(scope);

	return NULL;
}

/* 
 * @@@: External API
 *
 * evaluate an AML node
 * Returns a copy of the value in res  (must be freed by user)
 */
int
aml_evalnode(struct acpi_softc *sc, struct aml_node *node,
	     int argc, struct aml_value *argv,
	     struct aml_value *res)
{
	static int lastck;

	if (res)
		memset(res, 0, sizeof(struct aml_value));
	if (node == NULL || node->value == NULL)
		return (ACPI_E_BADVALUE);

	switch (node->value->type) {
	case AML_OBJTYPE_METHOD:
		aml_evalmethod(node, argc, argv, res);
		if (acpi_nalloc > lastck) {
			/* Check if our memory usage has increased */
			dnprintf(0,"Leaked: [%s] %d\n", 
				 aml_nodename(node), acpi_nalloc);
			lastck = acpi_nalloc;
		}
		break;
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_STRING:
	case AML_OBJTYPE_BUFFER:
	case AML_OBJTYPE_PACKAGE:
	case AML_OBJTYPE_EVENT:
	case AML_OBJTYPE_DEVICE:
	case AML_OBJTYPE_MUTEX:
	case AML_OBJTYPE_OPREGION:
	case AML_OBJTYPE_POWERRSRC:
	case AML_OBJTYPE_PROCESSOR:
	case AML_OBJTYPE_THERMZONE:
	case AML_OBJTYPE_DEBUGOBJ:
		if (res)
			aml_copyvalue(res, node->value);
		break;
	default:
		break;
	}
	return (0);
}

/*
 * evaluate an AML name
 * Returns a copy of the value in res  (must be freed by user)
 */
int
aml_evalname(struct acpi_softc *sc, struct aml_node *parent, const char *name,
	     int argc, struct aml_value *argv,
	     struct aml_value *res)
{
	return aml_evalnode(sc, aml_searchname(parent, name), argc, argv, res);
}

void
aml_walktree(struct aml_node *node)
{
	while(node) {
		aml_showvalue(node->value);
		aml_walktree(node->child);
		node = node->sibling;
	}
}

void
aml_walkroot(void)
{
	aml_walktree(aml_root.child);
}

int
aml_find_node(struct aml_node *node, const char *name,
	      void (*cbproc)(struct aml_node *, void *arg),
	      void *arg)
{
	const char *nn;

	while (node) {
		if ((nn = node->name) != NULL) {
			if (*nn == AMLOP_ROOTCHAR) nn++;
			while (*nn == AMLOP_PARENTPREFIX) nn++;
			if (!strcmp(name, nn))
				cbproc(node, arg);
		}
		aml_find_node(node->child, name, cbproc, arg);
		node = node->sibling;
	}
	return (0);
}

/*
 * @@@: Parser functions
 */
uint8_t *aml_parsename(struct aml_scope *);
uint8_t *aml_parseend(struct aml_scope *scope);
int      aml_parselength(struct aml_scope *);
int      aml_parseopcode(struct aml_scope *);

/* Get AML Opcode */
int
aml_parseopcode(struct aml_scope *scope)
{
	int opcode =  (scope->pos[0]);
	int twocode = (scope->pos[0]<<8) + scope->pos[1];

	/* Check if this is an embedded name */
	switch (opcode) {
	case AMLOP_ROOTCHAR:
	case AMLOP_PARENTPREFIX:
	case AMLOP_MULTINAMEPREFIX:
	case AMLOP_DUALNAMEPREFIX:
	case AMLOP_NAMECHAR:
	case 'A' ... 'Z':
		return AMLOP_NAMECHAR;
	}
	if (twocode == AMLOP_LNOTEQUAL ||
	    twocode == AMLOP_LLESSEQUAL ||
	    twocode == AMLOP_LGREATEREQUAL ||
	    opcode == AMLOP_EXTPREFIX)
	{
		scope->pos += 2;
		return twocode;
	}
	scope->pos += 1;
	return opcode;
}

/* Decode embedded AML Namestring */
uint8_t *
aml_parsename(struct aml_scope *scope)
{
	uint8_t *name = scope->pos;

	while (*scope->pos == AMLOP_ROOTCHAR || *scope->pos == AMLOP_PARENTPREFIX)
		scope->pos++;

	switch (*scope->pos) {
	case 0x00:
		break;
	case AMLOP_MULTINAMEPREFIX:
		scope->pos += 2+AML_NAMESEG_LEN*scope->pos[1];
		break;
	case AMLOP_DUALNAMEPREFIX:
		scope->pos += 1+AML_NAMESEG_LEN*2;
		break;
	default:
		scope->pos += AML_NAMESEG_LEN;
		break;
	}
	return name;
}

/* Decode AML Length field */
int
aml_parselength(struct aml_scope *scope)
{
	int len = (*scope->pos & 0xF);

	switch (*scope->pos >> 6) {
	case 0x00:
		len = scope->pos[0] & 0x3F;
		scope->pos += 1;
		break;
	case 0x01:
		len += (scope->pos[1]<<4L);
		scope->pos += 2;
		break;
	case 0x02:
		len += (scope->pos[1]<<4L) + (scope->pos[2]<<12L);
		scope->pos += 3;
		break;
	case 0x03:
		len += (scope->pos[1]<<4L) + (scope->pos[2]<<12L) + (scope->pos[3]<<20L);
		scope->pos += 4;
		break;
	}
	return len;
}

/* Get address of end of scope; based on current address */
uint8_t *
aml_parseend(struct aml_scope *scope)
{
	uint8_t *pos = scope->pos;
	int len;

	len = aml_parselength(scope);
	if (pos+len > scope->end) {
		dnprintf(0,"Bad scope... runover pos:%.4x new end:%.4x scope end:%.4x\n", 
		       aml_pc(pos), aml_pc(pos+len), aml_pc(scope->end));
		pos = scope->end;
	}
	return pos+len;
}

/*
 * @@@: Opcode utility functions
 */
int  aml_match(int, int64_t, struct aml_value *);
void aml_getpciaddr(struct aml_scope *, struct aml_value *);
void aml_fixref(struct aml_value **);
int64_t aml_parseint(struct aml_scope *, int);

int
aml_match(int op, int64_t mv1, struct aml_value *mv2)
{
	struct aml_value tmpint;

	memset(&tmpint, 0, sizeof(tmpint));
	_aml_setvalue(&tmpint, AML_OBJTYPE_INTEGER, mv1, NULL);
	switch (op) {
	case AML_MATCH_EQ:
		return aml_cmpvalue(&tmpint, mv2, AMLOP_LEQUAL);
	case AML_MATCH_LT:
		return aml_cmpvalue(&tmpint, mv2, AMLOP_LLESS);
	case AML_MATCH_LE:
		return aml_cmpvalue(&tmpint, mv2, AMLOP_LLESSEQUAL);
	case AML_MATCH_GE:
		return aml_cmpvalue(&tmpint, mv2, AMLOP_LGREATEREQUAL);
	case AML_MATCH_GT:
		return aml_cmpvalue(&tmpint, mv2, AMLOP_LGREATER);
	}
	return (1);
}

void
aml_getpciaddr(struct aml_scope *scope, struct aml_value *res)
{
	struct aml_node *node;
	struct aml_value *tmpres;

	/* PCI */
	tmpres = aml_alloctmp(scope, 1);
	node = aml_searchname(scope->node, "_ADR");
	if (node != NULL) {
		aml_evalterm(scope, node->value, tmpres);
		res->v_opregion.iobase += (aml_val2int(tmpres) << 16L);
	}
	node = aml_searchname(scope->node, "_BBN");
	if (node != NULL) {
		aml_evalterm(scope, node->value, tmpres);
		res->v_opregion.iobase += (aml_val2int(tmpres) << 48L);
	}
}

/* Fixup references for BufferFields/FieldUnits */
void
aml_fixref(struct aml_value **res)
{
	struct aml_value *oldres;

	while (*res && 
	       (*res)->type == AML_OBJTYPE_OBJREF && 
	       (*res)->v_objref.index == -1) 
	{
		oldres = (*res)->v_objref.ref;
		aml_delref(res);
		aml_addref(oldres);
		*res = oldres;
	}
}

int64_t
aml_parseint(struct aml_scope *scope, int opcode)
{
	uint8_t *np = scope->pos;
	struct aml_value *tmpval;
	int64_t rval;
	
	if (opcode == AML_ANYINT)
		opcode = aml_parseopcode(scope);
	switch (opcode) {
	case AMLOP_ZERO:
		rval = 0;
		break;
	case AMLOP_ONE:
		rval = 1;
		break;
	case AMLOP_ONES:
		rval = -1;
		break;
	case AMLOP_REVISION:
		rval = 0x101;
		break;
	case AMLOP_BYTEPREFIX:
		rval = *(uint8_t *)scope->pos;
		scope->pos += 1;
		break;
	case AMLOP_WORDPREFIX:
		rval = aml_letohost16(*(uint16_t *)scope->pos);
		scope->pos += 2;
		break;
	case AMLOP_DWORDPREFIX:
		rval = aml_letohost32(*(uint32_t *)scope->pos);
		scope->pos += 4;
		break;
	case AMLOP_QWORDPREFIX:
		rval = aml_letohost64(*(uint64_t *)scope->pos);
		scope->pos += 8;
		break;
	default:
		scope->pos = np;
		tmpval = aml_alloctmp(scope, 1);
		aml_parseterm(scope, tmpval);
		return aml_val2int(tmpval);
	}
	dnprintf(60,"%.4x: [%s] %s\n", 
		 aml_pc(scope->pos-opsize(opcode)), 
		 aml_nodename(scope->node),
		 aml_mnem(opcode));

	return rval;
}

struct aml_value *
aml_evaltarget(struct aml_scope *scope, struct aml_value *res)
{
	return res;
}

int
aml_evalterm(struct aml_scope *scope, struct aml_value *raw, 
	     struct aml_value *dst)
{
	int index, argc;
	struct aml_node *deref;
	struct aml_value *tmparg = NULL;

 loop:
	switch (raw->type) {
	case AML_OBJTYPE_NAMEREF:
		deref = aml_searchname(scope->node, raw->v_nameref);
		if (deref && deref->value) {
			raw = deref->value;
			goto loop;
		}
		aml_setvalue(scope, dst, raw, 0);
		break;
	case AML_OBJTYPE_OBJREF:
		while (raw->type == AML_OBJTYPE_OBJREF && raw->v_objref.index == -1) {
			raw = raw->v_objref.ref;
		}
		if (raw->type != AML_OBJTYPE_OBJREF)
			goto loop;

		index = raw->v_objref.index;
		raw = raw->v_objref.ref;
		switch (raw->type) {
		case AML_OBJTYPE_PACKAGE:
			if (index >= raw->length) {
				aml_setvalue(scope, dst, NULL, 0);
			}
			else {
				aml_setvalue(scope, dst, raw->v_package[index], 0);
			}
			goto loop;
			break;
		case AML_OBJTYPE_BUFFER:
			aml_setvalue(scope, dst, NULL, raw->v_buffer[index]);
			break;
		case AML_OBJTYPE_STRING:
			aml_setvalue(scope, dst, NULL, raw->v_string[index]);
			break;
		default:
			aml_die("evalterm");
			break;
		}
		break;

	case AML_OBJTYPE_METHOD:
		/* Read arguments from current scope */
		argc = AML_METHOD_ARGCOUNT(raw->v_method.flags);
		tmparg = aml_alloctmp(scope, argc);
		for (index=0; index<argc; index++) {
			aml_parseop(scope, &tmparg[index]);
			aml_addref(&tmparg[index]);
		}
		aml_evalmethod(raw->node, argc, tmparg, dst);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		aml_fieldio(scope, raw, dst, ACPI_IOREAD);
		break;
	default:
		aml_freevalue(dst);
		aml_setvalue(scope, dst, raw, 0);
		break;
	}
	return (0);
}


/*
 * @@@: Opcode functions
 */
struct aml_value *aml_parsenamed(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsenamedscope(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsemath(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsecompare(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parseif(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsewhile(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsebufpkg(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsemethod(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsesimple(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsefieldunit(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsebufferfield(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsemisc3(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsemuxaction(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsemisc2(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsematch(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parseref(struct aml_scope *, int, struct aml_value *);
struct aml_value *aml_parsestring(struct aml_scope *, int, struct aml_value *);

/* Parse named objects */
struct aml_value *
aml_parsenamed(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	uint8_t *name;

	AML_CHECKSTACK();
	name = aml_parsename(scope);

	res = aml_allocvalue(AML_OBJTYPE_UNINITIALIZED, 0, NULL);
	switch (opcode) {
	case AMLOP_NAME:
		aml_parseop(scope, res);
		break;
	case AMLOP_ALIAS:
		_aml_setvalue(res, AML_OBJTYPE_NAMEREF, 0, name);
		name = aml_parsename(scope);
		break;
	case AMLOP_EVENT:
		_aml_setvalue(res, AML_OBJTYPE_EVENT, 0, NULL);
		break;
	case AMLOP_MUTEX:
		_aml_setvalue(res, AML_OBJTYPE_MUTEX, 0, NULL);
		res->v_integer = aml_parseint(scope, AMLOP_BYTEPREFIX);
		break;
	case AMLOP_OPREGION:
		_aml_setvalue(res, AML_OBJTYPE_OPREGION, 0, NULL);
		res->v_opregion.iospace = aml_parseint(scope, AMLOP_BYTEPREFIX);
		res->v_opregion.iobase = aml_parseint(scope, AML_ANYINT);
		res->v_opregion.iolen = aml_parseint(scope, AML_ANYINT);
		if (res->v_opregion.iospace == GAS_PCI_CFG_SPACE)
			aml_getpciaddr(scope, res);
		break;
	}
	aml_createname(scope->node, name, res);

	return res;
}

/* Parse Named objects with scope */
struct aml_value *
aml_parsenamedscope(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	uint8_t *end, *name;
	struct aml_node *node;

	AML_CHECKSTACK();
	end = aml_parseend(scope);
	name = aml_parsename(scope);

	switch (opcode) {
	case AMLOP_DEVICE:
		res = aml_allocvalue(AML_OBJTYPE_DEVICE, 0, NULL);
		break;
	case AMLOP_SCOPE:
		res = NULL;
		break;
	case AMLOP_PROCESSOR:
		res = aml_allocvalue(AML_OBJTYPE_PROCESSOR, 0, NULL);
		res->v_processor.proc_id = aml_parseint(scope, AMLOP_BYTEPREFIX);
		res->v_processor.proc_addr = aml_parseint(scope, AMLOP_DWORDPREFIX);
		res->v_processor.proc_len = aml_parseint(scope, AMLOP_BYTEPREFIX);
		break;
	case AMLOP_POWERRSRC:
		res = aml_allocvalue(AML_OBJTYPE_POWERRSRC, 0, NULL);
		res->v_powerrsrc.pwr_level = aml_parseint(scope, AMLOP_BYTEPREFIX);
		res->v_powerrsrc.pwr_order = aml_parseint(scope, AMLOP_BYTEPREFIX);
		break;
	case AMLOP_THERMALZONE:
		res = aml_allocvalue(AML_OBJTYPE_THERMZONE, 0, NULL);
		break;
	}
	node = aml_createname(scope->node, name, res);
	aml_parsenode(node, scope->pos, &end, NULL);
	scope->pos = end;

	return res;
}

/* Parse math opcodes */
struct aml_value *
aml_parsemath(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmparg;
	int64_t i1, i2, i3;

	AML_CHECKSTACK();
	tmparg = aml_alloctmp(scope, 4);

	switch (opcode) {
	case AMLOP_LNOT:
		i2 = 0;
		i1 = aml_parseint(scope, AML_ANYINT);
		break;
	case AMLOP_LAND:
	case AMLOP_LOR:
		i1 = aml_parseint(scope, AML_ANYINT);
		i2 = aml_parseint(scope, AML_ANYINT);
		break;
	case AMLOP_NOT:
	case AMLOP_TOBCD:
	case AMLOP_FROMBCD:
	case AMLOP_TOINTEGER:
	case AMLOP_FINDSETLEFTBIT:
	case AMLOP_FINDSETRIGHTBIT:
		i2 = 0;
		i1 = aml_parseint(scope, AML_ANYINT);
		aml_parsetarget(scope, &tmparg[DST], NULL);
		break;
	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
		aml_parsetarget(scope, &tmparg[DST], NULL);
		aml_evalterm(scope, &tmparg[DST], &tmparg[LHS]);
		i1 = aml_val2int(&tmparg[LHS]);
		i2 = 1;
		break;
	case AMLOP_DIVIDE:
		i1 = aml_parseint(scope, AML_ANYINT);
		i2 = aml_parseint(scope, AML_ANYINT);
		aml_parsetarget(scope, &tmparg[DST2], NULL);  // remainder
		aml_parsetarget(scope, &tmparg[DST], NULL);   // quotient

		aml_setvalue(scope, &tmparg[DST2], NULL, (i1 % i2));
		break;
	default:
		i1 = aml_parseint(scope, AML_ANYINT);
		i2 = aml_parseint(scope, AML_ANYINT);
		aml_parsetarget(scope, &tmparg[DST], NULL);
		break;
	}
	i3 = aml_evalexpr(i1, i2, opcode);
	aml_setvalue(scope, res, NULL, i3);
	aml_setvalue(scope, &tmparg[DST], NULL, i3);

	return (res);
}

/* Parse logical comparison opcodes */
struct aml_value *
aml_parsecompare(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmparg;
	int rc;

	AML_CHECKSTACK();
	tmparg = aml_alloctmp(scope, 2);
	aml_parseterm(scope, &tmparg[LHS]);
	aml_parseterm(scope, &tmparg[RHS]);

	/* Compare both values */
	rc = aml_cmpvalue(&tmparg[LHS], &tmparg[RHS], opcode);  
	aml_setvalue(scope, res, NULL, rc);

	return res;
}

/* Parse IF/ELSE opcodes */
struct aml_value *
aml_parseif(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	int64_t test;
	uint8_t *end;

	AML_CHECKSTACK();
	end = aml_parseend(scope);
	test = aml_parseint(scope, AML_ANYINT);

	dnprintf(40, "@ iftest: %llx\n", test);
	while (test && scope->pos && scope->pos < end) {
		/* Parse if scope */
		aml_parseterm(scope, res);
	}
	if (*end == AMLOP_ELSE) {
		scope->pos = ++end;
		end = aml_parseend(scope);
		while (!test && scope->pos && scope->pos < end) {
			/* Parse ELSE scope */
			aml_parseterm(scope, res);
		}
	}
	if (scope->pos < end)
		scope->pos = end;
	return res;
}

struct aml_value *
aml_parsewhile(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	uint8_t *end, *start;
	int test, cnt;

	AML_CHECKSTACK();
	end = aml_parseend(scope);
	start = scope->pos;
	cnt = 0;
	do {
		test = 1;
		if (scope->pos == start || scope->pos == end) {
			scope->pos = start;
			test = aml_parseint(scope, AML_ANYINT);
			dnprintf(40, "@whiletest = %d %x\n", test, cnt++);
		}
		else if (*scope->pos == AMLOP_BREAK) {
			scope->pos++;
			test = 0;
		}
		else if (*scope->pos == AMLOP_CONTINUE) {
			scope->pos = start;
		}
		else {
			aml_parseterm(scope, res);
		}
	} while (test && scope->pos <= end && cnt < 0x199);
	/* XXX: shouldn't need breakout counter */

	dnprintf(40,"Set While end : %x\n", cnt);
	if (scope->pos < end)
		scope->pos = end;
	return res;
}

/* Parse Buffer/Package opcodes */
struct aml_value *
aml_parsebufpkg(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	uint8_t *end;
	int len;

	AML_CHECKSTACK();
	end = aml_parseend(scope);
	len = aml_parseint(scope, (opcode == AMLOP_PACKAGE) ? 
			   AMLOP_BYTEPREFIX : AML_ANYINT);

	switch (opcode) {
	case AMLOP_BUFFER:
		_aml_setvalue(res, AML_OBJTYPE_BUFFER, len, NULL);
		if (scope->pos < end) {
			memcpy(res->v_buffer, scope->pos, end-scope->pos);
		}
		if (len != end-scope->pos) {
			dnprintf(99,"buffer: %.4x %.4x\n", len, end-scope->pos);
		}
		break;
	case AMLOP_PACKAGE:
	case AMLOP_VARPACKAGE:
		_aml_setvalue(res, AML_OBJTYPE_PACKAGE, len, NULL);
		for (len=0; len < res->length && scope->pos < end; len++) {
			aml_parseop(scope, res->v_package[len]);
		}
		if (scope->pos != end) {
			dnprintf(99,"Package not equiv!! %.4x %.4x %d of %d\n", 
				 aml_pc(scope->pos), aml_pc(end), len, res->length);
		}
		break;
	}
	scope->pos = end;
	return res;
}

struct aml_value *
aml_parsemethod(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	uint8_t *end, *name;

	AML_CHECKSTACK();
	end = aml_parseend(scope);
	name = aml_parsename(scope);

	res = aml_allocvalue(AML_OBJTYPE_METHOD, 0, NULL);
	res->v_method.flags = aml_parseint(scope, AMLOP_BYTEPREFIX);
	res->v_method.start = scope->pos;
	res->v_method.end = end;
	aml_createname(scope->node, name, res);

	scope->pos = end;

	return res;
}

/* Parse simple type opcodes */
struct aml_value *
aml_parsesimple(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_node *node;

	AML_CHECKSTACK();
	switch (opcode) {
	case AMLOP_ZERO:
		_aml_setvalue(res, AML_OBJTYPE_INTEGER+AML_STATIC, 
			      aml_parseint(scope, opcode), 
			      NULL);
		break;
	case AMLOP_ONE:
	case AMLOP_ONES:
	case AMLOP_BYTEPREFIX:
	case AMLOP_WORDPREFIX:
	case AMLOP_DWORDPREFIX:
	case AMLOP_QWORDPREFIX:
	case AMLOP_REVISION:
		_aml_setvalue(res, AML_OBJTYPE_INTEGER, 
			      aml_parseint(scope, opcode), 
			      NULL);
		break;
	case AMLOP_DEBUG:
		_aml_setvalue(res, AML_OBJTYPE_DEBUGOBJ, 0, NULL);
		break;
	case AMLOP_STRINGPREFIX:
		_aml_setvalue(res, AML_OBJTYPE_STRING, -1, scope->pos);
		scope->pos += res->length+1;
		break;
	case AMLOP_NAMECHAR:
		_aml_setvalue(res, AML_OBJTYPE_NAMEREF, 0, NULL);
		res->v_nameref = aml_parsename(scope);
		node = aml_searchname(scope->node, res->v_nameref);
		if (node && node->value) {
			_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, node->value);
		}
		break;
	}  
	return res;
}

/* Parse field unit opcodes */
struct aml_value *
aml_parsefieldunit(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	uint8_t *end, *name;
	struct aml_value *fld;

	AML_CHECKSTACK();
	end = aml_parseend(scope);

	switch (opcode) {
	case AMLOP_FIELD:
		aml_parsetarget(scope, NULL, &res->v_field.ref1);
		break;
	case AMLOP_INDEXFIELD:
		aml_parsetarget(scope, NULL, &res->v_field.ref1);
		aml_parsetarget(scope, NULL, &res->v_field.ref2);
		break;
	case AMLOP_BANKFIELD:
		aml_parsetarget(scope, NULL, &res->v_field.ref1);
		aml_parsetarget(scope, NULL, &res->v_field.ref2);
		res->v_field.ref3 = aml_parseint(scope, AML_ANYINT);
		break;
	}
	res->v_field.flags = aml_parseint(scope, AMLOP_BYTEPREFIX);
	res->v_field.type = opcode;

	aml_fixref(&res->v_field.ref1);
	aml_fixref(&res->v_field.ref2);

	while (scope->pos < end) {
		switch (*scope->pos) {
		case 0x00: // reserved
			scope->pos++;
			res->v_field.bitlen = aml_parselength(scope);
			break;
		case 0x01: // attrib
			scope->pos++;
			/* XXX: do something with this */
			aml_parseint(scope, AMLOP_BYTEPREFIX);
			aml_parseint(scope, AMLOP_BYTEPREFIX);
			res->v_field.bitlen = 0;
			break;
		default:
			name = aml_parsename(scope);
			res->v_field.bitlen = aml_parselength(scope);

			/* Allocate new fieldunit */
			fld = aml_allocvalue(AML_OBJTYPE_FIELDUNIT, 0, NULL);

			/* Increase reference count on field */
			fld->v_field = res->v_field;
			aml_addref(fld->v_field.ref1);
			aml_addref(fld->v_field.ref2);

			aml_createname(scope->node, name, fld);
			break;
		}
		res->v_field.bitpos += res->v_field.bitlen;
	}
	/* Delete redundant reference */
	aml_delref(&res->v_field.ref1);
	aml_delref(&res->v_field.ref2);
	return res;
}

/* Parse CreateXXXField opcodes */
struct aml_value *
aml_parsebufferfield(struct aml_scope *scope, int opcode, 
		     struct aml_value *res)
{
	uint8_t *name;

	AML_CHECKSTACK();
	res = aml_allocvalue(AML_OBJTYPE_BUFFERFIELD, 0, NULL);
	res->v_field.type = opcode;
	aml_parsetarget(scope, NULL, &res->v_field.ref1);
	res->v_field.bitpos = aml_parseint(scope, AML_ANYINT);

	aml_fixref(&res->v_field.ref1);

	switch (opcode) {
	case AMLOP_CREATEFIELD:
		res->v_field.bitlen = aml_parseint(scope, AML_ANYINT);
		break;
	case AMLOP_CREATEBITFIELD:
		res->v_field.bitlen = 1;
		break;
	case AMLOP_CREATEBYTEFIELD:
		res->v_field.bitlen = 8;
		res->v_field.bitpos *= 8;
		break;
	case AMLOP_CREATEWORDFIELD:
		res->v_field.bitlen = 16;
		res->v_field.bitpos *= 8;
		break;
	case AMLOP_CREATEDWORDFIELD:
		res->v_field.bitlen = 32;
		res->v_field.bitpos *= 8;
		break;
	case AMLOP_CREATEQWORDFIELD:
		res->v_field.bitlen = 64;
		res->v_field.bitpos *= 8;
		break;
	}
	name = aml_parsename(scope);
	aml_createname(scope->node, name, res);

	return res;
}

/* Parse Mutex/Event action */
struct aml_value *
aml_parsemuxaction(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmparg;
	int64_t i1;

	AML_CHECKSTACK();

	tmparg = aml_alloctmp(scope, 1);
	aml_parsetarget(scope, tmparg, NULL);
	switch (opcode) {
	case AMLOP_ACQUIRE:
		/* Assert: tmparg is AML_OBJTYPE_MUTEX */
		i1 = aml_parseint(scope, AMLOP_WORDPREFIX);

		/* Return true if timed out */
		aml_setvalue(scope, res, NULL, 0);
		break;
	case AMLOP_RELEASE:
		break;

	case AMLOP_WAIT:
		/* Assert: tmparg is AML_OBJTYPE_EVENT */
		i1 = aml_parseint(scope, AML_ANYINT);

		/* Return true if timed out */
		aml_setvalue(scope, res, NULL, 0);
		break;
	case AMLOP_SIGNAL:
		break;
	case AMLOP_RESET:
		break;
	}

	return res;
}

/* Parse Miscellaneous opcodes */
struct aml_value *
aml_parsemisc2(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmparg, *dev;
	int i1, i2, i3;

	switch (opcode) {
	case AMLOP_NOTIFY:
		/* Assert: tmparg is nameref or objref */
		tmparg = aml_alloctmp(scope, 1);
		aml_parseop(scope, tmparg);
		dev = aml_dereftarget(scope, tmparg);

		i1 = aml_parseint(scope, AML_ANYINT);
		if (dev && dev->node) {
			dnprintf(0,"Notify: [%s] %.2x\n", 
				 aml_nodename(dev->node), i1);
			aml_notify(dev->node, i1);
		}
		break;
	case AMLOP_SLEEP:
		i1 = aml_parseint(scope, AML_ANYINT);
		dnprintf(10,"SLEEP: %x\n", i1);
		break;
	case AMLOP_STALL:
		i1 = aml_parseint(scope, AML_ANYINT);
		dnprintf(10,"STALL: %x\n", i1);
		break;
	case AMLOP_FATAL:
		i1 = aml_parseint(scope, AMLOP_BYTEPREFIX);
		i2 = aml_parseint(scope, AMLOP_DWORDPREFIX);
		i3 = aml_parseint(scope, AML_ANYINT);
		aml_die("FATAL: %x %x %x\n", i1, i2, i3);
		break;
	}
	return res;
}

/* Parse Miscellaneous opcodes */
struct aml_value *
aml_parsemisc3(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmparg;

	AML_CHECKSTACK();
	tmparg = aml_alloctmp(scope, 1);
	aml_parseterm(scope, tmparg);
	switch (opcode) {
	case AMLOP_SIZEOF:
		aml_setvalue(scope, res, NULL, tmparg->length);
		break;
	case AMLOP_OBJECTTYPE:
		aml_setvalue(scope, res, NULL, tmparg->type);
		break;
	}

	return res;
}

/* Parse AMLOP_MATCH */
struct aml_value *
aml_parsematch(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *pkg;
	int op1, op2, idx, mv1, mv2;

	AML_CHECKSTACK();
	pkg = aml_parseterm(scope, NULL);
	op1 = aml_parseint(scope, AMLOP_BYTEPREFIX);
	mv1 = aml_parseint(scope, AML_ANYINT);
	op2 = aml_parseint(scope, AMLOP_BYTEPREFIX);
	mv2 = aml_parseint(scope, AML_ANYINT);
	idx = aml_parseint(scope, AML_ANYINT);

	aml_setvalue(scope, res, NULL, -1);
	while (idx < pkg->length) {
		if (aml_match(op1, mv1, pkg->v_package[idx]) || 
		    aml_match(op2, mv2, pkg->v_package[idx])) 
		{
			aml_setvalue(scope, res, NULL, idx);
			break;
		}
		idx++;
	}
	aml_delref(&pkg);
	return res;
}

/* Parse referenced objects */
struct aml_value *
aml_parseref(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmparg;

	AML_CHECKSTACK();

	tmparg = aml_alloctmp(scope, 4);
	switch (opcode) {
	case AMLOP_INDEX:
		aml_parsetarget(scope, res, NULL);
		opcode = aml_parseint(scope, AML_ANYINT);
		aml_parsetarget(scope, &tmparg[DST], NULL);
    
		if (res->type == AML_OBJTYPE_OBJREF && res->v_objref.index == -1)
		{
			dnprintf(10,"fixup index\n");
			res->v_objref.index = opcode;
			aml_setvalue(scope, &tmparg[DST], res, 0);
		}
		break;
	case AMLOP_DEREFOF:
		aml_parseop(scope, res);
		break;
	case AMLOP_RETURN:
		aml_parseterm(scope, &tmparg[DST]);
		aml_setvalue(scope, res, &tmparg[DST], 0);
		while (scope) {
			scope->pos = scope->end;
			scope = scope->parent;
		}
		break;
	case AMLOP_ARG0 ... AMLOP_ARG6:
		if (scope && scope->args) {
			opcode -= AMLOP_ARG0;
			if (opcode >= scope->nargs) {
				aml_die("arg out of range: %x\n", opcode);
			}

			/* Create OBJREF to stack variable */
			_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, &scope->args[opcode]);
		}
		break;
	case AMLOP_LOCAL0 ... AMLOP_LOCAL7:
		if (scope && scope->locals) {
			/* Create OBJREF to stack variable */
			_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, 
				      &scope->locals[opcode - AMLOP_LOCAL0]);
		}
		break;
	case AMLOP_LOAD:
		aml_parseop(scope, &tmparg[LHS]);
		aml_parseop(scope, &tmparg[RHS]);
		break;
	case AMLOP_STORE:
		aml_parseterm(scope, res);
		aml_parsetarget(scope, &tmparg[DST], NULL);
		aml_setvalue(scope, &tmparg[DST], res, 0);
		break;
	case AMLOP_REFOF:
		_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, NULL);
		aml_parsetarget(scope, NULL, &res->v_objref.ref);
		break;
	case AMLOP_CONDREFOF:
		/* Returns true if object exists */
		aml_parseterm(scope, &tmparg[LHS]);
		aml_parsetarget(scope, &tmparg[DST], NULL);
		if (tmparg[LHS].type != AML_OBJTYPE_NAMEREF) {
			/* Object exists */
			aml_setvalue(scope, &tmparg[DST], &tmparg[LHS], 0);
			aml_setvalue(scope, res, NULL, 1);
		}
		else {
			/* Object doesn't exist */
			aml_setvalue(scope, res, NULL, 0);
		}
		break;
	}

	return res;
}

struct aml_value *
aml_parsestring(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmpval;
	int i1, i2;

	AML_CHECKSTACK();
	tmpval = aml_alloctmp(scope, 4);
	switch (opcode) {
	case AMLOP_MID:
		aml_parseterm(scope, &tmpval[0]);
		i1 = aml_parseint(scope, AML_ANYINT); // start
		i2 = aml_parseint(scope, AML_ANYINT); // length
		aml_parsetarget(scope, &tmpval[1], NULL);
		if (i1 > tmpval[0].length)
			i1 = tmpval[0].length;
		if (i1+i2 > tmpval[0].length)
			i2 = tmpval[0].length-i1;
		_aml_setvalue(res, AML_OBJTYPE_STRING, i2, tmpval[0].v_string+i1);
		break;
	case AMLOP_TODECSTRING:
	case AMLOP_TOHEXSTRING:
		i1 = aml_parseint(scope, AML_ANYINT);
		_aml_setvalue(res, AML_OBJTYPE_STRING, 10, NULL);
		snprintf(res->v_string, res->length, ((opcode == AMLOP_TODECSTRING) ? 
						      "%d" : "%x"), i1);
		break;
	default:
		aml_die("to_string");
		break;
	}

	return res;
}

struct aml_value *
aml_parseterm(struct aml_scope *scope, struct aml_value *res)
{
	struct aml_value *tmpres;

	/* If no value specified, allocate dynamic */
	if (res == NULL)
		res = aml_allocvalue(AML_OBJTYPE_UNINITIALIZED, 0, NULL);
	tmpres = aml_alloctmp(scope, 1);
	aml_parseop(scope, tmpres);
	aml_evalterm(scope, tmpres, res);
	return res;
}

struct aml_value *
aml_parsetarget(struct aml_scope *scope, struct aml_value *res, struct aml_value **opt)
{
	struct aml_value *dummy;

	/* If no value specified, allocate dynamic */
	if (res == NULL)
		res = aml_allocvalue(AML_OBJTYPE_UNINITIALIZED, 0, NULL);
	aml_parseop(scope, res);
	dummy = aml_evaltarget(scope, res);
	if (opt != NULL)
		*opt = dummy;
	return res;
}

struct aml_value *
aml_parseop(struct aml_scope *scope, struct aml_value *res)
{
	int opcode;
	struct aml_value *rval;

	opcode = aml_parseopcode(scope);
	dnprintf(60,"%.4x: [%s] %s\n", 
		 aml_pc(scope->pos-opsize(opcode)), 
		 aml_nodename(scope->node),
		 aml_mnem(opcode));
	aml_freevalue(res);

	switch (opcode) {
	case AMLOP_ZERO:
	case AMLOP_ONE:
	case AMLOP_ONES:
	case AMLOP_REVISION:
	case AMLOP_BYTEPREFIX:
	case AMLOP_WORDPREFIX:
	case AMLOP_DWORDPREFIX:
	case AMLOP_QWORDPREFIX:
	case AMLOP_STRINGPREFIX:
	case AMLOP_DEBUG:
	case AMLOP_NAMECHAR:
	case AMLOP_NOP:
		rval = aml_parsesimple(scope, opcode, res);
		break;
	case AMLOP_LNOT:
	case AMLOP_LAND:
	case AMLOP_LOR:
	case AMLOP_NOT:
	case AMLOP_TOBCD:
	case AMLOP_FROMBCD:
	case AMLOP_TOINTEGER:
	case AMLOP_FINDSETLEFTBIT:
	case AMLOP_FINDSETRIGHTBIT:
	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
	case AMLOP_DIVIDE:
	case AMLOP_ADD: 
	case AMLOP_SUBTRACT:
	case AMLOP_MULTIPLY:
	case AMLOP_SHL:
	case AMLOP_SHR:
	case AMLOP_AND:
	case AMLOP_NAND:
	case AMLOP_OR:
	case AMLOP_NOR:
	case AMLOP_XOR:
	case AMLOP_MOD:
		rval = aml_parsemath(scope, opcode, res);
		break;
	case AMLOP_MATCH:
		rval = aml_parsematch(scope, opcode, res);
		break;
	case AMLOP_LLESS:
	case AMLOP_LLESSEQUAL:
	case AMLOP_LEQUAL:
	case AMLOP_LNOTEQUAL:
	case AMLOP_LGREATEREQUAL:
	case AMLOP_LGREATER:
		rval = aml_parsecompare(scope, opcode, res);
		break;
	case AMLOP_NAME:
	case AMLOP_ALIAS:
	case AMLOP_EVENT:
	case AMLOP_MUTEX:
	case AMLOP_OPREGION:
		rval = aml_parsenamed(scope, opcode, res);
		break;
	case AMLOP_DEVICE:
	case AMLOP_SCOPE:
	case AMLOP_PROCESSOR:
	case AMLOP_POWERRSRC:
	case AMLOP_THERMALZONE:
		rval = aml_parsenamedscope(scope, opcode, res);
		break;
	case AMLOP_ACQUIRE:
	case AMLOP_RELEASE:
	case AMLOP_WAIT:
	case AMLOP_SIGNAL:
	case AMLOP_RESET:
		rval = aml_parsemuxaction(scope, opcode, res);
		break;
	case AMLOP_SLEEP:
	case AMLOP_STALL:
	case AMLOP_FATAL:
	case AMLOP_NOTIFY:
		rval = aml_parsemisc2(scope, opcode, res);
		break;
	case AMLOP_SIZEOF:
	case AMLOP_OBJECTTYPE:
		rval = aml_parsemisc3(scope, opcode, res);
		break;
	case AMLOP_CREATEFIELD:
	case AMLOP_CREATEBITFIELD:
	case AMLOP_CREATEBYTEFIELD:
	case AMLOP_CREATEWORDFIELD:
	case AMLOP_CREATEDWORDFIELD:
	case AMLOP_CREATEQWORDFIELD:
		rval = aml_parsebufferfield(scope, opcode, res);
		break;
	case AMLOP_FIELD:
	case AMLOP_INDEXFIELD:
	case AMLOP_BANKFIELD:
		rval = aml_parsefieldunit(scope, opcode, res);
		break;
	case AMLOP_BUFFER:
	case AMLOP_PACKAGE:
	case AMLOP_VARPACKAGE:
		rval = aml_parsebufpkg(scope, opcode, res);
		break;
	case AMLOP_IF:
		rval = aml_parseif(scope, opcode, res);
		break;
	case AMLOP_WHILE:
		rval = aml_parsewhile(scope, opcode, res);
		break;
	case AMLOP_MID:
	case AMLOP_TOSTRING:
	case AMLOP_TODECSTRING:
	case AMLOP_TOHEXSTRING:
		rval = aml_parsestring(scope, opcode, res);
		break;
	case AMLOP_INDEX:
	case AMLOP_REFOF:
	case AMLOP_DEREFOF:
	case AMLOP_CONDREFOF:
	case AMLOP_STORE:
	case AMLOP_RETURN:
	case AMLOP_LOAD:
	case AMLOP_ARG0 ... AMLOP_ARG6:
	case AMLOP_LOCAL0 ... AMLOP_LOCAL7:
		rval = aml_parseref(scope, opcode, res);
		break;
	case AMLOP_METHOD:
		rval = aml_parsemethod(scope, opcode, res);
		break;
	default:
		aml_die("Unknown opcode: %.4x", opcode);
		break;
	}
	return rval;
}

const char hext[] = "0123456789ABCDEF";

const char *
aml_eisaid(u_int32_t pid)
{
	static char id[8];

	id[0] = '@' + ((pid >> 2) & 0x1F);
	id[1] = '@' + ((pid << 3) & 0x18) + ((pid >> 13) & 0x7);
	id[2] = '@' + ((pid >> 8) & 0x1F);
	id[3] = hext[(pid >> 20) & 0xF];
	id[4] = hext[(pid >> 16) & 0xF];
	id[5] = hext[(pid >> 28) & 0xF];
	id[6] = hext[(pid >> 24) & 0xF];
	id[7] = 0;

	return id;
}

/*
 * @@@: Fixup DSDT code
 */
struct aml_fixup
{
	int      offset;
	u_int8_t oldv;
	u_int8_t newv;
};

struct aml_blacklist
{
	const char       *oem;
	const char       *oemtbl;
	u_int8_t          cksum;
	struct aml_fixup *fixtab;
};

struct aml_fixup __ibm300gl[] = {
	{ 0x19, 0x3a, 0x3b },
	{ -1 }
};

struct aml_blacklist amlfix_list[] = {
	{ "IBM   ", "CDTPWSNH", 0x41, __ibm300gl },
	{ NULL },
};

void
aml_fixup_dsdt(u_int8_t *acpi_hdr, u_int8_t *base, int len)
{
	struct acpi_table_header *hdr = (struct acpi_table_header *)acpi_hdr;
	struct aml_blacklist *fixlist;
	struct aml_fixup *fixtab;

	for (fixlist=amlfix_list; fixlist->oem; fixlist++) {
		if (!memcmp(fixlist->oem, hdr->oemid, 6) &&
		    !memcmp(fixlist->oemtbl, hdr->oemtableid, 8) &&
		    fixlist->cksum == hdr->checksum)
		{
			/* Found a potential fixup entry */
			for (fixtab = fixlist->fixtab; fixtab->offset != -1; fixtab++) {
				if (base[fixtab->offset] == fixtab->oldv)
					base[fixtab->offset] = fixtab->newv;
			}
		}
	}
}

/*
 * @@@: Default Object creation
 */
struct aml_defval
{
	const char        *name;
	int                type;
	int64_t            ival;
	const void        *bval;
	struct aml_value **gval;
};

struct aml_defval aml_defobj[] = {
	{ "_OS_", AML_OBJTYPE_STRING, -1, "OpenBSD" },
	{ "_REV", AML_OBJTYPE_INTEGER, 2, NULL },
	{ "_GL",  AML_OBJTYPE_MUTEX,   1, NULL, &aml_global_lock },
	{ NULL }
};

void
aml_create_defaultobjects()
{
	struct aml_value *tmp;
	struct aml_defval *def;

	for (def = aml_defobj; def->name; def++) {
		/* Allocate object value + add to namespace */
		tmp = aml_allocvalue(def->type, def->ival, def->bval);
		aml_createname(&aml_root, def->name, tmp);
		if (def->gval) {
			/* Set root object pointer */
			*def->gval = tmp;
		}
	}
}

int
acpi_parse_aml(struct acpi_softc *sc, u_int8_t *start, u_int32_t length)
{
	u_int8_t *end;

	dsdt_softc = sc;

	strlcpy(aml_root.name, "\\", sizeof(aml_root.name));
	if (aml_root.start == NULL) {
		aml_root.start = start;
		aml_root.end = start+length;
	}
	end = start+length;
	aml_parsenode(&aml_root, start, &end, NULL);
	dnprintf(50, " : parsed %d AML bytes\n", length);

	return (0);
}

/* XXX: kill me */
int aml_parse_length(struct acpi_context *ctx)
{
	return (0);
}
int64_t			aml_eparseint(struct acpi_context * ctx, int style)
{
	return (0);
}
struct aml_opcode	*aml_getopcode(struct acpi_context *ctx)
{
	return NULL;
}
void			acpi_freecontext(struct acpi_context *ctx)
{
}
const char		*aml_parse_name(struct acpi_context *ctx)
{
	return "";
}
