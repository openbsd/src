/*
 *  Store and retrieve mechanism.
 *
 *  Copyright (c) 1995-2000, Raphael Manfredi
 *  
 *  You may redistribute only under the same terms as Perl 5, as specified
 *  in the README file that comes with the distribution.
 *
 */

#define PERL_NO_GET_CONTEXT     /* we want efficiency */
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#ifndef PATCHLEVEL
#    include <patchlevel.h>		/* Perl's one, needed since 5.6 */
#    if !(defined(PERL_VERSION) || (SUBVERSION > 0 && defined(PATCHLEVEL)))
#        include <could_not_find_Perl_patchlevel.h>
#    endif
#endif

#if PERL_VERSION < 8
#include "ppport.h"             /* handle old perls */
#endif

#ifndef NETWARE
#if 0
#define DEBUGME /* Debug mode, turns assertions on as well */
#define DASSERT /* Assertion mode */
#endif
#else	/* NETWARE */
#if 0	/* On NetWare USE_PERLIO is not used */
#define DEBUGME /* Debug mode, turns assertions on as well */
#define DASSERT /* Assertion mode */
#endif
#endif

/*
 * Pre PerlIO time when none of USE_PERLIO and PERLIO_IS_STDIO is defined
 * Provide them with the necessary defines so they can build with pre-5.004.
 */
#ifndef USE_PERLIO
#ifndef PERLIO_IS_STDIO
#define PerlIO FILE
#define PerlIO_getc(x) getc(x)
#define PerlIO_putc(f,x) putc(x,f)
#define PerlIO_read(x,y,z) fread(y,1,z,x)
#define PerlIO_write(x,y,z) fwrite(y,1,z,x)
#define PerlIO_stdoutf printf
#endif	/* PERLIO_IS_STDIO */
#endif	/* USE_PERLIO */

/*
 * Earlier versions of perl might be used, we can't assume they have the latest!
 */

#ifndef PERL_VERSION		/* For perls < 5.6 */
#define PERL_VERSION PATCHLEVEL
#ifndef newRV_noinc
#define newRV_noinc(sv)		((Sv = newRV(sv)), --SvREFCNT(SvRV(Sv)), Sv)
#endif
#if (PATCHLEVEL <= 4)		/* Older perls (<= 5.004) lack PL_ namespace */
#define PL_sv_yes	sv_yes
#define PL_sv_no	sv_no
#define PL_sv_undef	sv_undef
#if (SUBVERSION <= 4)		/* 5.004_04 has been reported to lack newSVpvn */
#define newSVpvn newSVpv
#endif
#endif						/* PATCHLEVEL <= 4 */
#ifndef HvSHAREKEYS_off
#define HvSHAREKEYS_off(hv)	/* Ignore */
#endif
#ifndef AvFILLp				/* Older perls (<=5.003) lack AvFILLp */
#define AvFILLp AvFILL
#endif
typedef double NV;			/* Older perls lack the NV type */
#define	IVdf		"ld"	/* Various printf formats for Perl types */
#define	UVuf		"lu"
#define	UVof		"lo"
#define	UVxf		"lx"
#define INT2PTR(t,v) (t)(IV)(v)
#define PTR2UV(v)    (unsigned long)(v)
#endif						/* PERL_VERSION -- perls < 5.6 */

#ifndef NVef				/* The following were not part of perl 5.6 */
#if defined(USE_LONG_DOUBLE) && \
	defined(HAS_LONG_DOUBLE) && defined(PERL_PRIfldbl)
#define NVef		PERL_PRIeldbl
#define NVff		PERL_PRIfldbl
#define NVgf		PERL_PRIgldbl
#else
#define	NVef		"e"
#define	NVff		"f"
#define	NVgf		"g"
#endif
#endif

#ifdef DEBUGME

#ifndef DASSERT
#define DASSERT
#endif

/*
 * TRACEME() will only output things when the $Storable::DEBUGME is true.
 */

#define TRACEME(x)										\
  STMT_START {											\
	if (SvTRUE(perl_get_sv("Storable::DEBUGME", TRUE)))	\
		{ PerlIO_stdoutf x; PerlIO_stdoutf("\n"); }		\
  } STMT_END
#else
#define TRACEME(x)
#endif	/* DEBUGME */

#ifdef DASSERT
#define ASSERT(x,y)										\
  STMT_START {											\
	if (!(x)) {												\
		PerlIO_stdoutf("ASSERT FAILED (\"%s\", line %d): ",	\
			__FILE__, __LINE__);							\
		PerlIO_stdoutf y; PerlIO_stdoutf("\n");				\
	}														\
  } STMT_END
#else
#define ASSERT(x,y)
#endif

/*
 * Type markers.
 */

#define C(x) ((char) (x))	/* For markers with dynamic retrieval handling */

#define SX_OBJECT	C(0)	/* Already stored object */
#define SX_LSCALAR	C(1)	/* Scalar (large binary) follows (length, data) */
#define SX_ARRAY	C(2)	/* Array forthcominng (size, item list) */
#define SX_HASH		C(3)	/* Hash forthcoming (size, key/value pair list) */
#define SX_REF		C(4)	/* Reference to object forthcoming */
#define SX_UNDEF	C(5)	/* Undefined scalar */
#define SX_INTEGER	C(6)	/* Integer forthcoming */
#define SX_DOUBLE	C(7)	/* Double forthcoming */
#define SX_BYTE		C(8)	/* (signed) byte forthcoming */
#define SX_NETINT	C(9)	/* Integer in network order forthcoming */
#define SX_SCALAR	C(10)	/* Scalar (binary, small) follows (length, data) */
#define SX_TIED_ARRAY	C(11)	/* Tied array forthcoming */
#define SX_TIED_HASH	C(12)	/* Tied hash forthcoming */
#define SX_TIED_SCALAR	C(13)	/* Tied scalar forthcoming */
#define SX_SV_UNDEF	C(14)	/* Perl's immortal PL_sv_undef */
#define SX_SV_YES	C(15)	/* Perl's immortal PL_sv_yes */
#define SX_SV_NO	C(16)	/* Perl's immortal PL_sv_no */
#define SX_BLESS	C(17)	/* Object is blessed */
#define SX_IX_BLESS	C(18)	/* Object is blessed, classname given by index */
#define SX_HOOK		C(19)	/* Stored via hook, user-defined */
#define SX_OVERLOAD	C(20)	/* Overloaded reference */
#define SX_TIED_KEY	C(21)	/* Tied magic key forthcoming */
#define SX_TIED_IDX	C(22)	/* Tied magic index forthcoming */
#define SX_UTF8STR	C(23)	/* UTF-8 string forthcoming (small) */
#define SX_LUTF8STR	C(24)	/* UTF-8 string forthcoming (large) */
#define SX_FLAG_HASH	C(25)	/* Hash with flags forthcoming (size, flags, key/flags/value triplet list) */
#define SX_CODE         C(26)   /* Code references as perl source code */
#define SX_ERROR	C(27)	/* Error */

/*
 * Those are only used to retrieve "old" pre-0.6 binary images.
 */
#define SX_ITEM		'i'		/* An array item introducer */
#define SX_IT_UNDEF	'I'		/* Undefined array item */
#define SX_KEY		'k'		/* A hash key introducer */
#define SX_VALUE	'v'		/* A hash value introducer */
#define SX_VL_UNDEF	'V'		/* Undefined hash value */

/*
 * Those are only used to retrieve "old" pre-0.7 binary images
 */

#define SX_CLASS	'b'		/* Object is blessed, class name length <255 */
#define SX_LG_CLASS	'B'		/* Object is blessed, class name length >255 */
#define SX_STORED	'X'		/* End of object */

/*
 * Limits between short/long length representation.
 */

#define LG_SCALAR	255		/* Large scalar length limit */
#define LG_BLESS	127		/* Large classname bless limit */

/*
 * Operation types
 */

#define ST_STORE	0x1		/* Store operation */
#define ST_RETRIEVE	0x2		/* Retrieval operation */
#define ST_CLONE	0x4		/* Deep cloning operation */

/*
 * The following structure is used for hash table key retrieval. Since, when
 * retrieving objects, we'll be facing blessed hash references, it's best
 * to pre-allocate that buffer once and resize it as the need arises, never
 * freeing it (keys will be saved away someplace else anyway, so even large
 * keys are not enough a motivation to reclaim that space).
 *
 * This structure is also used for memory store/retrieve operations which
 * happen in a fixed place before being malloc'ed elsewhere if persistency
 * is required. Hence the aptr pointer.
 */
struct extendable {
	char *arena;		/* Will hold hash key strings, resized as needed */
	STRLEN asiz;		/* Size of aforementionned buffer */
	char *aptr;			/* Arena pointer, for in-place read/write ops */
	char *aend;			/* First invalid address */
};

/*
 * At store time:
 * A hash table records the objects which have already been stored.
 * Those are referred to as SX_OBJECT in the file, and their "tag" (i.e.
 * an arbitrary sequence number) is used to identify them.
 *
 * At retrieve time:
 * An array table records the objects which have already been retrieved,
 * as seen by the tag determind by counting the objects themselves. The
 * reference to that retrieved object is kept in the table, and is returned
 * when an SX_OBJECT is found bearing that same tag.
 *
 * The same processing is used to record "classname" for blessed objects:
 * indexing by a hash at store time, and via an array at retrieve time.
 */

typedef unsigned long stag_t;	/* Used by pre-0.6 binary format */

/*
 * The following "thread-safe" related defines were contributed by
 * Murray Nesbitt <murray@activestate.com> and integrated by RAM, who
 * only renamed things a little bit to ensure consistency with surrounding
 * code.	-- RAM, 14/09/1999
 *
 * The original patch suffered from the fact that the stcxt_t structure
 * was global.  Murray tried to minimize the impact on the code as much as
 * possible.
 *
 * Starting with 0.7, Storable can be re-entrant, via the STORABLE_xxx hooks
 * on objects.  Therefore, the notion of context needs to be generalized,
 * threading or not.
 */

#define MY_VERSION "Storable(" XS_VERSION ")"


/*
 * Conditional UTF8 support.
 *
 */
#ifdef SvUTF8_on
#define STORE_UTF8STR(pv, len)	STORE_PV_LEN(pv, len, SX_UTF8STR, SX_LUTF8STR)
#define HAS_UTF8_SCALARS
#ifdef HeKUTF8
#define HAS_UTF8_HASHES
#define HAS_UTF8_ALL
#else
/* 5.6 perl has utf8 scalars but not hashes */
#endif
#else
#define SvUTF8(sv) 0
#define STORE_UTF8STR(pv, len) CROAK(("panic: storing UTF8 in non-UTF8 perl"))
#endif
#ifndef HAS_UTF8_ALL
#define UTF8_CROAK() CROAK(("Cannot retrieve UTF8 data in non-UTF8 perl"))
#endif

#ifdef HvPLACEHOLDERS
#define HAS_RESTRICTED_HASHES
#else
#define HVhek_PLACEHOLD	0x200
#define RESTRICTED_HASH_CROAK() CROAK(("Cannot retrieve restricted hash"))
#endif

#ifdef HvHASKFLAGS
#define HAS_HASH_KEY_FLAGS
#endif

/*
 * Fields s_tainted and s_dirty are prefixed with s_ because Perl's include
 * files remap tainted and dirty when threading is enabled.  That's bad for
 * perl to remap such common words.	-- RAM, 29/09/00
 */

typedef struct stcxt {
	int entry;			/* flags recursion */
	int optype;			/* type of traversal operation */
	HV *hseen;			/* which objects have been seen, store time */
	AV *hook_seen;		/* which SVs were returned by STORABLE_freeze() */
	AV *aseen;			/* which objects have been seen, retrieve time */
	IV where_is_undef;		/* index in aseen of PL_sv_undef */
	HV *hclass;			/* which classnames have been seen, store time */
	AV *aclass;			/* which classnames have been seen, retrieve time */
	HV *hook;			/* cache for hook methods per class name */
	IV tagnum;			/* incremented at store time for each seen object */
	IV classnum;		/* incremented at store time for each seen classname */
	int netorder;		/* true if network order used */
	int s_tainted;		/* true if input source is tainted, at retrieve time */
	int forgive_me;		/* whether to be forgiving... */
	int deparse;        /* whether to deparse code refs */
	SV *eval;           /* whether to eval source code */
	int canonical;		/* whether to store hashes sorted by key */
#ifndef HAS_RESTRICTED_HASHES
        int derestrict;         /* whether to downgrade restrcted hashes */
#endif
#ifndef HAS_UTF8_ALL
        int use_bytes;         /* whether to bytes-ify utf8 */
#endif
        int accept_future_minor; /* croak immediately on future minor versions?  */
	int s_dirty;		/* context is dirty due to CROAK() -- can be cleaned */
	int membuf_ro;		/* true means membuf is read-only and msaved is rw */
	struct extendable keybuf;	/* for hash key retrieval */
	struct extendable membuf;	/* for memory store/retrieve operations */
	struct extendable msaved;	/* where potentially valid mbuf is saved */
	PerlIO *fio;		/* where I/O are performed, NULL for memory */
	int ver_major;		/* major of version for retrieved object */
	int ver_minor;		/* minor of version for retrieved object */
	SV *(**retrieve_vtbl)();	/* retrieve dispatch table */
	SV *prev;		/* contexts chained backwards in real recursion */
	SV *my_sv;		/* the blessed scalar who's SvPVX() I am */
} stcxt_t;

#define NEW_STORABLE_CXT_OBJ(cxt)					\
  STMT_START {										\
	SV *self = newSV(sizeof(stcxt_t) - 1);			\
	SV *my_sv = newRV_noinc(self);					\
	sv_bless(my_sv, gv_stashpv("Storable::Cxt", TRUE));	\
	cxt = (stcxt_t *)SvPVX(self);					\
	Zero(cxt, 1, stcxt_t);							\
	cxt->my_sv = my_sv;								\
  } STMT_END

#if defined(MULTIPLICITY) || defined(PERL_OBJECT) || defined(PERL_CAPI)

#if (PATCHLEVEL <= 4) && (SUBVERSION < 68)
#define dSTCXT_SV 									\
	SV *perinterp_sv = perl_get_sv(MY_VERSION, FALSE)
#else	/* >= perl5.004_68 */
#define dSTCXT_SV									\
	SV *perinterp_sv = *hv_fetch(PL_modglobal,		\
		MY_VERSION, sizeof(MY_VERSION)-1, TRUE)
#endif	/* < perl5.004_68 */

#define dSTCXT_PTR(T,name)							\
	T name = ((perinterp_sv && SvIOK(perinterp_sv) && SvIVX(perinterp_sv)	\
				? (T)SvPVX(SvRV(INT2PTR(SV*,SvIVX(perinterp_sv)))) : (T) 0))
#define dSTCXT										\
	dSTCXT_SV;										\
	dSTCXT_PTR(stcxt_t *, cxt)

#define INIT_STCXT							\
	dSTCXT;									\
	NEW_STORABLE_CXT_OBJ(cxt);				\
	sv_setiv(perinterp_sv, PTR2IV(cxt->my_sv))

#define SET_STCXT(x)								\
  STMT_START {										\
	dSTCXT_SV;										\
	sv_setiv(perinterp_sv, PTR2IV(x->my_sv));		\
  } STMT_END

#else /* !MULTIPLICITY && !PERL_OBJECT && !PERL_CAPI */

static stcxt_t *Context_ptr = NULL;
#define dSTCXT			stcxt_t *cxt = Context_ptr
#define SET_STCXT(x)		Context_ptr = x
#define INIT_STCXT						\
	dSTCXT;								\
	NEW_STORABLE_CXT_OBJ(cxt);			\
	SET_STCXT(cxt)


#endif /* MULTIPLICITY || PERL_OBJECT || PERL_CAPI */

/*
 * KNOWN BUG:
 *   Croaking implies a memory leak, since we don't use setjmp/longjmp
 *   to catch the exit and free memory used during store or retrieve
 *   operations.  This is not too difficult to fix, but I need to understand
 *   how Perl does it, and croaking is exceptional anyway, so I lack the
 *   motivation to do it.
 *
 * The current workaround is to mark the context as dirty when croaking,
 * so that data structures can be freed whenever we renter Storable code
 * (but only *then*: it's a workaround, not a fix).
 *
 * This is also imperfect, because we don't really know how far they trapped
 * the croak(), and when we were recursing, we won't be able to clean anything
 * but the topmost context stacked.
 */

#define CROAK(x)	STMT_START { cxt->s_dirty = 1; croak x; } STMT_END

/*
 * End of "thread-safe" related definitions.
 */

/*
 * LOW_32BITS
 *
 * Keep only the low 32 bits of a pointer (used for tags, which are not
 * really pointers).
 */

#if PTRSIZE <= 4
#define LOW_32BITS(x)	((I32) (x))
#else
#define LOW_32BITS(x)	((I32) ((unsigned long) (x) & 0xffffffffUL))
#endif

/*
 * oI, oS, oC
 *
 * Hack for Crays, where sizeof(I32) == 8, and which are big-endians.
 * Used in the WLEN and RLEN macros.
 */

#if INTSIZE > 4
#define oI(x)	((I32 *) ((char *) (x) + 4))
#define oS(x)	((x) - 4)
#define oC(x)	(x = 0)
#define CRAY_HACK
#else
#define oI(x)	(x)
#define oS(x)	(x)
#define oC(x)
#endif

/*
 * key buffer handling
 */
#define kbuf	(cxt->keybuf).arena
#define ksiz	(cxt->keybuf).asiz
#define KBUFINIT()						\
  STMT_START {							\
	if (!kbuf) {						\
		TRACEME(("** allocating kbuf of 128 bytes")); \
		New(10003, kbuf, 128, char);	\
		ksiz = 128;						\
	}									\
  } STMT_END
#define KBUFCHK(x)				\
  STMT_START {					\
	if (x >= ksiz) {			\
		TRACEME(("** extending kbuf to %d bytes (had %d)", x+1, ksiz)); \
		Renew(kbuf, x+1, char);	\
		ksiz = x+1;				\
	}							\
  } STMT_END

/*
 * memory buffer handling
 */
#define mbase	(cxt->membuf).arena
#define msiz	(cxt->membuf).asiz
#define mptr	(cxt->membuf).aptr
#define mend	(cxt->membuf).aend

#define MGROW	(1 << 13)
#define MMASK	(MGROW - 1)

#define round_mgrow(x)	\
	((unsigned long) (((unsigned long) (x) + MMASK) & ~MMASK))
#define trunc_int(x)	\
	((unsigned long) ((unsigned long) (x) & ~(sizeof(int)-1)))
#define int_aligned(x)	\
	((unsigned long) (x) == trunc_int(x))

#define MBUF_INIT(x)					\
  STMT_START {							\
	if (!mbase) {						\
		TRACEME(("** allocating mbase of %d bytes", MGROW)); \
		New(10003, mbase, MGROW, char);	\
		msiz = (STRLEN)MGROW;					\
	}									\
	mptr = mbase;						\
	if (x)								\
		mend = mbase + x;				\
	else								\
		mend = mbase + msiz;			\
  } STMT_END

#define MBUF_TRUNC(x)	mptr = mbase + x
#define MBUF_SIZE()		(mptr - mbase)

/*
 * MBUF_SAVE_AND_LOAD
 * MBUF_RESTORE
 *
 * Those macros are used in do_retrieve() to save the current memory
 * buffer into cxt->msaved, before MBUF_LOAD() can be used to retrieve
 * data from a string.
 */
#define MBUF_SAVE_AND_LOAD(in)			\
  STMT_START {							\
	ASSERT(!cxt->membuf_ro, ("mbase not already saved")); \
	cxt->membuf_ro = 1;					\
	TRACEME(("saving mbuf"));			\
	StructCopy(&cxt->membuf, &cxt->msaved, struct extendable); \
	MBUF_LOAD(in);						\
  } STMT_END

#define MBUF_RESTORE() 					\
  STMT_START {							\
	ASSERT(cxt->membuf_ro, ("mbase is read-only")); \
	cxt->membuf_ro = 0;					\
	TRACEME(("restoring mbuf"));		\
	StructCopy(&cxt->msaved, &cxt->membuf, struct extendable); \
  } STMT_END

/*
 * Use SvPOKp(), because SvPOK() fails on tainted scalars.
 * See store_scalar() for other usage of this workaround.
 */
#define MBUF_LOAD(v) 					\
  STMT_START {							\
	ASSERT(cxt->membuf_ro, ("mbase is read-only")); \
	if (!SvPOKp(v))						\
		CROAK(("Not a scalar string"));	\
	mptr = mbase = SvPV(v, msiz);		\
	mend = mbase + msiz;				\
  } STMT_END

#define MBUF_XTEND(x) 				\
  STMT_START {						\
	int nsz = (int) round_mgrow((x)+msiz);	\
	int offset = mptr - mbase;		\
	ASSERT(!cxt->membuf_ro, ("mbase is not read-only")); \
	TRACEME(("** extending mbase from %d to %d bytes (wants %d new)", \
		msiz, nsz, (x)));			\
	Renew(mbase, nsz, char);		\
	msiz = nsz;						\
	mptr = mbase + offset;			\
	mend = mbase + nsz;				\
  } STMT_END

#define MBUF_CHK(x) 				\
  STMT_START {						\
	if ((mptr + (x)) > mend)		\
		MBUF_XTEND(x);				\
  } STMT_END

#define MBUF_GETC(x) 				\
  STMT_START {						\
	if (mptr < mend)				\
		x = (int) (unsigned char) *mptr++;	\
	else							\
		return (SV *) 0;			\
  } STMT_END

#ifdef CRAY_HACK
#define MBUF_GETINT(x) 					\
  STMT_START {							\
	oC(x);								\
	if ((mptr + 4) <= mend) {			\
		memcpy(oI(&x), mptr, 4);		\
		mptr += 4;						\
	} else								\
		return (SV *) 0;				\
  } STMT_END
#else
#define MBUF_GETINT(x) 					\
  STMT_START {							\
	if ((mptr + sizeof(int)) <= mend) {	\
		if (int_aligned(mptr))			\
			x = *(int *) mptr;			\
		else							\
			memcpy(&x, mptr, sizeof(int));	\
		mptr += sizeof(int);			\
	} else								\
		return (SV *) 0;				\
  } STMT_END
#endif

#define MBUF_READ(x,s) 				\
  STMT_START {						\
	if ((mptr + (s)) <= mend) {		\
		memcpy(x, mptr, s);			\
		mptr += s;					\
	} else							\
		return (SV *) 0;			\
  } STMT_END

#define MBUF_SAFEREAD(x,s,z) 		\
  STMT_START {						\
	if ((mptr + (s)) <= mend) {		\
		memcpy(x, mptr, s);			\
		mptr += s;					\
	} else {						\
		sv_free(z);					\
		return (SV *) 0;			\
	}								\
  } STMT_END

#define MBUF_PUTC(c) 				\
  STMT_START {						\
	if (mptr < mend)				\
		*mptr++ = (char) c;			\
	else {							\
		MBUF_XTEND(1);				\
		*mptr++ = (char) c;			\
	}								\
  } STMT_END

#ifdef CRAY_HACK
#define MBUF_PUTINT(i) 				\
  STMT_START {						\
	MBUF_CHK(4);					\
	memcpy(mptr, oI(&i), 4);		\
	mptr += 4;						\
  } STMT_END
#else
#define MBUF_PUTINT(i) 				\
  STMT_START {						\
	MBUF_CHK(sizeof(int));			\
	if (int_aligned(mptr))			\
		*(int *) mptr = i;			\
	else							\
		memcpy(mptr, &i, sizeof(int));	\
	mptr += sizeof(int);			\
  } STMT_END
#endif

#define MBUF_WRITE(x,s) 			\
  STMT_START {						\
	MBUF_CHK(s);					\
	memcpy(mptr, x, s);				\
	mptr += s;						\
  } STMT_END

/*
 * Possible return values for sv_type().
 */

#define svis_REF		0
#define svis_SCALAR		1
#define svis_ARRAY		2
#define svis_HASH		3
#define svis_TIED		4
#define svis_TIED_ITEM	5
#define svis_CODE		6
#define svis_OTHER		7

/*
 * Flags for SX_HOOK.
 */

#define SHF_TYPE_MASK		0x03
#define SHF_LARGE_CLASSLEN	0x04
#define SHF_LARGE_STRLEN	0x08
#define SHF_LARGE_LISTLEN	0x10
#define SHF_IDX_CLASSNAME	0x20
#define SHF_NEED_RECURSE	0x40
#define SHF_HAS_LIST		0x80

/*
 * Types for SX_HOOK (last 2 bits in flags).
 */

#define SHT_SCALAR			0
#define SHT_ARRAY			1
#define SHT_HASH			2
#define SHT_EXTRA			3		/* Read extra byte for type */

/*
 * The following are held in the "extra byte"...
 */

#define SHT_TSCALAR			4		/* 4 + 0 -- tied scalar */
#define SHT_TARRAY			5		/* 4 + 1 -- tied array */
#define SHT_THASH			6		/* 4 + 2 -- tied hash */

/*
 * per hash flags for flagged hashes
 */

#define SHV_RESTRICTED		0x01

/*
 * per key flags for flagged hashes
 */

#define SHV_K_UTF8		0x01
#define SHV_K_WASUTF8		0x02
#define SHV_K_LOCKED		0x04
#define SHV_K_ISSV		0x08
#define SHV_K_PLACEHOLDER	0x10

/*
 * Before 0.6, the magic string was "perl-store" (binary version number 0).
 *
 * Since 0.6 introduced many binary incompatibilities, the magic string has
 * been changed to "pst0" to allow an old image to be properly retrieved by
 * a newer Storable, but ensure a newer image cannot be retrieved with an
 * older version.
 *
 * At 0.7, objects are given the ability to serialize themselves, and the
 * set of markers is extended, backward compatibility is not jeopardized,
 * so the binary version number could have remained unchanged.  To correctly
 * spot errors if a file making use of 0.7-specific extensions is given to
 * 0.6 for retrieval, the binary version was moved to "2".  And I'm introducing
 * a "minor" version, to better track this kind of evolution from now on.
 * 
 */
static const char old_magicstr[] = "perl-store"; /* Magic number before 0.6 */
static const char magicstr[] = "pst0";		 /* Used as a magic number */

#define MAGICSTR_BYTES  'p','s','t','0'
#define OLDMAGICSTR_BYTES  'p','e','r','l','-','s','t','o','r','e'

/* 5.6.x introduced the ability to have IVs as long long.
   However, Configure still defined BYTEORDER based on the size of a long.
   Storable uses the BYTEORDER value as part of the header, but doesn't
   explicity store sizeof(IV) anywhere in the header.  Hence on 5.6.x built
   with IV as long long on a platform that uses Configure (ie most things
   except VMS and Windows) headers are identical for the different IV sizes,
   despite the files containing some fields based on sizeof(IV)
   Erk. Broken-ness.
   5.8 is consistent - the following redifinition kludge is only needed on
   5.6.x, but the interwork is needed on 5.8 while data survives in files
   with the 5.6 header.

*/

#if defined (IVSIZE) && (IVSIZE == 8) && (LONGSIZE == 4)
#ifndef NO_56_INTERWORK_KLUDGE
#define USE_56_INTERWORK_KLUDGE
#endif
#if BYTEORDER == 0x1234
#undef BYTEORDER
#define BYTEORDER 0x12345678
#else
#if BYTEORDER == 0x4321
#undef BYTEORDER
#define BYTEORDER 0x87654321
#endif
#endif
#endif

#if BYTEORDER == 0x1234
#define BYTEORDER_BYTES  '1','2','3','4'
#else
#if BYTEORDER == 0x12345678
#define BYTEORDER_BYTES  '1','2','3','4','5','6','7','8'
#ifdef USE_56_INTERWORK_KLUDGE
#define BYTEORDER_BYTES_56  '1','2','3','4'
#endif
#else
#if BYTEORDER == 0x87654321
#define BYTEORDER_BYTES  '8','7','6','5','4','3','2','1'
#ifdef USE_56_INTERWORK_KLUDGE
#define BYTEORDER_BYTES_56  '4','3','2','1'
#endif
#else
#if BYTEORDER == 0x4321
#define BYTEORDER_BYTES  '4','3','2','1'
#else
#error Unknown byteoder. Please append your byteorder to Storable.xs
#endif
#endif
#endif
#endif

static const char byteorderstr[] = {BYTEORDER_BYTES, 0};
#ifdef USE_56_INTERWORK_KLUDGE
static const char byteorderstr_56[] = {BYTEORDER_BYTES_56, 0};
#endif

#define STORABLE_BIN_MAJOR	2		/* Binary major "version" */
#define STORABLE_BIN_MINOR	6		/* Binary minor "version" */

/* If we aren't 5.7.3 or later, we won't be writing out files that use the
 * new flagged hash introdued in 2.5, so put 2.4 in the binary header to
 * maximise ease of interoperation with older Storables.
 * Could we write 2.3s if we're on 5.005_03? NWC
 */
#if (PATCHLEVEL <= 6)
#define STORABLE_BIN_WRITE_MINOR	4
#else 
/* 
 * As of perl 5.7.3, utf8 hash key is introduced.
 * So this must change -- dankogai
*/
#define STORABLE_BIN_WRITE_MINOR	6
#endif /* (PATCHLEVEL <= 6) */

#if (PATCHLEVEL < 8 || (PATCHLEVEL == 8 && SUBVERSION < 1))
#define PL_sv_placeholder PL_sv_undef
#endif

/*
 * Useful store shortcuts...
 */

/*
 * Note that if you put more than one mark for storing a particular
 * type of thing, *and* in the retrieve_foo() function you mark both
 * the thingy's you get off with SEEN(), you *must* increase the
 * tagnum with cxt->tagnum++ along with this macro!
 *     - samv 20Jan04
 */
#define PUTMARK(x) 							\
  STMT_START {								\
	if (!cxt->fio)							\
		MBUF_PUTC(x);						\
	else if (PerlIO_putc(cxt->fio, x) == EOF)	\
		return -1;							\
  } STMT_END

#define WRITE_I32(x)					\
  STMT_START {							\
	ASSERT(sizeof(x) == sizeof(I32), ("writing an I32"));	\
	if (!cxt->fio)						\
		MBUF_PUTINT(x);					\
	else if (PerlIO_write(cxt->fio, oI(&x), oS(sizeof(x))) != oS(sizeof(x))) \
		return -1;					\
  } STMT_END

#ifdef HAS_HTONL
#define WLEN(x)						\
  STMT_START {						\
	if (cxt->netorder) {			\
		int y = (int) htonl(x);		\
		if (!cxt->fio)				\
			MBUF_PUTINT(y);			\
		else if (PerlIO_write(cxt->fio,oI(&y),oS(sizeof(y))) != oS(sizeof(y))) \
			return -1;				\
	} else {						\
		if (!cxt->fio)				\
			MBUF_PUTINT(x);			\
		else if (PerlIO_write(cxt->fio,oI(&x),oS(sizeof(x))) != oS(sizeof(x))) \
			return -1;				\
	}								\
  } STMT_END
#else
#define WLEN(x)	WRITE_I32(x)
#endif

#define WRITE(x,y) 							\
  STMT_START {								\
	if (!cxt->fio)							\
		MBUF_WRITE(x,y);					\
	else if (PerlIO_write(cxt->fio, x, y) != y)	\
		return -1;							\
  } STMT_END

#define STORE_PV_LEN(pv, len, small, large)			\
  STMT_START {							\
	if (len <= LG_SCALAR) {				\
		unsigned char clen = (unsigned char) len;	\
		PUTMARK(small);					\
		PUTMARK(clen);					\
		if (len)						\
			WRITE(pv, len);				\
	} else {							\
		PUTMARK(large);					\
		WLEN(len);						\
		WRITE(pv, len);					\
	}									\
  } STMT_END

#define STORE_SCALAR(pv, len)	STORE_PV_LEN(pv, len, SX_SCALAR, SX_LSCALAR)

/*
 * Store &PL_sv_undef in arrays without recursing through store().
 */
#define STORE_SV_UNDEF() 					\
  STMT_START {							\
	cxt->tagnum++;						\
	PUTMARK(SX_SV_UNDEF);					\
  } STMT_END

/*
 * Useful retrieve shortcuts...
 */

#define GETCHAR() \
	(cxt->fio ? PerlIO_getc(cxt->fio) : (mptr >= mend ? EOF : (int) *mptr++))

#define GETMARK(x) 								\
  STMT_START {									\
	if (!cxt->fio)								\
		MBUF_GETC(x);							\
	else if ((int) (x = PerlIO_getc(cxt->fio)) == EOF)	\
		return (SV *) 0;						\
  } STMT_END

#define READ_I32(x)						\
  STMT_START {							\
	ASSERT(sizeof(x) == sizeof(I32), ("reading an I32"));	\
	oC(x);								\
	if (!cxt->fio)						\
		MBUF_GETINT(x);					\
	else if (PerlIO_read(cxt->fio, oI(&x), oS(sizeof(x))) != oS(sizeof(x)))	\
		return (SV *) 0;				\
  } STMT_END

#ifdef HAS_NTOHL
#define RLEN(x)							\
  STMT_START {							\
	oC(x);								\
	if (!cxt->fio)						\
		MBUF_GETINT(x);					\
	else if (PerlIO_read(cxt->fio, oI(&x), oS(sizeof(x))) != oS(sizeof(x)))	\
		return (SV *) 0;				\
	if (cxt->netorder)					\
		x = (int) ntohl(x);				\
  } STMT_END
#else
#define RLEN(x) READ_I32(x)
#endif

#define READ(x,y) 							\
  STMT_START {								\
	if (!cxt->fio)							\
		MBUF_READ(x, y);					\
	else if (PerlIO_read(cxt->fio, x, y) != y)	\
		return (SV *) 0;					\
  } STMT_END

#define SAFEREAD(x,y,z)		 					\
  STMT_START {									\
	if (!cxt->fio)								\
		MBUF_SAFEREAD(x,y,z);					\
	else if (PerlIO_read(cxt->fio, x, y) != y)	 {	\
		sv_free(z);								\
		return (SV *) 0;						\
	}											\
  } STMT_END

/*
 * This macro is used at retrieve time, to remember where object 'y', bearing a
 * given tag 'tagnum', has been retrieved. Next time we see an SX_OBJECT marker,
 * we'll therefore know where it has been retrieved and will be able to
 * share the same reference, as in the original stored memory image.
 *
 * We also need to bless objects ASAP for hooks (which may compute "ref $x"
 * on the objects given to STORABLE_thaw and expect that to be defined), and
 * also for overloaded objects (for which we might not find the stash if the
 * object is not blessed yet--this might occur for overloaded objects that
 * refer to themselves indirectly: if we blessed upon return from a sub
 * retrieve(), the SX_OBJECT marker we'd found could not have overloading
 * restored on it because the underlying object would not be blessed yet!).
 *
 * To achieve that, the class name of the last retrieved object is passed down
 * recursively, and the first SEEN() call for which the class name is not NULL
 * will bless the object.
 *
 * i should be true iff sv is immortal (ie PL_sv_yes, PL_sv_no or PL_sv_undef)
 */
#define SEEN(y,c,i) 							\
  STMT_START {								\
	if (!y)									\
		return (SV *) 0;					\
	if (av_store(cxt->aseen, cxt->tagnum++, i ? (SV*)(y) : SvREFCNT_inc(y)) == 0) \
		return (SV *) 0;					\
	TRACEME(("aseen(#%d) = 0x%"UVxf" (refcnt=%d)", cxt->tagnum-1, \
		 PTR2UV(y), SvREFCNT(y)-1));		\
	if (c)									\
		BLESS((SV *) (y), c);				\
  } STMT_END

/*
 * Bless `s' in `p', via a temporary reference, required by sv_bless().
 */
#define BLESS(s,p) 							\
  STMT_START {								\
	SV *ref;								\
	HV *stash;								\
	TRACEME(("blessing 0x%"UVxf" in %s", PTR2UV(s), (p))); \
	stash = gv_stashpv((p), TRUE);			\
	ref = newRV_noinc(s);					\
	(void) sv_bless(ref, stash);			\
	SvRV(ref) = 0;							\
	SvREFCNT_dec(ref);						\
  } STMT_END
/*
 * sort (used in store_hash) - conditionally use qsort when
 * sortsv is not available ( <= 5.6.1 ).
 */

#if (PATCHLEVEL <= 6)

#if defined(USE_ITHREADS)

#define STORE_HASH_SORT \
        ENTER; { \
        PerlInterpreter *orig_perl = PERL_GET_CONTEXT; \
        SAVESPTR(orig_perl); \
        PERL_SET_CONTEXT(aTHX); \
        qsort((char *) AvARRAY(av), len, sizeof(SV *), sortcmp); \
        } LEAVE;

#else /* ! USE_ITHREADS */

#define STORE_HASH_SORT \
        qsort((char *) AvARRAY(av), len, sizeof(SV *), sortcmp);

#endif  /* USE_ITHREADS */

#else /* PATCHLEVEL > 6 */

#define STORE_HASH_SORT \
        sortsv(AvARRAY(av), len, Perl_sv_cmp);  

#endif /* PATCHLEVEL <= 6 */

static int store(pTHX_ stcxt_t *cxt, SV *sv);
static SV *retrieve(pTHX_ stcxt_t *cxt, char *cname);

/*
 * Dynamic dispatching table for SV store.
 */

static int store_ref(pTHX_ stcxt_t *cxt, SV *sv);
static int store_scalar(pTHX_ stcxt_t *cxt, SV *sv);
static int store_array(pTHX_ stcxt_t *cxt, AV *av);
static int store_hash(pTHX_ stcxt_t *cxt, HV *hv);
static int store_tied(pTHX_ stcxt_t *cxt, SV *sv);
static int store_tied_item(pTHX_ stcxt_t *cxt, SV *sv);
static int store_code(pTHX_ stcxt_t *cxt, CV *cv);
static int store_other(pTHX_ stcxt_t *cxt, SV *sv);
static int store_blessed(pTHX_ stcxt_t *cxt, SV *sv, int type, HV *pkg);

static int (*sv_store[])(pTHX_ stcxt_t *cxt, SV *sv) = {
	store_ref,										/* svis_REF */
	store_scalar,									/* svis_SCALAR */
	(int (*)(pTHX_ stcxt_t *cxt, SV *sv)) store_array,	/* svis_ARRAY */
	(int (*)(pTHX_ stcxt_t *cxt, SV *sv)) store_hash,		/* svis_HASH */
	store_tied,										/* svis_TIED */
	store_tied_item,								/* svis_TIED_ITEM */
	(int (*)(pTHX_ stcxt_t *cxt, SV *sv)) store_code,		/* svis_CODE */
	store_other,									/* svis_OTHER */
};

#define SV_STORE(x)	(*sv_store[x])

/*
 * Dynamic dispatching tables for SV retrieval.
 */

static SV *retrieve_lscalar(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_lutf8str(pTHX_ stcxt_t *cxt, char *cname);
static SV *old_retrieve_array(pTHX_ stcxt_t *cxt, char *cname);
static SV *old_retrieve_hash(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_ref(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_undef(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_integer(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_double(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_byte(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_netint(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_scalar(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_utf8str(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_tied_array(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_tied_hash(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_tied_scalar(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_other(pTHX_ stcxt_t *cxt, char *cname);

static SV *(*sv_old_retrieve[])(pTHX_ stcxt_t *cxt, char *cname) = {
	0,			/* SX_OBJECT -- entry unused dynamically */
	retrieve_lscalar,		/* SX_LSCALAR */
	old_retrieve_array,		/* SX_ARRAY -- for pre-0.6 binaries */
	old_retrieve_hash,		/* SX_HASH -- for pre-0.6 binaries */
	retrieve_ref,			/* SX_REF */
	retrieve_undef,			/* SX_UNDEF */
	retrieve_integer,		/* SX_INTEGER */
	retrieve_double,		/* SX_DOUBLE */
	retrieve_byte,			/* SX_BYTE */
	retrieve_netint,		/* SX_NETINT */
	retrieve_scalar,		/* SX_SCALAR */
	retrieve_tied_array,	/* SX_ARRAY */
	retrieve_tied_hash,		/* SX_HASH */
	retrieve_tied_scalar,	/* SX_SCALAR */
	retrieve_other,			/* SX_SV_UNDEF not supported */
	retrieve_other,			/* SX_SV_YES not supported */
	retrieve_other,			/* SX_SV_NO not supported */
	retrieve_other,			/* SX_BLESS not supported */
	retrieve_other,			/* SX_IX_BLESS not supported */
	retrieve_other,			/* SX_HOOK not supported */
	retrieve_other,			/* SX_OVERLOADED not supported */
	retrieve_other,			/* SX_TIED_KEY not supported */
	retrieve_other,			/* SX_TIED_IDX not supported */
	retrieve_other,			/* SX_UTF8STR not supported */
	retrieve_other,			/* SX_LUTF8STR not supported */
	retrieve_other,			/* SX_FLAG_HASH not supported */
	retrieve_other,			/* SX_CODE not supported */
	retrieve_other,			/* SX_ERROR */
};

static SV *retrieve_array(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_hash(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_sv_undef(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_sv_yes(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_sv_no(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_blessed(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_idx_blessed(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_hook(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_overloaded(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_tied_key(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_tied_idx(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_flag_hash(pTHX_ stcxt_t *cxt, char *cname);
static SV *retrieve_code(pTHX_ stcxt_t *cxt, char *cname);

static SV *(*sv_retrieve[])(pTHX_ stcxt_t *cxt, char *cname) = {
	0,			/* SX_OBJECT -- entry unused dynamically */
	retrieve_lscalar,		/* SX_LSCALAR */
	retrieve_array,			/* SX_ARRAY */
	retrieve_hash,			/* SX_HASH */
	retrieve_ref,			/* SX_REF */
	retrieve_undef,			/* SX_UNDEF */
	retrieve_integer,		/* SX_INTEGER */
	retrieve_double,		/* SX_DOUBLE */
	retrieve_byte,			/* SX_BYTE */
	retrieve_netint,		/* SX_NETINT */
	retrieve_scalar,		/* SX_SCALAR */
	retrieve_tied_array,	/* SX_ARRAY */
	retrieve_tied_hash,		/* SX_HASH */
	retrieve_tied_scalar,	/* SX_SCALAR */
	retrieve_sv_undef,		/* SX_SV_UNDEF */
	retrieve_sv_yes,		/* SX_SV_YES */
	retrieve_sv_no,			/* SX_SV_NO */
	retrieve_blessed,		/* SX_BLESS */
	retrieve_idx_blessed,	/* SX_IX_BLESS */
	retrieve_hook,			/* SX_HOOK */
	retrieve_overloaded,	/* SX_OVERLOAD */
	retrieve_tied_key,		/* SX_TIED_KEY */
	retrieve_tied_idx,		/* SX_TIED_IDX */
	retrieve_utf8str,		/* SX_UTF8STR  */
	retrieve_lutf8str,		/* SX_LUTF8STR */
	retrieve_flag_hash,		/* SX_HASH */
	retrieve_code,			/* SX_CODE */
	retrieve_other,			/* SX_ERROR */
};

#define RETRIEVE(c,x) (*(c)->retrieve_vtbl[(x) >= SX_ERROR ? SX_ERROR : (x)])

static SV *mbuf2sv(pTHX);

/***
 *** Context management.
 ***/

/*
 * init_perinterp
 *
 * Called once per "thread" (interpreter) to initialize some global context.
 */
static void init_perinterp(pTHX)
{
    INIT_STCXT;

    cxt->netorder = 0;		/* true if network order used */
    cxt->forgive_me = -1;	/* whether to be forgiving... */
}

/*
 * reset_context
 *
 * Called at the end of every context cleaning, to perform common reset
 * operations.
 */
static void reset_context(stcxt_t *cxt)
{
	cxt->entry = 0;
	cxt->s_dirty = 0;
	cxt->optype &= ~(ST_STORE|ST_RETRIEVE);		/* Leave ST_CLONE alone */
}

/*
 * init_store_context
 *
 * Initialize a new store context for real recursion.
 */
static void init_store_context(
        pTHX_
	stcxt_t *cxt,
	PerlIO *f,
	int optype,
	int network_order)
{
	TRACEME(("init_store_context"));

	cxt->netorder = network_order;
	cxt->forgive_me = -1;			/* Fetched from perl if needed */
	cxt->deparse = -1;				/* Idem */
	cxt->eval = NULL;				/* Idem */
	cxt->canonical = -1;			/* Idem */
	cxt->tagnum = -1;				/* Reset tag numbers */
	cxt->classnum = -1;				/* Reset class numbers */
	cxt->fio = f;					/* Where I/O are performed */
	cxt->optype = optype;			/* A store, or a deep clone */
	cxt->entry = 1;					/* No recursion yet */

	/*
	 * The `hseen' table is used to keep track of each SV stored and their
	 * associated tag numbers is special. It is "abused" because the
	 * values stored are not real SV, just integers cast to (SV *),
	 * which explains the freeing below.
	 *
	 * It is also one possible bottlneck to achieve good storing speed,
	 * so the "shared keys" optimization is turned off (unlikely to be
	 * of any use here), and the hash table is "pre-extended". Together,
	 * those optimizations increase the throughput by 12%.
	 */

	cxt->hseen = newHV();			/* Table where seen objects are stored */
	HvSHAREKEYS_off(cxt->hseen);

	/*
	 * The following does not work well with perl5.004_04, and causes
	 * a core dump later on, in a completely unrelated spot, which
	 * makes me think there is a memory corruption going on.
	 *
	 * Calling hv_ksplit(hseen, HBUCKETS) instead of manually hacking
	 * it below does not make any difference. It seems to work fine
	 * with perl5.004_68 but given the probable nature of the bug,
	 * that does not prove anything.
	 *
	 * It's a shame because increasing the amount of buckets raises
	 * store() throughput by 5%, but until I figure this out, I can't
	 * allow for this to go into production.
	 *
	 * It is reported fixed in 5.005, hence the #if.
	 */
#if PERL_VERSION >= 5
#define HBUCKETS	4096				/* Buckets for %hseen */
	HvMAX(cxt->hseen) = HBUCKETS - 1;	/* keys %hseen = $HBUCKETS; */
#endif

	/*
	 * The `hclass' hash uses the same settings as `hseen' above, but it is
	 * used to assign sequential tags (numbers) to class names for blessed
	 * objects.
	 *
	 * We turn the shared key optimization on.
	 */

	cxt->hclass = newHV();			/* Where seen classnames are stored */

#if PERL_VERSION >= 5
	HvMAX(cxt->hclass) = HBUCKETS - 1;	/* keys %hclass = $HBUCKETS; */
#endif

	/*
	 * The `hook' hash table is used to keep track of the references on
	 * the STORABLE_freeze hook routines, when found in some class name.
	 *
	 * It is assumed that the inheritance tree will not be changed during
	 * storing, and that no new method will be dynamically created by the
	 * hooks.
	 */

	cxt->hook = newHV();			/* Table where hooks are cached */

	/*
	 * The `hook_seen' array keeps track of all the SVs returned by
	 * STORABLE_freeze hooks for us to serialize, so that they are not
	 * reclaimed until the end of the serialization process.  Each SV is
	 * only stored once, the first time it is seen.
	 */

	cxt->hook_seen = newAV();		/* Lists SVs returned by STORABLE_freeze */
}

/*
 * clean_store_context
 *
 * Clean store context by
 */
static void clean_store_context(pTHX_ stcxt_t *cxt)
{
	HE *he;

	TRACEME(("clean_store_context"));

	ASSERT(cxt->optype & ST_STORE, ("was performing a store()"));

	/*
	 * Insert real values into hashes where we stored faked pointers.
	 */

	if (cxt->hseen) {
		hv_iterinit(cxt->hseen);
		while ((he = hv_iternext(cxt->hseen)))	/* Extra () for -Wall, grr.. */
			HeVAL(he) = &PL_sv_undef;
	}

	if (cxt->hclass) {
		hv_iterinit(cxt->hclass);
		while ((he = hv_iternext(cxt->hclass)))	/* Extra () for -Wall, grr.. */
			HeVAL(he) = &PL_sv_undef;
	}

	/*
	 * And now dispose of them...
	 *
	 * The surrounding if() protection has been added because there might be
	 * some cases where this routine is called more than once, during
	 * exceptionnal events.  This was reported by Marc Lehmann when Storable
	 * is executed from mod_perl, and the fix was suggested by him.
	 * 		-- RAM, 20/12/2000
	 */

	if (cxt->hseen) {
		HV *hseen = cxt->hseen;
		cxt->hseen = 0;
		hv_undef(hseen);
		sv_free((SV *) hseen);
	}

	if (cxt->hclass) {
		HV *hclass = cxt->hclass;
		cxt->hclass = 0;
		hv_undef(hclass);
		sv_free((SV *) hclass);
	}

	if (cxt->hook) {
		HV *hook = cxt->hook;
		cxt->hook = 0;
		hv_undef(hook);
		sv_free((SV *) hook);
	}

	if (cxt->hook_seen) {
		AV *hook_seen = cxt->hook_seen;
		cxt->hook_seen = 0;
		av_undef(hook_seen);
		sv_free((SV *) hook_seen);
	}

	cxt->forgive_me = -1;			/* Fetched from perl if needed */
	cxt->deparse = -1;				/* Idem */
	if (cxt->eval) {
	    SvREFCNT_dec(cxt->eval);
	}
	cxt->eval = NULL;				/* Idem */
	cxt->canonical = -1;			/* Idem */

	reset_context(cxt);
}

/*
 * init_retrieve_context
 *
 * Initialize a new retrieve context for real recursion.
 */
static void init_retrieve_context(pTHX_ stcxt_t *cxt, int optype, int is_tainted)
{
	TRACEME(("init_retrieve_context"));

	/*
	 * The hook hash table is used to keep track of the references on
	 * the STORABLE_thaw hook routines, when found in some class name.
	 *
	 * It is assumed that the inheritance tree will not be changed during
	 * storing, and that no new method will be dynamically created by the
	 * hooks.
	 */

	cxt->hook  = newHV();			/* Caches STORABLE_thaw */

	/*
	 * If retrieving an old binary version, the cxt->retrieve_vtbl variable
	 * was set to sv_old_retrieve. We'll need a hash table to keep track of
	 * the correspondance between the tags and the tag number used by the
	 * new retrieve routines.
	 */

	cxt->hseen = (((void*)cxt->retrieve_vtbl == (void*)sv_old_retrieve)
		      ? newHV() : 0);

	cxt->aseen = newAV();			/* Where retrieved objects are kept */
	cxt->where_is_undef = -1;		/* Special case for PL_sv_undef */
	cxt->aclass = newAV();			/* Where seen classnames are kept */
	cxt->tagnum = 0;				/* Have to count objects... */
	cxt->classnum = 0;				/* ...and class names as well */
	cxt->optype = optype;
	cxt->s_tainted = is_tainted;
	cxt->entry = 1;					/* No recursion yet */
#ifndef HAS_RESTRICTED_HASHES
        cxt->derestrict = -1;		/* Fetched from perl if needed */
#endif
#ifndef HAS_UTF8_ALL
        cxt->use_bytes = -1;		/* Fetched from perl if needed */
#endif
        cxt->accept_future_minor = -1;	/* Fetched from perl if needed */
}

/*
 * clean_retrieve_context
 *
 * Clean retrieve context by
 */
static void clean_retrieve_context(pTHX_ stcxt_t *cxt)
{
	TRACEME(("clean_retrieve_context"));

	ASSERT(cxt->optype & ST_RETRIEVE, ("was performing a retrieve()"));

	if (cxt->aseen) {
		AV *aseen = cxt->aseen;
		cxt->aseen = 0;
		av_undef(aseen);
		sv_free((SV *) aseen);
	}
	cxt->where_is_undef = -1;

	if (cxt->aclass) {
		AV *aclass = cxt->aclass;
		cxt->aclass = 0;
		av_undef(aclass);
		sv_free((SV *) aclass);
	}

	if (cxt->hook) {
		HV *hook = cxt->hook;
		cxt->hook = 0;
		hv_undef(hook);
		sv_free((SV *) hook);
	}

	if (cxt->hseen) {
		HV *hseen = cxt->hseen;
		cxt->hseen = 0;
		hv_undef(hseen);
		sv_free((SV *) hseen);		/* optional HV, for backward compat. */
	}

#ifndef HAS_RESTRICTED_HASHES
        cxt->derestrict = -1;		/* Fetched from perl if needed */
#endif
#ifndef HAS_UTF8_ALL
        cxt->use_bytes = -1;		/* Fetched from perl if needed */
#endif
        cxt->accept_future_minor = -1;	/* Fetched from perl if needed */

	reset_context(cxt);
}

/*
 * clean_context
 *
 * A workaround for the CROAK bug: cleanup the last context.
 */
static void clean_context(pTHX_ stcxt_t *cxt)
{
	TRACEME(("clean_context"));

	ASSERT(cxt->s_dirty, ("dirty context"));

	if (cxt->membuf_ro)
		MBUF_RESTORE();

	ASSERT(!cxt->membuf_ro, ("mbase is not read-only"));

	if (cxt->optype & ST_RETRIEVE)
		clean_retrieve_context(aTHX_ cxt);
	else if (cxt->optype & ST_STORE)
		clean_store_context(aTHX_ cxt);
	else
		reset_context(cxt);

	ASSERT(!cxt->s_dirty, ("context is clean"));
	ASSERT(cxt->entry == 0, ("context is reset"));
}

/*
 * allocate_context
 *
 * Allocate a new context and push it on top of the parent one.
 * This new context is made globally visible via SET_STCXT().
 */
static stcxt_t *allocate_context(pTHX_ stcxt_t *parent_cxt)
{
	stcxt_t *cxt;

	TRACEME(("allocate_context"));

	ASSERT(!parent_cxt->s_dirty, ("parent context clean"));

	NEW_STORABLE_CXT_OBJ(cxt);
	cxt->prev = parent_cxt->my_sv;
	SET_STCXT(cxt);

	ASSERT(!cxt->s_dirty, ("clean context"));

	return cxt;
}

/*
 * free_context
 *
 * Free current context, which cannot be the "root" one.
 * Make the context underneath globally visible via SET_STCXT().
 */
static void free_context(pTHX_ stcxt_t *cxt)
{
	stcxt_t *prev = (stcxt_t *)(cxt->prev ? SvPVX(SvRV(cxt->prev)) : 0);

	TRACEME(("free_context"));

	ASSERT(!cxt->s_dirty, ("clean context"));
	ASSERT(prev, ("not freeing root context"));

	SvREFCNT_dec(cxt->my_sv);
	SET_STCXT(prev);

	ASSERT(cxt, ("context not void"));
}

/***
 *** Predicates.
 ***/

/*
 * is_storing
 *
 * Tells whether we're in the middle of a store operation.
 */
int is_storing(pTHX)
{
	dSTCXT;

	return cxt->entry && (cxt->optype & ST_STORE);
}

/*
 * is_retrieving
 *
 * Tells whether we're in the middle of a retrieve operation.
 */
int is_retrieving(pTHX)
{
	dSTCXT;

	return cxt->entry && (cxt->optype & ST_RETRIEVE);
}

/*
 * last_op_in_netorder
 *
 * Returns whether last operation was made using network order.
 *
 * This is typically out-of-band information that might prove useful
 * to people wishing to convert native to network order data when used.
 */
int last_op_in_netorder(pTHX)
{
	dSTCXT;

	return cxt->netorder;
}

/***
 *** Hook lookup and calling routines.
 ***/

/*
 * pkg_fetchmeth
 *
 * A wrapper on gv_fetchmethod_autoload() which caches results.
 *
 * Returns the routine reference as an SV*, or null if neither the package
 * nor its ancestors know about the method.
 */
static SV *pkg_fetchmeth(
        pTHX_
	HV *cache,
	HV *pkg,
	char *method)
{
	GV *gv;
	SV *sv;

	/*
	 * The following code is the same as the one performed by UNIVERSAL::can
	 * in the Perl core.
	 */

	gv = gv_fetchmethod_autoload(pkg, method, FALSE);
	if (gv && isGV(gv)) {
		sv = newRV((SV*) GvCV(gv));
		TRACEME(("%s->%s: 0x%"UVxf, HvNAME(pkg), method, PTR2UV(sv)));
	} else {
		sv = newSVsv(&PL_sv_undef);
		TRACEME(("%s->%s: not found", HvNAME(pkg), method));
	}

	/*
	 * Cache the result, ignoring failure: if we can't store the value,
	 * it just won't be cached.
	 */

	(void) hv_store(cache, HvNAME(pkg), strlen(HvNAME(pkg)), sv, 0);

	return SvOK(sv) ? sv : (SV *) 0;
}

/*
 * pkg_hide
 *
 * Force cached value to be undef: hook ignored even if present.
 */
static void pkg_hide(
        pTHX_
	HV *cache,
	HV *pkg,
	char *method)
{
	(void) hv_store(cache,
		HvNAME(pkg), strlen(HvNAME(pkg)), newSVsv(&PL_sv_undef), 0);
}

/*
 * pkg_uncache
 *
 * Discard cached value: a whole fetch loop will be retried at next lookup.
 */
static void pkg_uncache(
        pTHX_
	HV *cache,
	HV *pkg,
	char *method)
{
	(void) hv_delete(cache, HvNAME(pkg), strlen(HvNAME(pkg)), G_DISCARD);
}

/*
 * pkg_can
 *
 * Our own "UNIVERSAL::can", which caches results.
 *
 * Returns the routine reference as an SV*, or null if the object does not
 * know about the method.
 */
static SV *pkg_can(
        pTHX_
	HV *cache,
	HV *pkg,
	char *method)
{
	SV **svh;
	SV *sv;

	TRACEME(("pkg_can for %s->%s", HvNAME(pkg), method));

	/*
	 * Look into the cache to see whether we already have determined
	 * where the routine was, if any.
	 *
	 * NOTA BENE: we don't use `method' at all in our lookup, since we know
	 * that only one hook (i.e. always the same) is cached in a given cache.
	 */

	svh = hv_fetch(cache, HvNAME(pkg), strlen(HvNAME(pkg)), FALSE);
	if (svh) {
		sv = *svh;
		if (!SvOK(sv)) {
			TRACEME(("cached %s->%s: not found", HvNAME(pkg), method));
			return (SV *) 0;
		} else {
			TRACEME(("cached %s->%s: 0x%"UVxf,
				HvNAME(pkg), method, PTR2UV(sv)));
			return sv;
		}
	}

	TRACEME(("not cached yet"));
	return pkg_fetchmeth(aTHX_ cache, pkg, method);		/* Fetch and cache */
}

/*
 * scalar_call
 *
 * Call routine as obj->hook(av) in scalar context.
 * Propagates the single returned value if not called in void context.
 */
static SV *scalar_call(
        pTHX_
	SV *obj,
	SV *hook,
	int cloning,
	AV *av,
	I32 flags)
{
	dSP;
	int count;
	SV *sv = 0;

	TRACEME(("scalar_call (cloning=%d)", cloning));

	ENTER;
	SAVETMPS;

	PUSHMARK(sp);
	XPUSHs(obj);
	XPUSHs(sv_2mortal(newSViv(cloning)));		/* Cloning flag */
	if (av) {
		SV **ary = AvARRAY(av);
		int cnt = AvFILLp(av) + 1;
		int i;
		XPUSHs(ary[0]);							/* Frozen string */
		for (i = 1; i < cnt; i++) {
			TRACEME(("pushing arg #%d (0x%"UVxf")...",
				 i, PTR2UV(ary[i])));
			XPUSHs(sv_2mortal(newRV(ary[i])));
		}
	}
	PUTBACK;

	TRACEME(("calling..."));
	count = perl_call_sv(hook, flags);		/* Go back to Perl code */
	TRACEME(("count = %d", count));

	SPAGAIN;

	if (count) {
		sv = POPs;
		SvREFCNT_inc(sv);		/* We're returning it, must stay alive! */
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return sv;
}

/*
 * array_call
 *
 * Call routine obj->hook(cloning) in list context.
 * Returns the list of returned values in an array.
 */
static AV *array_call(
        pTHX_
	SV *obj,
	SV *hook,
	int cloning)
{
	dSP;
	int count;
	AV *av;
	int i;

	TRACEME(("array_call (cloning=%d)", cloning));

	ENTER;
	SAVETMPS;

	PUSHMARK(sp);
	XPUSHs(obj);								/* Target object */
	XPUSHs(sv_2mortal(newSViv(cloning)));		/* Cloning flag */
	PUTBACK;

	count = perl_call_sv(hook, G_ARRAY);		/* Go back to Perl code */

	SPAGAIN;

	av = newAV();
	for (i = count - 1; i >= 0; i--) {
		SV *sv = POPs;
		av_store(av, i, SvREFCNT_inc(sv));
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return av;
}

/*
 * known_class
 *
 * Lookup the class name in the `hclass' table and either assign it a new ID
 * or return the existing one, by filling in `classnum'.
 *
 * Return true if the class was known, false if the ID was just generated.
 */
static int known_class(
        pTHX_
	stcxt_t *cxt,
	char *name,		/* Class name */
	int len,		/* Name length */
	I32 *classnum)
{
	SV **svh;
	HV *hclass = cxt->hclass;

	TRACEME(("known_class (%s)", name));

	/*
	 * Recall that we don't store pointers in this hash table, but tags.
	 * Therefore, we need LOW_32BITS() to extract the relevant parts.
	 */

	svh = hv_fetch(hclass, name, len, FALSE);
	if (svh) {
		*classnum = LOW_32BITS(*svh);
		return TRUE;
	}

	/*
	 * Unknown classname, we need to record it.
	 */

	cxt->classnum++;
	if (!hv_store(hclass, name, len, INT2PTR(SV*, cxt->classnum), 0))
		CROAK(("Unable to record new classname"));

	*classnum = cxt->classnum;
	return FALSE;
}

/***
 *** Sepcific store routines.
 ***/

/*
 * store_ref
 *
 * Store a reference.
 * Layout is SX_REF <object> or SX_OVERLOAD <object>.
 */
static int store_ref(pTHX_ stcxt_t *cxt, SV *sv)
{
	TRACEME(("store_ref (0x%"UVxf")", PTR2UV(sv)));

	/*
	 * Follow reference, and check if target is overloaded.
	 */

	sv = SvRV(sv);

	if (SvOBJECT(sv)) {
		HV *stash = (HV *) SvSTASH(sv);
		if (stash && Gv_AMG(stash)) {
			TRACEME(("ref (0x%"UVxf") is overloaded", PTR2UV(sv)));
			PUTMARK(SX_OVERLOAD);
		} else
			PUTMARK(SX_REF);
	} else
		PUTMARK(SX_REF);

	return store(aTHX_ cxt, sv);
}

/*
 * store_scalar
 *
 * Store a scalar.
 *
 * Layout is SX_LSCALAR <length> <data>, SX_SCALAR <length> <data> or SX_UNDEF.
 * The <data> section is omitted if <length> is 0.
 *
 * If integer or double, the layout is SX_INTEGER <data> or SX_DOUBLE <data>.
 * Small integers (within [-127, +127]) are stored as SX_BYTE <byte>.
 */
static int store_scalar(pTHX_ stcxt_t *cxt, SV *sv)
{
	IV iv;
	char *pv;
	STRLEN len;
	U32 flags = SvFLAGS(sv);			/* "cc -O" may put it in register */

	TRACEME(("store_scalar (0x%"UVxf")", PTR2UV(sv)));

	/*
	 * For efficiency, break the SV encapsulation by peaking at the flags
	 * directly without using the Perl macros to avoid dereferencing
	 * sv->sv_flags each time we wish to check the flags.
	 */

	if (!(flags & SVf_OK)) {			/* !SvOK(sv) */
		if (sv == &PL_sv_undef) {
			TRACEME(("immortal undef"));
			PUTMARK(SX_SV_UNDEF);
		} else {
			TRACEME(("undef at 0x%"UVxf, PTR2UV(sv)));
			PUTMARK(SX_UNDEF);
		}
		return 0;
	}

	/*
	 * Always store the string representation of a scalar if it exists.
	 * Gisle Aas provided me with this test case, better than a long speach:
	 *
	 *  perl -MDevel::Peek -le '$a="abc"; $a+0; Dump($a)'
	 *  SV = PVNV(0x80c8520)
	 *       REFCNT = 1
	 *       FLAGS = (NOK,POK,pNOK,pPOK)
	 *       IV = 0
	 *       NV = 0
	 *       PV = 0x80c83d0 "abc"\0
	 *       CUR = 3
	 *       LEN = 4
	 *
	 * Write SX_SCALAR, length, followed by the actual data.
	 *
	 * Otherwise, write an SX_BYTE, SX_INTEGER or an SX_DOUBLE as
	 * appropriate, followed by the actual (binary) data. A double
	 * is written as a string if network order, for portability.
	 *
	 * NOTE: instead of using SvNOK(sv), we test for SvNOKp(sv).
	 * The reason is that when the scalar value is tainted, the SvNOK(sv)
	 * value is false.
	 *
	 * The test for a read-only scalar with both POK and NOK set is meant
	 * to quickly detect &PL_sv_yes and &PL_sv_no without having to pay the
	 * address comparison for each scalar we store.
	 */

#define SV_MAYBE_IMMORTAL (SVf_READONLY|SVf_POK|SVf_NOK)

	if ((flags & SV_MAYBE_IMMORTAL) == SV_MAYBE_IMMORTAL) {
		if (sv == &PL_sv_yes) {
			TRACEME(("immortal yes"));
			PUTMARK(SX_SV_YES);
		} else if (sv == &PL_sv_no) {
			TRACEME(("immortal no"));
			PUTMARK(SX_SV_NO);
		} else {
			pv = SvPV(sv, len);			/* We know it's SvPOK */
			goto string;				/* Share code below */
		}
	} else if (flags & SVf_POK) {
            /* public string - go direct to string read.  */
            goto string_readlen;
        } else if (
#if (PATCHLEVEL <= 6)
            /* For 5.6 and earlier NV flag trumps IV flag, so only use integer
               direct if NV flag is off.  */
            (flags & (SVf_NOK | SVf_IOK)) == SVf_IOK
#else
            /* 5.7 rules are that if IV public flag is set, IV value is as
               good, if not better, than NV value.  */
            flags & SVf_IOK
#endif
            ) {
            iv = SvIV(sv);
            /*
             * Will come here from below with iv set if double is an integer.
             */
          integer:

            /* Sorry. This isn't in 5.005_56 (IIRC) or earlier.  */
#ifdef SVf_IVisUV
            /* Need to do this out here, else 0xFFFFFFFF becomes iv of -1
             * (for example) and that ends up in the optimised small integer
             * case. 
             */
            if ((flags & SVf_IVisUV) && SvUV(sv) > IV_MAX) {
                TRACEME(("large unsigned integer as string, value = %"UVuf, SvUV(sv)));
                goto string_readlen;
            }
#endif
            /*
             * Optimize small integers into a single byte, otherwise store as
             * a real integer (converted into network order if they asked).
             */

            if (iv >= -128 && iv <= 127) {
                unsigned char siv = (unsigned char) (iv + 128);	/* [0,255] */
                PUTMARK(SX_BYTE);
                PUTMARK(siv);
                TRACEME(("small integer stored as %d", siv));
            } else if (cxt->netorder) {
#ifndef HAS_HTONL
                TRACEME(("no htonl, fall back to string for integer"));
                goto string_readlen;
#else
                I32 niv;


#if IVSIZE > 4
                if (
#ifdef SVf_IVisUV
                    /* Sorry. This isn't in 5.005_56 (IIRC) or earlier.  */
                    ((flags & SVf_IVisUV) && SvUV(sv) > 0x7FFFFFFF) ||
#endif
                    (iv > 0x7FFFFFFF) || (iv < -0x80000000)) {
                    /* Bigger than 32 bits.  */
                    TRACEME(("large network order integer as string, value = %"IVdf, iv));
                    goto string_readlen;
                }
#endif

                niv = (I32) htonl((I32) iv);
                TRACEME(("using network order"));
                PUTMARK(SX_NETINT);
                WRITE_I32(niv);
#endif
            } else {
                PUTMARK(SX_INTEGER);
                WRITE(&iv, sizeof(iv));
            }
            
            TRACEME(("ok (integer 0x%"UVxf", value = %"IVdf")", PTR2UV(sv), iv));
	} else if (flags & SVf_NOK) {
            NV nv;
#if (PATCHLEVEL <= 6)
            nv = SvNV(sv);
            /*
             * Watch for number being an integer in disguise.
             */
            if (nv == (NV) (iv = I_V(nv))) {
                TRACEME(("double %"NVff" is actually integer %"IVdf, nv, iv));
                goto integer;		/* Share code above */
            }
#else

            SvIV_please(sv);
	    if (SvIOK_notUV(sv)) {
                iv = SvIV(sv);
                goto integer;		/* Share code above */
            }
            nv = SvNV(sv);
#endif

            if (cxt->netorder) {
                TRACEME(("double %"NVff" stored as string", nv));
                goto string_readlen;		/* Share code below */
            }

            PUTMARK(SX_DOUBLE);
            WRITE(&nv, sizeof(nv));

            TRACEME(("ok (double 0x%"UVxf", value = %"NVff")", PTR2UV(sv), nv));

	} else if (flags & (SVp_POK | SVp_NOK | SVp_IOK)) {
            I32 wlen; /* For 64-bit machines */

          string_readlen:
            pv = SvPV(sv, len);

            /*
             * Will come here from above  if it was readonly, POK and NOK but
             * neither &PL_sv_yes nor &PL_sv_no.
             */
          string:

            wlen = (I32) len; /* WLEN via STORE_SCALAR expects I32 */
            if (SvUTF8 (sv))
                STORE_UTF8STR(pv, wlen);
            else
                STORE_SCALAR(pv, wlen);
            TRACEME(("ok (scalar 0x%"UVxf" '%s', length = %"IVdf")",
                     PTR2UV(sv), SvPVX(sv), (IV)len));
	} else
            CROAK(("Can't determine type of %s(0x%"UVxf")",
                   sv_reftype(sv, FALSE),
                   PTR2UV(sv)));
        return 0;		/* Ok, no recursion on scalars */
}

/*
 * store_array
 *
 * Store an array.
 *
 * Layout is SX_ARRAY <size> followed by each item, in increading index order.
 * Each item is stored as <object>.
 */
static int store_array(pTHX_ stcxt_t *cxt, AV *av)
{
	SV **sav;
	I32 len = av_len(av) + 1;
	I32 i;
	int ret;

	TRACEME(("store_array (0x%"UVxf")", PTR2UV(av)));

	/* 
	 * Signal array by emitting SX_ARRAY, followed by the array length.
	 */

	PUTMARK(SX_ARRAY);
	WLEN(len);
	TRACEME(("size = %d", len));

	/*
	 * Now store each item recursively.
	 */

	for (i = 0; i < len; i++) {
		sav = av_fetch(av, i, 0);
		if (!sav) {
			TRACEME(("(#%d) undef item", i));
			STORE_SV_UNDEF();
			continue;
		}
		TRACEME(("(#%d) item", i));
		if ((ret = store(aTHX_ cxt, *sav)))	/* Extra () for -Wall, grr... */
			return ret;
	}

	TRACEME(("ok (array)"));

	return 0;
}


#if (PATCHLEVEL <= 6)

/*
 * sortcmp
 *
 * Sort two SVs
 * Borrowed from perl source file pp_ctl.c, where it is used by pp_sort.
 */
static int
sortcmp(const void *a, const void *b)
{
#if defined(USE_ITHREADS)
        dTHX;
#endif /* USE_ITHREADS */
        return sv_cmp(*(SV * const *) a, *(SV * const *) b);
}

#endif /* PATCHLEVEL <= 6 */

/*
 * store_hash
 *
 * Store a hash table.
 *
 * For a "normal" hash (not restricted, no utf8 keys):
 *
 * Layout is SX_HASH <size> followed by each key/value pair, in random order.
 * Values are stored as <object>.
 * Keys are stored as <length> <data>, the <data> section being omitted
 * if length is 0.
 *
 * For a "fancy" hash (restricted or utf8 keys):
 *
 * Layout is SX_FLAG_HASH <size> <hash flags> followed by each key/value pair,
 * in random order.
 * Values are stored as <object>.
 * Keys are stored as <flags> <length> <data>, the <data> section being omitted
 * if length is 0.
 * Currently the only hash flag is "restriced"
 * Key flags are as for hv.h
 */
static int store_hash(pTHX_ stcxt_t *cxt, HV *hv)
{
	I32 len = 
#ifdef HAS_RESTRICTED_HASHES
            HvTOTALKEYS(hv);
#else
            HvKEYS(hv);
#endif
	I32 i;
	int ret = 0;
	I32 riter;
	HE *eiter;
        int flagged_hash = ((SvREADONLY(hv)
#ifdef HAS_HASH_KEY_FLAGS
                             || HvHASKFLAGS(hv)
#endif
                                ) ? 1 : 0);
        unsigned char hash_flags = (SvREADONLY(hv) ? SHV_RESTRICTED : 0);

        if (flagged_hash) {
            /* needs int cast for C++ compilers, doesn't it?  */
            TRACEME(("store_hash (0x%"UVxf") (flags %x)", PTR2UV(hv),
                     (int) hash_flags));
        } else {
            TRACEME(("store_hash (0x%"UVxf")", PTR2UV(hv)));
        }

	/* 
	 * Signal hash by emitting SX_HASH, followed by the table length.
	 */

        if (flagged_hash) {
            PUTMARK(SX_FLAG_HASH);
            PUTMARK(hash_flags);
        } else {
            PUTMARK(SX_HASH);
        }
	WLEN(len);
	TRACEME(("size = %d", len));

	/*
	 * Save possible iteration state via each() on that table.
	 */

	riter = HvRITER(hv);
	eiter = HvEITER(hv);
	hv_iterinit(hv);

	/*
	 * Now store each item recursively.
	 *
     * If canonical is defined to some true value then store each
     * key/value pair in sorted order otherwise the order is random.
	 * Canonical order is irrelevant when a deep clone operation is performed.
	 *
	 * Fetch the value from perl only once per store() operation, and only
	 * when needed.
	 */

	if (
		!(cxt->optype & ST_CLONE) && (cxt->canonical == 1 ||
		(cxt->canonical < 0 && (cxt->canonical =
			(SvTRUE(perl_get_sv("Storable::canonical", TRUE)) ? 1 : 0))))
	) {
		/*
		 * Storing in order, sorted by key.
		 * Run through the hash, building up an array of keys in a
		 * mortal array, sort the array and then run through the
		 * array.  
		 */

		AV *av = newAV();

                /*av_extend (av, len);*/

		TRACEME(("using canonical order"));

		for (i = 0; i < len; i++) {
#ifdef HAS_RESTRICTED_HASHES
			HE *he = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS);
#else
			HE *he = hv_iternext(hv);
#endif
			SV *key = hv_iterkeysv(he);
			av_store(av, AvFILLp(av)+1, key);	/* av_push(), really */
		}
			
		STORE_HASH_SORT;

		for (i = 0; i < len; i++) {
#ifdef HAS_RESTRICTED_HASHES
			int placeholders = HvPLACEHOLDERS(hv);
#endif
                        unsigned char flags = 0;
			char *keyval;
			STRLEN keylen_tmp;
                        I32 keylen;
			SV *key = av_shift(av);
			/* This will fail if key is a placeholder.
			   Track how many placeholders we have, and error if we
			   "see" too many.  */
			HE *he  = hv_fetch_ent(hv, key, 0, 0);
			SV *val;

			if (he) {
				if (!(val =  HeVAL(he))) {
					/* Internal error, not I/O error */
					return 1;
				}
			} else {
#ifdef HAS_RESTRICTED_HASHES
				/* Should be a placeholder.  */
				if (placeholders-- < 0) {
					/* This should not happen - number of
					   retrieves should be identical to
					   number of placeholders.  */
			  		return 1;
				}
				/* Value is never needed, and PL_sv_undef is
				   more space efficient to store.  */
				val = &PL_sv_undef;
				ASSERT (flags == 0,
					("Flags not 0 but %d", flags));
				flags = SHV_K_PLACEHOLDER;
#else
				return 1;
#endif
			}
			
			/*
			 * Store value first.
			 */
			
			TRACEME(("(#%d) value 0x%"UVxf, i, PTR2UV(val)));

			if ((ret = store(aTHX_ cxt, val)))	/* Extra () for -Wall, grr... */
				goto out;

			/*
			 * Write key string.
			 * Keys are written after values to make sure retrieval
			 * can be optimal in terms of memory usage, where keys are
			 * read into a fixed unique buffer called kbuf.
			 * See retrieve_hash() for details.
			 */
			 
                        /* Implementation of restricted hashes isn't nicely
                           abstracted:  */
			if ((hash_flags & SHV_RESTRICTED) && SvREADONLY(val)) {
				flags |= SHV_K_LOCKED;
			}

			keyval = SvPV(key, keylen_tmp);
                        keylen = keylen_tmp;
#ifdef HAS_UTF8_HASHES
                        /* If you build without optimisation on pre 5.6
                           then nothing spots that SvUTF8(key) is always 0,
                           so the block isn't optimised away, at which point
                           the linker dislikes the reference to
                           bytes_from_utf8.  */
			if (SvUTF8(key)) {
                            const char *keysave = keyval;
                            bool is_utf8 = TRUE;

                            /* Just casting the &klen to (STRLEN) won't work
                               well if STRLEN and I32 are of different widths.
                               --jhi */
                            keyval = (char*)bytes_from_utf8((U8*)keyval,
                                                            &keylen_tmp,
                                                            &is_utf8);

                            /* If we were able to downgrade here, then than
                               means that we have  a key which only had chars
                               0-255, but was utf8 encoded.  */

                            if (keyval != keysave) {
                                keylen = keylen_tmp;
                                flags |= SHV_K_WASUTF8;
                            } else {
                                /* keylen_tmp can't have changed, so no need
                                   to assign back to keylen.  */
                                flags |= SHV_K_UTF8;
                            }
                        }
#endif

                        if (flagged_hash) {
                            PUTMARK(flags);
                            TRACEME(("(#%d) key '%s' flags %x %u", i, keyval, flags, *keyval));
                        } else {
                            /* This is a workaround for a bug in 5.8.0
                               that causes the HEK_WASUTF8 flag to be
                               set on an HEK without the hash being
                               marked as having key flags. We just
                               cross our fingers and drop the flag.
                               AMS 20030901 */
                            assert (flags == 0 || flags == SHV_K_WASUTF8);
                            TRACEME(("(#%d) key '%s'", i, keyval));
                        }
			WLEN(keylen);
			if (keylen)
				WRITE(keyval, keylen);
                        if (flags & SHV_K_WASUTF8)
                            Safefree (keyval);
		}

		/* 
		 * Free up the temporary array
		 */

		av_undef(av);
		sv_free((SV *) av);

	} else {

		/*
		 * Storing in "random" order (in the order the keys are stored
		 * within the hash).  This is the default and will be faster!
		 */
  
		for (i = 0; i < len; i++) {
			char *key;
			I32 len;
                        unsigned char flags;
#ifdef HV_ITERNEXT_WANTPLACEHOLDERS
                        HE *he = hv_iternext_flags(hv, HV_ITERNEXT_WANTPLACEHOLDERS);
#else
                        HE *he = hv_iternext(hv);
#endif
			SV *val = (he ? hv_iterval(hv, he) : 0);
                        SV *key_sv = NULL;
                        HEK *hek;

			if (val == 0)
				return 1;		/* Internal error, not I/O error */

                        /* Implementation of restricted hashes isn't nicely
                           abstracted:  */
                        flags
                            = (((hash_flags & SHV_RESTRICTED)
                                && SvREADONLY(val))
                                             ? SHV_K_LOCKED : 0);

                        if (val == &PL_sv_placeholder) {
                            flags |= SHV_K_PLACEHOLDER;
			    val = &PL_sv_undef;
			}

			/*
			 * Store value first.
			 */

			TRACEME(("(#%d) value 0x%"UVxf, i, PTR2UV(val)));

			if ((ret = store(aTHX_ cxt, val)))	/* Extra () for -Wall, grr... */
				goto out;


                        hek = HeKEY_hek(he);
                        len = HEK_LEN(hek);
                        if (len == HEf_SVKEY) {
                            /* This is somewhat sick, but the internal APIs are
                             * such that XS code could put one of these in in
                             * a regular hash.
                             * Maybe we should be capable of storing one if
                             * found.
                             */
                            key_sv = HeKEY_sv(he);
                            flags |= SHV_K_ISSV;
                        } else {
                            /* Regular string key. */
#ifdef HAS_HASH_KEY_FLAGS
                            if (HEK_UTF8(hek))
                                flags |= SHV_K_UTF8;
                            if (HEK_WASUTF8(hek))
                                flags |= SHV_K_WASUTF8;
#endif
                            key = HEK_KEY(hek);
                        }
			/*
			 * Write key string.
			 * Keys are written after values to make sure retrieval
			 * can be optimal in terms of memory usage, where keys are
			 * read into a fixed unique buffer called kbuf.
			 * See retrieve_hash() for details.
			 */

                        if (flagged_hash) {
                            PUTMARK(flags);
                            TRACEME(("(#%d) key '%s' flags %x", i, key, flags));
                        } else {
                            /* This is a workaround for a bug in 5.8.0
                               that causes the HEK_WASUTF8 flag to be
                               set on an HEK without the hash being
                               marked as having key flags. We just
                               cross our fingers and drop the flag.
                               AMS 20030901 */
                            assert (flags == 0 || flags == SHV_K_WASUTF8);
                            TRACEME(("(#%d) key '%s'", i, key));
                        }
                        if (flags & SHV_K_ISSV) {
                            store(aTHX_ cxt, key_sv);
                        } else {
                            WLEN(len);
                            if (len)
				WRITE(key, len);
                        }
		}
    }

	TRACEME(("ok (hash 0x%"UVxf")", PTR2UV(hv)));

out:
	HvRITER(hv) = riter;		/* Restore hash iterator state */
	HvEITER(hv) = eiter;

	return ret;
}

/*
 * store_code
 *
 * Store a code reference.
 *
 * Layout is SX_CODE <length> followed by a scalar containing the perl
 * source code of the code reference.
 */
static int store_code(pTHX_ stcxt_t *cxt, CV *cv)
{
#if PERL_VERSION < 6
    /*
	 * retrieve_code does not work with perl 5.005 or less
	 */
	return store_other(aTHX_ cxt, (SV*)cv);
#else
	dSP;
	I32 len;
	int count, reallen;
	SV *text, *bdeparse;

	TRACEME(("store_code (0x%"UVxf")", PTR2UV(cv)));

	if (
		cxt->deparse == 0 ||
		(cxt->deparse < 0 && !(cxt->deparse =
			SvTRUE(perl_get_sv("Storable::Deparse", TRUE)) ? 1 : 0))
	) {
		return store_other(aTHX_ cxt, (SV*)cv);
	}

	/*
	 * Require B::Deparse. At least B::Deparse 0.61 is needed for
	 * blessed code references.
	 */
	/* Ownership of both SVs is passed to load_module, which frees them. */
	load_module(PERL_LOADMOD_NOIMPORT, newSVpvn("B::Deparse",10), newSVnv(0.61));

	ENTER;
	SAVETMPS;

	/*
	 * create the B::Deparse object
	 */

	PUSHMARK(sp);
	XPUSHs(sv_2mortal(newSVpvn("B::Deparse",10)));
	PUTBACK;
	count = call_method("new", G_SCALAR);
	SPAGAIN;
	if (count != 1)
		CROAK(("Unexpected return value from B::Deparse::new\n"));
	bdeparse = POPs;

	/*
	 * call the coderef2text method
	 */

	PUSHMARK(sp);
	XPUSHs(bdeparse); /* XXX is this already mortal? */
	XPUSHs(sv_2mortal(newRV_inc((SV*)cv)));
	PUTBACK;
	count = call_method("coderef2text", G_SCALAR);
	SPAGAIN;
	if (count != 1)
		CROAK(("Unexpected return value from B::Deparse::coderef2text\n"));

	text = POPs;
	len = SvLEN(text);
	reallen = strlen(SvPV_nolen(text));

	/*
	 * Empty code references or XS functions are deparsed as
	 * "(prototype) ;" or ";".
	 */

	if (len == 0 || *(SvPV_nolen(text)+reallen-1) == ';') {
	    CROAK(("The result of B::Deparse::coderef2text was empty - maybe you're trying to serialize an XS function?\n"));
	}

	/* 
	 * Signal code by emitting SX_CODE.
	 */

	PUTMARK(SX_CODE);
	cxt->tagnum++;   /* necessary, as SX_CODE is a SEEN() candidate */
	TRACEME(("size = %d", len));
	TRACEME(("code = %s", SvPV_nolen(text)));

	/*
	 * Now store the source code.
	 */

	STORE_SCALAR(SvPV_nolen(text), len);

	FREETMPS;
	LEAVE;

	TRACEME(("ok (code)"));

	return 0;
#endif
}

/*
 * store_tied
 *
 * When storing a tied object (be it a tied scalar, array or hash), we lay out
 * a special mark, followed by the underlying tied object. For instance, when
 * dealing with a tied hash, we store SX_TIED_HASH <hash object>, where
 * <hash object> stands for the serialization of the tied hash.
 */
static int store_tied(pTHX_ stcxt_t *cxt, SV *sv)
{
	MAGIC *mg;
	SV *obj = NULL;
	int ret = 0;
	int svt = SvTYPE(sv);
	char mtype = 'P';

	TRACEME(("store_tied (0x%"UVxf")", PTR2UV(sv)));

	/*
	 * We have a small run-time penalty here because we chose to factorise
	 * all tieds objects into the same routine, and not have a store_tied_hash,
	 * a store_tied_array, etc...
	 *
	 * Don't use a switch() statement, as most compilers don't optimize that
	 * well for 2/3 values. An if() else if() cascade is just fine. We put
	 * tied hashes first, as they are the most likely beasts.
	 */

	if (svt == SVt_PVHV) {
		TRACEME(("tied hash"));
		PUTMARK(SX_TIED_HASH);			/* Introduces tied hash */
	} else if (svt == SVt_PVAV) {
		TRACEME(("tied array"));
		PUTMARK(SX_TIED_ARRAY);			/* Introduces tied array */
	} else {
		TRACEME(("tied scalar"));
		PUTMARK(SX_TIED_SCALAR);		/* Introduces tied scalar */
		mtype = 'q';
	}

	if (!(mg = mg_find(sv, mtype)))
		CROAK(("No magic '%c' found while storing tied %s", mtype,
			(svt == SVt_PVHV) ? "hash" :
				(svt == SVt_PVAV) ? "array" : "scalar"));

	/*
	 * The mg->mg_obj found by mg_find() above actually points to the
	 * underlying tied Perl object implementation. For instance, if the
	 * original SV was that of a tied array, then mg->mg_obj is an AV.
	 *
	 * Note that we store the Perl object as-is. We don't call its FETCH
	 * method along the way. At retrieval time, we won't call its STORE
	 * method either, but the tieing magic will be re-installed. In itself,
	 * that ensures that the tieing semantics are preserved since futher
	 * accesses on the retrieved object will indeed call the magic methods...
	 */

	/* [#17040] mg_obj is NULL for scalar self-ties. AMS 20030416 */
	obj = mg->mg_obj ? mg->mg_obj : newSV(0);
	if ((ret = store(aTHX_ cxt, obj)))
		return ret;

	TRACEME(("ok (tied)"));

	return 0;
}

/*
 * store_tied_item
 *
 * Stores a reference to an item within a tied structure:
 *
 *  . \$h{key}, stores both the (tied %h) object and 'key'.
 *  . \$a[idx], stores both the (tied @a) object and 'idx'.
 *
 * Layout is therefore either:
 *     SX_TIED_KEY <object> <key>
 *     SX_TIED_IDX <object> <index>
 */
static int store_tied_item(pTHX_ stcxt_t *cxt, SV *sv)
{
	MAGIC *mg;
	int ret;

	TRACEME(("store_tied_item (0x%"UVxf")", PTR2UV(sv)));

	if (!(mg = mg_find(sv, 'p')))
		CROAK(("No magic 'p' found while storing reference to tied item"));

	/*
	 * We discriminate between \$h{key} and \$a[idx] via mg_ptr.
	 */

	if (mg->mg_ptr) {
		TRACEME(("store_tied_item: storing a ref to a tied hash item"));
		PUTMARK(SX_TIED_KEY);
		TRACEME(("store_tied_item: storing OBJ 0x%"UVxf, PTR2UV(mg->mg_obj)));

		if ((ret = store(aTHX_ cxt, mg->mg_obj)))		/* Extra () for -Wall, grr... */
			return ret;

		TRACEME(("store_tied_item: storing PTR 0x%"UVxf, PTR2UV(mg->mg_ptr)));

		if ((ret = store(aTHX_ cxt, (SV *) mg->mg_ptr)))	/* Idem, for -Wall */
			return ret;
	} else {
		I32 idx = mg->mg_len;

		TRACEME(("store_tied_item: storing a ref to a tied array item "));
		PUTMARK(SX_TIED_IDX);
		TRACEME(("store_tied_item: storing OBJ 0x%"UVxf, PTR2UV(mg->mg_obj)));

		if ((ret = store(aTHX_ cxt, mg->mg_obj)))		/* Idem, for -Wall */
			return ret;

		TRACEME(("store_tied_item: storing IDX %d", idx));

		WLEN(idx);
	}

	TRACEME(("ok (tied item)"));

	return 0;
}

/*
 * store_hook		-- dispatched manually, not via sv_store[]
 *
 * The blessed SV is serialized by a hook.
 *
 * Simple Layout is:
 *
 *     SX_HOOK <flags> <len> <classname> <len2> <str> [<len3> <object-IDs>]
 *
 * where <flags> indicates how long <len>, <len2> and <len3> are, whether
 * the trailing part [] is present, the type of object (scalar, array or hash).
 * There is also a bit which says how the classname is stored between:
 *
 *     <len> <classname>
 *     <index>
 *
 * and when the <index> form is used (classname already seen), the "large
 * classname" bit in <flags> indicates how large the <index> is.
 * 
 * The serialized string returned by the hook is of length <len2> and comes
 * next.  It is an opaque string for us.
 *
 * Those <len3> object IDs which are listed last represent the extra references
 * not directly serialized by the hook, but which are linked to the object.
 *
 * When recursion is mandated to resolve object-IDs not yet seen, we have
 * instead, with <header> being flags with bits set to indicate the object type
 * and that recursion was indeed needed:
 *
 *     SX_HOOK <header> <object> <header> <object> <flags>
 *
 * that same header being repeated between serialized objects obtained through
 * recursion, until we reach flags indicating no recursion, at which point
 * we know we've resynchronized with a single layout, after <flags>.
 *
 * When storing a blessed ref to a tied variable, the following format is
 * used:
 *
 *     SX_HOOK <flags> <extra> ... [<len3> <object-IDs>] <magic object>
 *
 * The first <flags> indication carries an object of type SHT_EXTRA, and the
 * real object type is held in the <extra> flag.  At the very end of the
 * serialization stream, the underlying magic object is serialized, just like
 * any other tied variable.
 */
static int store_hook(
        pTHX_
	stcxt_t *cxt,
	SV *sv,
	int type,
	HV *pkg,
	SV *hook)
{
	I32 len;
	char *class;
	STRLEN len2;
	SV *ref;
	AV *av;
	SV **ary;
	int count;				/* really len3 + 1 */
	unsigned char flags;
	char *pv;
	int i;
	int recursed = 0;		/* counts recursion */
	int obj_type;			/* object type, on 2 bits */
	I32 classnum;
	int ret;
	int clone = cxt->optype & ST_CLONE;
	char mtype = '\0';				/* for blessed ref to tied structures */
	unsigned char eflags = '\0';	/* used when object type is SHT_EXTRA */

	TRACEME(("store_hook, class \"%s\", tagged #%d", HvNAME(pkg), cxt->tagnum));

	/*
	 * Determine object type on 2 bits.
	 */

	switch (type) {
	case svis_SCALAR:
		obj_type = SHT_SCALAR;
		break;
	case svis_ARRAY:
		obj_type = SHT_ARRAY;
		break;
	case svis_HASH:
		obj_type = SHT_HASH;
		break;
	case svis_TIED:
		/*
		 * Produced by a blessed ref to a tied data structure, $o in the
		 * following Perl code.
		 *
		 * 	my %h;
		 *  tie %h, 'FOO';
		 *	my $o = bless \%h, 'BAR';
		 *
		 * Signal the tie-ing magic by setting the object type as SHT_EXTRA
		 * (since we have only 2 bits in <flags> to store the type), and an
		 * <extra> byte flag will be emitted after the FIRST <flags> in the
		 * stream, carrying what we put in `eflags'.
		 */
		obj_type = SHT_EXTRA;
		switch (SvTYPE(sv)) {
		case SVt_PVHV:
			eflags = (unsigned char) SHT_THASH;
			mtype = 'P';
			break;
		case SVt_PVAV:
			eflags = (unsigned char) SHT_TARRAY;
			mtype = 'P';
			break;
		default:
			eflags = (unsigned char) SHT_TSCALAR;
			mtype = 'q';
			break;
		}
		break;
	default:
		CROAK(("Unexpected object type (%d) in store_hook()", type));
	}
	flags = SHF_NEED_RECURSE | obj_type;

	class = HvNAME(pkg);
	len = strlen(class);

	/*
	 * To call the hook, we need to fake a call like:
	 *
	 *    $object->STORABLE_freeze($cloning);
	 *
	 * but we don't have the $object here.  For instance, if $object is
	 * a blessed array, what we have in `sv' is the array, and we can't
	 * call a method on those.
	 *
	 * Therefore, we need to create a temporary reference to the object and
	 * make the call on that reference.
	 */

	TRACEME(("about to call STORABLE_freeze on class %s", class));

	ref = newRV_noinc(sv);				/* Temporary reference */
	av = array_call(aTHX_ ref, hook, clone);	/* @a = $object->STORABLE_freeze($c) */
	SvRV(ref) = 0;
	SvREFCNT_dec(ref);					/* Reclaim temporary reference */

	count = AvFILLp(av) + 1;
	TRACEME(("store_hook, array holds %d items", count));

	/*
	 * If they return an empty list, it means they wish to ignore the
	 * hook for this class (and not just this instance -- that's for them
	 * to handle if they so wish).
	 *
	 * Simply disable the cached entry for the hook (it won't be recomputed
	 * since it's present in the cache) and recurse to store_blessed().
	 */

	if (!count) {
		/*
		 * They must not change their mind in the middle of a serialization.
		 */

		if (hv_fetch(cxt->hclass, class, len, FALSE))
			CROAK(("Too late to ignore hooks for %s class \"%s\"",
				(cxt->optype & ST_CLONE) ? "cloning" : "storing", class));
	
		pkg_hide(aTHX_ cxt->hook, pkg, "STORABLE_freeze");

		ASSERT(!pkg_can(aTHX_ cxt->hook, pkg, "STORABLE_freeze"), ("hook invisible"));
		TRACEME(("ignoring STORABLE_freeze in class \"%s\"", class));

		return store_blessed(aTHX_ cxt, sv, type, pkg);
	}

	/*
	 * Get frozen string.
	 */

	ary = AvARRAY(av);
	pv = SvPV(ary[0], len2);

	/*
	 * If they returned more than one item, we need to serialize some
	 * extra references if not already done.
	 *
	 * Loop over the array, starting at position #1, and for each item,
	 * ensure it is a reference, serialize it if not already done, and
	 * replace the entry with the tag ID of the corresponding serialized
	 * object.
	 *
	 * We CHEAT by not calling av_fetch() and read directly within the
	 * array, for speed.
	 */

	for (i = 1; i < count; i++) {
		SV **svh;
		SV *rsv = ary[i];
		SV *xsv;
		AV *av_hook = cxt->hook_seen;

		if (!SvROK(rsv))
			CROAK(("Item #%d returned by STORABLE_freeze "
				"for %s is not a reference", i, class));
		xsv = SvRV(rsv);		/* Follow ref to know what to look for */

		/*
		 * Look in hseen and see if we have a tag already.
		 * Serialize entry if not done already, and get its tag.
		 */

		if ((svh = hv_fetch(cxt->hseen, (char *) &xsv, sizeof(xsv), FALSE)))
			goto sv_seen;		/* Avoid moving code too far to the right */

		TRACEME(("listed object %d at 0x%"UVxf" is unknown", i-1, PTR2UV(xsv)));

		/*
		 * We need to recurse to store that object and get it to be known
		 * so that we can resolve the list of object-IDs at retrieve time.
		 *
		 * The first time we do this, we need to emit the proper header
		 * indicating that we recursed, and what the type of object is (the
		 * object we're storing via a user-hook).  Indeed, during retrieval,
		 * we'll have to create the object before recursing to retrieve the
		 * others, in case those would point back at that object.
		 */

		/* [SX_HOOK] <flags> [<extra>] <object>*/
		if (!recursed++) {
			PUTMARK(SX_HOOK);
			PUTMARK(flags);
			if (obj_type == SHT_EXTRA)
				PUTMARK(eflags);
		} else
			PUTMARK(flags);

		if ((ret = store(aTHX_ cxt, xsv)))	/* Given by hook for us to store */
			return ret;

		svh = hv_fetch(cxt->hseen, (char *) &xsv, sizeof(xsv), FALSE);
		if (!svh)
			CROAK(("Could not serialize item #%d from hook in %s", i, class));

		/*
		 * It was the first time we serialized `xsv'.
		 *
		 * Keep this SV alive until the end of the serialization: if we
		 * disposed of it right now by decrementing its refcount, and it was
		 * a temporary value, some next temporary value allocated during
		 * another STORABLE_freeze might take its place, and we'd wrongly
		 * assume that new SV was already serialized, based on its presence
		 * in cxt->hseen.
		 *
		 * Therefore, push it away in cxt->hook_seen.
		 */

		av_store(av_hook, AvFILLp(av_hook)+1, SvREFCNT_inc(xsv));

	sv_seen:
		/*
		 * Dispose of the REF they returned.  If we saved the `xsv' away
		 * in the array of returned SVs, that will not cause the underlying
		 * referenced SV to be reclaimed.
		 */

		ASSERT(SvREFCNT(xsv) > 1, ("SV will survive disposal of its REF"));
		SvREFCNT_dec(rsv);			/* Dispose of reference */

		/*
		 * Replace entry with its tag (not a real SV, so no refcnt increment)
		 */

		ary[i] = *svh;
		TRACEME(("listed object %d at 0x%"UVxf" is tag #%"UVuf,
			 i-1, PTR2UV(xsv), PTR2UV(*svh)));
	}

	/*
	 * Allocate a class ID if not already done.
	 *
	 * This needs to be done after the recursion above, since at retrieval
	 * time, we'll see the inner objects first.  Many thanks to
	 * Salvador Ortiz Garcia <sog@msg.com.mx> who spot that bug and
	 * proposed the right fix.  -- RAM, 15/09/2000
	 */

	if (!known_class(aTHX_ cxt, class, len, &classnum)) {
		TRACEME(("first time we see class %s, ID = %d", class, classnum));
		classnum = -1;				/* Mark: we must store classname */
	} else {
		TRACEME(("already seen class %s, ID = %d", class, classnum));
	}

	/*
	 * Compute leading flags.
	 */

	flags = obj_type;
	if (((classnum == -1) ? len : classnum) > LG_SCALAR)
		flags |= SHF_LARGE_CLASSLEN;
	if (classnum != -1)
		flags |= SHF_IDX_CLASSNAME;
	if (len2 > LG_SCALAR)
		flags |= SHF_LARGE_STRLEN;
	if (count > 1)
		flags |= SHF_HAS_LIST;
	if (count > (LG_SCALAR + 1))
		flags |= SHF_LARGE_LISTLEN;

	/* 
	 * We're ready to emit either serialized form:
	 *
	 *   SX_HOOK <flags> <len> <classname> <len2> <str> [<len3> <object-IDs>]
	 *   SX_HOOK <flags> <index>           <len2> <str> [<len3> <object-IDs>]
	 *
	 * If we recursed, the SX_HOOK has already been emitted.
	 */

	TRACEME(("SX_HOOK (recursed=%d) flags=0x%x "
			"class=%"IVdf" len=%"IVdf" len2=%"IVdf" len3=%d",
		 recursed, flags, (IV)classnum, (IV)len, (IV)len2, count-1));

	/* SX_HOOK <flags> [<extra>] */
	if (!recursed) {
		PUTMARK(SX_HOOK);
		PUTMARK(flags);
		if (obj_type == SHT_EXTRA)
			PUTMARK(eflags);
	} else
		PUTMARK(flags);

	/* <len> <classname> or <index> */
	if (flags & SHF_IDX_CLASSNAME) {
		if (flags & SHF_LARGE_CLASSLEN)
			WLEN(classnum);
		else {
			unsigned char cnum = (unsigned char) classnum;
			PUTMARK(cnum);
		}
	} else {
		if (flags & SHF_LARGE_CLASSLEN)
			WLEN(len);
		else {
			unsigned char clen = (unsigned char) len;
			PUTMARK(clen);
		}
		WRITE(class, len);		/* Final \0 is omitted */
	}

	/* <len2> <frozen-str> */
	if (flags & SHF_LARGE_STRLEN) {
		I32 wlen2 = len2;		/* STRLEN might be 8 bytes */
		WLEN(wlen2);			/* Must write an I32 for 64-bit machines */
	} else {
		unsigned char clen = (unsigned char) len2;
		PUTMARK(clen);
	}
	if (len2)
		WRITE(pv, (SSize_t)len2);	/* Final \0 is omitted */

	/* [<len3> <object-IDs>] */
	if (flags & SHF_HAS_LIST) {
		int len3 = count - 1;
		if (flags & SHF_LARGE_LISTLEN)
			WLEN(len3);
		else {
			unsigned char clen = (unsigned char) len3;
			PUTMARK(clen);
		}

		/*
		 * NOTA BENE, for 64-bit machines: the ary[i] below does not yield a
		 * real pointer, rather a tag number, well under the 32-bit limit.
		 */

		for (i = 1; i < count; i++) {
			I32 tagval = htonl(LOW_32BITS(ary[i]));
			WRITE_I32(tagval);
			TRACEME(("object %d, tag #%d", i-1, ntohl(tagval)));
		}
	}

	/*
	 * Free the array.  We need extra care for indices after 0, since they
	 * don't hold real SVs but integers cast.
	 */

	if (count > 1)
		AvFILLp(av) = 0;	/* Cheat, nothing after 0 interests us */
	av_undef(av);
	sv_free((SV *) av);

	/*
	 * If object was tied, need to insert serialization of the magic object.
	 */

	if (obj_type == SHT_EXTRA) {
		MAGIC *mg;

		if (!(mg = mg_find(sv, mtype))) {
			int svt = SvTYPE(sv);
			CROAK(("No magic '%c' found while storing ref to tied %s with hook",
				mtype, (svt == SVt_PVHV) ? "hash" :
					(svt == SVt_PVAV) ? "array" : "scalar"));
		}

		TRACEME(("handling the magic object 0x%"UVxf" part of 0x%"UVxf,
			PTR2UV(mg->mg_obj), PTR2UV(sv)));

		/*
		 * [<magic object>]
		 */

		if ((ret = store(aTHX_ cxt, mg->mg_obj)))	/* Extra () for -Wall, grr... */
			return ret;
	}

	return 0;
}

/*
 * store_blessed	-- dispatched manually, not via sv_store[]
 *
 * Check whether there is a STORABLE_xxx hook defined in the class or in one
 * of its ancestors.  If there is, then redispatch to store_hook();
 *
 * Otherwise, the blessed SV is stored using the following layout:
 *
 *    SX_BLESS <flag> <len> <classname> <object>
 *
 * where <flag> indicates whether <len> is stored on 0 or 4 bytes, depending
 * on the high-order bit in flag: if 1, then length follows on 4 bytes.
 * Otherwise, the low order bits give the length, thereby giving a compact
 * representation for class names less than 127 chars long.
 *
 * Each <classname> seen is remembered and indexed, so that the next time
 * an object in the blessed in the same <classname> is stored, the following
 * will be emitted:
 *
 *    SX_IX_BLESS <flag> <index> <object>
 *
 * where <index> is the classname index, stored on 0 or 4 bytes depending
 * on the high-order bit in flag (same encoding as above for <len>).
 */
static int store_blessed(
        pTHX_
	stcxt_t *cxt,
	SV *sv,
	int type,
	HV *pkg)
{
	SV *hook;
	I32 len;
	char *class;
	I32 classnum;

	TRACEME(("store_blessed, type %d, class \"%s\"", type, HvNAME(pkg)));

	/*
	 * Look for a hook for this blessed SV and redirect to store_hook()
	 * if needed.
	 */

	hook = pkg_can(aTHX_ cxt->hook, pkg, "STORABLE_freeze");
	if (hook)
		return store_hook(aTHX_ cxt, sv, type, pkg, hook);

	/*
	 * This is a blessed SV without any serialization hook.
	 */

	class = HvNAME(pkg);
	len = strlen(class);

	TRACEME(("blessed 0x%"UVxf" in %s, no hook: tagged #%d",
		 PTR2UV(sv), class, cxt->tagnum));

	/*
	 * Determine whether it is the first time we see that class name (in which
	 * case it will be stored in the SX_BLESS form), or whether we already
	 * saw that class name before (in which case the SX_IX_BLESS form will be
	 * used).
	 */

	if (known_class(aTHX_ cxt, class, len, &classnum)) {
		TRACEME(("already seen class %s, ID = %d", class, classnum));
		PUTMARK(SX_IX_BLESS);
		if (classnum <= LG_BLESS) {
			unsigned char cnum = (unsigned char) classnum;
			PUTMARK(cnum);
		} else {
			unsigned char flag = (unsigned char) 0x80;
			PUTMARK(flag);
			WLEN(classnum);
		}
	} else {
		TRACEME(("first time we see class %s, ID = %d", class, classnum));
		PUTMARK(SX_BLESS);
		if (len <= LG_BLESS) {
			unsigned char clen = (unsigned char) len;
			PUTMARK(clen);
		} else {
			unsigned char flag = (unsigned char) 0x80;
			PUTMARK(flag);
			WLEN(len);					/* Don't BER-encode, this should be rare */
		}
		WRITE(class, len);				/* Final \0 is omitted */
	}

	/*
	 * Now emit the <object> part.
	 */

	return SV_STORE(type)(aTHX_ cxt, sv);
}

/*
 * store_other
 *
 * We don't know how to store the item we reached, so return an error condition.
 * (it's probably a GLOB, some CODE reference, etc...)
 *
 * If they defined the `forgive_me' variable at the Perl level to some
 * true value, then don't croak, just warn, and store a placeholder string
 * instead.
 */
static int store_other(pTHX_ stcxt_t *cxt, SV *sv)
{
	I32 len;
	static char buf[80];

	TRACEME(("store_other"));

	/*
	 * Fetch the value from perl only once per store() operation.
	 */

	if (
		cxt->forgive_me == 0 ||
		(cxt->forgive_me < 0 && !(cxt->forgive_me =
			SvTRUE(perl_get_sv("Storable::forgive_me", TRUE)) ? 1 : 0))
	)
		CROAK(("Can't store %s items", sv_reftype(sv, FALSE)));

	warn("Can't store item %s(0x%"UVxf")",
		sv_reftype(sv, FALSE), PTR2UV(sv));

	/*
	 * Store placeholder string as a scalar instead...
	 */

	(void) sprintf(buf, "You lost %s(0x%"UVxf")%c", sv_reftype(sv, FALSE),
		       PTR2UV(sv), (char) 0);

	len = strlen(buf);
	STORE_SCALAR(buf, len);
	TRACEME(("ok (dummy \"%s\", length = %"IVdf")", buf, (IV) len));

	return 0;
}

/***
 *** Store driving routines
 ***/

/*
 * sv_type
 *
 * WARNING: partially duplicates Perl's sv_reftype for speed.
 *
 * Returns the type of the SV, identified by an integer. That integer
 * may then be used to index the dynamic routine dispatch table.
 */
static int sv_type(pTHX_ SV *sv)
{
	switch (SvTYPE(sv)) {
	case SVt_NULL:
	case SVt_IV:
	case SVt_NV:
		/*
		 * No need to check for ROK, that can't be set here since there
		 * is no field capable of hodling the xrv_rv reference.
		 */
		return svis_SCALAR;
	case SVt_PV:
	case SVt_RV:
	case SVt_PVIV:
	case SVt_PVNV:
		/*
		 * Starting from SVt_PV, it is possible to have the ROK flag
		 * set, the pointer to the other SV being either stored in
		 * the xrv_rv (in the case of a pure SVt_RV), or as the
		 * xpv_pv field of an SVt_PV and its heirs.
		 *
		 * However, those SV cannot be magical or they would be an
		 * SVt_PVMG at least.
		 */
		return SvROK(sv) ? svis_REF : svis_SCALAR;
	case SVt_PVMG:
	case SVt_PVLV:		/* Workaround for perl5.004_04 "LVALUE" bug */
		if (SvRMAGICAL(sv) && (mg_find(sv, 'p')))
			return svis_TIED_ITEM;
		/* FALL THROUGH */
	case SVt_PVBM:
		if (SvRMAGICAL(sv) && (mg_find(sv, 'q')))
			return svis_TIED;
		return SvROK(sv) ? svis_REF : svis_SCALAR;
	case SVt_PVAV:
		if (SvRMAGICAL(sv) && (mg_find(sv, 'P')))
			return svis_TIED;
		return svis_ARRAY;
	case SVt_PVHV:
		if (SvRMAGICAL(sv) && (mg_find(sv, 'P')))
			return svis_TIED;
		return svis_HASH;
	case SVt_PVCV:
		return svis_CODE;
	default:
		break;
	}

	return svis_OTHER;
}

/*
 * store
 *
 * Recursively store objects pointed to by the sv to the specified file.
 *
 * Layout is <content> or SX_OBJECT <tagnum> if we reach an already stored
 * object (one for which storage has started -- it may not be over if we have
 * a self-referenced structure). This data set forms a stored <object>.
 */
static int store(pTHX_ stcxt_t *cxt, SV *sv)
{
	SV **svh;
	int ret;
	int type;
	HV *hseen = cxt->hseen;

	TRACEME(("store (0x%"UVxf")", PTR2UV(sv)));

	/*
	 * If object has already been stored, do not duplicate data.
	 * Simply emit the SX_OBJECT marker followed by its tag data.
	 * The tag is always written in network order.
	 *
	 * NOTA BENE, for 64-bit machines: the "*svh" below does not yield a
	 * real pointer, rather a tag number (watch the insertion code below).
	 * That means it probably safe to assume it is well under the 32-bit limit,
	 * and makes the truncation safe.
	 *		-- RAM, 14/09/1999
	 */

	svh = hv_fetch(hseen, (char *) &sv, sizeof(sv), FALSE);
	if (svh) {
		I32 tagval;

		if (sv == &PL_sv_undef) {
			/* We have seen PL_sv_undef before, but fake it as
			   if we have not.

			   Not the simplest solution to making restricted
			   hashes work on 5.8.0, but it does mean that
			   repeated references to the one true undef will
			   take up less space in the output file.
			*/
			/* Need to jump past the next hv_store, because on the
			   second store of undef the old hash value will be
			   SvREFCNT_dec()ed, and as Storable cheats horribly
			   by storing non-SVs in the hash a SEGV will ensure.
			   Need to increase the tag number so that the
			   receiver has no idea what games we're up to.  This
			   special casing doesn't affect hooks that store
			   undef, as the hook routine does its own lookup into
			   hseen.  Also this means that any references back
			   to PL_sv_undef (from the pathological case of hooks
			   storing references to it) will find the seen hash
			   entry for the first time, as if we didn't have this
			   hackery here. (That hseen lookup works even on 5.8.0
			   because it's a key of &PL_sv_undef and a value
			   which is a tag number, not a value which is
			   PL_sv_undef.)  */
			cxt->tagnum++;
			type = svis_SCALAR;
			goto undef_special_case;
		}
		
		tagval = htonl(LOW_32BITS(*svh));

		TRACEME(("object 0x%"UVxf" seen as #%d", PTR2UV(sv), ntohl(tagval)));

		PUTMARK(SX_OBJECT);
		WRITE_I32(tagval);
		return 0;
	}

	/*
	 * Allocate a new tag and associate it with the address of the sv being
	 * stored, before recursing...
	 *
	 * In order to avoid creating new SvIVs to hold the tagnum we just
	 * cast the tagnum to an SV pointer and store that in the hash.  This
	 * means that we must clean up the hash manually afterwards, but gives
	 * us a 15% throughput increase.
	 *
	 */

	cxt->tagnum++;
	if (!hv_store(hseen,
			(char *) &sv, sizeof(sv), INT2PTR(SV*, cxt->tagnum), 0))
		return -1;

	/*
	 * Store `sv' and everything beneath it, using appropriate routine.
	 * Abort immediately if we get a non-zero status back.
	 */

	type = sv_type(aTHX_ sv);

undef_special_case:
	TRACEME(("storing 0x%"UVxf" tag #%d, type %d...",
		 PTR2UV(sv), cxt->tagnum, type));

	if (SvOBJECT(sv)) {
		HV *pkg = SvSTASH(sv);
		ret = store_blessed(aTHX_ cxt, sv, type, pkg);
	} else
		ret = SV_STORE(type)(aTHX_ cxt, sv);

	TRACEME(("%s (stored 0x%"UVxf", refcnt=%d, %s)",
		ret ? "FAILED" : "ok", PTR2UV(sv),
		SvREFCNT(sv), sv_reftype(sv, FALSE)));

	return ret;
}

/*
 * magic_write
 *
 * Write magic number and system information into the file.
 * Layout is <magic> <network> [<len> <byteorder> <sizeof int> <sizeof long>
 * <sizeof ptr>] where <len> is the length of the byteorder hexa string.
 * All size and lenghts are written as single characters here.
 *
 * Note that no byte ordering info is emitted when <network> is true, since
 * integers will be emitted in network order in that case.
 */
static int magic_write(pTHX_ stcxt_t *cxt)
{
    /*
     * Starting with 0.6, the "use_network_order" byte flag is also used to
     * indicate the version number of the binary image, encoded in the upper
     * bits. The bit 0 is always used to indicate network order.
     */
    /*
     * Starting with 0.7, a full byte is dedicated to the minor version of
     * the binary format, which is incremented only when new markers are
     * introduced, for instance, but when backward compatibility is preserved.
     */

    /* Make these at compile time.  The WRITE() macro is sufficiently complex
       that it saves about 200 bytes doing it this way and only using it
       once.  */
    static const unsigned char network_file_header[] = {
        MAGICSTR_BYTES,
        (STORABLE_BIN_MAJOR << 1) | 1,
        STORABLE_BIN_WRITE_MINOR
    };
    static const unsigned char file_header[] = {
        MAGICSTR_BYTES,
        (STORABLE_BIN_MAJOR << 1) | 0,
        STORABLE_BIN_WRITE_MINOR,
        /* sizeof the array includes the 0 byte at the end:  */
        (char) sizeof (byteorderstr) - 1,
        BYTEORDER_BYTES,
        (unsigned char) sizeof(int),
	(unsigned char) sizeof(long),
        (unsigned char) sizeof(char *),
	(unsigned char) sizeof(NV)
    };
#ifdef USE_56_INTERWORK_KLUDGE
    static const unsigned char file_header_56[] = {
        MAGICSTR_BYTES,
        (STORABLE_BIN_MAJOR << 1) | 0,
        STORABLE_BIN_WRITE_MINOR,
        /* sizeof the array includes the 0 byte at the end:  */
        (char) sizeof (byteorderstr_56) - 1,
        BYTEORDER_BYTES_56,
        (unsigned char) sizeof(int),
	(unsigned char) sizeof(long),
        (unsigned char) sizeof(char *),
	(unsigned char) sizeof(NV)
    };
#endif
    const unsigned char *header;
    SSize_t length;

    TRACEME(("magic_write on fd=%d", cxt->fio ? PerlIO_fileno(cxt->fio) : -1));

    if (cxt->netorder) {
        header = network_file_header;
        length = sizeof (network_file_header);
    } else {
#ifdef USE_56_INTERWORK_KLUDGE
        if (SvTRUE(perl_get_sv("Storable::interwork_56_64bit", TRUE))) {
            header = file_header_56;
            length = sizeof (file_header_56);
        } else
#endif
        {
            header = file_header;
            length = sizeof (file_header);
        }
    }        

    if (!cxt->fio) {
        /* sizeof the array includes the 0 byte at the end.  */
        header += sizeof (magicstr) - 1;
        length -= sizeof (magicstr) - 1;
    }        

    WRITE( (unsigned char*) header, length);

    if (!cxt->netorder) {
	TRACEME(("ok (magic_write byteorder = 0x%lx [%d], I%d L%d P%d D%d)",
		 (unsigned long) BYTEORDER, (int) sizeof (byteorderstr) - 1,
		 (int) sizeof(int), (int) sizeof(long),
		 (int) sizeof(char *), (int) sizeof(NV)));
    }
    return 0;
}

/*
 * do_store
 *
 * Common code for store operations.
 *
 * When memory store is requested (f = NULL) and a non null SV* is given in
 * `res', it is filled with a new SV created out of the memory buffer.
 *
 * It is required to provide a non-null `res' when the operation type is not
 * dclone() and store() is performed to memory.
 */
static int do_store(
        pTHX_
	PerlIO *f,
	SV *sv,
	int optype,
	int network_order,
	SV **res)
{
	dSTCXT;
	int status;

	ASSERT(!(f == 0 && !(optype & ST_CLONE)) || res,
		("must supply result SV pointer for real recursion to memory"));

	TRACEME(("do_store (optype=%d, netorder=%d)",
		optype, network_order));

	optype |= ST_STORE;

	/*
	 * Workaround for CROAK leak: if they enter with a "dirty" context,
	 * free up memory for them now.
	 */

	if (cxt->s_dirty)
		clean_context(aTHX_ cxt);

	/*
	 * Now that STORABLE_xxx hooks exist, it is possible that they try to
	 * re-enter store() via the hooks.  We need to stack contexts.
	 */

	if (cxt->entry)
		cxt = allocate_context(aTHX_ cxt);

	cxt->entry++;

	ASSERT(cxt->entry == 1, ("starting new recursion"));
	ASSERT(!cxt->s_dirty, ("clean context"));

	/*
	 * Ensure sv is actually a reference. From perl, we called something
	 * like:
	 *       pstore(aTHX_ FILE, \@array);
	 * so we must get the scalar value behing that reference.
	 */

	if (!SvROK(sv))
		CROAK(("Not a reference"));
	sv = SvRV(sv);			/* So follow it to know what to store */

	/* 
	 * If we're going to store to memory, reset the buffer.
	 */

	if (!f)
		MBUF_INIT(0);

	/*
	 * Prepare context and emit headers.
	 */

	init_store_context(aTHX_ cxt, f, optype, network_order);

	if (-1 == magic_write(aTHX_ cxt))		/* Emit magic and ILP info */
		return 0;					/* Error */

	/*
	 * Recursively store object...
	 */

	ASSERT(is_storing(), ("within store operation"));

	status = store(aTHX_ cxt, sv);		/* Just do it! */

	/*
	 * If they asked for a memory store and they provided an SV pointer,
	 * make an SV string out of the buffer and fill their pointer.
	 *
	 * When asking for ST_REAL, it's MANDATORY for the caller to provide
	 * an SV, since context cleanup might free the buffer if we did recurse.
	 * (unless caller is dclone(), which is aware of that).
	 */

	if (!cxt->fio && res)
		*res = mbuf2sv(aTHX);

	/*
	 * Final cleanup.
	 *
	 * The "root" context is never freed, since it is meant to be always
	 * handy for the common case where no recursion occurs at all (i.e.
	 * we enter store() outside of any Storable code and leave it, period).
	 * We know it's the "root" context because there's nothing stacked
	 * underneath it.
	 *
	 * OPTIMIZATION:
	 *
	 * When deep cloning, we don't free the context: doing so would force
	 * us to copy the data in the memory buffer.  Sicne we know we're
	 * about to enter do_retrieve...
	 */

	clean_store_context(aTHX_ cxt);
	if (cxt->prev && !(cxt->optype & ST_CLONE))
		free_context(aTHX_ cxt);

	TRACEME(("do_store returns %d", status));

	return status == 0;
}

/*
 * pstore
 *
 * Store the transitive data closure of given object to disk.
 * Returns 0 on error, a true value otherwise.
 */
int pstore(pTHX_ PerlIO *f, SV *sv)
{
	TRACEME(("pstore"));
	return do_store(aTHX_ f, sv, 0, FALSE, (SV**) 0);

}

/*
 * net_pstore
 *
 * Same as pstore(), but network order is used for integers and doubles are
 * emitted as strings.
 */
int net_pstore(pTHX_ PerlIO *f, SV *sv)
{
	TRACEME(("net_pstore"));
	return do_store(aTHX_ f, sv, 0, TRUE, (SV**) 0);
}

/***
 *** Memory stores.
 ***/

/*
 * mbuf2sv
 *
 * Build a new SV out of the content of the internal memory buffer.
 */
static SV *mbuf2sv(pTHX)
{
	dSTCXT;

	return newSVpv(mbase, MBUF_SIZE());
}

/*
 * mstore
 *
 * Store the transitive data closure of given object to memory.
 * Returns undef on error, a scalar value containing the data otherwise.
 */
SV *mstore(pTHX_ SV *sv)
{
	SV *out;

	TRACEME(("mstore"));

	if (!do_store(aTHX_ (PerlIO*) 0, sv, 0, FALSE, &out))
		return &PL_sv_undef;

	return out;
}

/*
 * net_mstore
 *
 * Same as mstore(), but network order is used for integers and doubles are
 * emitted as strings.
 */
SV *net_mstore(pTHX_ SV *sv)
{
	SV *out;

	TRACEME(("net_mstore"));

	if (!do_store(aTHX_ (PerlIO*) 0, sv, 0, TRUE, &out))
		return &PL_sv_undef;

	return out;
}

/***
 *** Specific retrieve callbacks.
 ***/

/*
 * retrieve_other
 *
 * Return an error via croak, since it is not possible that we get here
 * under normal conditions, when facing a file produced via pstore().
 */
static SV *retrieve_other(pTHX_ stcxt_t *cxt, char *cname)
{
	if (
		cxt->ver_major != STORABLE_BIN_MAJOR &&
		cxt->ver_minor != STORABLE_BIN_MINOR
	) {
		CROAK(("Corrupted storable %s (binary v%d.%d), current is v%d.%d",
			cxt->fio ? "file" : "string",
			cxt->ver_major, cxt->ver_minor,
			STORABLE_BIN_MAJOR, STORABLE_BIN_MINOR));
	} else {
		CROAK(("Corrupted storable %s (binary v%d.%d)",
			cxt->fio ? "file" : "string",
			cxt->ver_major, cxt->ver_minor));
	}

	return (SV *) 0;		/* Just in case */
}

/*
 * retrieve_idx_blessed
 *
 * Layout is SX_IX_BLESS <index> <object> with SX_IX_BLESS already read.
 * <index> can be coded on either 1 or 5 bytes.
 */
static SV *retrieve_idx_blessed(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 idx;
	char *class;
	SV **sva;
	SV *sv;

	TRACEME(("retrieve_idx_blessed (#%d)", cxt->tagnum));
	ASSERT(!cname, ("no bless-into class given here, got %s", cname));

	GETMARK(idx);			/* Index coded on a single char? */
	if (idx & 0x80)
		RLEN(idx);

	/*
	 * Fetch classname in `aclass'
	 */

	sva = av_fetch(cxt->aclass, idx, FALSE);
	if (!sva)
		CROAK(("Class name #%"IVdf" should have been seen already", (IV) idx));

	class = SvPVX(*sva);	/* We know it's a PV, by construction */

	TRACEME(("class ID %d => %s", idx, class));

	/*
	 * Retrieve object and bless it.
	 */

	sv = retrieve(aTHX_ cxt, class);	/* First SV which is SEEN will be blessed */

	return sv;
}

/*
 * retrieve_blessed
 *
 * Layout is SX_BLESS <len> <classname> <object> with SX_BLESS already read.
 * <len> can be coded on either 1 or 5 bytes.
 */
static SV *retrieve_blessed(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 len;
	SV *sv;
	char buf[LG_BLESS + 1];		/* Avoid malloc() if possible */
	char *class = buf;

	TRACEME(("retrieve_blessed (#%d)", cxt->tagnum));
	ASSERT(!cname, ("no bless-into class given here, got %s", cname));

	/*
	 * Decode class name length and read that name.
	 *
	 * Short classnames have two advantages: their length is stored on one
	 * single byte, and the string can be read on the stack.
	 */

	GETMARK(len);			/* Length coded on a single char? */
	if (len & 0x80) {
		RLEN(len);
		TRACEME(("** allocating %d bytes for class name", len+1));
		New(10003, class, len+1, char);
	}
	READ(class, len);
	class[len] = '\0';		/* Mark string end */

	/*
	 * It's a new classname, otherwise it would have been an SX_IX_BLESS.
	 */

	TRACEME(("new class name \"%s\" will bear ID = %d", class, cxt->classnum));

	if (!av_store(cxt->aclass, cxt->classnum++, newSVpvn(class, len)))
		return (SV *) 0;

	/*
	 * Retrieve object and bless it.
	 */

	sv = retrieve(aTHX_ cxt, class);	/* First SV which is SEEN will be blessed */
	if (class != buf)
		Safefree(class);

	return sv;
}

/*
 * retrieve_hook
 *
 * Layout: SX_HOOK <flags> <len> <classname> <len2> <str> [<len3> <object-IDs>]
 * with leading mark already read, as usual.
 *
 * When recursion was involved during serialization of the object, there
 * is an unknown amount of serialized objects after the SX_HOOK mark.  Until
 * we reach a <flags> marker with the recursion bit cleared.
 *
 * If the first <flags> byte contains a type of SHT_EXTRA, then the real type
 * is held in the <extra> byte, and if the object is tied, the serialized
 * magic object comes at the very end:
 *
 *     SX_HOOK <flags> <extra> ... [<len3> <object-IDs>] <magic object>
 *
 * This means the STORABLE_thaw hook will NOT get a tied variable during its
 * processing (since we won't have seen the magic object by the time the hook
 * is called).  See comments below for why it was done that way.
 */
static SV *retrieve_hook(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 len;
	char buf[LG_BLESS + 1];		/* Avoid malloc() if possible */
	char *class = buf;
	unsigned int flags;
	I32 len2;
	SV *frozen;
	I32 len3 = 0;
	AV *av = 0;
	SV *hook;
	SV *sv;
	SV *rv;
	int obj_type;
	int clone = cxt->optype & ST_CLONE;
	char mtype = '\0';
	unsigned int extra_type = 0;

	TRACEME(("retrieve_hook (#%d)", cxt->tagnum));
	ASSERT(!cname, ("no bless-into class given here, got %s", cname));

	/*
	 * Read flags, which tell us about the type, and whether we need to recurse.
	 */

	GETMARK(flags);

	/*
	 * Create the (empty) object, and mark it as seen.
	 *
	 * This must be done now, because tags are incremented, and during
	 * serialization, the object tag was affected before recursion could
	 * take place.
	 */

	obj_type = flags & SHF_TYPE_MASK;
	switch (obj_type) {
	case SHT_SCALAR:
		sv = newSV(0);
		break;
	case SHT_ARRAY:
		sv = (SV *) newAV();
		break;
	case SHT_HASH:
		sv = (SV *) newHV();
		break;
	case SHT_EXTRA:
		/*
		 * Read <extra> flag to know the type of the object.
		 * Record associated magic type for later.
		 */
		GETMARK(extra_type);
		switch (extra_type) {
		case SHT_TSCALAR:
			sv = newSV(0);
			mtype = 'q';
			break;
		case SHT_TARRAY:
			sv = (SV *) newAV();
			mtype = 'P';
			break;
		case SHT_THASH:
			sv = (SV *) newHV();
			mtype = 'P';
			break;
		default:
			return retrieve_other(aTHX_ cxt, 0);	/* Let it croak */
		}
		break;
	default:
		return retrieve_other(aTHX_ cxt, 0);		/* Let it croak */
	}
	SEEN(sv, 0, 0);							/* Don't bless yet */

	/*
	 * Whilst flags tell us to recurse, do so.
	 *
	 * We don't need to remember the addresses returned by retrieval, because
	 * all the references will be obtained through indirection via the object
	 * tags in the object-ID list.
	 *
	 * We need to decrement the reference count for these objects
	 * because, if the user doesn't save a reference to them in the hook,
	 * they must be freed when this context is cleaned.
	 */

	while (flags & SHF_NEED_RECURSE) {
		TRACEME(("retrieve_hook recursing..."));
		rv = retrieve(aTHX_ cxt, 0);
		if (!rv)
			return (SV *) 0;
		SvREFCNT_dec(rv);
		TRACEME(("retrieve_hook back with rv=0x%"UVxf,
			 PTR2UV(rv)));
		GETMARK(flags);
	}

	if (flags & SHF_IDX_CLASSNAME) {
		SV **sva;
		I32 idx;

		/*
		 * Fetch index from `aclass'
		 */

		if (flags & SHF_LARGE_CLASSLEN)
			RLEN(idx);
		else
			GETMARK(idx);

		sva = av_fetch(cxt->aclass, idx, FALSE);
		if (!sva)
			CROAK(("Class name #%"IVdf" should have been seen already",
				(IV) idx));

		class = SvPVX(*sva);	/* We know it's a PV, by construction */
		TRACEME(("class ID %d => %s", idx, class));

	} else {
		/*
		 * Decode class name length and read that name.
		 *
		 * NOTA BENE: even if the length is stored on one byte, we don't read
		 * on the stack.  Just like retrieve_blessed(), we limit the name to
		 * LG_BLESS bytes.  This is an arbitrary decision.
		 */

		if (flags & SHF_LARGE_CLASSLEN)
			RLEN(len);
		else
			GETMARK(len);

		if (len > LG_BLESS) {
			TRACEME(("** allocating %d bytes for class name", len+1));
			New(10003, class, len+1, char);
		}

		READ(class, len);
		class[len] = '\0';		/* Mark string end */

		/*
		 * Record new classname.
		 */

		if (!av_store(cxt->aclass, cxt->classnum++, newSVpvn(class, len)))
			return (SV *) 0;
	}

	TRACEME(("class name: %s", class));

	/*
	 * Decode user-frozen string length and read it in an SV.
	 *
	 * For efficiency reasons, we read data directly into the SV buffer.
	 * To understand that code, read retrieve_scalar()
	 */

	if (flags & SHF_LARGE_STRLEN)
		RLEN(len2);
	else
		GETMARK(len2);

	frozen = NEWSV(10002, len2);
	if (len2) {
		SAFEREAD(SvPVX(frozen), len2, frozen);
		SvCUR_set(frozen, len2);
		*SvEND(frozen) = '\0';
	}
	(void) SvPOK_only(frozen);		/* Validates string pointer */
	if (cxt->s_tainted)				/* Is input source tainted? */
		SvTAINT(frozen);

	TRACEME(("frozen string: %d bytes", len2));

	/*
	 * Decode object-ID list length, if present.
	 */

	if (flags & SHF_HAS_LIST) {
		if (flags & SHF_LARGE_LISTLEN)
			RLEN(len3);
		else
			GETMARK(len3);
		if (len3) {
			av = newAV();
			av_extend(av, len3 + 1);	/* Leave room for [0] */
			AvFILLp(av) = len3;			/* About to be filled anyway */
		}
	}

	TRACEME(("has %d object IDs to link", len3));

	/*
	 * Read object-ID list into array.
	 * Because we pre-extended it, we can cheat and fill it manually.
	 *
	 * We read object tags and we can convert them into SV* on the fly
	 * because we know all the references listed in there (as tags)
	 * have been already serialized, hence we have a valid correspondance
	 * between each of those tags and the recreated SV.
	 */

	if (av) {
		SV **ary = AvARRAY(av);
		int i;
		for (i = 1; i <= len3; i++) {	/* We leave [0] alone */
			I32 tag;
			SV **svh;
			SV *xsv;

			READ_I32(tag);
			tag = ntohl(tag);
			svh = av_fetch(cxt->aseen, tag, FALSE);
			if (!svh) {
				if (tag == cxt->where_is_undef) {
					/* av_fetch uses PL_sv_undef internally, hence this
					   somewhat gruesome hack. */
					xsv = &PL_sv_undef;
					svh = &xsv;
				} else {
					CROAK(("Object #%"IVdf" should have been retrieved already",
					       (IV) tag));
				}
			}
			xsv = *svh;
			ary[i] = SvREFCNT_inc(xsv);
		}
	}

	/*
	 * Bless the object and look up the STORABLE_thaw hook.
	 */

	BLESS(sv, class);
	hook = pkg_can(aTHX_ cxt->hook, SvSTASH(sv), "STORABLE_thaw");
	if (!hook) {
		/*
		 * Hook not found.  Maybe they did not require the module where this
		 * hook is defined yet?
		 *
		 * If the require below succeeds, we'll be able to find the hook.
		 * Still, it only works reliably when each class is defined in a
		 * file of its own.
		 */

		SV *psv = newSVpvn("require ", 8);
		sv_catpv(psv, class);

		TRACEME(("No STORABLE_thaw defined for objects of class %s", class));
		TRACEME(("Going to require module '%s' with '%s'", class, SvPVX(psv)));

		perl_eval_sv(psv, G_DISCARD);
		sv_free(psv);

		/*
		 * We cache results of pkg_can, so we need to uncache before attempting
		 * the lookup again.
		 */

		pkg_uncache(aTHX_ cxt->hook, SvSTASH(sv), "STORABLE_thaw");
		hook = pkg_can(aTHX_ cxt->hook, SvSTASH(sv), "STORABLE_thaw");

		if (!hook)
			CROAK(("No STORABLE_thaw defined for objects of class %s "
					"(even after a \"require %s;\")", class, class));
	}

	/*
	 * If we don't have an `av' yet, prepare one.
	 * Then insert the frozen string as item [0].
	 */

	if (!av) {
		av = newAV();
		av_extend(av, 1);
		AvFILLp(av) = 0;
	}
	AvARRAY(av)[0] = SvREFCNT_inc(frozen);

	/*
	 * Call the hook as:
	 *
	 *   $object->STORABLE_thaw($cloning, $frozen, @refs);
	 * 
	 * where $object is our blessed (empty) object, $cloning is a boolean
	 * telling whether we're running a deep clone, $frozen is the frozen
	 * string the user gave us in his serializing hook, and @refs, which may
	 * be empty, is the list of extra references he returned along for us
	 * to serialize.
	 *
	 * In effect, the hook is an alternate creation routine for the class,
	 * the object itself being already created by the runtime.
	 */

	TRACEME(("calling STORABLE_thaw on %s at 0x%"UVxf" (%"IVdf" args)",
		 class, PTR2UV(sv), (IV) AvFILLp(av) + 1));

	rv = newRV(sv);
	(void) scalar_call(aTHX_ rv, hook, clone, av, G_SCALAR|G_DISCARD);
	SvREFCNT_dec(rv);

	/*
	 * Final cleanup.
	 */

	SvREFCNT_dec(frozen);
	av_undef(av);
	sv_free((SV *) av);
	if (!(flags & SHF_IDX_CLASSNAME) && class != buf)
		Safefree(class);

	/*
	 * If we had an <extra> type, then the object was not as simple, and
	 * we need to restore extra magic now.
	 */

	if (!extra_type)
		return sv;

	TRACEME(("retrieving magic object for 0x%"UVxf"...", PTR2UV(sv)));

	rv = retrieve(aTHX_ cxt, 0);		/* Retrieve <magic object> */

	TRACEME(("restoring the magic object 0x%"UVxf" part of 0x%"UVxf,
		PTR2UV(rv), PTR2UV(sv)));

	switch (extra_type) {
	case SHT_TSCALAR:
		sv_upgrade(sv, SVt_PVMG);
		break;
	case SHT_TARRAY:
		sv_upgrade(sv, SVt_PVAV);
		AvREAL_off((AV *)sv);
		break;
	case SHT_THASH:
		sv_upgrade(sv, SVt_PVHV);
		break;
	default:
		CROAK(("Forgot to deal with extra type %d", extra_type));
		break;
	}

	/*
	 * Adding the magic only now, well after the STORABLE_thaw hook was called
	 * means the hook cannot know it deals with an object whose variable is
	 * tied.  But this is happening when retrieving $o in the following case:
	 *
	 *	my %h;
	 *  tie %h, 'FOO';
	 *	my $o = bless \%h, 'BAR';
	 *
	 * The 'BAR' class is NOT the one where %h is tied into.  Therefore, as
	 * far as the 'BAR' class is concerned, the fact that %h is not a REAL
	 * hash but a tied one should not matter at all, and remain transparent.
	 * This means the magic must be restored by Storable AFTER the hook is
	 * called.
	 *
	 * That looks very reasonable to me, but then I've come up with this
	 * after a bug report from David Nesting, who was trying to store such
	 * an object and caused Storable to fail.  And unfortunately, it was
	 * also the easiest way to retrofit support for blessed ref to tied objects
	 * into the existing design.  -- RAM, 17/02/2001
	 */

	sv_magic(sv, rv, mtype, Nullch, 0);
	SvREFCNT_dec(rv);			/* Undo refcnt inc from sv_magic() */

	return sv;
}

/*
 * retrieve_ref
 *
 * Retrieve reference to some other scalar.
 * Layout is SX_REF <object>, with SX_REF already read.
 */
static SV *retrieve_ref(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *rv;
	SV *sv;

	TRACEME(("retrieve_ref (#%d)", cxt->tagnum));

	/*
	 * We need to create the SV that holds the reference to the yet-to-retrieve
	 * object now, so that we may record the address in the seen table.
	 * Otherwise, if the object to retrieve references us, we won't be able
	 * to resolve the SX_OBJECT we'll see at that point! Hence we cannot
	 * do the retrieve first and use rv = newRV(sv) since it will be too late
	 * for SEEN() recording.
	 */

	rv = NEWSV(10002, 0);
	SEEN(rv, cname, 0);		/* Will return if rv is null */
	sv = retrieve(aTHX_ cxt, 0);	/* Retrieve <object> */
	if (!sv)
		return (SV *) 0;	/* Failed */

	/*
	 * WARNING: breaks RV encapsulation.
	 *
	 * Now for the tricky part. We have to upgrade our existing SV, so that
	 * it is now an RV on sv... Again, we cheat by duplicating the code
	 * held in newSVrv(), since we already got our SV from retrieve().
	 *
	 * We don't say:
	 *
	 *		SvRV(rv) = SvREFCNT_inc(sv);
	 *
	 * here because the reference count we got from retrieve() above is
	 * already correct: if the object was retrieved from the file, then
	 * its reference count is one. Otherwise, if it was retrieved via
	 * an SX_OBJECT indication, a ref count increment was done.
	 */

	if (cname) {
		/* No need to do anything, as rv will already be PVMG.  */
		assert (SvTYPE(rv) >= SVt_RV);
	} else {
		sv_upgrade(rv, SVt_RV);
	}

	SvRV(rv) = sv;				/* $rv = \$sv */
	SvROK_on(rv);

	TRACEME(("ok (retrieve_ref at 0x%"UVxf")", PTR2UV(rv)));

	return rv;
}

/*
 * retrieve_overloaded
 *
 * Retrieve reference to some other scalar with overloading.
 * Layout is SX_OVERLOAD <object>, with SX_OVERLOAD already read.
 */
static SV *retrieve_overloaded(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *rv;
	SV *sv;
	HV *stash;

	TRACEME(("retrieve_overloaded (#%d)", cxt->tagnum));

	/*
	 * Same code as retrieve_ref(), duplicated to avoid extra call.
	 */

	rv = NEWSV(10002, 0);
	SEEN(rv, cname, 0);		/* Will return if rv is null */
	sv = retrieve(aTHX_ cxt, 0);	/* Retrieve <object> */
	if (!sv)
		return (SV *) 0;	/* Failed */

	/*
	 * WARNING: breaks RV encapsulation.
	 */

	sv_upgrade(rv, SVt_RV);
	SvRV(rv) = sv;				/* $rv = \$sv */
	SvROK_on(rv);

	/*
	 * Restore overloading magic.
	 */

	stash = SvTYPE(sv) ? (HV *) SvSTASH (sv) : 0;
	if (!stash) {
		CROAK(("Cannot restore overloading on %s(0x%"UVxf
		       ") (package <unknown>)",
		       sv_reftype(sv, FALSE),
		       PTR2UV(sv)));
	}
	if (!Gv_AMG(stash)) {
		SV *psv = newSVpvn("require ", 8);
		const char *package = HvNAME(stash);
		sv_catpv(psv, package);

		TRACEME(("No overloading defined for package %s", package));
		TRACEME(("Going to require module '%s' with '%s'", package, SvPVX(psv)));

		perl_eval_sv(psv, G_DISCARD);
		sv_free(psv);
		if (!Gv_AMG(stash)) {
			CROAK(("Cannot restore overloading on %s(0x%"UVxf
			       ") (package %s) (even after a \"require %s;\")",
			       sv_reftype(sv, FALSE),
			       PTR2UV(sv),
			       package, package));
		}
	}

	SvAMAGIC_on(rv);

	TRACEME(("ok (retrieve_overloaded at 0x%"UVxf")", PTR2UV(rv)));

	return rv;
}

/*
 * retrieve_tied_array
 *
 * Retrieve tied array
 * Layout is SX_TIED_ARRAY <object>, with SX_TIED_ARRAY already read.
 */
static SV *retrieve_tied_array(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *tv;
	SV *sv;

	TRACEME(("retrieve_tied_array (#%d)", cxt->tagnum));

	tv = NEWSV(10002, 0);
	SEEN(tv, cname, 0);			/* Will return if tv is null */
	sv = retrieve(aTHX_ cxt, 0);		/* Retrieve <object> */
	if (!sv)
		return (SV *) 0;		/* Failed */

	sv_upgrade(tv, SVt_PVAV);
	AvREAL_off((AV *)tv);
	sv_magic(tv, sv, 'P', Nullch, 0);
	SvREFCNT_dec(sv);			/* Undo refcnt inc from sv_magic() */

	TRACEME(("ok (retrieve_tied_array at 0x%"UVxf")", PTR2UV(tv)));

	return tv;
}

/*
 * retrieve_tied_hash
 *
 * Retrieve tied hash
 * Layout is SX_TIED_HASH <object>, with SX_TIED_HASH already read.
 */
static SV *retrieve_tied_hash(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *tv;
	SV *sv;

	TRACEME(("retrieve_tied_hash (#%d)", cxt->tagnum));

	tv = NEWSV(10002, 0);
	SEEN(tv, cname, 0);			/* Will return if tv is null */
	sv = retrieve(aTHX_ cxt, 0);		/* Retrieve <object> */
	if (!sv)
		return (SV *) 0;		/* Failed */

	sv_upgrade(tv, SVt_PVHV);
	sv_magic(tv, sv, 'P', Nullch, 0);
	SvREFCNT_dec(sv);			/* Undo refcnt inc from sv_magic() */

	TRACEME(("ok (retrieve_tied_hash at 0x%"UVxf")", PTR2UV(tv)));

	return tv;
}

/*
 * retrieve_tied_scalar
 *
 * Retrieve tied scalar
 * Layout is SX_TIED_SCALAR <object>, with SX_TIED_SCALAR already read.
 */
static SV *retrieve_tied_scalar(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *tv;
	SV *sv, *obj = NULL;

	TRACEME(("retrieve_tied_scalar (#%d)", cxt->tagnum));

	tv = NEWSV(10002, 0);
	SEEN(tv, cname, 0);			/* Will return if rv is null */
	sv = retrieve(aTHX_ cxt, 0);		/* Retrieve <object> */
	if (!sv) {
		return (SV *) 0;		/* Failed */
	}
	else if (SvTYPE(sv) != SVt_NULL) {
		obj = sv;
	}

	sv_upgrade(tv, SVt_PVMG);
	sv_magic(tv, obj, 'q', Nullch, 0);

	if (obj) {
		/* Undo refcnt inc from sv_magic() */
		SvREFCNT_dec(obj);
	}

	TRACEME(("ok (retrieve_tied_scalar at 0x%"UVxf")", PTR2UV(tv)));

	return tv;
}

/*
 * retrieve_tied_key
 *
 * Retrieve reference to value in a tied hash.
 * Layout is SX_TIED_KEY <object> <key>, with SX_TIED_KEY already read.
 */
static SV *retrieve_tied_key(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *tv;
	SV *sv;
	SV *key;

	TRACEME(("retrieve_tied_key (#%d)", cxt->tagnum));

	tv = NEWSV(10002, 0);
	SEEN(tv, cname, 0);			/* Will return if tv is null */
	sv = retrieve(aTHX_ cxt, 0);		/* Retrieve <object> */
	if (!sv)
		return (SV *) 0;		/* Failed */

	key = retrieve(aTHX_ cxt, 0);		/* Retrieve <key> */
	if (!key)
		return (SV *) 0;		/* Failed */

	sv_upgrade(tv, SVt_PVMG);
	sv_magic(tv, sv, 'p', (char *)key, HEf_SVKEY);
	SvREFCNT_dec(key);			/* Undo refcnt inc from sv_magic() */
	SvREFCNT_dec(sv);			/* Undo refcnt inc from sv_magic() */

	return tv;
}

/*
 * retrieve_tied_idx
 *
 * Retrieve reference to value in a tied array.
 * Layout is SX_TIED_IDX <object> <idx>, with SX_TIED_IDX already read.
 */
static SV *retrieve_tied_idx(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *tv;
	SV *sv;
	I32 idx;

	TRACEME(("retrieve_tied_idx (#%d)", cxt->tagnum));

	tv = NEWSV(10002, 0);
	SEEN(tv, cname, 0);			/* Will return if tv is null */
	sv = retrieve(aTHX_ cxt, 0);		/* Retrieve <object> */
	if (!sv)
		return (SV *) 0;		/* Failed */

	RLEN(idx);					/* Retrieve <idx> */

	sv_upgrade(tv, SVt_PVMG);
	sv_magic(tv, sv, 'p', Nullch, idx);
	SvREFCNT_dec(sv);			/* Undo refcnt inc from sv_magic() */

	return tv;
}


/*
 * retrieve_lscalar
 *
 * Retrieve defined long (string) scalar.
 *
 * Layout is SX_LSCALAR <length> <data>, with SX_LSCALAR already read.
 * The scalar is "long" in that <length> is larger than LG_SCALAR so it
 * was not stored on a single byte.
 */
static SV *retrieve_lscalar(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 len;
	SV *sv;

	RLEN(len);
	TRACEME(("retrieve_lscalar (#%d), len = %"IVdf, cxt->tagnum, (IV) len));

	/*
	 * Allocate an empty scalar of the suitable length.
	 */

	sv = NEWSV(10002, len);
	SEEN(sv, cname, 0);	/* Associate this new scalar with tag "tagnum" */

	/*
	 * WARNING: duplicates parts of sv_setpv and breaks SV data encapsulation.
	 *
	 * Now, for efficiency reasons, read data directly inside the SV buffer,
	 * and perform the SV final settings directly by duplicating the final
	 * work done by sv_setpv. Since we're going to allocate lots of scalars
	 * this way, it's worth the hassle and risk.
	 */

	SAFEREAD(SvPVX(sv), len, sv);
	SvCUR_set(sv, len);				/* Record C string length */
	*SvEND(sv) = '\0';				/* Ensure it's null terminated anyway */
	(void) SvPOK_only(sv);			/* Validate string pointer */
	if (cxt->s_tainted)				/* Is input source tainted? */
		SvTAINT(sv);				/* External data cannot be trusted */

	TRACEME(("large scalar len %"IVdf" '%s'", (IV) len, SvPVX(sv)));
	TRACEME(("ok (retrieve_lscalar at 0x%"UVxf")", PTR2UV(sv)));

	return sv;
}

/*
 * retrieve_scalar
 *
 * Retrieve defined short (string) scalar.
 *
 * Layout is SX_SCALAR <length> <data>, with SX_SCALAR already read.
 * The scalar is "short" so <length> is single byte. If it is 0, there
 * is no <data> section.
 */
static SV *retrieve_scalar(pTHX_ stcxt_t *cxt, char *cname)
{
	int len;
	SV *sv;

	GETMARK(len);
	TRACEME(("retrieve_scalar (#%d), len = %d", cxt->tagnum, len));

	/*
	 * Allocate an empty scalar of the suitable length.
	 */

	sv = NEWSV(10002, len);
	SEEN(sv, cname, 0);	/* Associate this new scalar with tag "tagnum" */

	/*
	 * WARNING: duplicates parts of sv_setpv and breaks SV data encapsulation.
	 */

	if (len == 0) {
		/*
		 * newSV did not upgrade to SVt_PV so the scalar is undefined.
		 * To make it defined with an empty length, upgrade it now...
		 * Don't upgrade to a PV if the original type contains more
		 * information than a scalar.
		 */
		if (SvTYPE(sv) <= SVt_PV) {
			sv_upgrade(sv, SVt_PV);
		}
		SvGROW(sv, 1);
		*SvEND(sv) = '\0';			/* Ensure it's null terminated anyway */
		TRACEME(("ok (retrieve_scalar empty at 0x%"UVxf")", PTR2UV(sv)));
	} else {
		/*
		 * Now, for efficiency reasons, read data directly inside the SV buffer,
		 * and perform the SV final settings directly by duplicating the final
		 * work done by sv_setpv. Since we're going to allocate lots of scalars
		 * this way, it's worth the hassle and risk.
		 */
		SAFEREAD(SvPVX(sv), len, sv);
		SvCUR_set(sv, len);			/* Record C string length */
		*SvEND(sv) = '\0';			/* Ensure it's null terminated anyway */
		TRACEME(("small scalar len %d '%s'", len, SvPVX(sv)));
	}

	(void) SvPOK_only(sv);			/* Validate string pointer */
	if (cxt->s_tainted)				/* Is input source tainted? */
		SvTAINT(sv);				/* External data cannot be trusted */

	TRACEME(("ok (retrieve_scalar at 0x%"UVxf")", PTR2UV(sv)));
	return sv;
}

/*
 * retrieve_utf8str
 *
 * Like retrieve_scalar(), but tag result as utf8.
 * If we're retrieving UTF8 data in a non-UTF8 perl, croaks.
 */
static SV *retrieve_utf8str(pTHX_ stcxt_t *cxt, char *cname)
{
    SV *sv;

    TRACEME(("retrieve_utf8str"));

    sv = retrieve_scalar(aTHX_ cxt, cname);
    if (sv) {
#ifdef HAS_UTF8_SCALARS
        SvUTF8_on(sv);
#else
        if (cxt->use_bytes < 0)
            cxt->use_bytes
                = (SvTRUE(perl_get_sv("Storable::drop_utf8", TRUE))
                   ? 1 : 0);
        if (cxt->use_bytes == 0)
            UTF8_CROAK();
#endif
    }

    return sv;
}

/*
 * retrieve_lutf8str
 *
 * Like retrieve_lscalar(), but tag result as utf8.
 * If we're retrieving UTF8 data in a non-UTF8 perl, croaks.
 */
static SV *retrieve_lutf8str(pTHX_ stcxt_t *cxt, char *cname)
{
    SV *sv;

    TRACEME(("retrieve_lutf8str"));

    sv = retrieve_lscalar(aTHX_ cxt, cname);
    if (sv) {
#ifdef HAS_UTF8_SCALARS
        SvUTF8_on(sv);
#else
        if (cxt->use_bytes < 0)
            cxt->use_bytes
                = (SvTRUE(perl_get_sv("Storable::drop_utf8", TRUE))
                   ? 1 : 0);
        if (cxt->use_bytes == 0)
            UTF8_CROAK();
#endif
    }
    return sv;
}

/*
 * retrieve_integer
 *
 * Retrieve defined integer.
 * Layout is SX_INTEGER <data>, whith SX_INTEGER already read.
 */
static SV *retrieve_integer(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *sv;
	IV iv;

	TRACEME(("retrieve_integer (#%d)", cxt->tagnum));

	READ(&iv, sizeof(iv));
	sv = newSViv(iv);
	SEEN(sv, cname, 0);	/* Associate this new scalar with tag "tagnum" */

	TRACEME(("integer %"IVdf, iv));
	TRACEME(("ok (retrieve_integer at 0x%"UVxf")", PTR2UV(sv)));

	return sv;
}

/*
 * retrieve_netint
 *
 * Retrieve defined integer in network order.
 * Layout is SX_NETINT <data>, whith SX_NETINT already read.
 */
static SV *retrieve_netint(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *sv;
	I32 iv;

	TRACEME(("retrieve_netint (#%d)", cxt->tagnum));

	READ_I32(iv);
#ifdef HAS_NTOHL
	sv = newSViv((int) ntohl(iv));
	TRACEME(("network integer %d", (int) ntohl(iv)));
#else
	sv = newSViv(iv);
	TRACEME(("network integer (as-is) %d", iv));
#endif
	SEEN(sv, cname, 0);	/* Associate this new scalar with tag "tagnum" */

	TRACEME(("ok (retrieve_netint at 0x%"UVxf")", PTR2UV(sv)));

	return sv;
}

/*
 * retrieve_double
 *
 * Retrieve defined double.
 * Layout is SX_DOUBLE <data>, whith SX_DOUBLE already read.
 */
static SV *retrieve_double(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *sv;
	NV nv;

	TRACEME(("retrieve_double (#%d)", cxt->tagnum));

	READ(&nv, sizeof(nv));
	sv = newSVnv(nv);
	SEEN(sv, cname, 0);	/* Associate this new scalar with tag "tagnum" */

	TRACEME(("double %"NVff, nv));
	TRACEME(("ok (retrieve_double at 0x%"UVxf")", PTR2UV(sv)));

	return sv;
}

/*
 * retrieve_byte
 *
 * Retrieve defined byte (small integer within the [-128, +127] range).
 * Layout is SX_BYTE <data>, whith SX_BYTE already read.
 */
static SV *retrieve_byte(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *sv;
	int siv;
	signed char tmp;	/* Workaround for AIX cc bug --H.Merijn Brand */

	TRACEME(("retrieve_byte (#%d)", cxt->tagnum));

	GETMARK(siv);
	TRACEME(("small integer read as %d", (unsigned char) siv));
	tmp = (unsigned char) siv - 128;
	sv = newSViv(tmp);
	SEEN(sv, cname, 0);	/* Associate this new scalar with tag "tagnum" */

	TRACEME(("byte %d", tmp));
	TRACEME(("ok (retrieve_byte at 0x%"UVxf")", PTR2UV(sv)));

	return sv;
}

/*
 * retrieve_undef
 *
 * Return the undefined value.
 */
static SV *retrieve_undef(pTHX_ stcxt_t *cxt, char *cname)
{
	SV* sv;

	TRACEME(("retrieve_undef"));

	sv = newSV(0);
	SEEN(sv, cname, 0);

	return sv;
}

/*
 * retrieve_sv_undef
 *
 * Return the immortal undefined value.
 */
static SV *retrieve_sv_undef(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *sv = &PL_sv_undef;

	TRACEME(("retrieve_sv_undef"));

	/* Special case PL_sv_undef, as av_fetch uses it internally to mark
	   deleted elements, and will return NULL (fetch failed) whenever it
	   is fetched.  */
	if (cxt->where_is_undef == -1) {
		cxt->where_is_undef = cxt->tagnum;
	}
	SEEN(sv, cname, 1);
	return sv;
}

/*
 * retrieve_sv_yes
 *
 * Return the immortal yes value.
 */
static SV *retrieve_sv_yes(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *sv = &PL_sv_yes;

	TRACEME(("retrieve_sv_yes"));

	SEEN(sv, cname, 1);
	return sv;
}

/*
 * retrieve_sv_no
 *
 * Return the immortal no value.
 */
static SV *retrieve_sv_no(pTHX_ stcxt_t *cxt, char *cname)
{
	SV *sv = &PL_sv_no;

	TRACEME(("retrieve_sv_no"));

	SEEN(sv, cname, 1);
	return sv;
}

/*
 * retrieve_array
 *
 * Retrieve a whole array.
 * Layout is SX_ARRAY <size> followed by each item, in increading index order.
 * Each item is stored as <object>.
 *
 * When we come here, SX_ARRAY has been read already.
 */
static SV *retrieve_array(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 len;
	I32 i;
	AV *av;
	SV *sv;

	TRACEME(("retrieve_array (#%d)", cxt->tagnum));

	/*
	 * Read length, and allocate array, then pre-extend it.
	 */

	RLEN(len);
	TRACEME(("size = %d", len));
	av = newAV();
	SEEN(av, cname, 0);			/* Will return if array not allocated nicely */
	if (len)
		av_extend(av, len);
	else
		return (SV *) av;		/* No data follow if array is empty */

	/*
	 * Now get each item in turn...
	 */

	for (i = 0; i < len; i++) {
		TRACEME(("(#%d) item", i));
		sv = retrieve(aTHX_ cxt, 0);			/* Retrieve item */
		if (!sv)
			return (SV *) 0;
		if (av_store(av, i, sv) == 0)
			return (SV *) 0;
	}

	TRACEME(("ok (retrieve_array at 0x%"UVxf")", PTR2UV(av)));

	return (SV *) av;
}

/*
 * retrieve_hash
 *
 * Retrieve a whole hash table.
 * Layout is SX_HASH <size> followed by each key/value pair, in random order.
 * Keys are stored as <length> <data>, the <data> section being omitted
 * if length is 0.
 * Values are stored as <object>.
 *
 * When we come here, SX_HASH has been read already.
 */
static SV *retrieve_hash(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 len;
	I32 size;
	I32 i;
	HV *hv;
	SV *sv;

	TRACEME(("retrieve_hash (#%d)", cxt->tagnum));

	/*
	 * Read length, allocate table.
	 */

	RLEN(len);
	TRACEME(("size = %d", len));
	hv = newHV();
	SEEN(hv, cname, 0);		/* Will return if table not allocated properly */
	if (len == 0)
		return (SV *) hv;	/* No data follow if table empty */
	hv_ksplit(hv, len);		/* pre-extend hash to save multiple splits */

	/*
	 * Now get each key/value pair in turn...
	 */

	for (i = 0; i < len; i++) {
		/*
		 * Get value first.
		 */

		TRACEME(("(#%d) value", i));
		sv = retrieve(aTHX_ cxt, 0);
		if (!sv)
			return (SV *) 0;

		/*
		 * Get key.
		 * Since we're reading into kbuf, we must ensure we're not
		 * recursing between the read and the hv_store() where it's used.
		 * Hence the key comes after the value.
		 */

		RLEN(size);						/* Get key size */
		KBUFCHK((STRLEN)size);					/* Grow hash key read pool if needed */
		if (size)
			READ(kbuf, size);
		kbuf[size] = '\0';				/* Mark string end, just in case */
		TRACEME(("(#%d) key '%s'", i, kbuf));

		/*
		 * Enter key/value pair into hash table.
		 */

		if (hv_store(hv, kbuf, (U32) size, sv, 0) == 0)
			return (SV *) 0;
	}

	TRACEME(("ok (retrieve_hash at 0x%"UVxf")", PTR2UV(hv)));

	return (SV *) hv;
}

/*
 * retrieve_hash
 *
 * Retrieve a whole hash table.
 * Layout is SX_HASH <size> followed by each key/value pair, in random order.
 * Keys are stored as <length> <data>, the <data> section being omitted
 * if length is 0.
 * Values are stored as <object>.
 *
 * When we come here, SX_HASH has been read already.
 */
static SV *retrieve_flag_hash(pTHX_ stcxt_t *cxt, char *cname)
{
    I32 len;
    I32 size;
    I32 i;
    HV *hv;
    SV *sv;
    int hash_flags;

    GETMARK(hash_flags);
    TRACEME(("retrieve_flag_hash (#%d)", cxt->tagnum));
    /*
     * Read length, allocate table.
     */

#ifndef HAS_RESTRICTED_HASHES
    if (hash_flags & SHV_RESTRICTED) {
        if (cxt->derestrict < 0)
            cxt->derestrict
                = (SvTRUE(perl_get_sv("Storable::downgrade_restricted", TRUE))
                   ? 1 : 0);
        if (cxt->derestrict == 0)
            RESTRICTED_HASH_CROAK();
    }
#endif

    RLEN(len);
    TRACEME(("size = %d, flags = %d", len, hash_flags));
    hv = newHV();
    SEEN(hv, cname, 0);		/* Will return if table not allocated properly */
    if (len == 0)
        return (SV *) hv;	/* No data follow if table empty */
    hv_ksplit(hv, len);		/* pre-extend hash to save multiple splits */

    /*
     * Now get each key/value pair in turn...
     */

    for (i = 0; i < len; i++) {
        int flags;
        int store_flags = 0;
        /*
         * Get value first.
         */

        TRACEME(("(#%d) value", i));
        sv = retrieve(aTHX_ cxt, 0);
        if (!sv)
            return (SV *) 0;

        GETMARK(flags);
#ifdef HAS_RESTRICTED_HASHES
        if ((hash_flags & SHV_RESTRICTED) && (flags & SHV_K_LOCKED))
            SvREADONLY_on(sv);
#endif

        if (flags & SHV_K_ISSV) {
            /* XXX you can't set a placeholder with an SV key.
               Then again, you can't get an SV key.
               Without messing around beyond what the API is supposed to do.
            */
            SV *keysv;
            TRACEME(("(#%d) keysv, flags=%d", i, flags));
            keysv = retrieve(aTHX_ cxt, 0);
            if (!keysv)
                return (SV *) 0;

            if (!hv_store_ent(hv, keysv, sv, 0))
                return (SV *) 0;
        } else {
            /*
             * Get key.
             * Since we're reading into kbuf, we must ensure we're not
             * recursing between the read and the hv_store() where it's used.
             * Hence the key comes after the value.
             */

            if (flags & SHV_K_PLACEHOLDER) {
                SvREFCNT_dec (sv);
                sv = &PL_sv_placeholder;
		store_flags |= HVhek_PLACEHOLD;
	    }
            if (flags & SHV_K_UTF8) {
#ifdef HAS_UTF8_HASHES
                store_flags |= HVhek_UTF8;
#else
                if (cxt->use_bytes < 0)
                    cxt->use_bytes
                        = (SvTRUE(perl_get_sv("Storable::drop_utf8", TRUE))
                           ? 1 : 0);
                if (cxt->use_bytes == 0)
                    UTF8_CROAK();
#endif
            }
#ifdef HAS_UTF8_HASHES
            if (flags & SHV_K_WASUTF8)
		store_flags |= HVhek_WASUTF8;
#endif

            RLEN(size);						/* Get key size */
            KBUFCHK((STRLEN)size);				/* Grow hash key read pool if needed */
            if (size)
                READ(kbuf, size);
            kbuf[size] = '\0';				/* Mark string end, just in case */
            TRACEME(("(#%d) key '%s' flags %X store_flags %X", i, kbuf,
		     flags, store_flags));

            /*
             * Enter key/value pair into hash table.
             */

#ifdef HAS_RESTRICTED_HASHES
            if (hv_store_flags(hv, kbuf, size, sv, 0, store_flags) == 0)
                return (SV *) 0;
#else
            if (!(store_flags & HVhek_PLACEHOLD))
                if (hv_store(hv, kbuf, size, sv, 0) == 0)
                    return (SV *) 0;
#endif
	}
    }
#ifdef HAS_RESTRICTED_HASHES
    if (hash_flags & SHV_RESTRICTED)
        SvREADONLY_on(hv);
#endif

    TRACEME(("ok (retrieve_hash at 0x%"UVxf")", PTR2UV(hv)));

    return (SV *) hv;
}

/*
 * retrieve_code
 *
 * Return a code reference.
 */
static SV *retrieve_code(pTHX_ stcxt_t *cxt, char *cname)
{
#if PERL_VERSION < 6
    CROAK(("retrieve_code does not work with perl 5.005 or less\n"));
#else
	dSP;
	int type, count, tagnum;
	SV *cv;
	SV *sv, *text, *sub;

	TRACEME(("retrieve_code (#%d)", cxt->tagnum));

	/*
	 *  Insert dummy SV in the aseen array so that we don't screw
	 *  up the tag numbers.  We would just make the internal
	 *  scalar an untagged item in the stream, but
	 *  retrieve_scalar() calls SEEN().  So we just increase the
	 *  tag number.
	 */
	tagnum = cxt->tagnum;
	sv = newSViv(0);
	SEEN(sv, cname, 0);

	/*
	 * Retrieve the source of the code reference
	 * as a small or large scalar
	 */

	GETMARK(type);
	switch (type) {
	case SX_SCALAR:
		text = retrieve_scalar(aTHX_ cxt, cname);
		break;
	case SX_LSCALAR:
		text = retrieve_lscalar(aTHX_ cxt, cname);
		break;
	default:
		CROAK(("Unexpected type %d in retrieve_code\n", type));
	}

	/*
	 * prepend "sub " to the source
	 */

	sub = newSVpvn("sub ", 4);
	sv_catpv(sub, SvPV_nolen(text)); /* XXX no sv_catsv! */
	SvREFCNT_dec(text);

	/*
	 * evaluate the source to a code reference and use the CV value
	 */

	if (cxt->eval == NULL) {
		cxt->eval = perl_get_sv("Storable::Eval", TRUE);
		SvREFCNT_inc(cxt->eval);
	}
	if (!SvTRUE(cxt->eval)) {
		if (
			cxt->forgive_me == 0 ||
			(cxt->forgive_me < 0 && !(cxt->forgive_me =
				SvTRUE(perl_get_sv("Storable::forgive_me", TRUE)) ? 1 : 0))
		) {
			CROAK(("Can't eval, please set $Storable::Eval to a true value"));
		} else {
			sv = newSVsv(sub);
			/* fix up the dummy entry... */
			av_store(cxt->aseen, tagnum, SvREFCNT_inc(sv));
			return sv;
		}
	}

	ENTER;
	SAVETMPS;

	if (SvROK(cxt->eval) && SvTYPE(SvRV(cxt->eval)) == SVt_PVCV) {
		SV* errsv = get_sv("@", TRUE);
		sv_setpv(errsv, "");					/* clear $@ */
		PUSHMARK(sp);
		XPUSHs(sv_2mortal(newSVsv(sub)));
		PUTBACK;
		count = call_sv(cxt->eval, G_SCALAR);
		SPAGAIN;
		if (count != 1)
			CROAK(("Unexpected return value from $Storable::Eval callback\n"));
		cv = POPs;
		if (SvTRUE(errsv)) {
			CROAK(("code %s caused an error: %s",
				SvPV_nolen(sub), SvPV_nolen(errsv)));
		}
		PUTBACK;
	} else {
		cv = eval_pv(SvPV_nolen(sub), TRUE);
	}
	if (cv && SvROK(cv) && SvTYPE(SvRV(cv)) == SVt_PVCV) {
	    sv = SvRV(cv);
	} else {
	    CROAK(("code %s did not evaluate to a subroutine reference\n", SvPV_nolen(sub)));
	}

	SvREFCNT_inc(sv); /* XXX seems to be necessary */
	SvREFCNT_dec(sub);

	FREETMPS;
	LEAVE;
	/* fix up the dummy entry... */
	av_store(cxt->aseen, tagnum, SvREFCNT_inc(sv));

	return sv;
#endif
}

/*
 * old_retrieve_array
 *
 * Retrieve a whole array in pre-0.6 binary format.
 *
 * Layout is SX_ARRAY <size> followed by each item, in increading index order.
 * Each item is stored as SX_ITEM <object> or SX_IT_UNDEF for "holes".
 *
 * When we come here, SX_ARRAY has been read already.
 */
static SV *old_retrieve_array(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 len;
	I32 i;
	AV *av;
	SV *sv;
	int c;

	TRACEME(("old_retrieve_array (#%d)", cxt->tagnum));

	/*
	 * Read length, and allocate array, then pre-extend it.
	 */

	RLEN(len);
	TRACEME(("size = %d", len));
	av = newAV();
	SEEN(av, 0, 0);				/* Will return if array not allocated nicely */
	if (len)
		av_extend(av, len);
	else
		return (SV *) av;		/* No data follow if array is empty */

	/*
	 * Now get each item in turn...
	 */

	for (i = 0; i < len; i++) {
		GETMARK(c);
		if (c == SX_IT_UNDEF) {
			TRACEME(("(#%d) undef item", i));
			continue;			/* av_extend() already filled us with undef */
		}
		if (c != SX_ITEM)
			(void) retrieve_other(aTHX_ (stcxt_t *) 0, 0);	/* Will croak out */
		TRACEME(("(#%d) item", i));
		sv = retrieve(aTHX_ cxt, 0);						/* Retrieve item */
		if (!sv)
			return (SV *) 0;
		if (av_store(av, i, sv) == 0)
			return (SV *) 0;
	}

	TRACEME(("ok (old_retrieve_array at 0x%"UVxf")", PTR2UV(av)));

	return (SV *) av;
}

/*
 * old_retrieve_hash
 *
 * Retrieve a whole hash table in pre-0.6 binary format.
 *
 * Layout is SX_HASH <size> followed by each key/value pair, in random order.
 * Keys are stored as SX_KEY <length> <data>, the <data> section being omitted
 * if length is 0.
 * Values are stored as SX_VALUE <object> or SX_VL_UNDEF for "holes".
 *
 * When we come here, SX_HASH has been read already.
 */
static SV *old_retrieve_hash(pTHX_ stcxt_t *cxt, char *cname)
{
	I32 len;
	I32 size;
	I32 i;
	HV *hv;
	SV *sv = (SV *) 0;
	int c;
	static SV *sv_h_undef = (SV *) 0;		/* hv_store() bug */

	TRACEME(("old_retrieve_hash (#%d)", cxt->tagnum));

	/*
	 * Read length, allocate table.
	 */

	RLEN(len);
	TRACEME(("size = %d", len));
	hv = newHV();
	SEEN(hv, 0, 0);			/* Will return if table not allocated properly */
	if (len == 0)
		return (SV *) hv;	/* No data follow if table empty */
	hv_ksplit(hv, len);		/* pre-extend hash to save multiple splits */

	/*
	 * Now get each key/value pair in turn...
	 */

	for (i = 0; i < len; i++) {
		/*
		 * Get value first.
		 */

		GETMARK(c);
		if (c == SX_VL_UNDEF) {
			TRACEME(("(#%d) undef value", i));
			/*
			 * Due to a bug in hv_store(), it's not possible to pass
			 * &PL_sv_undef to hv_store() as a value, otherwise the
			 * associated key will not be creatable any more. -- RAM, 14/01/97
			 */
			if (!sv_h_undef)
				sv_h_undef = newSVsv(&PL_sv_undef);
			sv = SvREFCNT_inc(sv_h_undef);
		} else if (c == SX_VALUE) {
			TRACEME(("(#%d) value", i));
			sv = retrieve(aTHX_ cxt, 0);
			if (!sv)
				return (SV *) 0;
		} else
			(void) retrieve_other(aTHX_ (stcxt_t *) 0, 0);	/* Will croak out */

		/*
		 * Get key.
		 * Since we're reading into kbuf, we must ensure we're not
		 * recursing between the read and the hv_store() where it's used.
		 * Hence the key comes after the value.
		 */

		GETMARK(c);
		if (c != SX_KEY)
			(void) retrieve_other(aTHX_ (stcxt_t *) 0, 0);	/* Will croak out */
		RLEN(size);						/* Get key size */
		KBUFCHK((STRLEN)size);					/* Grow hash key read pool if needed */
		if (size)
			READ(kbuf, size);
		kbuf[size] = '\0';				/* Mark string end, just in case */
		TRACEME(("(#%d) key '%s'", i, kbuf));

		/*
		 * Enter key/value pair into hash table.
		 */

		if (hv_store(hv, kbuf, (U32) size, sv, 0) == 0)
			return (SV *) 0;
	}

	TRACEME(("ok (retrieve_hash at 0x%"UVxf")", PTR2UV(hv)));

	return (SV *) hv;
}

/***
 *** Retrieval engine.
 ***/

/*
 * magic_check
 *
 * Make sure the stored data we're trying to retrieve has been produced
 * on an ILP compatible system with the same byteorder. It croaks out in
 * case an error is detected. [ILP = integer-long-pointer sizes]
 * Returns null if error is detected, &PL_sv_undef otherwise.
 *
 * Note that there's no byte ordering info emitted when network order was
 * used at store time.
 */
static SV *magic_check(pTHX_ stcxt_t *cxt)
{
    /* The worst case for a malicious header would be old magic (which is
       longer), major, minor, byteorder length byte of 255, 255 bytes of
       garbage, sizeof int, long, pointer, NV.
       So the worse of that we can read is 255 bytes of garbage plus 4.
       Err, I am assuming 8 bit bytes here. Please file a bug report if you're
       compiling perl on a system with chars that are larger than 8 bits.
       (Even Crays aren't *that* perverse).
    */
    unsigned char buf[4 + 255];
    unsigned char *current;
    int c;
    int length;
    int use_network_order;
    int use_NV_size;
    int version_major;
    int version_minor = 0;

    TRACEME(("magic_check"));

    /*
     * The "magic number" is only for files, not when freezing in memory.
     */

    if (cxt->fio) {
        /* This includes the '\0' at the end.  I want to read the extra byte,
           which is usually going to be the major version number.  */
        STRLEN len = sizeof(magicstr);
        STRLEN old_len;

        READ(buf, (SSize_t)(len));	/* Not null-terminated */

        /* Point at the byte after the byte we read.  */
        current = buf + --len;	/* Do the -- outside of macros.  */

        if (memNE(buf, magicstr, len)) {
            /*
             * Try to read more bytes to check for the old magic number, which
             * was longer.
             */

            TRACEME(("trying for old magic number"));

            old_len = sizeof(old_magicstr) - 1;
            READ(current + 1, (SSize_t)(old_len - len));
            
            if (memNE(buf, old_magicstr, old_len))
                CROAK(("File is not a perl storable"));
            current = buf + old_len;
        }
        use_network_order = *current;
    } else
	GETMARK(use_network_order);
        
    /*
     * Starting with 0.6, the "use_network_order" byte flag is also used to
     * indicate the version number of the binary, and therefore governs the
     * setting of sv_retrieve_vtbl. See magic_write().
     */

    version_major = use_network_order >> 1;
    cxt->retrieve_vtbl = version_major ? sv_retrieve : sv_old_retrieve;

    TRACEME(("magic_check: netorder = 0x%x", use_network_order));


    /*
     * Starting with 0.7 (binary major 2), a full byte is dedicated to the
     * minor version of the protocol.  See magic_write().
     */

    if (version_major > 1)
        GETMARK(version_minor);

    cxt->ver_major = version_major;
    cxt->ver_minor = version_minor;

    TRACEME(("binary image version is %d.%d", version_major, version_minor));

    /*
     * Inter-operability sanity check: we can't retrieve something stored
     * using a format more recent than ours, because we have no way to
     * know what has changed, and letting retrieval go would mean a probable
     * failure reporting a "corrupted" storable file.
     */

    if (
        version_major > STORABLE_BIN_MAJOR ||
        (version_major == STORABLE_BIN_MAJOR &&
         version_minor > STORABLE_BIN_MINOR)
        ) {
        int croak_now = 1;
        TRACEME(("but I am version is %d.%d", STORABLE_BIN_MAJOR,
                 STORABLE_BIN_MINOR));

        if (version_major == STORABLE_BIN_MAJOR) {
            TRACEME(("cxt->accept_future_minor is %d",
                     cxt->accept_future_minor));
            if (cxt->accept_future_minor < 0)
                cxt->accept_future_minor
                    = (SvTRUE(perl_get_sv("Storable::accept_future_minor",
                                          TRUE))
                       ? 1 : 0);
            if (cxt->accept_future_minor == 1)
                croak_now = 0;  /* Don't croak yet.  */
        }
        if (croak_now) {
            CROAK(("Storable binary image v%d.%d more recent than I am (v%d.%d)",
                   version_major, version_minor,
                   STORABLE_BIN_MAJOR, STORABLE_BIN_MINOR));
        }
    }

    /*
     * If they stored using network order, there's no byte ordering
     * information to check.
     */

    if ((cxt->netorder = (use_network_order & 0x1)))	/* Extra () for -Wall */
        return &PL_sv_undef;			/* No byte ordering info */

    /* In C truth is 1, falsehood is 0. Very convienient.  */
    use_NV_size = version_major >= 2 && version_minor >= 2;

    GETMARK(c);
    length = c + 3 + use_NV_size;
    READ(buf, length);	/* Not null-terminated */

    TRACEME(("byte order '%.*s' %d", c, buf, c));

#ifdef USE_56_INTERWORK_KLUDGE
    /* No point in caching this in the context as we only need it once per
       retrieve, and we need to recheck it each read.  */
    if (SvTRUE(perl_get_sv("Storable::interwork_56_64bit", TRUE))) {
        if ((c != (sizeof (byteorderstr_56) - 1))
            || memNE(buf, byteorderstr_56, c))
            CROAK(("Byte order is not compatible"));
    } else
#endif
    {
        if ((c != (sizeof (byteorderstr) - 1)) || memNE(buf, byteorderstr, c))
            CROAK(("Byte order is not compatible"));
    }

    current = buf + c;
    
    /* sizeof(int) */
    if ((int) *current++ != sizeof(int))
        CROAK(("Integer size is not compatible"));

    /* sizeof(long) */
    if ((int) *current++ != sizeof(long))
        CROAK(("Long integer size is not compatible"));

    /* sizeof(char *) */
    if ((int) *current != sizeof(char *))
        CROAK(("Pointer size is not compatible"));

    if (use_NV_size) {
        /* sizeof(NV) */
        if ((int) *++current != sizeof(NV))
            CROAK(("Double size is not compatible"));
    }

    return &PL_sv_undef;	/* OK */
}

/*
 * retrieve
 *
 * Recursively retrieve objects from the specified file and return their
 * root SV (which may be an AV or an HV for what we care).
 * Returns null if there is a problem.
 */
static SV *retrieve(pTHX_ stcxt_t *cxt, char *cname)
{
	int type;
	SV **svh;
	SV *sv;

	TRACEME(("retrieve"));

	/*
	 * Grab address tag which identifies the object if we are retrieving
	 * an older format. Since the new binary format counts objects and no
	 * longer explicitely tags them, we must keep track of the correspondance
	 * ourselves.
	 *
	 * The following section will disappear one day when the old format is
	 * no longer supported, hence the final "goto" in the "if" block.
	 */

	if (cxt->hseen) {						/* Retrieving old binary */
		stag_t tag;
		if (cxt->netorder) {
			I32 nettag;
			READ(&nettag, sizeof(I32));		/* Ordered sequence of I32 */
			tag = (stag_t) nettag;
		} else
			READ(&tag, sizeof(stag_t));		/* Original address of the SV */

		GETMARK(type);
		if (type == SX_OBJECT) {
			I32 tagn;
			svh = hv_fetch(cxt->hseen, (char *) &tag, sizeof(tag), FALSE);
			if (!svh)
				CROAK(("Old tag 0x%"UVxf" should have been mapped already",
					(UV) tag));
			tagn = SvIV(*svh);	/* Mapped tag number computed earlier below */

			/*
			 * The following code is common with the SX_OBJECT case below.
			 */

			svh = av_fetch(cxt->aseen, tagn, FALSE);
			if (!svh)
				CROAK(("Object #%"IVdf" should have been retrieved already",
					(IV) tagn));
			sv = *svh;
			TRACEME(("has retrieved #%d at 0x%"UVxf, tagn, PTR2UV(sv)));
			SvREFCNT_inc(sv);	/* One more reference to this same sv */
			return sv;			/* The SV pointer where object was retrieved */
		}

		/*
		 * Map new object, but don't increase tagnum. This will be done
		 * by each of the retrieve_* functions when they call SEEN().
		 *
		 * The mapping associates the "tag" initially present with a unique
		 * tag number. See test for SX_OBJECT above to see how this is perused.
		 */

		if (!hv_store(cxt->hseen, (char *) &tag, sizeof(tag),
				newSViv(cxt->tagnum), 0))
			return (SV *) 0;

		goto first_time;
	}

	/*
	 * Regular post-0.6 binary format.
	 */

	GETMARK(type);

	TRACEME(("retrieve type = %d", type));

	/*
	 * Are we dealing with an object we should have already retrieved?
	 */

	if (type == SX_OBJECT) {
		I32 tag;
		READ_I32(tag);
		tag = ntohl(tag);
		svh = av_fetch(cxt->aseen, tag, FALSE);
		if (!svh)
			CROAK(("Object #%"IVdf" should have been retrieved already",
				(IV) tag));
		sv = *svh;
		TRACEME(("had retrieved #%d at 0x%"UVxf, tag, PTR2UV(sv)));
		SvREFCNT_inc(sv);	/* One more reference to this same sv */
		return sv;			/* The SV pointer where object was retrieved */
	} else if (type >= SX_ERROR && cxt->ver_minor > STORABLE_BIN_MINOR) {
            if (cxt->accept_future_minor < 0)
                cxt->accept_future_minor
                    = (SvTRUE(perl_get_sv("Storable::accept_future_minor",
                                          TRUE))
                       ? 1 : 0);
            if (cxt->accept_future_minor == 1) {
                CROAK(("Storable binary image v%d.%d contains data of type %d. "
                       "This Storable is v%d.%d and can only handle data types up to %d",
                       cxt->ver_major, cxt->ver_minor, type,
                       STORABLE_BIN_MAJOR, STORABLE_BIN_MINOR, SX_ERROR - 1));
            }
        }

first_time:		/* Will disappear when support for old format is dropped */

	/*
	 * Okay, first time through for this one.
	 */

	sv = RETRIEVE(cxt, type)(aTHX_ cxt, cname);
	if (!sv)
		return (SV *) 0;			/* Failed */

	/*
	 * Old binary formats (pre-0.7).
	 *
	 * Final notifications, ended by SX_STORED may now follow.
	 * Currently, the only pertinent notification to apply on the
	 * freshly retrieved object is either:
	 *    SX_CLASS <char-len> <classname> for short classnames.
	 *    SX_LG_CLASS <int-len> <classname> for larger one (rare!).
	 * Class name is then read into the key buffer pool used by
	 * hash table key retrieval.
	 */

	if (cxt->ver_major < 2) {
		while ((type = GETCHAR()) != SX_STORED) {
			I32 len;
			switch (type) {
			case SX_CLASS:
				GETMARK(len);			/* Length coded on a single char */
				break;
			case SX_LG_CLASS:			/* Length coded on a regular integer */
				RLEN(len);
				break;
			case EOF:
			default:
				return (SV *) 0;		/* Failed */
			}
			KBUFCHK((STRLEN)len);			/* Grow buffer as necessary */
			if (len)
				READ(kbuf, len);
			kbuf[len] = '\0';			/* Mark string end */
			BLESS(sv, kbuf);
		}
	}

	TRACEME(("ok (retrieved 0x%"UVxf", refcnt=%d, %s)", PTR2UV(sv),
		SvREFCNT(sv) - 1, sv_reftype(sv, FALSE)));

	return sv;	/* Ok */
}

/*
 * do_retrieve
 *
 * Retrieve data held in file and return the root object.
 * Common routine for pretrieve and mretrieve.
 */
static SV *do_retrieve(
        pTHX_
	PerlIO *f,
	SV *in,
	int optype)
{
	dSTCXT;
	SV *sv;
	int is_tainted;				/* Is input source tainted? */
	int pre_06_fmt = 0;			/* True with pre Storable 0.6 formats */

	TRACEME(("do_retrieve (optype = 0x%x)", optype));

	optype |= ST_RETRIEVE;

	/*
	 * Sanity assertions for retrieve dispatch tables.
	 */

	ASSERT(sizeof(sv_old_retrieve) == sizeof(sv_retrieve),
		("old and new retrieve dispatch table have same size"));
	ASSERT(sv_old_retrieve[SX_ERROR] == retrieve_other,
		("SX_ERROR entry correctly initialized in old dispatch table"));
	ASSERT(sv_retrieve[SX_ERROR] == retrieve_other,
		("SX_ERROR entry correctly initialized in new dispatch table"));

	/*
	 * Workaround for CROAK leak: if they enter with a "dirty" context,
	 * free up memory for them now.
	 */

	if (cxt->s_dirty)
		clean_context(aTHX_ cxt);

	/*
	 * Now that STORABLE_xxx hooks exist, it is possible that they try to
	 * re-enter retrieve() via the hooks.
	 */

	if (cxt->entry)
		cxt = allocate_context(aTHX_ cxt);

	cxt->entry++;

	ASSERT(cxt->entry == 1, ("starting new recursion"));
	ASSERT(!cxt->s_dirty, ("clean context"));

	/*
	 * Prepare context.
	 *
	 * Data is loaded into the memory buffer when f is NULL, unless `in' is
	 * also NULL, in which case we're expecting the data to already lie
	 * in the buffer (dclone case).
	 */

	KBUFINIT();			 		/* Allocate hash key reading pool once */

	if (!f && in) {
#ifdef SvUTF8_on
		if (SvUTF8(in)) {
			STRLEN length;
			const char *orig = SvPV(in, length);
			char *asbytes;
			/* This is quite deliberate. I want the UTF8 routines
			   to encounter the '\0' which perl adds at the end
			   of all scalars, so that any new string also has
			   this.
			*/
			STRLEN klen_tmp = length + 1;
			bool is_utf8 = TRUE;

			/* Just casting the &klen to (STRLEN) won't work
			   well if STRLEN and I32 are of different widths.
			   --jhi */
			asbytes = (char*)bytes_from_utf8((U8*)orig,
							 &klen_tmp,
							 &is_utf8);
			if (is_utf8) {
				CROAK(("Frozen string corrupt - contains characters outside 0-255"));
			}
			if (asbytes != orig) {
				/* String has been converted.
				   There is no need to keep any reference to
				   the old string.  */
				in = sv_newmortal();
				/* We donate the SV the malloc()ed string
				   bytes_from_utf8 returned us.  */
				SvUPGRADE(in, SVt_PV);
				SvPOK_on(in);
				SvPVX(in) = asbytes;
				SvLEN(in) = klen_tmp;
				SvCUR(in) = klen_tmp - 1;
			}
		}
#endif
		MBUF_SAVE_AND_LOAD(in);
	}

	/*
	 * Magic number verifications.
	 *
	 * This needs to be done before calling init_retrieve_context()
	 * since the format indication in the file are necessary to conduct
	 * some of the initializations.
	 */

	cxt->fio = f;				/* Where I/O are performed */

	if (!magic_check(aTHX_ cxt))
		CROAK(("Magic number checking on storable %s failed",
			cxt->fio ? "file" : "string"));

	TRACEME(("data stored in %s format",
		cxt->netorder ? "net order" : "native"));

	/*
	 * Check whether input source is tainted, so that we don't wrongly
	 * taint perfectly good values...
	 *
	 * We assume file input is always tainted.  If both `f' and `in' are
	 * NULL, then we come from dclone, and tainted is already filled in
	 * the context.  That's a kludge, but the whole dclone() thing is
	 * already quite a kludge anyway! -- RAM, 15/09/2000.
	 */

	is_tainted = f ? 1 : (in ? SvTAINTED(in) : cxt->s_tainted);
	TRACEME(("input source is %s", is_tainted ? "tainted" : "trusted"));
	init_retrieve_context(aTHX_ cxt, optype, is_tainted);

	ASSERT(is_retrieving(), ("within retrieve operation"));

	sv = retrieve(aTHX_ cxt, 0);		/* Recursively retrieve object, get root SV */

	/*
	 * Final cleanup.
	 */

	if (!f && in)
		MBUF_RESTORE();

	pre_06_fmt = cxt->hseen != NULL;	/* Before we clean context */

	/*
	 * The "root" context is never freed.
	 */

	clean_retrieve_context(aTHX_ cxt);
	if (cxt->prev)				/* This context was stacked */
		free_context(aTHX_ cxt);		/* It was not the "root" context */

	/*
	 * Prepare returned value.
	 */

	if (!sv) {
		TRACEME(("retrieve ERROR"));
#if (PATCHLEVEL <= 4) 
		/* perl 5.00405 seems to screw up at this point with an
		   'attempt to modify a read only value' error reported in the
		   eval { $self = pretrieve(*FILE) } in _retrieve.
		   I can't see what the cause of this error is, but I suspect a
		   bug in 5.004, as it seems to be capable of issuing spurious
		   errors or core dumping with matches on $@. I'm not going to
		   spend time on what could be a fruitless search for the cause,
		   so here's a bodge. If you're running 5.004 and don't like
		   this inefficiency, either upgrade to a newer perl, or you are
		   welcome to find the problem and send in a patch.
		 */
		return newSV(0);
#else
		return &PL_sv_undef;		/* Something went wrong, return undef */
#endif
	}

	TRACEME(("retrieve got %s(0x%"UVxf")",
		sv_reftype(sv, FALSE), PTR2UV(sv)));

	/*
	 * Backward compatibility with Storable-0.5@9 (which we know we
	 * are retrieving if hseen is non-null): don't create an extra RV
	 * for objects since we special-cased it at store time.
	 *
	 * Build a reference to the SV returned by pretrieve even if it is
	 * already one and not a scalar, for consistency reasons.
	 */

	if (pre_06_fmt) {			/* Was not handling overloading by then */
		SV *rv;
		TRACEME(("fixing for old formats -- pre 0.6"));
		if (sv_type(aTHX_ sv) == svis_REF && (rv = SvRV(sv)) && SvOBJECT(rv)) {
			TRACEME(("ended do_retrieve() with an object -- pre 0.6"));
			return sv;
		}
	}

	/*
	 * If reference is overloaded, restore behaviour.
	 *
	 * NB: minor glitch here: normally, overloaded refs are stored specially
	 * so that we can croak when behaviour cannot be re-installed, and also
	 * avoid testing for overloading magic at each reference retrieval.
	 *
	 * Unfortunately, the root reference is implicitely stored, so we must
	 * check for possible overloading now.  Furthermore, if we don't restore
	 * overloading, we cannot croak as if the original ref was, because we
	 * have no way to determine whether it was an overloaded ref or not in
	 * the first place.
	 *
	 * It's a pity that overloading magic is attached to the rv, and not to
	 * the underlying sv as blessing is.
	 */

	if (SvOBJECT(sv)) {
		HV *stash = (HV *) SvSTASH(sv);
		SV *rv = newRV_noinc(sv);
		if (stash && Gv_AMG(stash)) {
			SvAMAGIC_on(rv);
			TRACEME(("restored overloading on root reference"));
		}
		TRACEME(("ended do_retrieve() with an object"));
		return rv;
	}

	TRACEME(("regular do_retrieve() end"));

	return newRV_noinc(sv);
}

/*
 * pretrieve
 *
 * Retrieve data held in file and return the root object, undef on error.
 */
SV *pretrieve(pTHX_ PerlIO *f)
{
	TRACEME(("pretrieve"));
	return do_retrieve(aTHX_ f, Nullsv, 0);
}

/*
 * mretrieve
 *
 * Retrieve data held in scalar and return the root object, undef on error.
 */
SV *mretrieve(pTHX_ SV *sv)
{
	TRACEME(("mretrieve"));
	return do_retrieve(aTHX_ (PerlIO*) 0, sv, 0);
}

/***
 *** Deep cloning
 ***/

/*
 * dclone
 *
 * Deep clone: returns a fresh copy of the original referenced SV tree.
 *
 * This is achieved by storing the object in memory and restoring from
 * there. Not that efficient, but it should be faster than doing it from
 * pure perl anyway.
 */
SV *dclone(pTHX_ SV *sv)
{
	dSTCXT;
	int size;
	stcxt_t *real_context;
	SV *out;

	TRACEME(("dclone"));

	/*
	 * Workaround for CROAK leak: if they enter with a "dirty" context,
	 * free up memory for them now.
	 */

	if (cxt->s_dirty)
		clean_context(aTHX_ cxt);

	/*
	 * do_store() optimizes for dclone by not freeing its context, should
	 * we need to allocate one because we're deep cloning from a hook.
	 */

	if (!do_store(aTHX_ (PerlIO*) 0, sv, ST_CLONE, FALSE, (SV**) 0))
		return &PL_sv_undef;				/* Error during store */

	/*
	 * Because of the above optimization, we have to refresh the context,
	 * since a new one could have been allocated and stacked by do_store().
	 */

	{ dSTCXT; real_context = cxt; }		/* Sub-block needed for macro */
	cxt = real_context;					/* And we need this temporary... */

	/*
	 * Now, `cxt' may refer to a new context.
	 */

	ASSERT(!cxt->s_dirty, ("clean context"));
	ASSERT(!cxt->entry, ("entry will not cause new context allocation"));

	size = MBUF_SIZE();
	TRACEME(("dclone stored %d bytes", size));
	MBUF_INIT(size);

	/*
	 * Since we're passing do_retrieve() both a NULL file and sv, we need
	 * to pre-compute the taintedness of the input by setting cxt->tainted
	 * to whatever state our own input string was.	-- RAM, 15/09/2000
	 *
	 * do_retrieve() will free non-root context.
	 */

	cxt->s_tainted = SvTAINTED(sv);
	out = do_retrieve(aTHX_ (PerlIO*) 0, Nullsv, ST_CLONE);

	TRACEME(("dclone returns 0x%"UVxf, PTR2UV(out)));

	return out;
}

/***
 *** Glue with perl.
 ***/

/*
 * The Perl IO GV object distinguishes between input and output for sockets
 * but not for plain files. To allow Storable to transparently work on
 * plain files and sockets transparently, we have to ask xsubpp to fetch the
 * right object for us. Hence the OutputStream and InputStream declarations.
 *
 * Before perl 5.004_05, those entries in the standard typemap are not
 * defined in perl include files, so we do that here.
 */

#ifndef OutputStream
#define OutputStream	PerlIO *
#define InputStream		PerlIO *
#endif	/* !OutputStream */

MODULE = Storable	PACKAGE = Storable::Cxt

void
DESTROY(self)
    SV *self
PREINIT:
	stcxt_t *cxt = (stcxt_t *)SvPVX(SvRV(self));
PPCODE:
	if (kbuf)
		Safefree(kbuf);
	if (!cxt->membuf_ro && mbase)
		Safefree(mbase);
	if (cxt->membuf_ro && (cxt->msaved).arena)
		Safefree((cxt->msaved).arena);


MODULE = Storable	PACKAGE = Storable

PROTOTYPES: ENABLE

BOOT:
    init_perinterp(aTHX);
    gv_fetchpv("Storable::drop_utf8",   GV_ADDMULTI, SVt_PV);
#ifdef DEBUGME
    /* Only disable the used only once warning if we are in debugging mode.  */
    gv_fetchpv("Storable::DEBUGME",   GV_ADDMULTI, SVt_PV);
#endif
#ifdef USE_56_INTERWORK_KLUDGE
    gv_fetchpv("Storable::interwork_56_64bit",   GV_ADDMULTI, SVt_PV);
#endif

void
init_perinterp()
 CODE:
  init_perinterp(aTHX);

int
pstore(f,obj)
OutputStream	f
SV *	obj
 CODE:
  RETVAL = pstore(aTHX_ f, obj);
 OUTPUT:
  RETVAL

int
net_pstore(f,obj)
OutputStream	f
SV *	obj
 CODE:
  RETVAL = net_pstore(aTHX_ f, obj);
 OUTPUT:
  RETVAL

SV *
mstore(obj)
SV *	obj
 CODE:
  RETVAL = mstore(aTHX_ obj);
 OUTPUT:
  RETVAL

SV *
net_mstore(obj)
SV *	obj
 CODE:
  RETVAL = net_mstore(aTHX_ obj);
 OUTPUT:
  RETVAL

SV *
pretrieve(f)
InputStream	f
 CODE:
  RETVAL = pretrieve(aTHX_ f);
 OUTPUT:
  RETVAL

SV *
mretrieve(sv)
SV *	sv
 CODE:
  RETVAL = mretrieve(aTHX_ sv);
 OUTPUT:
  RETVAL

SV *
dclone(sv)
SV *	sv
 CODE:
  RETVAL = dclone(aTHX_ sv);
 OUTPUT:
  RETVAL

int
last_op_in_netorder()
 CODE:
  RETVAL = last_op_in_netorder(aTHX);
 OUTPUT:
  RETVAL

int
is_storing()
 CODE:
  RETVAL = is_storing(aTHX);
 OUTPUT:
  RETVAL

int
is_retrieving()
 CODE:
  RETVAL = is_retrieving(aTHX);
 OUTPUT:
  RETVAL
