/*    mg.c
 *
 *    Copyright (c) 1991-1997, Larry Wall
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

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
# include <unistd.h>
#endif

#if defined(HAS_GETGROUPS) || defined(HAS_SETGROUPS)
#  ifndef NGROUPS
#    define NGROUPS 32
#  endif
#endif

/*
 * Use the "DESTRUCTOR" scope cleanup to reinstate magic.
 */

struct magic_state {
    SV* mgs_sv;
    U32 mgs_flags;
};
typedef struct magic_state MGS;

static void restore_magic _((void *p));

static void
save_magic(mgs, sv)
MGS* mgs;
SV* sv;
{
    assert(SvMAGICAL(sv));

    mgs->mgs_sv = sv;
    mgs->mgs_flags = SvMAGICAL(sv) | SvREADONLY(sv);
    SAVEDESTRUCTOR(restore_magic, mgs);

    SvMAGICAL_off(sv);
    SvREADONLY_off(sv);
    SvFLAGS(sv) |= (SvFLAGS(sv) & (SVp_IOK|SVp_NOK|SVp_POK)) >> PRIVSHIFT;
}

static void
restore_magic(p)
void* p;
{
    MGS* mgs = (MGS*)p;
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
    MGS mgs;
    MAGIC* mg;
    MAGIC** mgp;
    int mgp_valid = 0;

    ENTER;
    save_magic(&mgs, sv);

    mgp = &SvMAGIC(sv);
    while ((mg = *mgp) != 0) {
	MGVTBL* vtbl = mg->mg_virtual;
	if (!(mg->mg_flags & MGf_GSKIP) && vtbl && vtbl->svt_get) {
	    (*vtbl->svt_get)(sv, mg);
	    /* Ignore this magic if it's been deleted */
	    if ((mg == (mgp_valid ? *mgp : SvMAGIC(sv))) &&
		  (mg->mg_flags & MGf_GSKIP))
		mgs.mgs_flags = 0;
	}
	/* Advance to next magic (complicated by possible deletion) */
	if (mg == (mgp_valid ? *mgp : SvMAGIC(sv))) {
	    mgp = &mg->mg_moremagic;
	    mgp_valid = 1;
	}
	else
	    mgp = &SvMAGIC(sv);	/* Re-establish pointer after sv_upgrade */
    }

    LEAVE;
    return 0;
}

int
mg_set(sv)
SV* sv;
{
    MGS mgs;
    MAGIC* mg;
    MAGIC* nextmg;

    ENTER;
    save_magic(&mgs, sv);

    for (mg = SvMAGIC(sv); mg; mg = nextmg) {
	MGVTBL* vtbl = mg->mg_virtual;
	nextmg = mg->mg_moremagic;	/* it may delete itself */
	if (mg->mg_flags & MGf_GSKIP) {
	    mg->mg_flags &= ~MGf_GSKIP;	/* setting requires another read */
	    mgs.mgs_flags = 0;
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
	    MGS mgs;

	    ENTER;
	    save_magic(&mgs, sv);
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
    MGS mgs;
    MAGIC* mg;

    ENTER;
    save_magic(&mgs, sv);

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
I32 klen;
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
	    if (mg->mg_len >= 0)
		Safefree(mg->mg_ptr);
	    else if (mg->mg_len == HEf_SVKEY)
		SvREFCNT_dec((SV*)mg->mg_ptr);
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
    register REGEXP *rx;
    char *t;

    switch (*mg->mg_ptr) {
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '&':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    paren = atoi(mg->mg_ptr);
	  getparen:
	    if (paren <= rx->nparens &&
		(s = rx->startp[paren]) &&
		(t = rx->endp[paren]))
	    {
		i = t - s;
		if (i >= 0)
		    return i;
	    }
	}
	return 0;
    case '+':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    paren = rx->lastparen;
	    if (paren)
		goto getparen;
	}
	return 0;
    case '`':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    if ((s = rx->subbeg) && rx->startp[0]) {
		i = rx->startp[0] - s;
		if (i >= 0)
		    return i;
	    }
	}
	return 0;
    case '\'':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    if (rx->subend && (s = rx->endp[0])) {
		i = rx->subend - s;
		if (i >= 0)
		    return i;
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
    register REGEXP *rx;
    char *t;

    switch (*mg->mg_ptr) {
    case '\001':		/* ^A */
	sv_setsv(sv, bodytarget);
	break;
    case '\004':		/* ^D */
	sv_setiv(sv, (IV)(debug & 32767));
	break;
    case '\005':  /* ^E */
#ifdef VMS
	{
#	    include <descrip.h>
#	    include <starlet.h>
	    char msg[255];
	    $DESCRIPTOR(msgdsc,msg);
	    sv_setnv(sv,(double) vaxc$errno);
	    if (sys$getmsg(vaxc$errno,&msgdsc.dsc$w_length,&msgdsc,0,0) & 1)
		sv_setpvn(sv,msgdsc.dsc$a_pointer,msgdsc.dsc$w_length);
	    else
		sv_setpv(sv,"");
	}
#else
#ifdef OS2
	if (!(_emx_env & 0x200)) {	/* Under DOS */
	    sv_setnv(sv, (double)errno);
	    sv_setpv(sv, errno ? Strerror(errno) : "");
	} else {
	    if (errno != errno_isOS2)
		Perl_rc = _syserrno();
	    sv_setnv(sv, (double)Perl_rc);
	    sv_setpv(sv, os2error(Perl_rc));
	}
#else
	sv_setnv(sv, (double)errno);
	sv_setpv(sv, errno ? Strerror(errno) : "");
#endif
#endif
	SvNOK_on(sv);	/* what a wonderful hack! */
	break;
    case '\006':		/* ^F */
	sv_setiv(sv, (IV)maxsysfd);
	break;
    case '\010':		/* ^H */
	sv_setiv(sv, (IV)hints);
	break;
    case '\t':			/* ^I */
	if (inplace)
	    sv_setpv(sv, inplace);
	else
	    sv_setsv(sv, &sv_undef);
	break;
    case '\017':		/* ^O */
	sv_setpv(sv, osname);
	break;
    case '\020':		/* ^P */
	sv_setiv(sv, (IV)perldb);
	break;
    case '\023':		/* ^S */
	if (lex_state != LEX_NOTPARSING)
	    SvOK_off(sv);
	else if (in_eval)
	    sv_setiv(sv, 1);
	else
	    sv_setiv(sv, 0);
	break;
    case '\024':		/* ^T */
#ifdef BIG_TIME
 	sv_setnv(sv, basetime);
#else
	sv_setiv(sv, (IV)basetime);
#endif
	break;
    case '\027':		/* ^W */
	sv_setiv(sv, (IV)dowarn);
	break;
    case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': case '&':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    paren = atoi(GvENAME((GV*)mg->mg_obj));
	  getparen:
	    if (paren <= rx->nparens &&
		(s = rx->startp[paren]) &&
		(t = rx->endp[paren]))
	    {
		i = t - s;
	      getrx:
		if (i >= 0) {
		    bool was_tainted;
		    if (tainting) {
			was_tainted = tainted;
			tainted = FALSE;
		    }
		    sv_setpvn(sv,s,i);
		    if (tainting)
			tainted = was_tainted || rx->exec_tainted;
		    break;
		}
	    }
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '+':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    paren = rx->lastparen;
	    if (paren)
		goto getparen;
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '`':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    if ((s = rx->subbeg) && rx->startp[0]) {
		i = rx->startp[0] - s;
		goto getrx;
	    }
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '\'':
	if (curpm && (rx = curpm->op_pmregexp)) {
	    if (rx->subend && (s = rx->endp[0])) {
		i = rx->subend - s;
		goto getrx;
	    }
	}
	sv_setsv(sv,&sv_undef);
	break;
    case '.':
#ifndef lint
	if (GvIO(last_in_gv)) {
	    sv_setiv(sv, (IV)IoLINES(GvIO(last_in_gv)));
	}
#endif
	break;
    case '?':
	sv_setiv(sv, (IV)STATUS_CURRENT);
#ifdef COMPLEX_STATUS
	LvTARGOFF(sv) = statusvalue;
	LvTARGLEN(sv) = statusvalue_vms;
#endif
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
	sv_setiv(sv, (IV)IoPAGE_LEN(GvIOp(defoutgv)));
	break;
    case '-':
	sv_setiv(sv, (IV)IoLINES_LEFT(GvIOp(defoutgv)));
	break;
    case '%':
	sv_setiv(sv, (IV)IoPAGE(GvIOp(defoutgv)));
	break;
#endif
    case ':':
	break;
    case '/':
	break;
    case '[':
	sv_setiv(sv, (IV)curcop->cop_arybase);
	break;
    case '|':
	sv_setiv(sv, (IV)(IoFLAGS(GvIOp(defoutgv)) & IOf_FLUSH) != 0 );
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
	sv_setnv(sv, (double)((errno == EVMSERR) ? vaxc$errno : errno));
	sv_setpv(sv, errno ? Strerror(errno) : "");
#else
	{
	int saveerrno = errno;
	sv_setnv(sv, (double)errno);
#ifdef OS2
	if (errno == errno_isOS2) sv_setpv(sv, os2error(Perl_rc));
	else
#endif
	sv_setpv(sv, errno ? Strerror(errno) : "");
	errno = saveerrno;
	}
#endif
	SvNOK_on(sv);	/* what a wonderful hack! */
	break;
    case '<':
	sv_setiv(sv, (IV)uid);
	break;
    case '>':
	sv_setiv(sv, (IV)euid);
	break;
    case '(':
	sv_setiv(sv, (IV)gid);
	sv_setpvf(sv, "%Vd", (IV)gid);
	goto add_groups;
    case ')':
	sv_setiv(sv, (IV)egid);
	sv_setpvf(sv, "%Vd", (IV)egid);
      add_groups:
#ifdef HAS_GETGROUPS
	{
	    Groups_t gary[NGROUPS];
	    i = getgroups(NGROUPS,gary);
	    while (--i >= 0)
		sv_catpvf(sv, " %Vd", (IV)gary[i]);
	}
#endif
	SvIOK_on(sv);	/* what a wonderful hack! */
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
    char *ptr;
    STRLEN len, klen;
    I32 i;

    s = SvPV(sv,len);
    ptr = MgPV(mg,klen);
    my_setenv(ptr, s);

#ifdef DYNAMIC_ENV_FETCH
     /* We just undefd an environment var.  Is a replacement */
     /* waiting in the wings? */
    if (!len) {
	SV **valp;
	if ((valp = hv_fetch(GvHVn(envgv), ptr, klen, FALSE)))
	    s = SvPV(*valp, len);
    }
#endif

#if !defined(OS2) && !defined(AMIGAOS) && !defined(WIN32)
			    /* And you'll never guess what the dog had */
			    /*   in its mouth... */
    if (tainting) {
	MgTAINTEDDIR_off(mg);
#ifdef VMS
	if (s && klen == 8 && strEQ(ptr, "DCL$PATH")) {
	    char pathbuf[256], eltbuf[256], *cp, *elt = s;
	    struct stat sbuf;
	    int i = 0, j = 0;

	    do {          /* DCL$PATH may be a search list */
		while (1) {   /* as may dev portion of any element */
		    if ( ((cp = strchr(elt,'[')) || (cp = strchr(elt,'<'))) ) {
			if ( *(cp+1) == '.' || *(cp+1) == '-' ||
			     cando_by_name(S_IWUSR,0,elt) ) {
			    MgTAINTEDDIR_on(mg);
			    return 0;
			}
		    }
		    if ((cp = strchr(elt, ':')) != Nullch)
			*cp = '\0';
		    if (my_trnlnm(elt, eltbuf, j++))
			elt = eltbuf;
		    else
			break;
		}
		j = 0;
	    } while (my_trnlnm(s, pathbuf, i++) && (elt = pathbuf));
	}
#endif /* VMS */
	if (s && klen == 4 && strEQ(ptr,"PATH")) {
	    char *strend = s + len;

	    while (s < strend) {
		struct stat st;
		s = delimcpy(tokenbuf, tokenbuf + sizeof tokenbuf,
			     s, strend, ':', &i);
		s++;
		if (i >= sizeof tokenbuf   /* too long -- assume the worst */
		      || *tokenbuf != '/'
		      || (Stat(tokenbuf, &st) == 0 && (st.st_mode & 2)) ) {
		    MgTAINTEDDIR_on(mg);
		    return 0;
		}
	    }
	}
    }
#endif /* neither OS2 nor AMIGAOS nor WIN32 */

    return 0;
}

int
magic_clearenv(sv,mg)
SV* sv;
MAGIC* mg;
{
    my_setenv(MgPV(mg,na),Nullch);
    return 0;
}

int
magic_set_all_env(sv,mg)
SV* sv;
MAGIC* mg;
{
#if defined(VMS)
    die("Can't make list assignment to %%ENV on this system");
#else
    if (localizing) {
	HE* entry;
	magic_clear_all_env(sv,mg);
	hv_iterinit((HV*)sv);
	while (entry = hv_iternext((HV*)sv)) {
	    I32 keylen;
	    my_setenv(hv_iterkey(entry, &keylen),
		      SvPV(hv_iterval((HV*)sv, entry), na));
	}
    }
#endif
    return 0;
}

int
magic_clear_all_env(sv,mg)
SV* sv;
MAGIC* mg;
{
#if defined(VMS)
    die("Can't make list assignment to %%ENV on this system");
#else
#ifdef WIN32
    char *envv = GetEnvironmentStrings();
    char *cur = envv;
    STRLEN len;
    while (*cur) {
	char *end = strchr(cur,'=');
	if (end && end != cur) {
	    *end = '\0';
	    my_setenv(cur,Nullch);
	    *end = '=';
	    cur += strlen(end+1)+1;
	}
	else if ((len = strlen(cur)))
	    cur += len+1;
    }
    FreeEnvironmentStrings(envv);
#else
    I32 i;

    if (environ == origenviron)
	New(901, environ, 1, char*);
    else
	for (i = 0; environ[i]; i++)
	    Safefree(environ[i]);
    environ[0] = Nullch;

#endif
#endif
    return 0;
}

int
magic_getsig(sv,mg)
SV* sv;
MAGIC* mg;
{
    I32 i;
    /* Are we fetching a signal entry? */
    i = whichsig(MgPV(mg,na));
    if (i) {
    	if(psig_ptr[i])
    	    sv_setsv(sv,psig_ptr[i]);
    	else {
    	    Sighandler_t sigstate = rsignal_state(i);

    	    /* cache state so we don't fetch it again */
    	    if(sigstate == SIG_IGN)
    	    	sv_setpv(sv,"IGNORE");
    	    else
    	    	sv_setsv(sv,&sv_undef);
    	    psig_ptr[i] = SvREFCNT_inc(sv);
    	    SvTEMP_off(sv);
    	}
    }
    return 0;
}
int
magic_clearsig(sv,mg)
SV* sv;
MAGIC* mg;
{
    I32 i;
    /* Are we clearing a signal entry? */
    i = whichsig(MgPV(mg,na));
    if (i) {
    	if(psig_ptr[i]) {
    	    SvREFCNT_dec(psig_ptr[i]);
    	    psig_ptr[i]=0;
    	}
    	if(psig_name[i]) {
    	    SvREFCNT_dec(psig_name[i]);
    	    psig_name[i]=0;
    	}
    }
    return 0;
}

int
magic_setsig(sv,mg)
SV* sv;
MAGIC* mg;
{
    register char *s;
    I32 i;
    SV** svp;

    s = MgPV(mg,na);
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
	SvREFCNT_dec(psig_name[i]);
	SvREFCNT_dec(psig_ptr[i]);
	psig_ptr[i] = SvREFCNT_inc(sv);
	SvTEMP_off(sv); /* Make sure it doesn't go away on us */
	psig_name[i] = newSVpv(s, strlen(s));
	SvREADONLY_on(psig_name[i]);
    }
    if (SvTYPE(sv) == SVt_PVGV || SvROK(sv)) {
	if (i)
	    (void)rsignal(i, sighandler);
	else
	    *svp = SvREFCNT_inc(sv);
	return 0;
    }
    s = SvPV_force(sv,na);
    if (strEQ(s,"IGNORE")) {
	if (i)
	    (void)rsignal(i, SIG_IGN);
	else
	    *svp = 0;
    }
    else if (strEQ(s,"DEFAULT") || !*s) {
	if (i)
	    (void)rsignal(i, SIG_DFL);
	else
	    *svp = 0;
    }
    else {
	/*
	 * We should warn if HINT_STRICT_REFS, but without
	 * access to a known hint bit in a known OP, we can't
	 * tell whether HINT_STRICT_REFS is in force or not.
	 */
	if (!strchr(s,':') && !strchr(s,'\''))
	    sv_setpv(sv, form("main::%s", s));
	if (i)
	    (void)rsignal(i, sighandler);
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

int
magic_setnkeys(sv,mg)
SV* sv;
MAGIC* mg;
{
    if (LvTARG(sv)) {
	hv_ksplit((HV*)LvTARG(sv), SvIV(sv));
	LvTARG(sv) = Nullsv;	/* Don't allow a ref to reassign this. */
    }
    return 0;
}

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
    if (mg->mg_ptr) {
	if (mg->mg_len >= 0)
	    PUSHs(sv_2mortal(newSVpv(mg->mg_ptr, mg->mg_len)));
	else if (mg->mg_len == HEf_SVKEY)
	    PUSHs((SV*)mg->mg_ptr);
    }
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
    if (mg->mg_ptr) {
	if (mg->mg_len >= 0)
	    PUSHs(sv_2mortal(newSVpv(mg->mg_ptr, mg->mg_len)));
	else if (mg->mg_len == HEf_SVKEY)
	    PUSHs((SV*)mg->mg_ptr);
    }
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
    svp = av_fetch(GvAV(gv),
		     atoi(MgPV(mg,na)), FALSE);
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
    mg->mg_flags &= ~MGf_MINMATCH;

    return 0;
}

int
magic_getglob(sv,mg)
SV* sv;
MAGIC* mg;
{
    if (SvFAKE(sv)) {			/* FAKE globs can get coerced */
	SvFAKE_off(sv);
	gv_efullname3(sv,((GV*)sv), "*");
	SvFAKE_on(sv);
    }
    else
	gv_efullname3(sv,((GV*)sv), "*");	/* a gv value, be nice */
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
	gp_free((GV*)sv);
    GvGP(sv) = gp_ref(GvGP(gv));
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
    TAINT_IF((mg->mg_len & 1) ||
	     (mg->mg_len & 2) && mg->mg_obj == sv);	/* kludge */
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
magic_getdefelem(sv,mg)
SV* sv;
MAGIC* mg;
{
    SV *targ = Nullsv;
    if (LvTARGLEN(sv)) {
	if (mg->mg_obj) {
	    HV* hv = (HV*)LvTARG(sv);
	    HE* he = hv_fetch_ent(hv, mg->mg_obj, FALSE, 0);
	    if (he)
		targ = HeVAL(he);
	}
	else {
	    AV* av = (AV*)LvTARG(sv);
	    if ((I32)LvTARGOFF(sv) <= AvFILL(av))
		targ = AvARRAY(av)[LvTARGOFF(sv)];
	}
	if (targ && targ != &sv_undef) {
	    /* somebody else defined it for us */
	    SvREFCNT_dec(LvTARG(sv));
	    LvTARG(sv) = SvREFCNT_inc(targ);
	    LvTARGLEN(sv) = 0;
	    SvREFCNT_dec(mg->mg_obj);
	    mg->mg_obj = Nullsv;
	    mg->mg_flags &= ~MGf_REFCOUNTED;
	}
    }
    else
	targ = LvTARG(sv);
    sv_setsv(sv, targ ? targ : &sv_undef);
    return 0;
}

int
magic_setdefelem(sv,mg)
SV* sv;
MAGIC* mg;
{
    if (LvTARGLEN(sv))
	vivify_defelem(sv);
    if (LvTARG(sv)) {
	sv_setsv(LvTARG(sv), sv);
	SvSETMAGIC(LvTARG(sv));
    }
    return 0;
}

int
magic_freedefelem(sv,mg)
SV* sv;
MAGIC* mg;
{
    SvREFCNT_dec(LvTARG(sv));
    return 0;
}

void
vivify_defelem(sv)
SV* sv;
{
    MAGIC* mg;
    SV* value;

    if (!LvTARGLEN(sv) || !(mg = mg_find(sv, 'y')))
	return;
    if (mg->mg_obj) {
	HV* hv = (HV*)LvTARG(sv);
	HE* he = hv_fetch_ent(hv, mg->mg_obj, TRUE, 0);
	if (!he || (value = HeVAL(he)) == &sv_undef)
	    croak(no_helem, SvPV(mg->mg_obj, na));
    }
    else {
	AV* av = (AV*)LvTARG(sv);
	if ((I32)LvTARGLEN(sv) < 0 && (I32)LvTARGOFF(sv) > AvFILL(av))
	    LvTARG(sv) = Nullsv;	/* array can't be extended */
	else {
	    SV** svp = av_fetch(av, LvTARGOFF(sv), TRUE);
	    if (!svp || (value = *svp) == &sv_undef)
		croak(no_aelem, (I32)LvTARGOFF(sv));
	}
    }
    (void)SvREFCNT_inc(value);
    SvREFCNT_dec(LvTARG(sv));
    LvTARG(sv) = value;
    LvTARGLEN(sv) = 0;
    SvREFCNT_dec(mg->mg_obj);
    mg->mg_obj = Nullsv;
    mg->mg_flags &= ~MGf_REFCOUNTED;
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
magic_setfm(sv,mg)
SV* sv;
MAGIC* mg;
{
    sv_unmagic(sv, 'f');
    SvCOMPILED_off(sv);
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

#ifdef USE_LOCALE_COLLATE
int
magic_setcollxfrm(sv,mg)
SV* sv;
MAGIC* mg;
{
    /*
     * René Descartes said "I think not."
     * and vanished with a faint plop.
     */
    if (mg->mg_ptr) {
	Safefree(mg->mg_ptr);
	mg->mg_ptr = NULL;
	mg->mg_len = -1;
    }
    return 0;
}
#endif /* USE_LOCALE_COLLATE */

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
	/* will anyone ever use this? */
	SETERRNO(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv), 4);
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
	perldb = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
	break;
    case '\024':	/* ^T */
#ifdef BIG_TIME
	basetime = (Time_t)(SvNOK(sv) ? SvNVX(sv) : sv_2nv(sv));
#else
	basetime = (Time_t)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
#endif
	break;
    case '\027':	/* ^W */
	dowarn = (bool)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	break;
    case '.':
	if (localizing) {
	    if (localizing == 1)
		save_sptr((SV**)&last_in_gv);
	}
	else if (SvOK(sv) && GvIO(last_in_gv))
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
	{
	    IO *io = GvIOp(defoutgv);
	    if ((SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv)) == 0)
		IoFLAGS(io) &= ~IOf_FLUSH;
	    else {
		if (!(IoFLAGS(io) & IOf_FLUSH)) {
		    PerlIO *ofp = IoOFP(io);
		    if (ofp)
			(void)PerlIO_flush(ofp);
		    IoFLAGS(io) |= IOf_FLUSH;
		}
	    }
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
	if (SvOK(sv) || SvGMAGICAL(sv))
	    ors = savepv(SvPV(sv,orslen));
	else {
	    ors = Nullch;
	    orslen = 0;
	}
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
#ifdef COMPLEX_STATUS
	if (localizing == 2) {
	    statusvalue = LvTARGOFF(sv);
	    statusvalue_vms = LvTARGLEN(sv);
	}
	else
#endif
#ifdef VMSISH_STATUS
	if (VMSISH_STATUS)
	    STATUS_NATIVE_SET((U32)(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv)));
	else
#endif
	    STATUS_POSIX_SET(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv));
	break;
    case '!':
	SETERRNO(SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv),
		 (SvIV(sv) == EVMSERR) ? 4 : vaxc$errno);
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
#ifdef HAS_SETGROUPS
	{
	    char *p = SvPV(sv, na);
	    Groups_t gary[NGROUPS];

	    SET_NUMERIC_STANDARD();
	    while (isSPACE(*p))
		++p;
	    egid = I_V(atof(p));
	    for (i = 0; i < NGROUPS; ++i) {
		while (*p && !isSPACE(*p))
		    ++p;
		while (isSPACE(*p))
		    ++p;
		if (!*p)
		    break;
		gary[i] = I_V(atof(p));
	    }
	    if (i)
		(void)setgroups(i, gary);
	}
#else  /* HAS_SETGROUPS */
	egid = SvIOK(sv) ? SvIVX(sv) : sv_2iv(sv);
#endif /* HAS_SETGROUPS */
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
		if (origargv[i] == s + 1
#ifdef OS2
		    || origargv[i] == s + 2
#endif 
		   )
		    s += strlen(++s);	/* this one is ok too */
		else
		    break;
	    }
	    /* can grab env area too? */
	    if (origenviron && (origenviron[0] == s + 1
#ifdef OS2
				|| (origenviron[0] == s + 9 && (s += 8))
#endif 
	       )) {
		my_setenv("NoNe  SuCh", Nullch);
					    /* force copy of environment */
		for (i = 0; origenviron[i]; i++)
		    if (origenviron[i] == s + 1)
			s += strlen(++s);
		    else
			break;
	    }
	    origalen = s - origargv[0];
	}
	s = SvPV_force(sv,len);
	i = len;
	if (i >= origalen) {
	    i = origalen;
	    /* don't allow system to limit $0 seen by script */
	    /* SvCUR_set(sv, i); *SvEND(sv) = '\0'; */
	    Copy(s, origargv[0], i, char);
	    s = origargv[0]+i;
	    *s = '\0';
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

static SV* sig_sv;

static void
unwind_handler_stack(p)
    void *p;
{
    U32 flags = *(U32*)p;

    if (flags & 1)
	savestack_ix -= 5; /* Unprotect save in progress. */
    /* cxstack_ix-- Not needed, die already unwound it. */
    if (flags & 64)
	SvREFCNT_dec(sig_sv);
}

Signal_t
sighandler(sig)
int sig;
{
    dSP;
    GV *gv;
    HV *st;
    SV *sv, *tSv = Sv;
    CV *cv;
    AV *oldstack;
    OP *myop = op;
    U32 flags = 0;
    I32 o_save_i = savestack_ix, type;
    CONTEXT *cx;
    XPV *tXpv = Xpv;
    
    if (savestack_ix + 15 <= savestack_max)
	flags |= 1;
    if (cxstack_ix < cxstack_max - 2)
	flags |= 2;
    if (markstack_ptr < markstack_max - 2)
	flags |= 4;
    if (retstack_ix < retstack_max - 2)
	flags |= 8;
    if (scopestack_ix < scopestack_max - 3)
	flags |= 16;

    if (flags & 2) {		/* POPBLOCK may decrease cxstack too early. */
	cxstack_ix++;		/* Protect from overwrite. */
	cx = &cxstack[cxstack_ix];
	type = cx->cx_type;		/* Can be during partial write. */
	cx->cx_type = CXt_NULL;		/* Make it safe for unwind. */
    }
    if (!psig_ptr[sig])
	die("Signal SIG%s received, but no signal handler set.\n",
	    sig_name[sig]);

    /* Max number of items pushed there is 3*n or 4. We cannot fix
       infinity, so we fix 4 (in fact 5): */
    if (flags & 1) {
	savestack_ix += 5;		/* Protect save in progress. */
	o_save_i = savestack_ix;
	SAVEDESTRUCTOR(unwind_handler_stack, (void*)&flags);
    }
    if (flags & 4) 
	markstack_ptr++;		/* Protect mark. */
    if (flags & 8) {
	retstack_ix++;
	retstack[retstack_ix] = NULL;
    }
    if (flags & 16)
	scopestack_ix += 1;
    /* sv_2cv is too complicated, try a simpler variant first: */
    if (!SvROK(psig_ptr[sig]) || !(cv = (CV*)SvRV(psig_ptr[sig])) 
	|| SvTYPE(cv) != SVt_PVCV)
	cv = sv_2cv(psig_ptr[sig],&st,&gv,TRUE);

    if (!cv || !CvROOT(cv)) {
	if (dowarn)
	    warn("SIG%s handler \"%s\" not defined.\n",
		sig_name[sig], GvENAME(gv) );
	return;
    }

    oldstack = curstack;
    if (curstack != signalstack)
	AvFILL(signalstack) = 0;
    SWITCHSTACK(curstack, signalstack);

    if(psig_name[sig]) {
    	sv = SvREFCNT_inc(psig_name[sig]);
	flags |= 64;
	sig_sv = sv;
    } else {
	sv = sv_newmortal();
	sv_setpv(sv,sig_name[sig]);
    }
    PUSHMARK(sp);
    PUSHs(sv);
    PUTBACK;

    perl_call_sv((SV*)cv, G_DISCARD);

    SWITCHSTACK(signalstack, oldstack);
    if (flags & 1)
	savestack_ix -= 8; /* Unprotect save in progress. */
    if (flags & 2) {
	cxstack[cxstack_ix].cx_type = type;
	cxstack_ix -= 1;
    }
    if (flags & 4) 
	markstack_ptr--;
    if (flags & 8) 
	retstack_ix--;
    if (flags & 16)
	scopestack_ix -= 1;
    if (flags & 64)
	SvREFCNT_dec(sv);
    op = myop;			/* Apparently not needed... */
    
    Sv = tSv;			/* Restore global temporaries. */
    Xpv = tXpv;
    return;
}
