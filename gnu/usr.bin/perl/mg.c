/*    mg.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "Sam sat on the ground and put his head in his hands.  'I wish I had never
 * come here, and I don't want to see no more magic,' he said, and fell silent."
 */

#include "EXTERN.h"
#include "perl.h"

/* Omit -- it causes too much grief on mixed systems.
#ifdef I_UNISTD
# include <unistd.h>
#endif
*/

/*
 * Use the "DESTRUCTOR" scope cleanup to reinstate magic.
 */

struct magic_state {
    SV* mgs_sv;
    U32 mgs_flags;
};
typedef struct magic_state MGS;

static void restore_magic _((void *p));

static MGS *
save_magic(sv)
SV* sv;
{
    MGS* mgs;

    assert(SvMAGICAL(sv));

    mgs = (MGS*)safemalloc(sizeof(MGS));
    mgs->mgs_sv = sv;
    mgs->mgs_flags = SvMAGICAL(sv) | SvREADONLY(sv);
    SAVEDESTRUCTOR(restore_magic, mgs);

    SvMAGICAL_off(sv);
    SvREADONLY_off(sv);
    SvFLAGS(sv) |= (SvFLAGS(sv) & (SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;

    return mgs;
}

static void
restore_magic(p)
void* p;
{
    MGS *mgs = (MGS*)p;
    SV* sv = mgs->mgs_sv;

    if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv))
    {
	if (mgs->mgs_flags)
	    SvFLAGS(sv) |= mgs->mgs_flags;
	else
	    mg_magical(sv);
	if (SvGMAGICAL(sv))
	    SvFLAGS(sv) &= ~(SVf_IOK|SVf_NOK|SVf_POK);
    }

    Safefree(mgs);
}


void
mg_magical(sv)
SV* sv;
{
    MAGIC* mg;
    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
	MGVTBL* vtbl = mg->mg_virtual;
	if (vtbl) {
	    if (vtbl->svt_get && !(mg->mg_flags & MGf_GSKIP))
		SvGMAGICAL_on(sv);
	    if (vtbl->svt_set)
		SvSMAGICAL_on(sv);
	    if (!(SvFLAGS(sv) & (SVs_GMG|SVs_SMG)) || vtbl->svt_clear)
		SvRMAGICAL_on(sv);
	}
    }
}

int
mg_get(sv)
SV* sv;
{
    MGS* mgs;
    MAGIC* mg;
    MAGIC** mgp;

    ENTER;
    mgs = save_magic(sv);

    mgp = &SvMAGIC(sv);
    while ((mg = *mgp) != 0) {
	MGVTBL* vtbl = mg->mg_virtual;
	if (!(mg->mg_flags & MGf_GSKIP) && vtbl && vtbl->svt_get) {
	    (*vtbl->svt_get)(sv, mg);
	    /* Ignore this magic if it's been deleted */
	    if (*mgp == mg && (mg->mg_flags & MGf_GSKIP))
		mgs->mgs_flags = 0;
	}
	/* Advance to next magic (complicated by possible deletion) */
	if (*mgp == mg)
	    mgp = &mg->mg_moremagic;
    }

    LEAVE;
    return 0;
}

int
mg_set(sv)
SV* sv;
{
    MGS* mgs;
    MAGIC* mg;
    MAGIC* nextmg;

    ENTER;
    mgs = save_magic(sv);

    for (mg = SvMAGIC(sv); mg; mg = nextmg) {
	MGVTBL* vtbl = mg->mg_virtual;
	nextmg = mg->mg_moremagic;	/* it may delete itself */
	if (mg->mg_flags & MGf_GSKIP) {
	    mg->mg_flags &= ~MGf_GSKIP;	/* setting requires another read */
	    mgs->mgs_flags = 0;
	}
	if (vtbl && vtbl->svt_set)
	    (*vtbl->svt_set)(sv, mg);
    }

    LEAVE;
    return 0;
}

U32
mg_len(sv)
SV* sv;
{
    MAGIC* mg;
    char *junk;
    STRLEN len;

    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
	MGVTBL* vtbl = mg->mg_virtual;
	if (vtbl && vtbl->svt_len) {
	    ENTER;
	    save_magic(sv);
	    /* omit MGf_GSKIP -- not changed here */
	    len = (*vtbl->svt_len)(sv, mg);
	    LEAVE;
	    return len;
	}
    }

    junk = SvPV(sv, len);
    return len;
}

int
mg_clear(sv)
SV* sv;
{
    MAGIC* mg;

    ENTER;
    save_magic(sv);

    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
	MGVTBL* vtbl = mg->mg_virtual;
	/* omit GSKIP -- never set here */
	
	if (vtbl && vtbl->svt_clear)
	    (*vtbl->svt_clear)(sv, mg);
    }

    LEAVE;
    return 0;
}

MAGIC*
mg_find(sv, type)
SV* sv;
int type;
{
    MAGIC* mg;
    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
	if (mg->mg_type == type)
	    return mg;
    }
    return 0;
}

int
mg_copy(sv, nsv, key, klen)
SV* sv;
SV* nsv;
char *key;
STRLEN klen;
{
    int count = 0;
    MAGIC* mg;
    for (mg = SvMAGIC(sv); mg; mg = mg->mg_moremagic) {
	if (isUPPER(mg->mg_type)) {
	    sv_magic(nsv, mg->mg_obj, toLOWER(mg->mg_type), key, klen);
	    count++;
	}
    }
    return count;
}

int
mg_free(sv)
SV* sv;
{
    MAGIC* mg;
    MAGIC* moremagic;
    for (mg = SvMAGIC(sv); mg; mg = moremagic) {
	MGVTBL* vtbl = mg->mg_virtual;
	moremagic = mg->mg_moremagic;
	if (vtbl && vtbl->svt_free)
	    (*vtbl->svt_free)(sv, mg);
	if (mg->mg_ptr && mg->mg_type != 'g')
	    Safefree(mg->mg_ptr);
	if (mg->mg_flags & MGf_REFCOUNTED)
	    SvREFCNT_dec(mg->mg_obj);
	Safefree(mg);
    }
    SvMAGIC(sv) = 0;
    return 0;
}

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

U32
magic_len(sv, mg)
SV *sv;
MAGIC *mg;
{
    register I32 paren;
    register char *s;
    register I32 i;
    char *t;

    switch (*mg->mg_ptr) {
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '&':
	if (curpm) {
	    paren = atoi(mg->mg_ptr);
	  getparen:
	    if (curpm->op_pmregexp &&
	      paren <= curpm->op_pmregexp->nparens &&
	      (s = curpm->op_pmregexp->startp[paren]) &&
	      (t = curpm->op_pmregexp->endp[paren]) ) {
		i = t - s;
		if (i >= 0)
		    return i;
	    }
	}
	return 0;
	break;
    case '+':
	if (curpm) {
	    paren = curpm->op_pmregexp->lastparen;
	    if (!paren)
		return 0;
	    goto getparen;
	}
	return 0;
	break;
    case '`':
	if (curpm) {
	    if (curpm->op_pmregexp &&
	      (s = curpm->op_pmregexp->subbeg) ) {
		i = curpm->op_pmregexp->startp[0] - s;
		if (i >= 0)
		    return i;
	    }
	}
	return 0;
    case '\'':
	if (curpm) {
	    if (curpm->op_pmregexp &&
	      (s = curpm->op_pmregexp->endp[0]) ) {
		return (STRLEN) (curpm->op_pmregexp->subend - s);
	    }
	}
	return 0;
    case ',':
	return (STRLEN)ofslen;
    case '\\':
	return (STRLEN)orslen;
    }
    magic_get(sv,mg);
    if (!SvPOK(sv) && SvNIOK(sv))
	sv_2pv(sv, &na);
    if (SvPOK(sv))
	return SvCUR(sv);
    return 0;
}

int
magic_get(sv, mg)
SV *sv;
MAGIC *mg;
{
    register I32 paren;
    register char *s;
    register I32 i;
    char *t;

    switch (*mg->mg_ptr) {
    case '\001':		/* ^A */
	sv_setsv(sv, bodytarget);
	break;
    case '\004':		/* ^D */
	sv_setiv(sv,(I32)(debug & 32767));
	break;
    case '\005':  /* ^E */
#ifdef VMS
	{
#	    include <descrip.h>
#	    include <starlet.h>
	    char msg[255];
	    $DESCRIPTOR(msgdsc,msg);
	    sv_setnv(sv,(double)vaxc$errno);
	    if (sys$getmsg(vaxc$errno,&msgdsc.dsc$w_length,&msgdsc,0,0) & 1)
		sv_setpvn(sv,msgdsc.dsc$a_pointer,msgdsc.dsc$w_length);
	    else
		sv_setpv(sv,"");
	}
#else
	sv_setnv(sv,(double)errno);
	sv_setpv(sv, errno ? Strerror(errno) : "");
#endif
	SvNOK_on(sv);	/* what a wonderful hack! */
	break;
    case '\006':		/* ^F */
	sv_setiv(sv,(I32)maxsysfd);
	break;
    case '\010':		/* ^H */
	sv_setiv(sv,(I32)hints);
	break;
    case '\t':			/* ^I */
	if (inplace)
	    sv_setpv(sv, inplace);
	else
	    sv_setsv(sv,&sv_undef);
	break;
    case '\017':		/* ^O */
	sv_setpv(sv,osname);
	break;
    case '\020':		/* ^P */
	sv_setiv(sv,(I32)perldb);
	break;
    case '\024':		/* ^T */
	sv_setiv(sv,(I32)basetime);
	break;
    case '\027':		/* ^W */
	sv_setiv(sv,(I32)dowarn);
	break;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '&':
	if (curpm) {
	    paren = atoi(GvENAME(mg->mg_obj));
	  getparen:
	    if (curpm->op_pmregexp &&
	      paren <= curpm->op_pmregexp->nparens &&
	      (s = curpm->op_pmregexp->startp[paren]) &&
	      (t = curpm->op_pmregexp->endp[paren]) ) {
		i = t - s;
		if (i >= 0) {
		    MAGIC *tmg;
		    sv_setpvn(sv,s,i);
		    if (tainting && (tmg = mg_find(sv,'t')))
			tmg->mg_len = 0;	/* guarantee $1 untainted */
		    break;
		}
	    }
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '+':
	if (curpm) {
	    paren = curpm->op_pmregexp->lastparen;
	    if (paren)
		goto getparen;
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '`':
	if (curpm) {
	    if (curpm->op_pmregexp &&
	      (s = curpm->op_pmregexp->subbeg) ) {
		i = curpm->op_pmregexp->startp[0] - s;
		if (i >= 0) {
		    sv_setpvn(sv,s,i);
		    break;
		}
	    }
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '\'':
	if (curpm) {
	    if (curpm->op_pmregexp &&
	      (s = curpm->op_pmregexp->endp[0]) ) {
		sv_setpvn(sv,s, curpm->op_pmregexp->subend - s);
		break;
	    }
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '.':
#ifndef lint
	if (GvIO(last_in_gv)) {
	    sv_setiv(sv,(I32)IoLINES(GvIO(last_in_gv)));
	}
#endif
	break;
    case '?':
	sv_setiv(sv,(I32)statusvalue);
	break;
    case '^':
	s = IoTOP_NAME(GvIOp(defoutgv));
	if (s)
	    sv_setpv(sv,s);
	else {
	    sv_setpv(sv,GvENAME(defoutgv));
	    sv_catpv(sv,"_TOP");
	}
	break;
    case '~':
	s = IoFMT_NAME(GvIOp(defoutgv));
	if (!s)
	    s = GvENAME(defoutgv);
	sv_setpv(sv,s);
	break;
#ifndef lint
    case '=':
	sv_setiv(sv,(I32)IoPAGE_LEN(GvIOp(defoutgv)));
	break;
    case '-':
	sv_setiv(sv,(I32)IoLINES_LEFT(GvIOp(defoutgv)));
	break;
    case '%':
	sv_setiv(sv,(I32)IoPAGE(GvIOp(defoutgv)));
	break;
#endif
    case ':':
	break;
    case '/':
	break;
    case '[':
	sv_setiv(sv,(I32)curcop->cop_arybase);
	break;
    case '|':
	sv_setiv(sv, (IoFLAGS(GvIOp(defoutgv)) & IOf_FLUSH) != 0 );
	break;
    case ',':
	sv_setpvn(sv,ofs,ofslen);
	break;
    case '\\':
	sv_setpvn(sv,ors,orslen);
	break;
    case '#':
	sv_setpv(sv,ofmt);
	break;
    case '!':
#ifdef VMS
	sv_setnv(sv,(double)((errno == EVMSERR) ? vaxc$errno : errno));
#else
	sv_setnv(sv,(double)errno);
#endif
	sv_setpv(sv, errno ? Strerror(errno) : "");
	SvNOK_on(sv);	/* what a wonderful hack! */
	break;
    case '<':
	sv_setiv(sv,(I32)uid);
	break;
    case '>':
	sv_setiv(sv,(I32)euid);
	break;
    case '(':
	s = buf;
	(void)sprintf(s,"%d",(int)gid);
	goto add_groups;
    case ')':
	s = buf;
	(void)sprintf(s,"%d",(int)egid);
      add_groups:
	while (*s) s++;
#ifdef HAS_GETGROUPS
#ifndef NGROUPS
#define NGROUPS 32
#endif
	{
	    Groups_t gary[NGROUPS];

	    i = getgroups(NGROUPS,gary);
	    while (--i >= 0) {
		(void)sprintf(s," %ld", (long)gary[i]);
		while (*s) s++;
	    }
	}
#endif
	sv_setpv(sv,buf);
	break;
    case '*':
	break;
    case '0':
	break;
    }
    return 0;
}

int
magic_getuvar(sv, mg)
SV *sv;
MAGIC *mg;
{
    struct ufuncs *uf = (struct ufuncs *)mg->mg_ptr;

    if (uf && uf->uf_val)
	(*uf->uf_val)(uf->uf_index, sv);
    return 0;
}

int
magic_setenv(sv,mg)
SV* sv;
MAGIC* mg;
{
    register char *s;
    STRLEN len;
    I32 i;
    s = SvPV(sv,len);
    my_setenv(mg->mg_ptr,s);
#ifdef DYNAMIC_ENV_FETCH
     /* We just undefd an environment var.  Is a replacement */
     /* waiting in the wings? */
    if (!len) {
	SV **envsvp;
	if (envsvp = hv_fetch(GvHVn(envgv),mg->mg_ptr,mg->mg_len,FALSE))
	    s = SvPV(*envsvp,len);
    }
#endif
			    /* And you'll never guess what the dog had */
			    /*   in its mouth... */
    if (tainting) {
	if (s && strEQ(mg->mg_ptr,"PATH")) {
	    char *strend = s + len;

	    while (s < strend) {
		s = cpytill(tokenbuf,s,strend,':',&i);
		s++;
		if (*tokenbuf != '/'
		  || (Stat(tokenbuf,&statbuf) && (statbuf.st_mode & 2)) )
		    MgTAINTEDDIR_on(mg);
	    }
	}
    }
    return 0;
}

int
magic_clearenv(sv,mg)
SV* sv;
MAGIC* mg;
{
    my_setenv(mg->mg_ptr,Nullch);
    return 0;
}

#ifdef HAS_SIGACTION
/* set up reliable signal() clone */

typedef void (*Sigfunc) _((int));

static
Sigfunc rsignal(signo,handler)
int signo;
Sigfunc handler;
{
    struct sigaction act,oact;
    
    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
#ifdef SIGALRM    
    if (signo == SIGALRM) {
#else
    if (0) {
#endif        
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;	/* SunOS */
#endif	
    } else {
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;	/* SVR4, 4.3+BSD */
#endif
    }
    if (sigaction(signo, &act, &oact) < 0)
    	return(SIG_ERR);
    else
    	return(oact.sa_handler);
}

#else

/* ah well, so much for reliability */

#define rsignal(x,y) signal(x,y)

#endif


int
magic_setsig(sv,mg)
SV* sv;
MAGIC* mg;
{
    register char *s;
    I32 i;
    SV** svp;

    s = mg->mg_ptr;
    if (*s == '_') {
	if (strEQ(s,"__DIE__"))
	    svp = &diehook;
	else if (strEQ(s,"__WARN__"))
	    svp = &warnhook;
	else if (strEQ(s,"__PARSE__"))
	    svp = &parsehook;
	else
	    croak("No such hook: %s", s);
	i = 0;
	if (*svp) {
	    SvREFCNT_dec(*svp);
	    *svp = 0;
	}
    }
    else {
	i = whichsig(s);	/* ...no, a brick */
	if (!i) {
	    if (dowarn || strEQ(s,"ALARM"))
		warn("No such signal: SIG%s", s);
	    return 0;
	}
    }
    if (SvTYPE(sv) == SVt_PVGV || SvROK(sv)) {
	if (i)
	    (void)rsignal(i,sighandler);
	else
	    *svp = SvREFCNT_inc(sv);
	return 0;
    }
    s = SvPV_force(sv,na);
    if (strEQ(s,"IGNORE")) {
	if (i)
	    (void)rsignal(i,SIG_IGN);
	else
	    *svp = 0;
    }
    else if (strEQ(s,"DEFAULT") || !*s) {
	if (i)
	    (void)rsignal(i,SIG_DFL);
	else
	    *svp = 0;
    }
    else {
	if (!strchr(s,':') && !strchr(s,'\'')) {
	    sprintf(tokenbuf, "main::%s",s);
	    sv_setpv(sv,tokenbuf);
	}
	if (i)
	    (void)rsignal(i,sighandler);
	else
	    *svp = SvREFCNT_inc(sv);
    }
    return 0;
}

int
magic_setisa(sv,mg)
SV* sv;
MAGIC* mg;
{
    sub_generation++;
    return 0;
}

#ifdef OVERLOAD

int
magic_setamagic(sv,mg)
SV* sv;
MAGIC* mg;
{
    /* HV_badAMAGIC_on(Sv_STASH(sv)); */
    amagic_generation++;

    return 0;
}
#endif /* OVERLOAD */

static int
magic_methpack(sv,mg,meth)
SV* sv;
MAGIC* mg;
char *meth;
{
    dSP;

    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    EXTEND(sp, 2);
    PUSHs(mg->mg_obj);
    if (mg->mg_ptr)
	PUSHs(sv_2mortal(newSVpv(mg->mg_ptr, mg->mg_len)));
    else if (mg->mg_type == 'p')
	PUSHs(sv_2mortal(newSViv(mg->mg_len)));
    PUTBACK;

    if (perl_call_method(meth, G_SCALAR))
	sv_setsv(sv, *stack_sp--);

    FREETMPS;
    LEAVE;
    return 0;
}

int
magic_getpack(sv,mg)
SV* sv;
MAGIC* mg;
{
    magic_methpack(sv,mg,"FETCH");
    if (mg->mg_ptr)
	mg->mg_flags |= MGf_GSKIP;
    return 0;
}

int
magic_setpack(sv,mg)
SV* sv;
MAGIC* mg;
{
    dSP;

    PUSHMARK(sp);
    EXTEND(sp, 3);
    PUSHs(mg->mg_obj);
    if (mg->mg_ptr)
	PUSHs(sv_2mortal(newSVpv(mg->mg_ptr, mg->mg_len)));
    else if (mg->mg_type == 'p')
	PUSHs(sv_2mortal(newSViv(mg->mg_len)));
    PUSHs(sv);
    PUTBACK;

    perl_call_method("STORE", G_SCALAR|G_DISCARD);

    return 0;
}

int
magic_clearpack(sv,mg)
SV* sv;
MAGIC* mg;
{
    return magic_methpack(sv,mg,"DELETE");
}

int magic_wipepack(sv,mg)
SV* sv;
MAGIC* mg;
{
    dSP;

    PUSHMARK(sp);
    XPUSHs(mg->mg_obj);
    PUTBACK;

    perl_call_method("CLEAR", G_SCALAR|G_DISCARD);

    return 0;
}

int
magic_nextpack(sv,mg,key)
SV* sv;
MAGIC* mg;
SV* key;
{
    dSP;
    char *meth = SvOK(key) ? "NEXTKEY" : "FIRSTKEY";

    ENTER;
    SAVETMPS;
    PUSHMARK(sp);
    EXTEND(sp, 2);
    PUSHs(mg->mg_obj);
    if (SvOK(key))
	PUSHs(key);
    PUTBACK;

    if (perl_call_method(meth, G_SCALAR))
	sv_setsv(key, *stack_sp--);

    FREETMPS;
    LEAVE;
    return 0;
}

int
magic_existspack(sv,mg)
SV* sv;
MAGIC* mg;
{
    return magic_methpack(sv,mg,"EXISTS");
} 

int
magic_setdbline(sv,mg)
SV* sv;
MAGIC* mg;
{
    OP *o;
    I32 i;
    GV* gv;
    SV** svp;

    gv = DBline;
    i = SvTRUE(sv);
    svp = av_fetch(GvAV(gv),atoi(mg->mg_ptr), FALSE);
    if (svp && SvIOKp(*svp) && (o = (OP*)SvSTASH(*svp)))
	o->op_private = i;
    else
	warn("Can't break at that line\n");
    return 0;
}

int
magic_getarylen(sv,mg)
SV* sv;
MAGIC* mg;
{
    sv_setiv(sv, AvFILL((AV*)mg->mg_obj) + curcop->cop_arybase);
    return 0;
}

int
magic_setarylen(sv,mg)
SV* sv;
MAGIC* mg;
{
    av_fill((AV*)mg->mg_obj, SvIV(sv) - curcop->cop_arybase);
    return 0;
}

int
magic_getpos(sv,mg)
SV* sv;
MAGIC* mg;
{
    SV* lsv = LvTARG(sv);
    
    if (SvTYPE(lsv) >= SVt_PVMG && SvMAGIC(lsv)) {
	mg = mg_find(lsv, 'g');
	if (mg && mg->mg_len >= 0) {
	    sv_setiv(sv, mg->mg_len + curcop->cop_arybase);
	    return 0;
	}
    }
    (void)SvOK_off(sv);
    return 0;
}

int
magic_setpos(sv,mg)
SV* sv;
MAGIC* mg;
{
    SV* lsv = LvTARG(sv);
    SSize_t pos;
    STRLEN len;

    mg = 0;
    
    if (SvTYPE(lsv) >= SVt_PVMG && SvMAGIC(lsv))
	mg = mg_find(lsv, 'g');
    if (!mg) {
	if (!SvOK(sv))
	    return 0;
	sv_magic(lsv, (SV*)0, 'g', Nullch, 0);
	mg = mg_find(lsv, 'g');
    }
    else if (!SvOK(sv)) {
	mg->mg_len = -1;
	return 0;
    }
    len = SvPOK(lsv) ? SvCUR(lsv) : sv_len(lsv);

    pos = SvIV(sv) - curcop->cop_arybase;
    if (pos < 0) {
	pos += len;
	if (pos < 0)
	    pos = 0;
    }
    else if (pos > len)
	pos = len;
    mg->mg_len = pos;

    return 0;
}

int
magic_getglob(sv,mg)
SV* sv;
MAGIC* mg;
{
    gv_efullname(sv,((GV*)sv));/* a gv value, be nice */
    return 0;
}

int
magic_setglob(sv,mg)
SV* sv;
MAGIC* mg;
{
    register char *s;
    GV* gv;

    if (!SvOK(sv))
	return 0;
    s = SvPV(sv, na);
    if (*s == '*' && s[1])
	s++;
    gv = gv_fetchpv(s,TRUE, SVt_PVGV);
    if (sv == (SV*)gv)
	return 0;
    if (GvGP(sv))
	gp_free(sv);
    GvGP(sv) = gp_ref(GvGP(gv));
    if (!GvAV(gv))
	gv_AVadd(gv);
    if (!GvHV(gv))
	gv_HVadd(gv);
    if (!GvIOp(gv))
	GvIOp(gv) = newIO();
    return 0;
}

int
magic_setsubstr(sv,mg)
SV* sv;
MAGIC* mg;
{
    STRLEN len;
    char *tmps = SvPV(sv,len);
    sv_insert(LvTARG(sv),LvTARGOFF(sv),LvTARGLEN(sv), tmps, len);
    return 0;
}

int
magic_gettaint(sv,mg)
SV* sv;
MAGIC* mg;
{
    if (mg->mg_len & 1)
	tainted = TRUE;
    else if (mg->mg_len & 2 && mg->mg_obj == sv)	/* kludge */
	tainted = TRUE;
    return 0;
}

int
magic_settaint(sv,mg)
SV* sv;
MAGIC* mg;
{
    if (localizing) {
	if (localizing == 1)
	    mg->mg_len <<= 1;
	else
	    mg->mg_len >>= 1;
    }
    else if (tainted)
	mg->mg_len |= 1;
    else
	mg->mg_len &= ~1;
    return 0;
}

int
magic_setvec(sv,mg)
SV* sv;
MAGIC* mg;
{
    do_vecset(sv);	/* XXX slurp this routine */
    return 0;
}

int
magic_setmglob(sv,mg)
SV* sv;
MAGIC* mg;
{
    mg->mg_len = -1;
    SvSCREAM_off(sv);
    return 0;
}

int
magic_setbm(sv,mg)
SV* sv;
MAGIC* mg;
{
    sv_unmagic(sv, 'B');
    SvVALID_off(sv);
    return 0;
}

int
magic_setuvar(sv,mg)
SV* sv;
MAGIC* mg;
{
    struct ufuncs *uf = (struct ufuncs *)mg->mg_ptr;

    if (uf && uf->uf_set)
	(*uf->uf_set)(uf->uf_index, sv);
    return 0;
}

int
magic_set(sv,mg)
SV* sv;
MAGIC* mg;
{
    register char *s;
    I32 i;
    STRLEN len;
    switch (*mg->mg_ptr) {
    case '\001':	/* ^A */
	sv_setsv(bodytarget, sv);
	break;
    case '\004':	/* ^D */
	debug = (SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv)) | 0x80000000;
	DEBUG_x(dump_all());
	break;
    case '\005':  /* ^E */
#ifdef VMS
	set_vaxc_errno(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
#else
	SETERRNO(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv),4);		/* will anyone ever use this? */
#endif
	break;
    case '\006':	/* ^F */
	maxsysfd = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	break;
    case '\010':	/* ^H */
	hints = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	break;
    case '\t':	/* ^I */
	if (inplace)
	    Safefree(inplace);
	if (SvOK(sv))
	    inplace = savepv(SvPV(sv,na));
	else
	    inplace = Nullch;
	break;
    case '\017':	/* ^O */
	if (osname)
	    Safefree(osname);
	if (SvOK(sv))
	    osname = savepv(SvPV(sv,na));
	else
	    osname = Nullch;
	break;
    case '\020':	/* ^P */
	i = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	if (i != perldb) {
	    if (perldb)
		oldlastpm = curpm;
	    else
		curpm = oldlastpm;
	}
	perldb = i;
	break;
    case '\024':	/* ^T */
	basetime = (Time_t)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	break;
    case '\027':	/* ^W */
	dowarn = (bool)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	break;
    case '.':
	if (localizing) {
	    if (localizing == 1)
		save_sptr((SV**)&last_in_gv);
	}
	else if (SvOK(sv))
	    IoLINES(GvIOp(last_in_gv)) = (long)SvIV(sv);
	break;
    case '^':
	Safefree(IoTOP_NAME(GvIOp(defoutgv)));
	IoTOP_NAME(GvIOp(defoutgv)) = s = savepv(SvPV(sv,na));
	IoTOP_GV(GvIOp(defoutgv)) = gv_fetchpv(s,TRUE, SVt_PVIO);
	break;
    case '~':
	Safefree(IoFMT_NAME(GvIOp(defoutgv)));
	IoFMT_NAME(GvIOp(defoutgv)) = s = savepv(SvPV(sv,na));
	IoFMT_GV(GvIOp(defoutgv)) = gv_fetchpv(s,TRUE, SVt_PVIO);
	break;
    case '=':
	IoPAGE_LEN(GvIOp(defoutgv)) = (long)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	break;
    case '-':
	IoLINES_LEFT(GvIOp(defoutgv)) = (long)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	if (IoLINES_LEFT(GvIOp(defoutgv)) < 0L)
	    IoLINES_LEFT(GvIOp(defoutgv)) = 0L;
	break;
    case '%':
	IoPAGE(GvIOp(defoutgv)) = (long)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	break;
    case '|':
	IoFLAGS(GvIOp(defoutgv)) &= ~IOf_FLUSH;
	if ((SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv)) != 0) {
	    IoFLAGS(GvIOp(defoutgv)) |= IOf_FLUSH;
	}
	break;
    case '*':
	i = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	multiline = (i != 0);
	break;
    case '/':
	SvREFCNT_dec(nrs);
	nrs = newSVsv(sv);
	SvREFCNT_dec(rs);
	rs = SvREFCNT_inc(nrs);
	break;
    case '\\':
	if (ors)
	    Safefree(ors);
	ors = savepv(SvPV(sv,orslen));
	break;
    case ',':
	if (ofs)
	    Safefree(ofs);
	ofs = savepv(SvPV(sv, ofslen));
	break;
    case '#':
	if (ofmt)
	    Safefree(ofmt);
	ofmt = savepv(SvPV(sv,na));
	break;
    case '[':
	compiling.cop_arybase = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	break;
    case '?':
	statusvalue = FIXSTATUS(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	break;
    case '!':
	SETERRNO(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv),SvIV(sv) == EVMSERR ? 4 : vaxc$errno);		/* will anyone ever use this? */
	break;
    case '<':
	uid = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	if (delaymagic) {
	    delaymagic |= DM_RUID;
	    break;				/* don't do magic till later */
	}
#ifdef HAS_SETRUID
	(void)setruid((Uid_t)uid);
#else
#ifdef HAS_SETREUID
	(void)setreuid((Uid_t)uid, (Uid_t)-1);
#else
#ifdef HAS_SETRESUID
      (void)setresuid((Uid_t)uid, (Uid_t)-1, (Uid_t)-1);
#else
	if (uid == euid)		/* special case $< = $> */
	    (void)setuid(uid);
	else {
	    uid = (I32)getuid();
	    croak("setruid() not implemented");
	}
#endif
#endif
#endif
	uid = (I32)getuid();
	tainting |= (uid && (euid != uid || egid != gid));
	break;
    case '>':
	euid = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	if (delaymagic) {
	    delaymagic |= DM_EUID;
	    break;				/* don't do magic till later */
	}
#ifdef HAS_SETEUID
	(void)seteuid((Uid_t)euid);
#else
#ifdef HAS_SETREUID
	(void)setreuid((Uid_t)-1, (Uid_t)euid);
#else
#ifdef HAS_SETRESUID
	(void)setresuid((Uid_t)-1, (Uid_t)euid, (Uid_t)-1);
#else
	if (euid == uid)		/* special case $> = $< */
	    setuid(euid);
	else {
	    euid = (I32)geteuid();
	    croak("seteuid() not implemented");
	}
#endif
#endif
#endif
	euid = (I32)geteuid();
	tainting |= (uid && (euid != uid || egid != gid));
	break;
    case '(':
	gid = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	if (delaymagic) {
	    delaymagic |= DM_RGID;
	    break;				/* don't do magic till later */
	}
#ifdef HAS_SETRGID
	(void)setrgid((Gid_t)gid);
#else
#ifdef HAS_SETREGID
	(void)setregid((Gid_t)gid, (Gid_t)-1);
#else
#ifdef HAS_SETRESGID
      (void)setresgid((Gid_t)gid, (Gid_t)-1, (Gid_t) 1);
#else
	if (gid == egid)			/* special case $( = $) */
	    (void)setgid(gid);
	else {
	    gid = (I32)getgid();
	    croak("setrgid() not implemented");
	}
#endif
#endif
#endif
	gid = (I32)getgid();
	tainting |= (uid && (euid != uid || egid != gid));
	break;
    case ')':
	egid = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	if (delaymagic) {
	    delaymagic |= DM_EGID;
	    break;				/* don't do magic till later */
	}
#ifdef HAS_SETEGID
	(void)setegid((Gid_t)egid);
#else
#ifdef HAS_SETREGID
	(void)setregid((Gid_t)-1, (Gid_t)egid);
#else
#ifdef HAS_SETRESGID
	(void)setresgid((Gid_t)-1, (Gid_t)egid, (Gid_t)-1);
#else
	if (egid == gid)			/* special case $) = $( */
	    (void)setgid(egid);
	else {
	    egid = (I32)getegid();
	    croak("setegid() not implemented");
	}
#endif
#endif
#endif
	egid = (I32)getegid();
	tainting |= (uid && (euid != uid || egid != gid));
	break;
    case ':':
	chopset = SvPV_force(sv,na);
	break;
    case '0':
	if (!origalen) {
	    s = origargv[0];
	    s += strlen(s);
	    /* See if all the arguments are contiguous in memory */
	    for (i = 1; i < origargc; i++) {
		if (origargv[i] == s + 1)
		    s += strlen(++s);	/* this one is ok too */
	    }
	    if (origenviron[0] == s + 1) {	/* can grab env area too? */
		my_setenv("NoNeSuCh", Nullch);
					    /* force copy of environment */
		for (i = 0; origenviron[i]; i++)
		    if (origenviron[i] == s + 1)
			s += strlen(++s);
	    }
	    origalen = s - origargv[0];
	}
	s = SvPV_force(sv,len);
	i = len;
	if (i >= origalen) {
	    i = origalen;
	    SvCUR_set(sv, i);
	    *SvEND(sv) = '\0';
	    Copy(s, origargv[0], i, char);
	}
	else {
	    Copy(s, origargv[0], i, char);
	    s = origargv[0]+i;
	    *s++ = '\0';
	    while (++i < origalen)
		*s++ = ' ';
	    s = origargv[0]+i;
	    for (i = 1; i < origargc; i++)
		origargv[i] = Nullch;
	}
	break;
    }
    return 0;
}

I32
whichsig(sig)
char *sig;
{
    register char **sigv;

    for (sigv = sig_name+1; *sigv; sigv++)
	if (strEQ(sig,*sigv))
	    return sig_num[sigv - sig_name];
#ifdef SIGCLD
    if (strEQ(sig,"CHLD"))
	return SIGCLD;
#endif
#ifdef SIGCHLD
    if (strEQ(sig,"CLD"))
	return SIGCHLD;
#endif
    return 0;
}

Signal_t
sighandler(sig)
int sig;
{
    dSP;
    GV *gv;
    HV *st;
    SV *sv;
    CV *cv;
    AV *oldstack;
    char *signame; 

#ifdef OS2		/* or anybody else who requires SIG_ACK */
    signal(sig, SIG_ACK);
#endif

    signame = sig_name[sig];
    cv = sv_2cv(*hv_fetch(GvHVn(siggv),signame,strlen(signame),
			  TRUE),
		&st, &gv, TRUE);
    if (!cv || !CvROOT(cv) &&
	*signame == 'C' && instr(signame,"LD")) {
	
	if (signame[1] == 'H')
	    cv = sv_2cv(*hv_fetch(GvHVn(siggv),"CLD",3,TRUE),
			&st, &gv, TRUE);
	else
	    cv = sv_2cv(*hv_fetch(GvHVn(siggv),"CHLD",4,TRUE),
			&st, &gv, TRUE);
	/* gag */
    }
    if (!cv || !CvROOT(cv)) {
	if (dowarn)
	    warn("SIG%s handler \"%s\" not defined.\n",
		signame, GvENAME(gv) );
	return;
    }

    oldstack = stack;
    if (stack != signalstack)
	AvFILL(signalstack) = 0;
    SWITCHSTACK(stack, signalstack);

    sv = sv_newmortal();
    sv_setpv(sv,signame);
    PUSHMARK(sp);
    PUSHs(sv);
    PUTBACK;

    perl_call_sv((SV*)cv, G_DISCARD);

    SWITCHSTACK(signalstack, oldstack);

    return;
}
