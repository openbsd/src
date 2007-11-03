/* $OpenBSD: dsdt.c,v 1.93 2007/11/03 17:48:10 ckuethe Exp $ */
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
#include <sys/proc.h>

#include <machine/bus.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_command.h>
#endif

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#define opsize(opcode) (((opcode) & 0xFF00) ? 2 : 1)

#define AML_CHECKSTACK()

#define AML_FIELD_RESERVED	0x00
#define AML_FIELD_ATTRIB	0x01

#define AML_REVISION		0x01
#define AML_INTSTRLEN		16
#define AML_NAMESEG_LEN		4

#define aml_valid(pv)	 ((pv) != NULL)

#define aml_ipaddr(n) ((n)-aml_root.start)

extern int hz;

struct aml_scope;

int			aml_cmpvalue(struct aml_value *, struct aml_value *, int);
void			aml_copyvalue(struct aml_value *, struct aml_value *);

void			aml_setvalue(struct aml_scope *, struct aml_value *,
			    struct aml_value *, int64_t);
void			aml_freevalue(struct aml_value *);
struct aml_value	*aml_allocvalue(int, int64_t, const void *);
struct aml_value	*_aml_setvalue(struct aml_value *, int, int64_t,
			    const void *);

u_int64_t		aml_convradix(u_int64_t, int, int);
int64_t			aml_evalexpr(int64_t, int64_t, int);
int			aml_lsb(u_int64_t);
int			aml_msb(u_int64_t);

int			aml_tstbit(const u_int8_t *, int);
void			aml_setbit(u_int8_t *, int, int);

void			aml_bufcpy(void *, int, const void *, int, int);
int			aml_evalinteger(struct acpi_softc *, struct aml_node *,
			    const char *, int, struct aml_value *, int64_t *);

void			_aml_delref(struct aml_value **val, const char *, int);
void			aml_delref(struct aml_value **);
void			aml_addref(struct aml_value *);
int			aml_pc(uint8_t *);

#define aml_delref(x) _aml_delref(x, __FUNCTION__, __LINE__)

struct aml_value	*aml_parseop(struct aml_scope *, struct aml_value *);
struct aml_value	*aml_parsetarget(struct aml_scope *, struct aml_value *,
			    struct aml_value **);
struct aml_value	*aml_parseterm(struct aml_scope *, struct aml_value *);

struct aml_value	*aml_evaltarget(struct aml_scope *scope,
			    struct aml_value *res);
int			aml_evalterm(struct aml_scope *scope,
			    struct aml_value *raw, struct aml_value *dst);
void			aml_gasio(struct acpi_softc *, int, uint64_t, uint64_t,
			    int, int, int, void *, int);

struct aml_opcode	*aml_findopcode(int);

#define acpi_os_malloc(sz) _acpi_os_malloc(sz, __FUNCTION__, __LINE__)
#define acpi_os_free(ptr)  _acpi_os_free(ptr, __FUNCTION__, __LINE__)

void			*_acpi_os_malloc(size_t, const char *, int);
void			_acpi_os_free(void *, const char *, int);
void			acpi_sleep(int);
void			acpi_stall(int);

struct aml_value	*aml_callosi(struct aml_scope *, struct aml_value *);
struct aml_value	*aml_callmethod(struct aml_scope *, struct aml_value *);
struct aml_value	*aml_evalmethod(struct aml_scope *, struct aml_node *,
			    int, struct aml_value *, struct aml_value *);

const char *aml_getname(const char *);
void			aml_dump(int, u_int8_t *);
void			_aml_die(const char *fn, int line, const char *fmt, ...);
#define aml_die(x...)	_aml_die(__FUNCTION__, __LINE__, x)

/*
 * @@@: Global variables
 */
int			aml_intlen = 64;
struct aml_node		aml_root;
struct aml_value	*aml_global_lock;
struct acpi_softc	*dsdt_softc;

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

/* Perfect Hash key */
#define HASH_OFF		6904
#define HASH_SIZE		179
#define HASH_KEY(k)		(((k) ^ HASH_OFF) % HASH_SIZE)

/*
 * XXX this array should be sorted, and then aml_findopcode() should
 * do a binary search
 */
struct aml_opcode **aml_ophash;
struct aml_opcode aml_table[] = {
	/* Simple types */
	{ AMLOP_ZERO,		"Zero",		"c",	aml_parsesimple },
	{ AMLOP_ONE,		"One",		"c",	aml_parsesimple },
	{ AMLOP_ONES,		"Ones",		"c",	aml_parsesimple },
	{ AMLOP_REVISION,	"Revision",	"R",	aml_parsesimple },
	{ AMLOP_BYTEPREFIX,	".Byte",	"b",	aml_parsesimple },
	{ AMLOP_WORDPREFIX,	".Word",	"w",	aml_parsesimple },
	{ AMLOP_DWORDPREFIX,	".DWord",	"d",	aml_parsesimple },
	{ AMLOP_QWORDPREFIX,	".QWord",	"q",	aml_parsesimple },
	{ AMLOP_STRINGPREFIX,	".String",	"a",	aml_parsesimple },
	{ AMLOP_DEBUG,		"DebugOp",	"D",	aml_parsesimple },
	{ AMLOP_BUFFER,		"Buffer",	"piB",	aml_parsebufpkg },
	{ AMLOP_PACKAGE,	"Package",	"pbT",	aml_parsebufpkg },
	{ AMLOP_VARPACKAGE,	"VarPackage",	"piT",	aml_parsebufpkg },

	/* Simple objects */
	{ AMLOP_LOCAL0,		"Local0",	"L",	aml_parseref },
	{ AMLOP_LOCAL1,		"Local1",	"L",	aml_parseref },
	{ AMLOP_LOCAL2,		"Local2",	"L",	aml_parseref },
	{ AMLOP_LOCAL3,		"Local3",	"L",	aml_parseref },
	{ AMLOP_LOCAL4,		"Local4",	"L",	aml_parseref },
	{ AMLOP_LOCAL5,		"Local5",	"L",	aml_parseref },
	{ AMLOP_LOCAL6,		"Local6",	"L",	aml_parseref },
	{ AMLOP_LOCAL7,		"Local7",	"L",	aml_parseref },
	{ AMLOP_ARG0,		"Arg0",		"A",	aml_parseref },
	{ AMLOP_ARG1,		"Arg1",		"A",	aml_parseref },
	{ AMLOP_ARG2,		"Arg2",		"A",	aml_parseref },
	{ AMLOP_ARG3,		"Arg3",		"A",	aml_parseref },
	{ AMLOP_ARG4,		"Arg4",		"A",	aml_parseref },
	{ AMLOP_ARG5,		"Arg5",		"A",	aml_parseref },
	{ AMLOP_ARG6,		"Arg6",		"A",	aml_parseref },

	/* Control flow */
	{ AMLOP_IF,		"If",		"pI",	aml_parseif },
	{ AMLOP_ELSE,		"Else",		"pT" },
	{ AMLOP_WHILE,		"While",	"piT",	aml_parsewhile },
	{ AMLOP_BREAK,		"Break",	"" },
	{ AMLOP_CONTINUE,	"Continue",	"" },
	{ AMLOP_RETURN,		"Return",	"t",	aml_parseref },
	{ AMLOP_FATAL,		"Fatal",	"bdi",	aml_parsemisc2 },
	{ AMLOP_NOP,		"Nop",		"",	aml_parsesimple },
	{ AMLOP_BREAKPOINT,	"BreakPoint",	"" },

	/* Arithmetic operations */
	{ AMLOP_INCREMENT,	"Increment",	"t",	aml_parsemath },
	{ AMLOP_DECREMENT,	"Decrement",	"t",	aml_parsemath },
	{ AMLOP_ADD,		"Add",		"iir",	aml_parsemath },
	{ AMLOP_SUBTRACT,	"Subtract",	"iir",	aml_parsemath },
	{ AMLOP_MULTIPLY,	"Multiply",	"iir",	aml_parsemath },
	{ AMLOP_DIVIDE,		"Divide",	"iirr",	aml_parsemath },
	{ AMLOP_SHL,		"ShiftLeft",	"iir",	aml_parsemath },
	{ AMLOP_SHR,		"ShiftRight",	"iir",	aml_parsemath },
	{ AMLOP_AND,		"And",		"iir",	aml_parsemath },
	{ AMLOP_NAND,		"Nand",		"iir",	aml_parsemath },
	{ AMLOP_OR,		"Or",		"iir",	aml_parsemath },
	{ AMLOP_NOR,		"Nor",		"iir",	aml_parsemath },
	{ AMLOP_XOR,		"Xor",		"iir",	aml_parsemath },
	{ AMLOP_NOT,		"Not",		"ir",	aml_parsemath },
	{ AMLOP_MOD,		"Mod",		"iir",	aml_parsemath },
	{ AMLOP_FINDSETLEFTBIT,	"FindSetLeftBit", "ir",	aml_parsemath },
	{ AMLOP_FINDSETRIGHTBIT,"FindSetRightBit", "ir",aml_parsemath },

	/* Logical test operations */
	{ AMLOP_LAND,		"LAnd",		"ii",	aml_parsemath },
	{ AMLOP_LOR,		"LOr",		"ii",	aml_parsemath },
	{ AMLOP_LNOT,		"LNot",		"i",	aml_parsemath },
	{ AMLOP_LNOTEQUAL,	"LNotEqual",	"tt",	aml_parsecompare },
	{ AMLOP_LLESSEQUAL,	"LLessEqual",	"tt",	aml_parsecompare },
	{ AMLOP_LGREATEREQUAL,	"LGreaterEqual", "tt",	aml_parsecompare },
	{ AMLOP_LEQUAL,		"LEqual",	"tt",	aml_parsecompare },
	{ AMLOP_LGREATER,	"LGreater",	"tt",	aml_parsecompare },
	{ AMLOP_LLESS,		"LLess",	"tt",	aml_parsecompare },

	/* Named objects */
	{ AMLOP_NAMECHAR,	".NameRef",	"n",	aml_parsesimple	},
	{ AMLOP_ALIAS,		"Alias",	"nN",	aml_parsenamed },
	{ AMLOP_NAME,		"Name",	"Nt",	aml_parsenamed },
	{ AMLOP_EVENT,		"Event",	"N",	aml_parsenamed },
	{ AMLOP_MUTEX,		"Mutex",	"Nb",	aml_parsenamed },
	{ AMLOP_DATAREGION,	"DataRegion",	"Nttt",	aml_parsenamed },
	{ AMLOP_OPREGION,	"OpRegion",	"Nbii",	aml_parsenamed },
	{ AMLOP_SCOPE,		"Scope",	"pNT",	aml_parsenamedscope },
	{ AMLOP_DEVICE,		"Device",	"pNT",	aml_parsenamedscope },
	{ AMLOP_POWERRSRC,	"Power Resource", "pNbwT", aml_parsenamedscope },
	{ AMLOP_THERMALZONE,	"ThermalZone",	"pNT",	aml_parsenamedscope },
	{ AMLOP_PROCESSOR,	"Processor",	"pNbdbT", aml_parsenamedscope },
	{ AMLOP_METHOD,		"Method",	"pNfM",	aml_parsemethod },

	/* Field operations */
	{ AMLOP_FIELD,		"Field",	"pnfF",		aml_parsefieldunit },
	{ AMLOP_INDEXFIELD,	"IndexField",	"pntfF",	aml_parsefieldunit },
	{ AMLOP_BANKFIELD,	"BankField",	"pnnifF",	aml_parsefieldunit },
	{ AMLOP_CREATEFIELD,	"CreateField",	"tiiN",		aml_parsebufferfield },
	{ AMLOP_CREATEQWORDFIELD, "CreateQWordField","tiN",	aml_parsebufferfield },
	{ AMLOP_CREATEDWORDFIELD, "CreateDWordField","tiN",	aml_parsebufferfield },
	{ AMLOP_CREATEWORDFIELD, "CreateWordField", "tiN",	aml_parsebufferfield },
	{ AMLOP_CREATEBYTEFIELD, "CreateByteField", "tiN",	aml_parsebufferfield },
	{ AMLOP_CREATEBITFIELD,	"CreateBitField", "tiN",	aml_parsebufferfield },

	/* Conversion operations */
	{ AMLOP_TOINTEGER,	"ToInteger",	"tr",	aml_parsemath },
	{ AMLOP_TOBUFFER,	"ToBuffer",	"tr",	},
	{ AMLOP_TODECSTRING,	"ToDecString",	"ir",	aml_parsestring },
	{ AMLOP_TOHEXSTRING,	"ToHexString",	"ir",	aml_parsestring },
	{ AMLOP_TOSTRING,	"ToString",	"t",	aml_parsestring },
	{ AMLOP_MID,		"Mid",		"tiir",	aml_parsestring },
	{ AMLOP_FROMBCD,	"FromBCD",	"ir",	aml_parsemath },
	{ AMLOP_TOBCD,		"ToBCD",	"ir",	aml_parsemath },

	/* Mutex/Signal operations */
	{ AMLOP_ACQUIRE,	"Acquire",	"tw",	aml_parsemuxaction },
	{ AMLOP_RELEASE,	"Release",	"t",	aml_parsemuxaction },
	{ AMLOP_SIGNAL,		"Signal",	"t",	aml_parsemuxaction },
	{ AMLOP_WAIT,		"Wait",		"ti",	aml_parsemuxaction },
	{ AMLOP_RESET,		"Reset",	"t",	aml_parsemuxaction },

	{ AMLOP_INDEX,		"Index",	"tir",	aml_parseref },
	{ AMLOP_DEREFOF,	"DerefOf",	"t",	aml_parseref },
	{ AMLOP_REFOF,		"RefOf",	"t",	aml_parseref },
	{ AMLOP_CONDREFOF,	"CondRef",	"nr",	aml_parseref },

	{ AMLOP_LOADTABLE,	"LoadTable",	"tttttt" },
	{ AMLOP_STALL,		"Stall",	"i",	aml_parsemisc2 },
	{ AMLOP_SLEEP,		"Sleep",	"i",	aml_parsemisc2 },
	{ AMLOP_LOAD,		"Load",		"nt",	aml_parseref },
	{ AMLOP_UNLOAD,		"Unload",	"t" },
	{ AMLOP_STORE,		"Store",	"tr",	aml_parseref },
	{ AMLOP_CONCAT,		"Concat",	"ttr",	aml_parsestring },
	{ AMLOP_CONCATRES,	"ConcatRes",	"ttt" },
	{ AMLOP_NOTIFY,		"Notify",	"ti",	aml_parsemisc2 },
	{ AMLOP_SIZEOF,		"Sizeof",	"t",	aml_parsemisc3 },
	{ AMLOP_MATCH,		"Match",	"tbibii", aml_parsematch },
	{ AMLOP_OBJECTTYPE,	"ObjectType",	"t",	aml_parsemisc3 },
	{ AMLOP_COPYOBJECT,	"CopyObject",	"tr",	aml_parseref },
};

int aml_pc(uint8_t *src)
{
	return src - aml_root.start;
}

struct aml_scope *aml_lastscope;

void _aml_die(const char *fn, int line, const char *fmt, ...)
{
	struct aml_scope *root;
	va_list ap;
	int idx;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);

	for (root = aml_lastscope; root && root->pos; root = root->parent) {
		printf("%.4x Called: %s\n", aml_pc(root->pos),
		    aml_nodename(root->node));
		for (idx = 0; idx < root->nargs; idx++) {
			printf("  arg%d: ", idx);
			aml_showvalue(&root->args[idx], 0);
		}
		for (idx = 0; root->locals && idx < AML_MAX_LOCAL; idx++) {
			if (root->locals[idx].type) {
				printf("  local%d: ", idx);
				aml_showvalue(&root->locals[idx], 0);
			}
		}
	}

	/* XXX: don't panic */
	panic("aml_die %s:%d", fn, line);
}

void
aml_hashopcodes(void)
{
	int i;

	/* Dynamically allocate hash table */
	aml_ophash = (struct aml_opcode **)acpi_os_malloc(HASH_SIZE*sizeof(struct aml_opcode *));
	for (i = 0; i < sizeof(aml_table) / sizeof(aml_table[0]); i++)
		aml_ophash[HASH_KEY(aml_table[i].opcode)] = &aml_table[i];
}

struct aml_opcode *
aml_findopcode(int opcode)
{
	struct aml_opcode *hop;

	hop = aml_ophash[HASH_KEY(opcode)];
	if (hop && hop->opcode == opcode)
		return hop;
	return NULL;
}

const char *
aml_mnem(int opcode, uint8_t *pos)
{
	struct aml_opcode *tab;
	static char mnemstr[32];

	if ((tab = aml_findopcode(opcode)) != NULL) {
		strlcpy(mnemstr, tab->mnem, sizeof(mnemstr));
		if (pos != NULL) {
			switch (opcode) {
			case AMLOP_STRINGPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "\"%s\"", pos);
				break;
			case AMLOP_BYTEPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "0x%.2x",
					 *(uint8_t *)pos);
				break;
			case AMLOP_WORDPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "0x%.4x",
					 *(uint16_t *)pos);
				break;
			case AMLOP_DWORDPREFIX:
				snprintf(mnemstr, sizeof(mnemstr), "0x%.4x",
					 *(uint16_t *)pos);
				break;
			case AMLOP_NAMECHAR:
				strlcpy(mnemstr, aml_getname(pos), sizeof(mnemstr));
				break;
			}
		}
		return mnemstr;
	}
	return ("xxx");
}

const char *
aml_args(int opcode)
{
	struct aml_opcode *tab;

	if ((tab = aml_findopcode(opcode)) != NULL)
		return tab->args;
	return ("");
}

struct aml_notify_data {
	struct aml_node		*node;
	char			pnpid[20];
	void			*cbarg;
	int			(*cbproc)(struct aml_node *, int, void *);
	int			poll;

	SLIST_ENTRY(aml_notify_data) link;
};

SLIST_HEAD(aml_notify_head, aml_notify_data);
struct aml_notify_head aml_notify_list =
    LIST_HEAD_INITIALIZER(&aml_notify_list);

/*
 *  @@@: Memory management functions
 */

long acpi_nalloc;

struct acpi_memblock {
	size_t size;
};

void *
_acpi_os_malloc(size_t size, const char *fn, int line)
{
	struct acpi_memblock *sptr;

	sptr = malloc(size+sizeof(*sptr), M_DEVBUF, M_WAITOK | M_ZERO);
	dnprintf(99, "alloc: %x %s:%d\n", sptr, fn, line);
	if (sptr) {
		acpi_nalloc += size;
		sptr->size = size;
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

		dnprintf(99, "free: %x %s:%d\n", sptr, fn, line);
		free(sptr, M_DEVBUF);
	}
}

void
acpi_sleep(int ms)
{
	int to = ms * hz / 1000;

	if (cold)
		delay(ms * 1000);
	else {
		if (to <= 0)
			to = 1;
		while (tsleep(dsdt_softc, PWAIT, "asleep", to) !=
		    EWOULDBLOCK);
	}
}

void
acpi_stall(int us)
{
	delay(us);
}

int
acpi_mutex_acquire(struct aml_value *val, int timeout)
{
	/* XXX we currently do not have concurrency so assume mutex succeeds */
	dnprintf(50, "acpi_mutex_acquire\n");

	return (0);
#if 0
	struct acpi_mutex *mtx = val->v_mutex;
	int rv = 0, ts, tries = 0;

	if (val->type != AML_OBJTYPE_MUTEX) {
		printf("acpi_mutex_acquire: invalid mutex\n");
		return (1);
	}

	if (timeout == 0xffff)
		timeout = 0;

	/* lock recursion be damned, panic if that happens */
	rw_enter_write(&mtx->amt_lock);
	while (mtx->amt_ref_count) {
		rw_exit_write(&mtx->amt_lock);
		/* block access */
		ts = tsleep(mtx, PWAIT, mtx->amt_name, timeout / hz);
		if (ts == EWOULDBLOCK) {
			rv = 1; /* mutex not acquired */
			goto done;
		}
		tries++;
		rw_enter_write(&mtx->amt_lock);
	}

	mtx->amt_ref_count++;
	rw_exit_write(&mtx->amt_lock);
done:
	return (rv);
#endif
}

void
acpi_mutex_release(struct aml_value *val)
{
	dnprintf(50, "acpi_mutex_release\n");
#if 0
	struct acpi_mutex *mtx = val->v_mutex;

	/* sanity */
	if (val->type != AML_OBJTYPE_MUTEX) {
		printf("acpi_mutex_acquire: invalid mutex\n");
		return;
	}

	rw_enter_write(&mtx->amt_lock);

	if (mtx->amt_ref_count == 0) {
		printf("acpi_mutex_release underflow %s\n", mtx->amt_name);
		goto done;
	}

	mtx->amt_ref_count--;
	wakeup(mtx); /* wake all of them up */
done:
	rw_exit_write(&mtx->amt_lock);
#endif
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

/* Read/Write to hardware I/O fields */
void
aml_gasio(struct acpi_softc *sc, int type, uint64_t base, uint64_t length,
    int bitpos, int bitlen, int size, void *buf, int mode)
{
	dnprintf(10, "-- aml_gasio: %.2x"
	    " base:%llx len:%llx bpos:%.4x blen:%.4x sz:%.2x mode=%s\n",
	    type, base, length, bitpos, bitlen, size,
	    mode==ACPI_IOREAD?"read":"write");
	acpi_gasio(sc, mode, type, base+(bitpos>>3),
	    (size>>3), (bitlen>>3), buf);
#ifdef ACPI_DEBUG
	while (bitlen > 0) {
		dnprintf(10, "%.2x ", *(uint8_t *)buf);
		buf++;
		bitlen -=8;
	}
	dnprintf(10, "\n");
#endif
}

/*
 * @@@: Notify functions
 */
void
acpi_poll(void *arg)
{
	dsdt_softc->sc_poll = 1;
	dsdt_softc->sc_wakeup = 0;
	wakeup(dsdt_softc);

	timeout_add(&dsdt_softc->sc_dev_timeout, 10 * hz);
}

void
aml_register_notify(struct aml_node *node, const char *pnpid,
    int (*proc)(struct aml_node *, int, void *), void *arg, int poll)
{
	struct aml_notify_data	*pdata;
	extern int acpi_poll_enabled;

	dnprintf(10, "aml_register_notify: %s %s %x\n",
	    node->name, pnpid ? pnpid : "", proc);

	pdata = acpi_os_malloc(sizeof(struct aml_notify_data));
	pdata->node = node;
	pdata->cbarg = arg;
	pdata->cbproc = proc;
	pdata->poll = poll;

	if (pnpid)
		strlcpy(pdata->pnpid, pnpid, sizeof(pdata->pnpid));

	SLIST_INSERT_HEAD(&aml_notify_list, pdata, link);

	if (poll && !acpi_poll_enabled)
		timeout_add(&dsdt_softc->sc_dev_timeout, 10 * hz);
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

void acpi_poll_notify(void)
{
	struct aml_notify_data	*pdata = NULL;

	SLIST_FOREACH(pdata, &aml_notify_list, link)
		if (pdata->cbproc && pdata->poll)
			pdata->cbproc(pdata->node, 0, pdata->cbarg);
}

/*
 * @@@: Namespace functions
 */

struct aml_node *__aml_search(struct aml_node *, uint8_t *);
void aml_delchildren(struct aml_node *);


/* Search for a name in children nodes */
struct aml_node *
__aml_search(struct aml_node *root, uint8_t *nameseg)
{
	if (root == NULL)
		return NULL;
	for (root = root->child; root; root = root->sibling) {
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
		return namebuf+1;
	}
	return namebuf;
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
			for (pp = &root->child; *pp; pp = &(*pp)->sibling)
				;
			node->parent = root;
			node->sibling = NULL;
			*pp = node;
		}
		root = node;
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

struct aml_value	*aml_alloctmp(struct aml_scope *, int);
struct aml_scope	*aml_pushscope(struct aml_scope *, uint8_t *,
			    uint8_t *, struct aml_node *);
struct aml_scope	*aml_popscope(struct aml_scope *);
int			aml_parsenode(struct aml_scope *, struct aml_node *,
			    uint8_t *, uint8_t **, struct aml_value *);

#define AML_LHS		0
#define AML_RHS		1
#define AML_DST		2
#define AML_DST2	3

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

	aml_lastscope = scope;

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
		for (idx = 0; idx < ol->nobj; idx++) {
			aml_freevalue(&ol->obj[idx]);
		}
		acpi_os_free(ol);
	}
	acpi_os_free(scope);

	aml_lastscope = nscope;

	return nscope;
}

int
aml_parsenode(struct aml_scope *parent, struct aml_node *node, uint8_t *start,
    uint8_t **end, struct aml_value *res)
{
	struct aml_scope *scope;

	/* Don't parse zero-length scope */
	if (start == *end)
		return 0;
	scope = aml_pushscope(parent, start, *end, node);
	if (res == NULL)
		res = aml_alloctmp(scope, 1);
	while (scope != parent) {
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
void aml_unlockfield(struct aml_scope *, struct aml_value *);
void aml_lockfield(struct aml_scope *, struct aml_value *);

/* Copy from a bufferfield to an integer/buffer */
void
aml_setbufint(struct aml_value *dst, int bitpos, int bitlen,
    struct aml_value *src)
{
	if (src->type != AML_OBJTYPE_BUFFER)
		aml_die("wrong setbufint type\n");

#if 1
	/* Return buffer type */
	_aml_setvalue(dst, AML_OBJTYPE_BUFFER, (bitlen+7)>>3, NULL);
	aml_bufcpy(dst->v_buffer, 0, src->v_buffer, bitpos, bitlen);
#else
	if (bitlen < aml_intlen) {
		/* XXX: Endian issues?? */
		/* Return integer type */
		_aml_setvalue(dst, AML_OBJTYPE_INTEGER, 0, NULL);
		aml_bufcpy(&dst->v_integer, 0, src->v_buffer, bitpos, bitlen);
	} else {
		/* Return buffer type */
		_aml_setvalue(dst, AML_OBJTYPE_BUFFER, (bitlen+7)>>3, NULL);
		aml_bufcpy(dst->v_buffer, 0, src->v_buffer, bitpos, bitlen);
	}
#endif
}

/* Copy from a string/integer/buffer to a bufferfield */
void
aml_getbufint(struct aml_value *src, int bitpos, int bitlen,
    struct aml_value *dst)
{
	if (dst->type != AML_OBJTYPE_BUFFER)
		aml_die("wrong getbufint type\n");
	switch (src->type) {
	case AML_OBJTYPE_INTEGER:
		if (bitlen >= aml_intlen)
			bitlen = aml_intlen;
		aml_bufcpy(dst->v_buffer, bitpos, &src->v_integer, 0, bitlen);
		break;
	case AML_OBJTYPE_BUFFER:
		if (bitlen >= 8*src->length)
			bitlen = 8*src->length;
		aml_bufcpy(dst->v_buffer, bitpos, src->v_buffer, 0, bitlen);
		break;
	case AML_OBJTYPE_STRING:
		if (bitlen >= 8*src->length)
			bitlen = 8*src->length;
		aml_bufcpy(dst->v_buffer, bitpos, src->v_string, 0, bitlen);
		break;
	}
}

void
aml_lockfield(struct aml_scope *scope, struct aml_value *field)
{
	if (AML_FIELD_LOCK(field->v_field.flags) == AML_FIELD_LOCK_ON) {
		/* XXX: do locking here */
	}
}

void
aml_unlockfield(struct aml_scope *scope, struct aml_value *field)
{
	if (AML_FIELD_LOCK(field->v_field.flags) == AML_FIELD_LOCK_ON) {
		/* XXX: do unlocking here */
	}
}

void *aml_getbuffer(struct aml_value *, int *);

void *
aml_getbuffer(struct aml_value *val, int *bitlen)
{
	switch (val->type) {
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_STATICINT:
		*bitlen = aml_intlen;
		return (&val->v_integer);

	case AML_OBJTYPE_BUFFER:
	case AML_OBJTYPE_STRING:
		*bitlen = val->length<<3;
		return (val->v_buffer);

	default:
		aml_die("getvbi");
	}

	return (NULL);
}

/*
 * Buffer/Region: read/write to bitfields
 */
void
aml_fieldio(struct aml_scope *scope, struct aml_value *field,
    struct aml_value *res, int mode)
{
	struct aml_value *pop, tf;
	int bpos, blen, aligned, mask;
	void    *iobuf, *iobuf2;
	uint64_t iobase;

	pop = field->v_field.ref1;
	bpos = field->v_field.bitpos;
	blen = field->v_field.bitlen;

	dnprintf(55,"--fieldio: %s [%s] bp:%.4x bl:%.4x\n",
	    mode == ACPI_IOREAD ? "rd" : "wr",
	    aml_nodename(field->node), bpos, blen);

	aml_lockfield(scope, field);
	switch (field->v_field.type) {
	case AMLOP_INDEXFIELD:
		/* Set Index */
		memcpy(&tf, field->v_field.ref2, sizeof(struct aml_value));
		tf.v_field.bitpos += (bpos & 7);
		tf.v_field.bitlen  = blen;

		aml_setvalue(scope, pop, NULL, bpos>>3);
		aml_fieldio(scope, &tf, res, mode);
#ifdef ACPI_DEBUG
		dnprintf(55, "-- post indexfield %x,%x @ %x,%x\n",
		    bpos & 3, blen,
		    field->v_field.ref2->v_field.bitpos,
		    field->v_field.ref2->v_field.bitlen);

		iobuf = aml_getbuffer(res, &aligned);
		aml_dump(aligned >> 3, iobuf);
#endif
		break;
	case AMLOP_BANKFIELD:
		/* Set Bank */
		memcpy(&tf, field->v_field.ref2, sizeof(struct aml_value));
		tf.v_field.bitpos += (bpos & 7);
		tf.v_field.bitlen  = blen;

		aml_setvalue(scope, pop, NULL, field->v_field.ref3);
		aml_fieldio(scope, &tf, res, mode);
#ifdef ACPI_DEBUG
		dnprintf(55, "-- post bankfield %x,%x @ %x,%x\n",
		    bpos & 3, blen,
		    field->v_field.ref2->v_field.bitpos,
		    field->v_field.ref2->v_field.bitlen);

		iobuf = aml_getbuffer(res, &aligned);
		aml_dump(aligned >> 3, iobuf);
#endif
		break;
	case AMLOP_FIELD:
		/* This is an I/O field */
		if (pop->type != AML_OBJTYPE_OPREGION)
			aml_die("Not an opregion!\n");

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

		/* Pre-allocate return value for reads */
		if (mode == ACPI_IOREAD)
			_aml_setvalue(res, AML_OBJTYPE_BUFFER,
			    (field->v_field.bitlen+7)>>3, NULL);

		/* Get aligned bitpos/bitlength */
		blen = ((bpos & mask) + blen + mask) & ~mask;
		bpos = bpos & ~mask;
		aligned = (bpos == field->v_field.bitpos &&
		    blen == field->v_field.bitlen);
		iobase = pop->v_opregion.iobase;

		/* Check for aligned reads/writes */
		if (aligned) {
			iobuf = aml_getbuffer(res, &aligned);
			aml_gasio(scope->sc, pop->v_opregion.iospace,
			    iobase, pop->v_opregion.iolen, bpos, blen,
			    mask + 1, iobuf, mode);
#ifdef ACPI_DEBUG
			dnprintf(55, "aligned: %s @ %.4x:%.4x + %.4x\n",
			    mode == ACPI_IOREAD ? "rd" : "wr",
			    bpos, blen, aligned);

			aml_dump(blen >> 3, iobuf);
#endif
		}
		else if (mode == ACPI_IOREAD) {
			iobuf = acpi_os_malloc(blen>>3);
			aml_gasio(scope->sc, pop->v_opregion.iospace,
			    iobase, pop->v_opregion.iolen, bpos, blen,
			    mask + 1, iobuf, mode);

			/* ASSERT: res is buffer type as it was set above */
			aml_bufcpy(res->v_buffer, 0, iobuf, 
			    field->v_field.bitpos & mask,
			    field->v_field.bitlen);

#ifdef ACPI_DEBUG
			dnprintf(55,"non-aligned read: %.4x:%.4x : ", 
			    field->v_field.bitpos & mask,
			    field->v_field.bitlen);

			aml_dump(blen >> 3, iobuf);
			dnprintf(55,"post-read: ");
			aml_dump((field->v_field.bitlen+7)>>3, res->v_buffer);
#endif
			acpi_os_free(iobuf);
		}
		else {
			iobuf = acpi_os_malloc(blen >> 3);
			switch (AML_FIELD_UPDATE(field->v_field.flags)) {
			case AML_FIELD_WRITEASONES:
				memset(iobuf, 0xFF, blen >> 3);
				break;
			case AML_FIELD_PRESERVE:
				aml_gasio(scope->sc, pop->v_opregion.iospace,
				    iobase, pop->v_opregion.iolen, bpos, blen,
				    mask + 1, iobuf, ACPI_IOREAD);
				break;
			}
			/* Copy into IOBUF */
			iobuf2 = aml_getbuffer(res, &aligned);
			aml_bufcpy(iobuf, field->v_field.bitpos & mask,
			    iobuf2, 0, field->v_field.bitlen);

#ifdef ACPI_DEBUG
			dnprintf(55,"non-aligned write: %.4x:%.4x : ", 
			    field->v_field.bitpos & mask,
			    field->v_field.bitlen);

			aml_dump(blen >> 3, iobuf);
#endif
			aml_gasio(scope->sc, pop->v_opregion.iospace,
			    iobase, pop->v_opregion.iolen, bpos, blen,
			    mask + 1, iobuf, mode);

			acpi_os_free(iobuf);
		}
		/* Verify that I/O is in range */
#if 0
		/*
		 * XXX: some I/O ranges are on dword boundaries, but their
		 * length is incorrect eg. dword access, but length of
		 * opregion is 2 bytes.
		 */
		if ((bpos+blen) >= (pop->v_opregion.iolen * 8)) {
			aml_die("Out of bounds I/O!!! region:%x:%llx:%x %x\n",
			    pop->v_opregion.iospace, pop->v_opregion.iobase,
			    pop->v_opregion.iolen, bpos+blen);
		}
#endif
		break;
	default:
		/* This is a buffer field */
		if (mode == ACPI_IOREAD)
			aml_setbufint(res, bpos, blen, pop);
		else
			aml_getbufint(res, bpos, blen, pop);
		break;
	}
	aml_unlockfield(scope, field);
}

/*
 * @@@: Value set/compare/alloc/free routines
 */
int64_t aml_str2int(const char *, int);
struct aml_value *aml_derefvalue(struct aml_scope *, struct aml_value *, int);
#define aml_dereftarget(s, v)	aml_derefvalue(s, v, ACPI_IOWRITE)
#define aml_derefterm(s, v, m)	aml_derefvalue(s, v, ACPI_IOREAD)

void
aml_showvalue(struct aml_value *val, int lvl)
{
	int idx;

	if (val == NULL)
		return;

	if (val->node)
		printf(" [%s]", aml_nodename(val->node));
	printf(" %p cnt:%.2x stk:%.2x", val, val->refcnt, val->stack);
	switch (val->type) {
	case AML_OBJTYPE_STATICINT:
	case AML_OBJTYPE_INTEGER:
		printf(" integer: %llx\n", val->v_integer);
		break;
	case AML_OBJTYPE_STRING:
		printf(" string: %s\n", val->v_string);
		break;
	case AML_OBJTYPE_METHOD:
		printf(" method: %.2x\n", val->v_method.flags);
		break;
	case AML_OBJTYPE_PACKAGE:
		printf(" package: %.2x\n", val->length);
		for (idx = 0; idx < val->length; idx++)
			aml_showvalue(val->v_package[idx], lvl);
		break;
	case AML_OBJTYPE_BUFFER:
		printf(" buffer: %.2x {", val->length);
		for (idx = 0; idx < val->length; idx++)
			printf("%s%.2x", idx ? ", " : "", val->v_buffer[idx]);
		printf("}\n");
		break;
	case AML_OBJTYPE_FIELDUNIT:
	case AML_OBJTYPE_BUFFERFIELD:
		printf(" field: bitpos=%.4x bitlen=%.4x ref1:%x ref2:%x [%s]\n",
		    val->v_field.bitpos, val->v_field.bitlen,
		    val->v_field.ref1, val->v_field.ref2,
		    aml_mnem(val->v_field.type, NULL));
		aml_showvalue(val->v_field.ref1, lvl);
		aml_showvalue(val->v_field.ref2, lvl);
		break;
	case AML_OBJTYPE_MUTEX:
		printf(" mutex: %s ref: %d\n",
		    val->v_mutex ?  val->v_mutex->amt_name : "",
		    val->v_mutex ?  val->v_mutex->amt_ref_count : 0);
		break;
	case AML_OBJTYPE_EVENT:
		printf(" event:\n");
		break;
	case AML_OBJTYPE_OPREGION:
		printf(" opregion: %.2x,%.8llx,%x\n",
		    val->v_opregion.iospace, val->v_opregion.iobase,
		    val->v_opregion.iolen);
		break;
	case AML_OBJTYPE_NAMEREF:
		printf(" nameref: %s\n", aml_getname(val->v_nameref));
		break;
	case AML_OBJTYPE_DEVICE:
		printf(" device:\n");
		break;
	case AML_OBJTYPE_PROCESSOR:
		printf(" cpu: %.2x,%.4x,%.2x\n",
		    val->v_processor.proc_id, val->v_processor.proc_addr,
		    val->v_processor.proc_len);
		break;
	case AML_OBJTYPE_THERMZONE:
		printf(" thermzone:\n");
		break;
	case AML_OBJTYPE_POWERRSRC:
		printf(" pwrrsrc: %.2x,%.2x\n",
		    val->v_powerrsrc.pwr_level, val->v_powerrsrc.pwr_order);
		break;
	case AML_OBJTYPE_OBJREF:
		printf(" objref: %p index:%x\n", val->v_objref.ref,
		    val->v_objref.index);
		aml_showvalue(val->v_objref.ref, lvl);
		break;
	default:
		printf(" !!type: %x\n", val->type);
	}
}

/* Perform DeRef on value. If ACPI_IOREAD, will perform buffer/IO field read */
struct aml_value *
aml_derefvalue(struct aml_scope *scope, struct aml_value *ref, int mode)
{
	struct aml_node *node;
	struct aml_value *tmp;
	int64_t tmpint;
	int argc, index;

	for (;;) {
		switch (ref->type) {
		case AML_OBJTYPE_NAMEREF:
			node = aml_searchname(scope->node, ref->v_nameref);
			if (node == NULL || node->value == NULL)
				return ref;
			ref = node->value;
			break;

		case AML_OBJTYPE_OBJREF:
			index = ref->v_objref.index;
			ref = aml_dereftarget(scope, ref->v_objref.ref);
			if (index != -1) {
				if (index >= ref->length)
					aml_die("index.buf out of bounds: "
					    "%d/%d\n", index, ref->length);
				switch (ref->type) {
				case AML_OBJTYPE_PACKAGE:
					ref = ref->v_package[index];
					break;
				case AML_OBJTYPE_STATICINT:
				case AML_OBJTYPE_INTEGER:
					/* Convert to temporary buffer */
					if (ref->node)
						aml_die("named integer index\n");
					tmpint = ref->v_integer;
					_aml_setvalue(ref, AML_OBJTYPE_BUFFER,
					    aml_intlen>>3, &tmpint);
					/* FALLTHROUGH */
				case AML_OBJTYPE_BUFFER:
				case AML_OBJTYPE_STRING:
					/* Return contents at this index */
					tmp = aml_alloctmp(scope, 1);
					if (mode == ACPI_IOREAD) {
						/* Shortcut: return integer
						 * contents of buffer at index */
						_aml_setvalue(tmp,
						    AML_OBJTYPE_INTEGER,
						    ref->v_buffer[index], NULL);
					} else {
						_aml_setvalue(tmp,
						    AML_OBJTYPE_BUFFERFIELD,
						    0, NULL);
						tmp->v_field.type =
						    AMLOP_CREATEBYTEFIELD;
						tmp->v_field.bitpos = index * 8;
						tmp->v_field.bitlen = 8;
						tmp->v_field.ref1 = ref;
						aml_addref(ref);
					}
					return tmp;
				default:
					aml_die("unknown index type: %d", ref->type);
					break;
				}
			}
			break;

		case AML_OBJTYPE_METHOD:
			/* Read arguments from current scope */
			argc = AML_METHOD_ARGCOUNT(ref->v_method.flags);
			tmp = aml_alloctmp(scope, argc+1);
			for (index = 0; index < argc; index++) {
				aml_parseop(scope, &tmp[index]);
				aml_addref(&tmp[index]);
			}
			ref = aml_evalmethod(scope, ref->node, argc, tmp, &tmp[argc]);
			break;

		case AML_OBJTYPE_BUFFERFIELD:
		case AML_OBJTYPE_FIELDUNIT:
			if (mode == ACPI_IOREAD) {
				/* Read I/O field into temporary storage */
				tmp = aml_alloctmp(scope, 1);
				aml_fieldio(scope, ref, tmp, ACPI_IOREAD);
				return tmp;
			}
			return ref;

		default:
			return ref;
		}

	}
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
		dnprintf(50, "null val2int\n");
		return (0);
	}
	switch (rval->type) {
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_STATICINT:
		ival = rval->v_integer;
		break;
	case AML_OBJTYPE_BUFFER:
		aml_bufcpy(&ival, 0, rval->v_buffer, 0,
		    min(aml_intlen, rval->length*8));
		break;
	case AML_OBJTYPE_STRING:
		ival = (strncmp(rval->v_string, "0x", 2) == 0) ?
		    aml_str2int(rval->v_string+2, 16) :
		    aml_str2int(rval->v_string, 10);
		break;
	}
	return (ival);
}

/* Sets value into LHS: lhs must already be cleared */
struct aml_value *
_aml_setvalue(struct aml_value *lhs, int type, int64_t ival, const void *bval)
{
	memset(&lhs->_, 0x0, sizeof(lhs->_));

	lhs->type = type;
	switch (lhs->type) {
	case AML_OBJTYPE_INTEGER:
	case AML_OBJTYPE_STATICINT:
		lhs->length = aml_intlen>>3;
		lhs->v_integer = ival;
		break;
	case AML_OBJTYPE_METHOD:
		lhs->v_method.flags = ival;
		lhs->v_method.fneval = bval;
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
		lhs->v_package = (struct aml_value **)acpi_os_malloc(ival *
		    sizeof(struct aml_value *));
		for (ival = 0; ival < lhs->length; ival++)
			lhs->v_package[ival] = aml_allocvalue(
			    AML_OBJTYPE_UNINITIALIZED, 0, NULL);
		break;
	}
	return lhs;
}

/* Copy object to another value: lhs must already be cleared */
void
aml_copyvalue(struct aml_value *lhs, struct aml_value *rhs)
{
	int idx;

	lhs->type = rhs->type  & ~AML_STATIC;
	switch (lhs->type) {
	case AML_OBJTYPE_UNINITIALIZED:
		break;
	case AML_OBJTYPE_STATICINT:
	case AML_OBJTYPE_INTEGER:
		lhs->length = aml_intlen>>3;
		lhs->v_integer = rhs->v_integer;
		break;
	case AML_OBJTYPE_MUTEX:
		lhs->v_mutex = rhs->v_mutex;
		break;
	case AML_OBJTYPE_POWERRSRC:
		lhs->v_powerrsrc = rhs->v_powerrsrc;
		break;
	case AML_OBJTYPE_METHOD:
		lhs->v_method = rhs->v_method;
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
	case AML_OBJTYPE_PROCESSOR:
		lhs->v_processor = rhs->v_processor;
		break;
	case AML_OBJTYPE_NAMEREF:
		lhs->v_nameref = rhs->v_nameref;
		break;
	case AML_OBJTYPE_PACKAGE:
		_aml_setvalue(lhs, rhs->type, rhs->length, NULL);
		for (idx = 0; idx < rhs->length; idx++)
			aml_copyvalue(lhs->v_package[idx], rhs->v_package[idx]);
		break;
	case AML_OBJTYPE_OBJREF:
		lhs->v_objref = rhs->v_objref;
		break;
	default:
		printf("copyvalue: %x", rhs->type);
		break;
	}
}

int is_local(struct aml_scope *, struct aml_value *);

int is_local(struct aml_scope *scope, struct aml_value *val)
{
	return val->stack;
}

/* Guts of the code: Assign one value to another.  LHS may contain a previous value */
void
aml_setvalue(struct aml_scope *scope, struct aml_value *lhs,
    struct aml_value *rhs, int64_t ival)
{
	struct aml_value tmpint;

	/* Use integer as result */
	if (rhs == NULL) {
		memset(&tmpint, 0, sizeof(tmpint));
		rhs = _aml_setvalue(&tmpint, AML_OBJTYPE_INTEGER, ival, NULL);
	}

	if (is_local(scope, lhs)) {
		/* ACPI: Overwrite writing to LocalX */
		aml_freevalue(lhs);
	}
	else {
		lhs = aml_dereftarget(scope, lhs);
	}

	switch (lhs->type) {
	case AML_OBJTYPE_UNINITIALIZED:
		aml_copyvalue(lhs, rhs);
		break;
	case AML_OBJTYPE_BUFFERFIELD:
	case AML_OBJTYPE_FIELDUNIT:
		aml_fieldio(scope, lhs, rhs, ACPI_IOWRITE);
		break;
	case AML_OBJTYPE_DEBUGOBJ:
#ifdef ACPI_DEBUG
		printf("-- debug --\n");
		aml_showvalue(rhs, 50);
#endif
		break;
	case AML_OBJTYPE_STATICINT:
		if (lhs->node) {
			lhs->v_integer = aml_val2int(rhs);
		}
		break;
	case AML_OBJTYPE_INTEGER:
		lhs->v_integer = aml_val2int(rhs);
		break;
	case AML_OBJTYPE_BUFFER:
		if (lhs->node)
			dnprintf(40, "named.buffer\n");
		aml_freevalue(lhs);
		if (rhs->type == AML_OBJTYPE_BUFFER)
			_aml_setvalue(lhs, AML_OBJTYPE_BUFFER, rhs->length,
			    rhs->v_buffer);
		else if (rhs->type == AML_OBJTYPE_INTEGER ||
			    rhs->type == AML_OBJTYPE_STATICINT)
			_aml_setvalue(lhs, AML_OBJTYPE_BUFFER,
			    sizeof(rhs->v_integer), &rhs->v_integer);
		else if (rhs->type == AML_OBJTYPE_STRING)
			_aml_setvalue(lhs, AML_OBJTYPE_BUFFER, rhs->length+1,
			    rhs->v_string);
		else {
			/* aml_showvalue(rhs); */
			aml_die("setvalue.buf : %x", aml_pc(scope->pos));
		}
		break;
	case AML_OBJTYPE_STRING:
		if (lhs->node)
			dnprintf(40, "named string\n");
		aml_freevalue(lhs);
		if (rhs->type == AML_OBJTYPE_STRING)
			_aml_setvalue(lhs, AML_OBJTYPE_STRING, rhs->length,
			    rhs->v_string);
		else if (rhs->type == AML_OBJTYPE_BUFFER)
			_aml_setvalue(lhs, AML_OBJTYPE_STRING, rhs->length,
			    rhs->v_buffer);
		else if (rhs->type == AML_OBJTYPE_INTEGER || rhs->type == AML_OBJTYPE_STATICINT) {
			_aml_setvalue(lhs, AML_OBJTYPE_STRING, 10, NULL);
			snprintf(lhs->v_string, lhs->length, "%lld",
			    rhs->v_integer);
		} else {
			//aml_showvalue(rhs);
			aml_die("setvalue.str");
		}
		break;
	default:
		/* XXX: */
		dnprintf(10, "setvalue.unknown: %x", lhs->type);
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
		for (idx = 0; idx < val->length; idx++) {
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
_aml_delref(struct aml_value **val, const char *fn, int line)
{
	if (val == NULL || *val == NULL)
		return;
	if ((*val)->stack > 0) {
		/* Don't delete locals */
		return;
	}
	if ((*val)->refcnt & ~0xFF)
		printf("-- invalid ref: %x:%s:%d\n", (*val)->refcnt, fn, line);
	if (--(*val)->refcnt == 0) {
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
	int64_t res;

	switch (opcode) {
		/* Math operations */
	case AMLOP_INCREMENT:
	case AMLOP_ADD:
		res = (lhs + rhs);
		break;
	case AMLOP_DECREMENT:
	case AMLOP_SUBTRACT:
		res = (lhs - rhs);
		break;
	case AMLOP_MULTIPLY:
		res = (lhs * rhs);
		break;
	case AMLOP_DIVIDE:
		res = (lhs / rhs);
		break;
	case AMLOP_MOD:
		res = (lhs % rhs);
		break;
	case AMLOP_SHL:
		res = (lhs << rhs);
		break;
	case AMLOP_SHR:
		res = (lhs >> rhs);
		break;
	case AMLOP_AND:
		res = (lhs & rhs);
		break;
	case AMLOP_NAND:
		res = ~(lhs & rhs);
		break;
	case AMLOP_OR:
		res = (lhs | rhs);
		break;
	case AMLOP_NOR:
		res = ~(lhs | rhs);
		break;
	case AMLOP_XOR:
		res = (lhs ^ rhs);
		break;
	case AMLOP_NOT:
		res = ~(lhs);
		break;

		/* Conversion/misc */
	case AMLOP_FINDSETLEFTBIT:
		res = aml_msb(lhs);
		break;
	case AMLOP_FINDSETRIGHTBIT:
		res = aml_lsb(lhs);
		break;
	case AMLOP_TOINTEGER:
		res = (lhs);
		break;
	case AMLOP_FROMBCD:
		res = aml_convradix(lhs, 16, 10);
		break;
	case AMLOP_TOBCD:
		res = aml_convradix(lhs, 10, 16);
		break;

		/* Logical/Comparison */
	case AMLOP_LAND:
		res = (lhs && rhs);
		break;
	case AMLOP_LOR:
		res = (lhs || rhs);
		break;
	case AMLOP_LNOT:
		res = (!lhs);
		break;
	case AMLOP_LNOTEQUAL:
		res = (lhs != rhs);
		break;
	case AMLOP_LLESSEQUAL:
		res = (lhs <= rhs);
		break;
	case AMLOP_LGREATEREQUAL:
		res = (lhs >= rhs);
		break;
	case AMLOP_LEQUAL:
		res = (lhs == rhs);
		break;
	case AMLOP_LGREATER:
		res = (lhs > rhs);
		break;
	case AMLOP_LLESS:
		res = (lhs < rhs);
		break;
	}

	dnprintf(50,"aml_evalexpr: %s %llx %llx = %llx\n",
		 aml_mnem(opcode, NULL), lhs, rhs, res);

	return res;
}

int
aml_cmpvalue(struct aml_value *lhs, struct aml_value *rhs, int opcode)
{
	int rc, lt, rt;

	rc = 0;
	lt = lhs->type & ~AML_STATIC;
	rt = rhs->type & ~AML_STATIC;
	if (lt == rt) {
		switch (lt) {
		case AML_OBJTYPE_STATICINT:
		case AML_OBJTYPE_INTEGER:
			rc = (lhs->v_integer - rhs->v_integer);
			break;
		case AML_OBJTYPE_STRING:
			rc = strncmp(lhs->v_string, rhs->v_string,
			    min(lhs->length, rhs->length));
			if (rc == 0)
				rc = lhs->length - rhs->length;
			break;
		case AML_OBJTYPE_BUFFER:
			rc = memcmp(lhs->v_buffer, rhs->v_buffer,
			    min(lhs->length, rhs->length));
			if (rc == 0)
				rc = lhs->length - rhs->length;
			break;
		}
	} else if (lt == AML_OBJTYPE_INTEGER) {
		rc = lhs->v_integer - aml_val2int(rhs);
	} else if (rt == AML_OBJTYPE_INTEGER) {
		rc = aml_val2int(lhs) - rhs->v_integer;
	} else {
		aml_die("mismatched compare\n");
	}
	return aml_evalexpr(rc, 0, opcode);
}

/*
 * aml_bufcpy copies/shifts buffer data, special case for aligned transfers
 * dstPos/srcPos are bit positions within destination/source buffers
 */
void
aml_bufcpy(void *pvDst, int dstPos, const void *pvSrc, int srcPos, int len)
{
	const u_int8_t *pSrc = pvSrc;
	u_int8_t *pDst = pvDst;
	int		idx;

	if (aml_bytealigned(dstPos|srcPos|len)) {
		/* Aligned transfer: use memcpy */
		memcpy(pDst+aml_bytepos(dstPos), pSrc+aml_bytepos(srcPos),
		    aml_bytelen(len));
		return;
	}

	/* Misaligned transfer: perform bitwise copy (slow) */
	for (idx = 0; idx < len; idx++)
		aml_setbit(pDst, idx + dstPos, aml_tstbit(pSrc, idx + srcPos));
}

struct aml_value *
aml_callmethod(struct aml_scope *scope, struct aml_value *val)
{
	while (scope->pos < scope->end)
		aml_parseterm(scope, val);
	return val;
}

/*
 * Evaluate an AML method
 *
 * Returns a copy of the result in res (must be freed by user)
 */
struct aml_value *
aml_evalmethod(struct aml_scope *parent, struct aml_node *node,
    int argc, struct aml_value *argv, struct aml_value *res)
{
	struct aml_scope *scope;

	scope = aml_pushscope(parent, node->value->v_method.start,
	    node->value->v_method.end, node);
	scope->args = argv;
	scope->nargs = argc;

	if (res == NULL)
		res = aml_alloctmp(scope, 1);

#ifdef ACPI_DEBUG
	dnprintf(10, "calling [%s] (%d args)\n",
	    aml_nodename(node), scope->nargs);
	for (argc = 0; argc < scope->nargs; argc++) {
		dnprintf(10, "  arg%d: ", argc);
		aml_showvalue(&scope->args[argc], 10);
	}
	node->value->v_method.fneval(scope, res);
	dnprintf(10, "[%s] returns: ", aml_nodename(node));
	aml_showvalue(res, 10);
#else
	node->value->v_method.fneval(scope, res);
#endif
	/* Free any temporary children nodes */
	aml_delchildren(node);
	aml_popscope(scope);

	return res;
}

/*
 * @@@: External API
 *
 * evaluate an AML node
 * Returns a copy of the value in res  (must be freed by user)
 */
int
aml_evalnode(struct acpi_softc *sc, struct aml_node *node,
    int argc, struct aml_value *argv, struct aml_value *res)
{
	static int lastck;
	struct aml_node *ref;

	if (res)
		memset(res, 0, sizeof(struct aml_value));
	if (node == NULL || node->value == NULL)
		return (ACPI_E_BADVALUE);

	switch (node->value->type) {
	case AML_OBJTYPE_METHOD:
		aml_evalmethod(NULL, node, argc, argv, res);
		if (acpi_nalloc > lastck) {
			/* Check if our memory usage has increased */
			dnprintf(10, "Leaked: [%s] %d\n",
			    aml_nodename(node), acpi_nalloc);
			lastck = acpi_nalloc;
		}
		break;
	case AML_OBJTYPE_STATICINT:
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
	case AML_OBJTYPE_NAMEREF:
		if (res == NULL)
			break;
		if ((ref = aml_searchname(node, node->value->v_nameref)) != NULL)
			_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, ref);
		else
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
    int argc, struct aml_value *argv, struct aml_value *res)
{
	return aml_evalnode(sc, aml_searchname(parent, name), argc, argv, res);
}

int
aml_evalinteger(struct acpi_softc *sc, struct aml_node *parent,
    const char *name, int argc, struct aml_value *argv, int64_t *ival)
{
	struct aml_value res;

	if (name != NULL)
		parent = aml_searchname(parent, name);
	if (aml_evalnode(sc, parent, argc, argv, &res) == 0) {
		*ival = aml_val2int(&res);
		aml_freevalue(&res);
		return 0;
	}
	return 1;
}

void
aml_walknodes(struct aml_node *node, int mode,
    int (*nodecb)(struct aml_node *, void *), void *arg)
{
	struct aml_node *child;

	if (node == NULL)
		return;
	if (mode == AML_WALK_PRE)
		nodecb(node, arg);
	for (child = node->child; child; child = child->sibling)
		aml_walknodes(child, mode, nodecb, arg);
	if (mode == AML_WALK_POST)
		nodecb(node, arg);
}

void
aml_walktree(struct aml_node *node)
{
	while (node) {
		aml_showvalue(node->value, 0);
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
    int (*cbproc)(struct aml_node *, void *arg), void *arg)
{
	const char *nn;
	int st = 0;

	while (node) {
		if ((nn = node->name) != NULL) {
			if (*nn == AMLOP_ROOTCHAR) nn++;
			while (*nn == AMLOP_PARENTPREFIX) nn++;
			if (!strcmp(name, nn))
				st = cbproc(node, arg);
		}
		/* Only recurse if cbproc() wants us to */
		if (!st)
			aml_find_node(node->child, name, cbproc, arg);
		node = node->sibling;
	}
	return st;
}

/*
 * @@@: Parser functions
 */
uint8_t *aml_parsename(struct aml_scope *);
uint8_t *aml_parseend(struct aml_scope *scope);
int	aml_parselength(struct aml_scope *);
int	aml_parseopcode(struct aml_scope *);

/* Get AML Opcode */
int
aml_parseopcode(struct aml_scope *scope)
{
	int opcode = (scope->pos[0]);
	int twocode = (scope->pos[0]<<8) + scope->pos[1];

	/* Check if this is an embedded name */
	switch (opcode) {
	case AMLOP_ROOTCHAR:
	case AMLOP_PARENTPREFIX:
	case AMLOP_MULTINAMEPREFIX:
	case AMLOP_DUALNAMEPREFIX:
	case AMLOP_NAMECHAR:
		return AMLOP_NAMECHAR;
	}
	if (opcode >= 'A' && opcode <= 'Z') {
		return AMLOP_NAMECHAR;
	}
	if (twocode == AMLOP_LNOTEQUAL || twocode == AMLOP_LLESSEQUAL ||
	    twocode == AMLOP_LGREATEREQUAL || opcode == AMLOP_EXTPREFIX) {
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

/* Decode AML Length field 
 *  AML Length field is encoded:
 *    byte0    byte1    byte2    byte3
 *    00xxxxxx                             : if upper bits == 00, length = xxxxxx
 *    01--xxxx yyyyyyyy                    : if upper bits == 01, length = yyyyyyyyxxxx
 *    10--xxxx yyyyyyyy zzzzzzzz           : if upper bits == 10, length = zzzzzzzzyyyyyyyyxxxx
 *    11--xxxx yyyyyyyy zzzzzzzz wwwwwwww  : if upper bits == 11, length = wwwwwwwwzzzzzzzzyyyyyyyyxxxx
 */
int
aml_parselength(struct aml_scope *scope)
{
	int len;
	uint8_t lcode;

	lcode = *(scope->pos++);
	if (lcode <= 0x3F) {
		return lcode;
	}
	
	/* lcode >= 0x40, multibyte length, get first byte of extended length */
	len = lcode & 0xF;
	len += *(scope->pos++) << 4L;
	if (lcode >= 0x80) {
		len += *(scope->pos++) << 12L;
	}
	if (lcode >= 0xC0) {
		len += *(scope->pos++) << 20L;
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
		dnprintf(10,
		    "Bad scope... runover pos:%.4x new end:%.4x scope "
		    "end:%.4x\n", aml_pc(pos), aml_pc(pos+len),
		    aml_pc(scope->end));
		pos = scope->end;
	}
	return pos+len;
}

/*
 * @@@: Opcode utility functions
 */
int		aml_match(int, int64_t, struct aml_value *);
void		aml_fixref(struct aml_value **);
int64_t		aml_parseint(struct aml_scope *, int);
void		aml_resize(struct aml_value *val, int newsize);

void
aml_resize(struct aml_value *val, int newsize)
{
	void *oldptr;
	int oldsize;

	if (val->length >= newsize)
		return;
	oldsize = val->length;
	switch (val->type) {
	case AML_OBJTYPE_BUFFER:
		oldptr = val->v_buffer;
		_aml_setvalue(val, val->type, newsize, NULL);
		memcpy(val->v_buffer, oldptr, oldsize);
		acpi_os_free(oldptr);
		break;
	case AML_OBJTYPE_STRING:
		oldptr = val->v_string;
		_aml_setvalue(val, val->type, newsize+1, NULL);
		memcpy(val->v_string, oldptr, oldsize);
		acpi_os_free(oldptr);
		break;
	}
}


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

int amlop_delay;

u_int64_t
aml_getpciaddr(struct acpi_softc *sc, struct aml_node *root)
{
	struct aml_value tmpres;
	u_int64_t pciaddr;

	/* PCI */
	pciaddr = 0;
	if (!aml_evalname(dsdt_softc, root, "_ADR", 0, NULL, &tmpres)) {
		/* Device:Function are bits 16-31,32-47 */
		pciaddr += (aml_val2int(&tmpres) << 16L);
		aml_freevalue(&tmpres);
		dnprintf(20, "got _adr [%s]\n", aml_nodename(root));
	} else {
		/* Mark invalid */
		pciaddr += (0xFFFF << 16L);
		return pciaddr;
	}

	if (!aml_evalname(dsdt_softc, root, "_BBN", 0, NULL, &tmpres)) {
		/* PCI bus is in bits 48-63 */
		pciaddr += (aml_val2int(&tmpres) << 48L);
		aml_freevalue(&tmpres);
		dnprintf(20, "got _bbn [%s]\n", aml_nodename(root));
	}
	dnprintf(20, "got pciaddr: %s:%llx\n", aml_nodename(root), pciaddr);
	return pciaddr;
}

/* Fixup references for BufferFields/FieldUnits */
void
aml_fixref(struct aml_value **res)
{
	struct aml_value *oldres;

	while (*res && (*res)->type == AML_OBJTYPE_OBJREF &&
	    (*res)->v_objref.index == -1) {
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
		rval = AML_REVISION;
		break;
	case AMLOP_BYTEPREFIX:
		np = scope->pos;
		rval = *(uint8_t *)scope->pos;
		scope->pos += 1;
		break;
	case AMLOP_WORDPREFIX:
		np = scope->pos;
		rval = aml_letohost16(*(uint16_t *)scope->pos);
		scope->pos += 2;
		break;
	case AMLOP_DWORDPREFIX:
		np = scope->pos;
		rval = aml_letohost32(*(uint32_t *)scope->pos);
		scope->pos += 4;
		break;
	case AMLOP_QWORDPREFIX:
		np = scope->pos;
		rval = aml_letohost64(*(uint64_t *)scope->pos);
		scope->pos += 8;
		break;
	default:
		scope->pos = np;
		tmpval = aml_alloctmp(scope, 1);
		aml_parseterm(scope, tmpval);
		return aml_val2int(tmpval);
	}
	dnprintf(15, "%.4x: [%s] %s\n", aml_pc(scope->pos-opsize(opcode)),
	    aml_nodename(scope->node), aml_mnem(opcode, np));
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
	struct aml_value *deref;

	aml_freevalue(dst);
	deref = aml_derefterm(scope, raw, 0);
	aml_copyvalue(dst, deref);
	return 0;
}


/*
 * @@@: Opcode functions
 */

/* Parse named objects */
struct aml_value *
aml_parsenamed(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	uint8_t *name;
	int s, offs = 0;

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
		/* XXX mutex is unused since we don't have concurrency */
		_aml_setvalue(res, AML_OBJTYPE_MUTEX, 0, NULL);
		res->v_mutex = (struct acpi_mutex *)acpi_os_malloc(
		    sizeof(struct acpi_mutex));
		res->v_mutex->amt_synclevel = aml_parseint(scope,
		    AMLOP_BYTEPREFIX);
		s = strlen(aml_getname(name));
		if (s > 4)
			offs = s - 4;
		strlcpy(res->v_mutex->amt_name, aml_getname(name) + offs,
		    ACPI_MTX_MAXNAME);
		rw_init(&res->v_mutex->amt_lock, res->v_mutex->amt_name);
		break;
	case AMLOP_OPREGION:
		_aml_setvalue(res, AML_OBJTYPE_OPREGION, 0, NULL);
		res->v_opregion.iospace = aml_parseint(scope, AMLOP_BYTEPREFIX);
		res->v_opregion.iobase = aml_parseint(scope, AML_ANYINT);
		res->v_opregion.iolen = aml_parseint(scope, AML_ANYINT);
		if (res->v_opregion.iospace == GAS_PCI_CFG_SPACE) {
			res->v_opregion.iobase += aml_getpciaddr(dsdt_softc,
			    scope->node);
			dnprintf(20, "got ioaddr: %s.%s:%llx\n",
			    aml_nodename(scope->node), aml_getname(name),
			    res->v_opregion.iobase);
		}
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
	aml_parsenode(scope, node, scope->pos, &end, NULL);
	scope->pos = end;

	return res;
}

/* Parse math opcodes */
struct aml_value *
aml_parsemath(struct aml_scope *scope, int opcode, struct aml_value *res)
{
	struct aml_value *tmparg;
	int64_t i1, i2, i3;

	tmparg = aml_alloctmp(scope, 1);
	AML_CHECKSTACK();
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
		aml_parsetarget(scope, tmparg, NULL);
		break;
	case AMLOP_INCREMENT:
	case AMLOP_DECREMENT:
		aml_parsetarget(scope, tmparg, NULL);
		i1 = aml_val2int(aml_derefterm(scope, tmparg, 0));
		i2 = 1;
		break;
	case AMLOP_DIVIDE:
		i1 = aml_parseint(scope, AML_ANYINT);
		i2 = aml_parseint(scope, AML_ANYINT);

		aml_parsetarget(scope, tmparg, NULL);	// remainder
		aml_setvalue(scope, tmparg, NULL, (i1 % i2));

		aml_parsetarget(scope, tmparg, NULL);	// quotient
		break;
	default:
		i1 = aml_parseint(scope, AML_ANYINT);
		i2 = aml_parseint(scope, AML_ANYINT);
		aml_parsetarget(scope, tmparg, NULL);
		break;
	}
	i3 = aml_evalexpr(i1, i2, opcode);
	aml_setvalue(scope, res, NULL, i3);
	aml_setvalue(scope, tmparg, NULL, i3);
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
	aml_parseterm(scope, &tmparg[AML_LHS]);
	aml_parseterm(scope, &tmparg[AML_RHS]);

	/* Compare both values */
	rc = aml_cmpvalue(&tmparg[AML_LHS], &tmparg[AML_RHS], opcode);
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
	while (test && scope->pos < end) {
		/* Parse if scope */
		aml_parseterm(scope, res);
	}
	if (scope->pos >= scope->end)
		return res;

	if (*end == AMLOP_ELSE) {
		scope->pos = ++end;
		end = aml_parseend(scope);
		while (!test && scope->pos < end) {
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
		} else if (*scope->pos == AMLOP_BREAK) {
			scope->pos++;
			test = 0;
		} else if (*scope->pos == AMLOP_CONTINUE) {
			scope->pos = start;
		} else {
			aml_parseterm(scope, res);
		}
	} while (test && scope->pos <= end && cnt < 0x199);
	/* XXX: shouldn't need breakout counter */

	dnprintf(40, "Set While end : %x\n", cnt);
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
			dnprintf(99, "buffer: %.4x %.4x\n", len, end-scope->pos);
		}
		break;
	case AMLOP_PACKAGE:
	case AMLOP_VARPACKAGE:
		_aml_setvalue(res, AML_OBJTYPE_PACKAGE, len, NULL);
		for (len = 0; len < res->length && scope->pos < end; len++) {
			aml_parseop(scope, res->v_package[len]);
		}
		if (scope->pos != end) {
			dnprintf(99, "Package not equiv!! %.4x %.4x %d of %d\n",
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
	res->v_method.fneval = aml_callmethod;
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
		    aml_parseint(scope, opcode), NULL);
		break;
	case AMLOP_ONE:
	case AMLOP_ONES:
	case AMLOP_BYTEPREFIX:
	case AMLOP_WORDPREFIX:
	case AMLOP_DWORDPREFIX:
	case AMLOP_QWORDPREFIX:
	case AMLOP_REVISION:
		_aml_setvalue(res, AML_OBJTYPE_INTEGER,
		    aml_parseint(scope, opcode), NULL);
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
		if (node && node->value)
			_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, node->value);
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
	int rv;

	AML_CHECKSTACK();

	tmparg = aml_alloctmp(scope, 1);
	aml_parsetarget(scope, tmparg, NULL);
	switch (opcode) {
	case AMLOP_ACQUIRE:
		/* Assert: tmparg is AML_OBJTYPE_MUTEX */
		i1 = aml_parseint(scope, AMLOP_WORDPREFIX);
		rv = acpi_mutex_acquire(tmparg->v_objref.ref, i1);
		/* Return true if timed out */
		aml_setvalue(scope, res, NULL, rv);
		break;
	case AMLOP_RELEASE:
		acpi_mutex_release(tmparg->v_objref.ref);
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

	AML_CHECKSTACK();

	switch (opcode) {
	case AMLOP_NOTIFY:
		/* Assert: tmparg is nameref or objref */
		tmparg = aml_alloctmp(scope, 1);
		aml_parseop(scope, tmparg);
		dev = aml_dereftarget(scope, tmparg);

		i1 = aml_parseint(scope, AML_ANYINT);
		if (dev && dev->node) {
			dnprintf(10, "Notify: [%s] %.2x\n",
			    aml_nodename(dev->node), i1);
			aml_notify(dev->node, i1);
		}
		break;
	case AMLOP_SLEEP:
		i1 = aml_parseint(scope, AML_ANYINT);
		dnprintf(50, "SLEEP: %x\n", i1);
		if (i1)
			acpi_sleep(i1);
		else {
			dnprintf(10, "acpi_sleep(0)\n");
		}
		break;
	case AMLOP_STALL:
		i1 = aml_parseint(scope, AML_ANYINT);
		dnprintf(50, "STALL: %x\n", i1);
		if (i1)
			acpi_stall(i1);
		else {
			dnprintf(10, "acpi_stall(0)\n");
		}
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
		    aml_match(op2, mv2, pkg->v_package[idx])) {
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

	switch (opcode) {
	case AMLOP_INDEX:
		tmparg = aml_alloctmp(scope, 1);
		_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, NULL);
		aml_parsetarget(scope, tmparg, NULL);

		res->v_objref.index = aml_parseint(scope, AML_ANYINT);
		res->v_objref.ref = aml_dereftarget(scope, tmparg);

		aml_parsetarget(scope, tmparg, NULL);
		aml_setvalue(scope, tmparg, res, 0);
		break;
	case AMLOP_DEREFOF:
		aml_parseop(scope, res);
		break;
	case AMLOP_RETURN:
		tmparg = aml_alloctmp(scope, 1);
		aml_parseterm(scope, tmparg);
		aml_setvalue(scope, res, tmparg, 0);
		scope->pos = scope->end;
		break;
	case AMLOP_ARG0:
	case AMLOP_ARG1:
	case AMLOP_ARG2:
	case AMLOP_ARG3:
	case AMLOP_ARG4:
	case AMLOP_ARG5:
	case AMLOP_ARG6:
		opcode -= AMLOP_ARG0;
		if (scope->args == NULL || opcode >= scope->nargs)
			aml_die("arg %d out of range", opcode);

		/* Create OBJREF to stack variable */
		_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1,
		    &scope->args[opcode]);
		break;
	case AMLOP_LOCAL0:
	case AMLOP_LOCAL1:
	case AMLOP_LOCAL2:
	case AMLOP_LOCAL3:
	case AMLOP_LOCAL4:
	case AMLOP_LOCAL5:
	case AMLOP_LOCAL6:
	case AMLOP_LOCAL7:
		opcode -= AMLOP_LOCAL0;

		/* No locals exist.. lazy allocate */
		if (scope->locals == NULL) {
			dnprintf(10, "Lazy alloc locals\n");
			scope->locals = aml_alloctmp(scope, AML_MAX_LOCAL);
		}

		/* Create OBJREF to stack variable */
		_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1,
		    &scope->locals[opcode]);
		res->v_objref.ref->stack = opcode+AMLOP_LOCAL0;
		break;
	case AMLOP_LOAD:
		tmparg = aml_alloctmp(scope, 2);
		aml_parseop(scope, &tmparg[0]);
		aml_parseop(scope, &tmparg[1]);
		break;
	case AMLOP_STORE:
		tmparg = aml_alloctmp(scope, 1);
		aml_parseterm(scope, res);
		aml_parsetarget(scope, tmparg, NULL);

		while (tmparg->type == AML_OBJTYPE_OBJREF) {
			if (tmparg->v_objref.index != -1)
				break;
			tmparg = tmparg->v_objref.ref;
		}
		aml_setvalue(scope, tmparg, res, 0);
		break;
	case AMLOP_REFOF:
		_aml_setvalue(res, AML_OBJTYPE_OBJREF, -1, NULL);
		aml_parsetarget(scope, NULL, &res->v_objref.ref);
		break;
	case AMLOP_CONDREFOF:
		/* Returns true if object exists */
		tmparg = aml_alloctmp(scope, 2);
		aml_parsetarget(scope, &tmparg[0], NULL);
		aml_parsetarget(scope, &tmparg[1], NULL);
		if (tmparg[0].type != AML_OBJTYPE_NAMEREF) {
			/* Object exists */
			aml_freevalue(&tmparg[1]);
			aml_setvalue(scope, &tmparg[1], &tmparg[0], 0);
			aml_setvalue(scope, res, NULL, 1);
		} else {
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
	switch (opcode) {
	case AMLOP_CONCAT:
		tmpval = aml_alloctmp(scope, 4);
		aml_parseterm(scope, &tmpval[AML_LHS]);
		aml_parseterm(scope, &tmpval[AML_RHS]);
		aml_parsetarget(scope, &tmpval[AML_DST], NULL);
		if (tmpval[AML_LHS].type == AML_OBJTYPE_BUFFER &&
		    tmpval[AML_RHS].type == AML_OBJTYPE_BUFFER) {
			aml_resize(&tmpval[AML_LHS],
			    tmpval[AML_LHS].length+tmpval[AML_RHS].length);
			memcpy(&tmpval[AML_LHS].v_buffer+tmpval[AML_LHS].length,
			    tmpval[AML_RHS].v_buffer, tmpval[AML_RHS].length);
			aml_setvalue(scope, &tmpval[AML_DST], &tmpval[AML_LHS], 0);
		}
		if (tmpval[AML_LHS].type == AML_OBJTYPE_STRING &&
		    tmpval[AML_RHS].type == AML_OBJTYPE_STRING) {
			aml_resize(&tmpval[AML_LHS],
			    tmpval[AML_LHS].length+tmpval[AML_RHS].length);
			memcpy(&tmpval[AML_LHS].v_string+tmpval[AML_LHS].length,
			    tmpval[AML_RHS].v_buffer, tmpval[AML_RHS].length);
			aml_setvalue(scope, &tmpval[AML_DST], &tmpval[AML_LHS], 0);
		} else {
			aml_die("concat");
		}
		break;
	case AMLOP_MID:
		tmpval = aml_alloctmp(scope, 2);
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
		_aml_setvalue(res, AML_OBJTYPE_STRING, 20, NULL);
		snprintf(res->v_string, res->length,
		    ((opcode == AMLOP_TODECSTRING) ? "%d" : "%x"), i1);
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
aml_parsetarget(struct aml_scope *scope, struct aml_value *res,
    struct aml_value **opt)
{
	struct aml_value *dummy;

	/* If no value specified, allocate dynamic */
	if (res == NULL)
		res = aml_allocvalue(AML_OBJTYPE_UNINITIALIZED, 0, NULL);
	aml_parseop(scope, res);
	if (opt == NULL)
		opt = &dummy;

	*opt = aml_evaltarget(scope, res);

	return res;
}

int odp;

/* Main Opcode Parser/Evaluator */
struct aml_value *
aml_parseop(struct aml_scope *scope, struct aml_value *res)
{
	int opcode;
	struct aml_opcode *htab;
	struct aml_value *rv = NULL;

	if (odp++ > 25)
		panic("depth");

	aml_freevalue(res);
	opcode = aml_parseopcode(scope);
	dnprintf(15, "%.4x: [%s] %s\n", aml_pc(scope->pos-opsize(opcode)),
	    aml_nodename(scope->node), aml_mnem(opcode, scope->pos));
	delay(amlop_delay);

	htab = aml_findopcode(opcode);
	if (htab && htab->handler) {
		rv = htab->handler(scope, opcode, res);
	} else {
		/* No opcode handler */
		aml_die("Unknown opcode: %.4x @ %.4x", opcode,
		    aml_pc(scope->pos - opsize(opcode)));
	}
	odp--;
	return rv;
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
struct aml_fixup {
	int		offset;
	u_int8_t	oldv, newv;
} __ibm300gl[] = {
	{ 0x19, 0x3a, 0x3b },
	{ -1 }
};

struct aml_blacklist {
	const char	*oem, *oemtbl;
	struct aml_fixup *fixtab;
	u_int8_t	cksum;
} amlfix_list[] = {
	{ "IBM   ", "CDTPWSNH", __ibm300gl, 0x41 },
	{ NULL },
};

void
aml_fixup_dsdt(u_int8_t *acpi_hdr, u_int8_t *base, int len)
{
	struct acpi_table_header *hdr = (struct acpi_table_header *)acpi_hdr;
	struct aml_blacklist *fixlist;
	struct aml_fixup *fixtab;

	for (fixlist = amlfix_list; fixlist->oem; fixlist++) {
		if (!memcmp(fixlist->oem, hdr->oemid, 6) &&
		    !memcmp(fixlist->oemtbl, hdr->oemtableid, 8) &&
		    fixlist->cksum == hdr->checksum) {
			/* Found a potential fixup entry */
			for (fixtab = fixlist->fixtab; fixtab->offset != -1;
			    fixtab++) {
				if (base[fixtab->offset] == fixtab->oldv)
					base[fixtab->offset] = fixtab->newv;
			}
		}
	}
}

/*
 * @@@: Default Object creation
 */
struct aml_defval {
	const char		*name;
	int			type;
	int64_t			ival;
	const void		*bval;
	struct aml_value	**gval;
} aml_defobj[] = {
	{ "_OS_", AML_OBJTYPE_STRING, -1, "OpenBSD" },
	{ "_REV", AML_OBJTYPE_INTEGER, 2, NULL },
	{ "_GL", AML_OBJTYPE_MUTEX, 1, NULL, &aml_global_lock },
	{ "_OSI", AML_OBJTYPE_METHOD, 1, aml_callosi },
	{ NULL }
};

/* _OSI Default Method:
 * Returns True if string argument matches list of known OS strings
 * We return True for Windows to fake out nasty bad AML
 */
char *aml_valid_osi[] = {
	"OpenBSD",
	"Windows 2000",
	"Windows 2001",
	"Windows 2001.1",
	"Windows 2001 SP0",
	"Windows 2001 SP1",
	"Windows 2001 SP2",
	"Windows 2001 SP3",
	"Windows 2001 SP4",
	"Windows 2006",
	NULL
};

struct aml_value *
aml_callosi(struct aml_scope *scope, struct aml_value *val)
{
	struct aml_value tmpstr, *arg;
	int idx, result;

	/* Perform comparison with valid strings */
	result = 0;
	memset(&tmpstr, 0, sizeof(tmpstr));
	tmpstr.type = AML_OBJTYPE_STRING;
	arg = aml_derefvalue(scope, &scope->args[0], ACPI_IOREAD);

	for (idx=0; !result && aml_valid_osi[idx] != NULL; idx++) {
		tmpstr.v_string = aml_valid_osi[idx];
		tmpstr.length = strlen(tmpstr.v_string);

		result = aml_cmpvalue(arg, &tmpstr, AMLOP_LEQUAL);
	}
	aml_setvalue(scope, val, NULL, result);
	return val;
}

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
aml_print_resource(union acpi_resource *crs, void *arg)
{
	int typ = AML_CRSTYPE(crs);

	switch (typ) {
	case LR_EXTIRQ:
		printf("extirq\tflags:%.2x len:%.2x irq:%.4x\n",
		    crs->lr_extirq.flags, crs->lr_extirq.irq_count,
		    aml_letohost32(crs->lr_extirq.irq[0]));
		break;
	case SR_IRQ:
		printf("irq\t%.4x %.2x\n", aml_letohost16(crs->sr_irq.irq_mask),
		    crs->sr_irq.irq_flags);
		break;
	case SR_DMA:
		printf("dma\t%.2x %.2x\n", crs->sr_dma.channel,
		    crs->sr_dma.flags);
		break;
	case SR_IOPORT:
		printf("ioport\tflags:%.2x _min:%.4x _max:%.4x _aln:%.2x _len:%.2x\n",
		    crs->sr_ioport.flags, crs->sr_ioport._min,
		    crs->sr_ioport._max, crs->sr_ioport._aln,
		    crs->sr_ioport._len);
		break;
	case SR_STARTDEP:
		printf("startdep\n");
		break;
	case SR_ENDDEP:
		printf("enddep\n");
		break;
	case LR_WORD:
		printf("word\ttype:%.2x flags:%.2x tflag:%.2x gra:%.4x min:%.4x max:%.4x tra:%.4x len:%.4x\n",
			crs->lr_word.type, crs->lr_word.flags, crs->lr_word.tflags,
			crs->lr_word._gra, crs->lr_word._min, crs->lr_word._max,
			crs->lr_word._tra, crs->lr_word._len);
		break;
	case LR_DWORD:
		printf("dword\ttype:%.2x flags:%.2x tflag:%.2x gra:%.8x min:%.8x max:%.8x tra:%.8x len:%.8x\n",
			crs->lr_dword.type, crs->lr_dword.flags, crs->lr_dword.tflags,
			crs->lr_dword._gra, crs->lr_dword._min, crs->lr_dword._max,
			crs->lr_dword._tra, crs->lr_dword._len);
		break;
	case LR_QWORD:
		printf("dword\ttype:%.2x flags:%.2x tflag:%.2x gra:%.16llx min:%.16llx max:%.16llx tra:%.16llx len:%.16llx\n",
			crs->lr_qword.type, crs->lr_qword.flags, crs->lr_qword.tflags,
			crs->lr_qword._gra, crs->lr_qword._min, crs->lr_qword._max,
			crs->lr_qword._tra, crs->lr_qword._len);
		break;
	default:
		printf("unknown type: %x\n", typ);
		break;
	}
	return (0);
}

union acpi_resource *aml_mapresource(union acpi_resource *);

union acpi_resource *
aml_mapresource(union acpi_resource *crs)
{
	static union acpi_resource map;
	int rlen;

	rlen = AML_CRSLEN(crs);
	if (rlen >= sizeof(map))
		return crs;

	memset(&map, 0, sizeof(map));
	memcpy(&map, crs, rlen);

	return &map;
}

int
aml_parse_resource(int length, uint8_t *buffer,
    int (*crs_enum)(union acpi_resource *, void *), void *arg)
{
	int off, rlen;
	union acpi_resource *crs;

	for (off = 0; off < length; off += rlen) {
		crs = (union acpi_resource *)(buffer+off);

		rlen = AML_CRSLEN(crs);
		if (crs->hdr.typecode == 0x79 || rlen <= 3)
			break;

		crs = aml_mapresource(crs);
#ifdef ACPI_DEBUG
		aml_print_resource(crs, NULL);
#endif
		crs_enum(crs, arg);
	}

	return 0;
}

void
aml_foreachpkg(struct aml_value *pkg, int start,
	       void (*fn)(struct aml_value *, void *),
	       void *arg)
{
	int idx;

	if (pkg->type != AML_OBJTYPE_PACKAGE)
		return;
	for (idx=start; idx<pkg->length; idx++)
		fn(pkg->v_package[idx], arg);
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
	aml_parsenode(NULL, &aml_root, start, &end, NULL);
	dnprintf(50, " : parsed %d AML bytes\n", length);

	return (0);
}

/*
 * Walk nodes and perform fixups for nameref
 */
int aml_fixup_node(struct aml_node *, void *);

int aml_fixup_node(struct aml_node *node, void *arg)
{
	struct aml_value *val = arg;
	int i;

	if (node->value == NULL)
		return (0);
	if (arg == NULL)
		aml_fixup_node(node, node->value);
	else if (val->type == AML_OBJTYPE_NAMEREF) {
		node = aml_searchname(node, val->v_nameref);
		if (node && node->value) {
			_aml_setvalue(val, AML_OBJTYPE_OBJREF, -1,
			    node->value);
		}
	} else if (val->type == AML_OBJTYPE_PACKAGE) {
		for (i = 0; i < val->length; i++)
			aml_fixup_node(node, val->v_package[i]);
	} else if (val->type == AML_OBJTYPE_OPREGION) {
		if (val->v_opregion.iospace != GAS_PCI_CFG_SPACE)
			return (0);
		if (ACPI_PCI_FN(val->v_opregion.iobase) != 0xFFFF)
			return (0);
		val->v_opregion.iobase =
		    ACPI_PCI_REG(val->v_opregion.iobase) +
		    aml_getpciaddr(dsdt_softc, node);
		dnprintf(20, "late ioaddr : %s:%llx\n",
		    aml_nodename(node), val->v_opregion.iobase);
	}
	return (0);
}

void
aml_postparse()
{
	aml_walknodes(&aml_root, AML_WALK_PRE, aml_fixup_node, NULL);
}
