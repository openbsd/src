/* 

 DB_File.xs -- Perl 5 interface to Berkeley DB 

 written by Paul Marquess (pmarquess@bfsec.bt.co.uk)
 last modified 14th November 1995
 version 1.01

 All comments/suggestions/problems are welcome

 Changes:
	0.1 - 	Initial Release
	0.2 - 	No longer bombs out if dbopen returns an error.
	0.3 - 	Added some support for multiple btree compares
	1.0 - 	Complete support for multiple callbacks added.
	      	Fixed a problem with pushing a value onto an empty list.
	1.01 - 	Fixed a SunOS core dump problem.
		The return value from TIEHASH wasn't set to NULL when
		dbopen returned an error.
*/

#include "EXTERN.h"  
#include "perl.h"
#include "XSUB.h"

#include <db.h>

#include <fcntl.h> 

typedef struct {
	DBTYPE	type ;
	DB * 	dbp ;
	SV *	compare ;
	SV *	prefix ;
	SV *	hash ;
	} DB_File_type;

typedef DB_File_type * DB_File ;
typedef DBT DBTKEY ;

union INFO {
        HASHINFO 	hash ;
        RECNOINFO 	recno ;
        BTREEINFO 	btree ;
      } ;


/* #define TRACE  */

#define db_DESTROY(db)                  ((db->dbp)->close)(db->dbp)
#define db_DELETE(db, key, flags)       ((db->dbp)->del)(db->dbp, &key, flags)
#define db_STORE(db, key, value, flags) ((db->dbp)->put)(db->dbp, &key, &value, flags)
#define db_FETCH(db, key, flags)        ((db->dbp)->get)(db->dbp, &key, &value, flags)

#define db_close(db)			((db->dbp)->close)(db->dbp)
#define db_del(db, key, flags)          ((db->dbp)->del)(db->dbp, &key, flags)
#define db_fd(db)                       ((db->dbp)->fd)(db->dbp) 
#define db_put(db, key, value, flags)   ((db->dbp)->put)(db->dbp, &key, &value, flags)
#define db_get(db, key, value, flags)   ((db->dbp)->get)(db->dbp, &key, &value, flags)
#define db_seq(db, key, value, flags)   ((db->dbp)->seq)(db->dbp, &key, &value, flags)
#define db_sync(db, flags)              ((db->dbp)->sync)(db->dbp, flags)


#define OutputValue(arg, name)  \
	{ if (RETVAL == 0) sv_setpvn(arg, name.data, name.size) ; }

#define OutputKey(arg, name)	 				\
	{ if (RETVAL == 0) \
	  { 							\
		if (db->type != DB_RECNO) 			\
		    sv_setpvn(arg, name.data, name.size); 	\
		else 						\
		    sv_setiv(arg, (I32)*(I32*)name.data - 1); 	\
	  } 							\
	}

/* Internal Global Data */
static recno_t Value ; 
static DB_File CurrentDB ;
static recno_t zero = 0 ;
static DBTKEY empty = { &zero, sizeof(recno_t) } ;


static int
btree_compare(key1, key2)
const DBT * key1 ;
const DBT * key2 ;
{
    dSP ;
    void * data1, * data2 ;
    int retval ;
    int count ;
    
    data1 = key1->data ;
    data2 = key2->data ;

    /* As newSVpv will assume that the data pointer is a null terminated C 
       string if the size parameter is 0, make sure that data points to an 
       empty string if the length is 0
    */
    if (key1->size == 0)
        data1 = "" ; 
    if (key2->size == 0)
        data2 = "" ;

    ENTER ;
    SAVETMPS;

    PUSHMARK(sp) ;
    EXTEND(sp,2) ;
    PUSHs(sv_2mortal(newSVpv(data1,key1->size)));
    PUSHs(sv_2mortal(newSVpv(data2,key2->size)));
    PUTBACK ;

    count = perl_call_sv(CurrentDB->compare, G_SCALAR); 

    SPAGAIN ;

    if (count != 1)
        croak ("DB_File btree_compare: expected 1 return value from %s, got %d\n", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;
    return (retval) ;

}

static DB_Prefix_t
btree_prefix(key1, key2)
const DBT * key1 ;
const DBT * key2 ;
{
    dSP ;
    void * data1, * data2 ;
    int retval ;
    int count ;
    
    data1 = key1->data ;
    data2 = key2->data ;

    /* As newSVpv will assume that the data pointer is a null terminated C 
       string if the size parameter is 0, make sure that data points to an 
       empty string if the length is 0
    */
    if (key1->size == 0)
        data1 = "" ;
    if (key2->size == 0)
        data2 = "" ;

    ENTER ;
    SAVETMPS;

    PUSHMARK(sp) ;
    EXTEND(sp,2) ;
    PUSHs(sv_2mortal(newSVpv(data1,key1->size)));
    PUSHs(sv_2mortal(newSVpv(data2,key2->size)));
    PUTBACK ;

    count = perl_call_sv(CurrentDB->prefix, G_SCALAR); 

    SPAGAIN ;

    if (count != 1)
        croak ("DB_File btree_prefix: expected 1 return value from %s, got %d\n", count) ;
 
    retval = POPi ;
 
    PUTBACK ;
    FREETMPS ;
    LEAVE ;

    return (retval) ;
}

static DB_Hash_t
hash_cb(data, size)
const void * data ;
size_t size ;
{
    dSP ;
    int retval ;
    int count ;

    if (size == 0)
        data = "" ;

    PUSHMARK(sp) ;
    XPUSHs(sv_2mortal(newSVpv((char*)data,size)));
    PUTBACK ;

    count = perl_call_sv(CurrentDB->hash, G_SCALAR); 

    SPAGAIN ;

    if (count != 1)
        croak ("DB_File hash_cb: expected 1 return value from %s, got %d\n", count) ;

    retval = POPi ;

    PUTBACK ;
    FREETMPS ;
    LEAVE ;

    return (retval) ;
}


#ifdef TRACE

static void
PrintHash(hash)
HASHINFO hash ;
{
    printf ("HASH Info\n") ;
    printf ("  hash      = %s\n", (hash.hash != NULL ? "redefined" : "default")) ;
    printf ("  bsize     = %d\n", hash.bsize) ;
    printf ("  ffactor   = %d\n", hash.ffactor) ;
    printf ("  nelem     = %d\n", hash.nelem) ;
    printf ("  cachesize = %d\n", hash.cachesize) ;
    printf ("  lorder    = %d\n", hash.lorder) ;

}

static void
PrintRecno(recno)
RECNOINFO recno ;
{
    printf ("RECNO Info\n") ;
    printf ("  flags     = %d\n", recno.flags) ;
    printf ("  cachesize = %d\n", recno.cachesize) ;
    printf ("  psize     = %d\n", recno.psize) ;
    printf ("  lorder    = %d\n", recno.lorder) ;
    printf ("  reclen    = %d\n", recno.reclen) ;
    printf ("  bval      = %d\n", recno.bval) ;
    printf ("  bfname    = %s\n", recno.bfname) ;
}

PrintBtree(btree)
BTREEINFO btree ;
{
    printf ("BTREE Info\n") ;
    printf ("  compare    = %s\n", (btree.compare ? "redefined" : "default")) ;
    printf ("  prefix     = %s\n", (btree.prefix ? "redefined" : "default")) ;
    printf ("  flags      = %d\n", btree.flags) ;
    printf ("  cachesize  = %d\n", btree.cachesize) ;
    printf ("  psize      = %d\n", btree.psize) ;
    printf ("  maxkeypage = %d\n", btree.maxkeypage) ;
    printf ("  minkeypage = %d\n", btree.minkeypage) ;
    printf ("  lorder     = %d\n", btree.lorder) ;
}

#else

#define PrintRecno(recno)
#define PrintHash(hash)
#define PrintBtree(btree)

#endif /* TRACE */


static I32
GetArrayLength(db)
DB * db ;
{
    DBT		key ;
    DBT		value ;
    int		RETVAL ;

    RETVAL = (db->seq)(db, &key, &value, R_LAST) ;
    if (RETVAL == 0)
        RETVAL = *(I32 *)key.data ;
    else if (RETVAL == 1) /* No key means empty file */
        RETVAL = 0 ;

    return (RETVAL) ;
}

static DB_File
ParseOpenInfo(name, flags, mode, sv, string)
char * name ;
int    flags ;
int    mode ;
SV *   sv ;
char * string ;
{
    SV **	svp;
    HV *	action ;
    union INFO	info ;
    DB_File	RETVAL = (DB_File)safemalloc(sizeof(DB_File_type)) ;
    void *	openinfo = NULL ;
    /* DBTYPE	type = DB_HASH ; */

    RETVAL->hash = RETVAL->compare = RETVAL->prefix = NULL ;
    RETVAL->type = DB_HASH ;

    if (sv)
    {
        if (! SvROK(sv) )
            croak ("type parameter is not a reference") ;

        action = (HV*)SvRV(sv);
        if (sv_isa(sv, "DB_File::HASHINFO"))
        {
            RETVAL->type = DB_HASH ;
            openinfo = (void*)&info ;
  
            svp = hv_fetch(action, "hash", 4, FALSE); 

            if (svp && SvOK(*svp))
            {
                info.hash.hash = hash_cb ;
		RETVAL->hash = newSVsv(*svp) ;
            }
            else
	        info.hash.hash = NULL ;

           svp = hv_fetch(action, "bsize", 5, FALSE);
           info.hash.bsize = svp ? SvIV(*svp) : 0;
           
           svp = hv_fetch(action, "ffactor", 7, FALSE);
           info.hash.ffactor = svp ? SvIV(*svp) : 0;
         
           svp = hv_fetch(action, "nelem", 5, FALSE);
           info.hash.nelem = svp ? SvIV(*svp) : 0;
         
           svp = hv_fetch(action, "cachesize", 9, FALSE);
           info.hash.cachesize = svp ? SvIV(*svp) : 0;
         
           svp = hv_fetch(action, "lorder", 6, FALSE);
           info.hash.lorder = svp ? SvIV(*svp) : 0;

           PrintHash(info) ; 
        }
        else if (sv_isa(sv, "DB_File::BTREEINFO"))
        {
            RETVAL->type = DB_BTREE ;
            openinfo = (void*)&info ;
   
            svp = hv_fetch(action, "compare", 7, FALSE);
            if (svp && SvOK(*svp))
            {
                info.btree.compare = btree_compare ;
		RETVAL->compare = newSVsv(*svp) ;
            }
            else
                info.btree.compare = NULL ;

            svp = hv_fetch(action, "prefix", 6, FALSE);
            if (svp && SvOK(*svp))
            {
                info.btree.prefix = btree_prefix ;
		RETVAL->prefix = newSVsv(*svp) ;
            }
            else
                info.btree.prefix = NULL ;

            svp = hv_fetch(action, "flags", 5, FALSE);
            info.btree.flags = svp ? SvIV(*svp) : 0;
   
            svp = hv_fetch(action, "cachesize", 9, FALSE);
            info.btree.cachesize = svp ? SvIV(*svp) : 0;
         
            svp = hv_fetch(action, "minkeypage", 10, FALSE);
            info.btree.minkeypage = svp ? SvIV(*svp) : 0;
        
            svp = hv_fetch(action, "maxkeypage", 10, FALSE);
            info.btree.maxkeypage = svp ? SvIV(*svp) : 0;

            svp = hv_fetch(action, "psize", 5, FALSE);
            info.btree.psize = svp ? SvIV(*svp) : 0;
         
            svp = hv_fetch(action, "lorder", 6, FALSE);
            info.btree.lorder = svp ? SvIV(*svp) : 0;

            PrintBtree(info) ;
         
        }
        else if (sv_isa(sv, "DB_File::RECNOINFO"))
        {
            RETVAL->type = DB_RECNO ;
            openinfo = (void *)&info ;

            svp = hv_fetch(action, "flags", 5, FALSE);
            info.recno.flags = (u_long) svp ? SvIV(*svp) : 0;
         
            svp = hv_fetch(action, "cachesize", 9, FALSE);
            info.recno.cachesize = (u_int) svp ? SvIV(*svp) : 0;
         
            svp = hv_fetch(action, "psize", 5, FALSE);
            info.recno.psize = (int) svp ? SvIV(*svp) : 0;
         
            svp = hv_fetch(action, "lorder", 6, FALSE);
            info.recno.lorder = (int) svp ? SvIV(*svp) : 0;
         
            svp = hv_fetch(action, "reclen", 6, FALSE);
            info.recno.reclen = (size_t) svp ? SvIV(*svp) : 0;
         
	    svp = hv_fetch(action, "bval", 4, FALSE);
            if (svp && SvOK(*svp))
            {
                if (SvPOK(*svp))
		    info.recno.bval = (u_char)*SvPV(*svp, na) ;
		else
		    info.recno.bval = (u_char)(unsigned long) SvIV(*svp) ;
            }
            else
 	    {
		if (info.recno.flags & R_FIXEDLEN)
                    info.recno.bval = (u_char) ' ' ;
		else
                    info.recno.bval = (u_char) '\n' ;
	    }
         
            svp = hv_fetch(action, "bfname", 6, FALSE); 
            info.recno.bfname = (char *) svp ? SvPV(*svp,na) : 0;

            PrintRecno(info) ;
        }
        else
            croak("type is not of type DB_File::HASHINFO, DB_File::BTREEINFO or DB_File::RECNOINFO");
    }


    RETVAL->dbp = dbopen(name, flags, mode, RETVAL->type, openinfo) ; 

#if 0
    /* kludge mode on: RETVAL->type for DB_RECNO is set to DB_BTREE
		       so remember a DB_RECNO by saving the address
		       of one of it's internal routines
    */
    if (RETVAL->dbp && type == DB_RECNO)
        DB_recno_close = RETVAL->dbp->close ;
#endif


    return (RETVAL) ;
}


static int
not_here(s)
char *s;
{
    croak("DB_File::%s not implemented on this architecture", s);
    return -1;
}

static double 
constant(name, arg)
char *name;
int arg;
{
    errno = 0;
    switch (*name) {
    case 'A':
	break;
    case 'B':
	if (strEQ(name, "BTREEMAGIC"))
#ifdef BTREEMAGIC
	    return BTREEMAGIC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "BTREEVERSION"))
#ifdef BTREEVERSION
	    return BTREEVERSION;
#else
	    goto not_there;
#endif
	break;
    case 'C':
	break;
    case 'D':
	if (strEQ(name, "DB_LOCK"))
#ifdef DB_LOCK
	    return DB_LOCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DB_SHMEM"))
#ifdef DB_SHMEM
	    return DB_SHMEM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "DB_TXN"))
#ifdef DB_TXN
	    return (U32)DB_TXN;
#else
	    goto not_there;
#endif
	break;
    case 'E':
	break;
    case 'F':
	break;
    case 'G':
	break;
    case 'H':
	if (strEQ(name, "HASHMAGIC"))
#ifdef HASHMAGIC
	    return HASHMAGIC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "HASHVERSION"))
#ifdef HASHVERSION
	    return HASHVERSION;
#else
	    goto not_there;
#endif
	break;
    case 'I':
	break;
    case 'J':
	break;
    case 'K':
	break;
    case 'L':
	break;
    case 'M':
	if (strEQ(name, "MAX_PAGE_NUMBER"))
#ifdef MAX_PAGE_NUMBER
	    return (U32)MAX_PAGE_NUMBER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "MAX_PAGE_OFFSET"))
#ifdef MAX_PAGE_OFFSET
	    return MAX_PAGE_OFFSET;
#else
	    goto not_there;
#endif
	if (strEQ(name, "MAX_REC_NUMBER"))
#ifdef MAX_REC_NUMBER
	    return (U32)MAX_REC_NUMBER;
#else
	    goto not_there;
#endif
	break;
    case 'N':
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	break;
    case 'R':
	if (strEQ(name, "RET_ERROR"))
#ifdef RET_ERROR
	    return RET_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RET_SPECIAL"))
#ifdef RET_SPECIAL
	    return RET_SPECIAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "RET_SUCCESS"))
#ifdef RET_SUCCESS
	    return RET_SUCCESS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_CURSOR"))
#ifdef R_CURSOR
	    return R_CURSOR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_DUP"))
#ifdef R_DUP
	    return R_DUP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_FIRST"))
#ifdef R_FIRST
	    return R_FIRST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_FIXEDLEN"))
#ifdef R_FIXEDLEN
	    return R_FIXEDLEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_IAFTER"))
#ifdef R_IAFTER
	    return R_IAFTER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_IBEFORE"))
#ifdef R_IBEFORE
	    return R_IBEFORE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_LAST"))
#ifdef R_LAST
	    return R_LAST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_NEXT"))
#ifdef R_NEXT
	    return R_NEXT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_NOKEY"))
#ifdef R_NOKEY
	    return R_NOKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_NOOVERWRITE"))
#ifdef R_NOOVERWRITE
	    return R_NOOVERWRITE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_PREV"))
#ifdef R_PREV
	    return R_PREV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_RECNOSYNC"))
#ifdef R_RECNOSYNC
	    return R_RECNOSYNC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_SETCURSOR"))
#ifdef R_SETCURSOR
	    return R_SETCURSOR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "R_SNAPSHOT"))
#ifdef R_SNAPSHOT
	    return R_SNAPSHOT;
#else
	    goto not_there;
#endif
	break;
    case 'S':
	break;
    case 'T':
	break;
    case 'U':
	break;
    case 'V':
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    case '_':
	if (strEQ(name, "__R_UNUSED"))
#ifdef __R_UNUSED
	    return __R_UNUSED;
#else
	    goto not_there;
#endif
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}

MODULE = DB_File	PACKAGE = DB_File	PREFIX = db_

double
constant(name,arg)
	char *		name
	int		arg


DB_File
db_TIEHASH(dbtype, name=undef, flags=O_RDWR, mode=0640, type=DB_HASH)
	char *		dbtype
	int		flags
	int		mode
	CODE:
	{
	    char *	name = (char *) NULL ; 
	    SV *	sv = (SV *) NULL ; 

	    if (items >= 2 && SvOK(ST(1))) 
	        name = (char*) SvPV(ST(1), na) ; 

            if (items == 5)
	        sv = ST(4) ;

	    RETVAL = ParseOpenInfo(name, flags, mode, sv, "new") ;
	    if (RETVAL->dbp == NULL)
	        RETVAL = NULL ;
	}
	OUTPUT:	
	    RETVAL

BOOT:
    newXS("DB_File::TIEARRAY", XS_DB_File_db_TIEHASH, file);

int
db_DESTROY(db)
	DB_File		db
	INIT:
	  CurrentDB = db ;
	CLEANUP:
	  if (db->hash)
	    SvREFCNT_dec(db->hash) ;
	  if (db->compare)
	    SvREFCNT_dec(db->compare) ;
	  if (db->prefix)
	    SvREFCNT_dec(db->prefix) ;
	  Safefree(db) ;


int
db_DELETE(db, key, flags=0)
	DB_File		db
	DBTKEY		key
	u_int		flags
	INIT:
	  CurrentDB = db ;

int
db_FETCH(db, key, flags=0)
	DB_File		db
	DBTKEY		key
	u_int		flags
	CODE:
	{
	    DBT		value  ;

	    CurrentDB = db ;
	    RETVAL = ((db->dbp)->get)(db->dbp, &key, &value, flags) ;
	    ST(0) = sv_newmortal();
	    if (RETVAL == 0)
	        sv_setpvn(ST(0), value.data, value.size);
	}

int
db_STORE(db, key, value, flags=0)
	DB_File		db
	DBTKEY		key
	DBT		value
	u_int		flags
	INIT:
	  CurrentDB = db ;


int
db_FIRSTKEY(db)
	DB_File		db
	CODE:
	{
	    DBTKEY		key ;
	    DBT		value ;
	    DB *	Db = db->dbp ;

	    CurrentDB = db ;
	    RETVAL = (Db->seq)(Db, &key, &value, R_FIRST) ;
	    ST(0) = sv_newmortal();
	    if (RETVAL == 0)
	    {
	        if (Db->type != DB_RECNO)
	            sv_setpvn(ST(0), key.data, key.size);
	        else
	            sv_setiv(ST(0), (I32)*(I32*)key.data - 1);
	    }
	}

int
db_NEXTKEY(db, key)
	DB_File		db
	DBTKEY		key
	CODE:
	{
	    DBT		value ;
	    DB *	Db = db->dbp ;

	    CurrentDB = db ;
	    RETVAL = (Db->seq)(Db, &key, &value, R_NEXT) ;
	    ST(0) = sv_newmortal();
	    if (RETVAL == 0)
	    {
	        if (Db->type != DB_RECNO)
	            sv_setpvn(ST(0), key.data, key.size);
	        else
	            sv_setiv(ST(0), (I32)*(I32*)key.data - 1);
	    }
	}

#
# These would be nice for RECNO
#

int
unshift(db, ...)
	DB_File		db
	CODE:
	{
	    DBTKEY	key ;
	    DBT		value ;
	    int		i ;
	    int		One ;
	    DB *	Db = db->dbp ;

	    CurrentDB = db ;
	    RETVAL = -1 ;
	    for (i = items-1 ; i > 0 ; --i)
	    {
	        value.data = SvPV(ST(i), na) ;
	        value.size = na ;
	        One = 1 ;
	        key.data = &One ;
	        key.size = sizeof(int) ;
	        RETVAL = (Db->put)(Db, &key, &value, R_IBEFORE) ;
	        if (RETVAL != 0)
	            break;
	    }
	}
	OUTPUT:
	    RETVAL

I32
pop(db)
	DB_File		db
	CODE:
	{
	    DBTKEY	key ;
	    DBT		value ;
	    DB *	Db = db->dbp ;

	    CurrentDB = db ;
	    /* First get the final value */
	    RETVAL = (Db->seq)(Db, &key, &value, R_LAST) ;	
	    ST(0) = sv_newmortal();
	    /* Now delete it */
	    if (RETVAL == 0)
	    {
	        RETVAL = (Db->del)(Db, &key, R_CURSOR) ;
	        if (RETVAL == 0)
	            sv_setpvn(ST(0), value.data, value.size);
	    }
	}

I32
shift(db)
	DB_File		db
	CODE:
	{
	    DBTKEY	key ;
	    DBT		value ;
	    DB *	Db = db->dbp ;

	    CurrentDB = db ;
	    /* get the first value */
	    RETVAL = (Db->seq)(Db, &key, &value, R_FIRST) ;	
	    ST(0) = sv_newmortal();
	    /* Now delete it */
	    if (RETVAL == 0)
	    {
	        RETVAL = (Db->del)(Db, &key, R_CURSOR) ;
	        if (RETVAL == 0)
	            sv_setpvn(ST(0), value.data, value.size);
	    }
	}


I32
push(db, ...)
	DB_File		db
	CODE:
	{
	    DBTKEY	key ;
	    DBTKEY *	keyptr = &key ; 
	    DBT		value ;
	    DB *	Db = db->dbp ;
	    int		i ;

	    CurrentDB = db ;
	    /* Set the Cursor to the Last element */
	    RETVAL = (Db->seq)(Db, &key, &value, R_LAST) ;
	    if (RETVAL >= 0)
	    {
		if (RETVAL == 1)
		    keyptr = &empty ;
	        for (i = items - 1 ; i > 0 ; --i)
	        {
	            value.data = SvPV(ST(i), na) ;
	            value.size = na ;
	            RETVAL = (Db->put)(Db, keyptr, &value, R_IAFTER) ;
	            if (RETVAL != 0)
	                break;
	        }
	    }
	}
	OUTPUT:
	    RETVAL


I32
length(db)
	DB_File		db
	CODE:
	    CurrentDB = db ;
	    RETVAL = GetArrayLength(db->dbp) ;
	OUTPUT:
	    RETVAL


#
# Now provide an interface to the rest of the DB functionality
#

int
db_del(db, key, flags=0)
	DB_File		db
	DBTKEY		key
	u_int		flags
	INIT:
	  CurrentDB = db ;


int
db_get(db, key, value, flags=0)
	DB_File		db
	DBTKEY		key
	DBT		value
	u_int		flags
	INIT:
	  CurrentDB = db ;
	OUTPUT:
	  value

int
db_put(db, key, value, flags=0)
	DB_File		db
	DBTKEY		key
	DBT		value
	u_int		flags
	INIT:
	  CurrentDB = db ;
	OUTPUT:
	  key		if (flags & (R_IAFTER|R_IBEFORE)) OutputKey(ST(1), key);

int
db_fd(db)
	DB_File		db
	INIT:
	  CurrentDB = db ;

int
db_sync(db, flags=0)
	DB_File		db
	u_int		flags
	INIT:
	  CurrentDB = db ;


int
db_seq(db, key, value, flags)
	DB_File		db
	DBTKEY		key 
	DBT		value
	u_int		flags
	INIT:
	  CurrentDB = db ;
	OUTPUT:
	  key
	  value
