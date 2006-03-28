typedef char *pvcontents;
typedef char *strconst;
typedef U32 PV;
typedef char *op_tr_array;
typedef int comment_t;
typedef SV *svindex;
typedef OP *opindex;
typedef char *pvindex;

#define BGET_FREAD(argp, len, nelem)	\
	 bl_read(bstate->bs_fdata,(char*)(argp),(len),(nelem))
#define BGET_FGETC() bl_getc(bstate->bs_fdata)

/* all this should be made endianness-agnostic */

#define BGET_U8(arg)	arg = BGET_FGETC()
#define BGET_U16(arg)	\
	BGET_FREAD(&arg, sizeof(U16), 1)
#define BGET_U32(arg)	\
	BGET_FREAD(&arg, sizeof(U32), 1)
#define BGET_UV(arg)	\
	BGET_FREAD(&arg, sizeof(UV), 1)
#define BGET_PADOFFSET(arg)	\
	BGET_FREAD(&arg, sizeof(PADOFFSET), 1)
#define BGET_long(arg)		\
	BGET_FREAD(&arg, sizeof(long), 1)

#define BGET_I32(arg)	BGET_U32(arg)
#define BGET_IV(arg)	BGET_UV(arg)

#define BGET_PV(arg)	STMT_START {					\
	BGET_U32(arg);							\
	if (arg) {							\
	    Newx(bstate->bs_pv.xpv_pv, arg, char);			\
	    bl_read(bstate->bs_fdata, bstate->bs_pv.xpv_pv, arg, 1);	\
	    bstate->bs_pv.xpv_len = arg;				\
	    bstate->bs_pv.xpv_cur = arg - 1;				\
	} else {							\
	    bstate->bs_pv.xpv_pv = 0;					\
	    bstate->bs_pv.xpv_len = 0;					\
	    bstate->bs_pv.xpv_cur = 0;					\
	}								\
    } STMT_END

#ifdef BYTELOADER_LOG_COMMENTS
#  define BGET_comment_t(arg) \
    STMT_START {							\
	char buf[1024];							\
	int i = 0;							\
	do {								\
	    arg = BGET_FGETC();						\
	    buf[i++] = (char)arg;					\
	} while (arg != '\n' && arg != EOF);				\
	buf[i] = '\0';							\
	PerlIO_printf(PerlIO_stderr(), "%s", buf);			\
    } STMT_END
#else
#  define BGET_comment_t(arg) \
	do { arg = BGET_FGETC(); } while (arg != '\n' && arg != EOF)
#endif


#define BGET_op_tr_array(arg) do {			\
	unsigned short *ary, len;			\
	BGET_U16(len);					\
	Newx(ary, len, unsigned short);		\
	BGET_FREAD(ary, sizeof(unsigned short), len);	\
	arg = (char *) ary;				\
    } while (0)

#define BGET_pvcontents(arg)	arg = bstate->bs_pv.xpv_pv
#define BGET_strconst(arg) STMT_START {	\
	for (arg = PL_tokenbuf; (*arg = BGET_FGETC()); arg++) /* nothing */; \
	arg = PL_tokenbuf;			\
    } STMT_END

#define BGET_NV(arg) STMT_START {	\
	char *str;			\
	BGET_strconst(str);		\
	arg = Atof(str);		\
    } STMT_END

#define BGET_objindex(arg, type) STMT_START {	\
	BGET_U32(ix);				\
	arg = (type)bstate->bs_obj_list[ix];	\
    } STMT_END
#define BGET_svindex(arg) BGET_objindex(arg, svindex)
#define BGET_opindex(arg) BGET_objindex(arg, opindex)
#define BGET_pvindex(arg) STMT_START {			\
	BGET_objindex(arg, pvindex);			\
	arg = arg ? savepv(arg) : arg;			\
    } STMT_END

#define BSET_ldspecsv(sv, arg) sv = specialsv_list[arg]
#define BSET_ldspecsvx(sv, arg) STMT_START {	\
	BSET_ldspecsv(sv, arg);			\
	BSET_OBJ_STOREX(sv);			\
    } STMT_END

#define BSET_stpv(pv, arg) STMT_START {		\
	BSET_OBJ_STORE(pv, arg);		\
	SAVEFREEPV(pv);				\
    } STMT_END
				    
#define BSET_sv_refcnt_add(svrefcnt, arg)	svrefcnt += arg
#define BSET_gp_refcnt_add(gprefcnt, arg)	gprefcnt += arg
#define BSET_gp_share(sv, arg) STMT_START {	\
	gp_free((GV*)sv);			\
	GvGP(sv) = GvGP(arg);			\
    } STMT_END

#define BSET_gv_fetchpv(sv, arg)	sv = (SV*)gv_fetchpv(arg, TRUE, SVt_PV)
#define BSET_gv_fetchpvx(sv, arg) STMT_START {	\
	BSET_gv_fetchpv(sv, arg);		\
	BSET_OBJ_STOREX(sv);			\
    } STMT_END

#define BSET_gv_stashpv(sv, arg)	sv = (SV*)gv_stashpv(arg, TRUE)
#define BSET_gv_stashpvx(sv, arg) STMT_START {	\
	BSET_gv_stashpv(sv, arg);		\
	BSET_OBJ_STOREX(sv);			\
    } STMT_END

#define BSET_sv_magic(sv, arg)		sv_magic(sv, Nullsv, arg, 0, 0)
#define BSET_mg_name(mg, arg)	mg->mg_ptr = arg; mg->mg_len = bstate->bs_pv.xpv_cur
#define BSET_mg_namex(mg, arg)			\
	(mg->mg_ptr = (char*)SvREFCNT_inc((SV*)arg),	\
	 mg->mg_len = HEf_SVKEY)
#define BSET_xmg_stash(sv, arg) *(SV**)&(((XPVMG*)SvANY(sv))->xmg_stash) = (arg)
#define BSET_sv_upgrade(sv, arg)	(void)SvUPGRADE(sv, arg)
#define BSET_xrv(sv, arg) SvRV_set(sv, arg)
#define BSET_xpv(sv)	do {	\
	SvPV_set(sv, bstate->bs_pv.xpv_pv);	\
	SvCUR_set(sv, bstate->bs_pv.xpv_cur);	\
	SvLEN_set(sv, bstate->bs_pv.xpv_len);	\
    } while (0)
#define BSET_xpv_cur(sv, arg) SvCUR_set(sv, arg)
#define BSET_xpv_len(sv, arg) SvLEN_set(sv, arg)
#define BSET_xiv(sv, arg) SvIV_set(sv, arg)
#define BSET_xnv(sv, arg) SvNV_set(sv, arg)

#define BSET_av_extend(sv, arg)	av_extend((AV*)sv, arg)

#define BSET_av_push(sv, arg)	av_push((AV*)sv, arg)
#define BSET_av_pushx(sv, arg)	(AvARRAY(sv)[++AvFILLp(sv)] = arg)
#define BSET_hv_store(sv, arg)	\
	hv_store((HV*)sv, bstate->bs_pv.xpv_pv, bstate->bs_pv.xpv_cur, arg, 0)
#define BSET_pv_free(pv)	Safefree(pv.xpv_pv)


#ifdef USE_ITHREADS

/* copied after the code in newPMOP() */
#define BSET_pregcomp(o, arg) \
    STMT_START { \
        SV* repointer; \
	REGEXP* rx = arg ? \
	    CALLREGCOMP(aTHX_ arg, arg + bstate->bs_pv.xpv_cur, cPMOPx(o)) : \
	    Null(REGEXP*); \
        if(av_len((AV*) PL_regex_pad[0]) > -1) { \
            repointer = av_pop((AV*)PL_regex_pad[0]); \
            cPMOPx(o)->op_pmoffset = SvIV(repointer); \
            SvREPADTMP_off(repointer); \
            sv_setiv(repointer,PTR2IV(rx)); \
        } else { \
            repointer = newSViv(PTR2IV(rx)); \
            av_push(PL_regex_padav,SvREFCNT_inc(repointer)); \
            cPMOPx(o)->op_pmoffset = av_len(PL_regex_padav); \
            PL_regex_pad = AvARRAY(PL_regex_padav); \
        } \
    } STMT_END

#else
#define BSET_pregcomp(o, arg) \
    STMT_START { \
	PM_SETRE(((PMOP*)o), (arg ? \
	     CALLREGCOMP(aTHX_ arg, arg + bstate->bs_pv.xpv_cur, cPMOPx(o)): \
	     Null(REGEXP*))); \
    } STMT_END

#endif /* USE_THREADS */


#define BSET_newsv(sv, arg)				\
	    switch(arg) {				\
	    case SVt_PVAV:				\
		sv = (SV*)newAV();			\
		break;					\
	    case SVt_PVHV:				\
		sv = (SV*)newHV();			\
		break;					\
	    default:					\
		sv = NEWSV(0,0);			\
		SvUPGRADE(sv, (arg));			\
	    }
#define BSET_newsvx(sv, arg) STMT_START {		\
	    BSET_newsv(sv, arg &  SVTYPEMASK);		\
	    SvFLAGS(sv) = arg;				\
	    BSET_OBJ_STOREX(sv);			\
	} STMT_END

#define BSET_newop(o, arg)	NewOpSz(666, o, arg)
#define BSET_newopx(o, arg) STMT_START {	\
	register int sz = arg & 0x7f;		\
	register OP* newop;			\
	BSET_newop(newop, sz);			\
	/* newop->op_next = o; XXX */		\
	o = newop;				\
	arg >>=7;				\
	BSET_op_type(o, arg);			\
	BSET_OBJ_STOREX(o);			\
    } STMT_END

#define BSET_newopn(o, arg) STMT_START {	\
	OP *oldop = o;				\
	BSET_newop(o, arg);			\
	oldop->op_next = o;			\
    } STMT_END

#define BSET_ret(foo) STMT_START {		\
	Safefree(bstate->bs_obj_list);		\
	return 0;				\
    } STMT_END

#define BSET_op_pmstashpv(op, arg)	PmopSTASHPV_set(op, arg)

/* 
 * stolen from toke.c: better if that was a function.
 * in toke.c there are also #ifdefs for dosish systems and i/o layers
 */

#if defined(HAS_FCNTL) && defined(F_SETFD)
#define set_clonex(fp)				\
	STMT_START {				\
	    int fd = PerlIO_fileno(fp);		\
	    fcntl(fd,F_SETFD,fd >= 3);		\
	} STMT_END
#else
#define set_clonex(fp)
#endif

#define BSET_data(dummy,arg)						\
    STMT_START {							\
	GV *gv;								\
	char *pname = "main";						\
	if (arg == 'D')							\
	    pname = HvNAME(PL_curstash ? PL_curstash : PL_defstash);	\
	gv = gv_fetchpv(Perl_form(aTHX_ "%s::DATA", pname), TRUE, SVt_PVIO);\
	GvMULTI_on(gv);							\
	if (!GvIO(gv))							\
	    GvIOp(gv) = newIO();					\
	IoIFP(GvIOp(gv)) = PL_rsfp;					\
	set_clonex(PL_rsfp);						\
	/* Mark this internal pseudo-handle as clean */			\
	IoFLAGS(GvIOp(gv)) |= IOf_UNTAINT;				\
	if (PL_preprocess)						\
	    IoTYPE(GvIOp(gv)) = IoTYPE_PIPE;				\
	else if ((PerlIO*)PL_rsfp == PerlIO_stdin())			\
	    IoTYPE(GvIOp(gv)) = IoTYPE_STD;				\
	else								\
	    IoTYPE(GvIOp(gv)) = IoTYPE_RDONLY;				\
	Safefree(bstate->bs_obj_list);					\
	return 1;							\
    } STMT_END

/* stolen from op.c */
#define BSET_load_glob(foo, gv)						\
    STMT_START {							\
        GV *glob_gv;							\
        ENTER;								\
        Perl_load_module(aTHX_ PERL_LOADMOD_NOIMPORT,			\
                newSVpvn("File::Glob", 10), Nullsv, Nullsv, Nullsv);	\
        glob_gv = gv_fetchpv("File::Glob::csh_glob", FALSE, SVt_PVCV);	\
        GvCV(gv) = GvCV(glob_gv);					\
        SvREFCNT_inc((SV*)GvCV(gv));					\
        GvIMPORTED_CV_on(gv);						\
        LEAVE;								\
    } STMT_END

/*
 * Kludge special-case workaround for OP_MAPSTART
 * which needs the ppaddr for OP_GREPSTART. Blech.
 */
#define BSET_op_type(o, arg) STMT_START {	\
	o->op_type = arg;			\
	if (arg == OP_MAPSTART)			\
	    arg = OP_GREPSTART;			\
	o->op_ppaddr = PL_ppaddr[arg];		\
    } STMT_END
#define BSET_op_ppaddr(o, arg) Perl_croak(aTHX_ "op_ppaddr not yet implemented")
#define BSET_curpad(pad, arg) STMT_START {	\
	PL_comppad = (AV *)arg;			\
	pad = AvARRAY(arg);			\
    } STMT_END

#ifdef USE_ITHREADS
#define BSET_cop_file(cop, arg)		CopFILE_set(cop,arg)
#define BSET_cop_stashpv(cop, arg)	CopSTASHPV_set(cop,arg)
#else
/* this works now that Sarathy's changed the CopFILE_set macro to do the SvREFCNT_inc()
	-- BKS 6-2-2000 */
/* that really meant the actual CopFILEGV_set */
#define BSET_cop_filegv(cop, arg)	CopFILEGV_set(cop,arg)
#define BSET_cop_stash(cop,arg)		CopSTASH_set(cop,(HV*)arg)
#endif

/* this is simply stolen from the code in newATTRSUB() */
#define BSET_push_begin(ary,cv)				\
	STMT_START {					\
            I32 oldscope = PL_scopestack_ix;		\
            ENTER;					\
            SAVECOPFILE(&PL_compiling);			\
            SAVECOPLINE(&PL_compiling);			\
            if (!PL_beginav)				\
                PL_beginav = newAV();			\
            av_push(PL_beginav, (SV*)cv);		\
	    GvCV(CvGV(cv)) = 0;               /* cv has been hijacked */\
            call_list(oldscope, PL_beginav);		\
            PL_curcop = &PL_compiling;			\
            PL_compiling.op_private = (U8)(PL_hints & HINT_PRIVATE_MASK);\
            LEAVE;					\
	} STMT_END
#define BSET_push_init(ary,cv)				\
	STMT_START {					\
	    av_unshift((PL_initav ? PL_initav : 	\
		(PL_initav = newAV(), PL_initav)), 1); 	\
	    av_store(PL_initav, 0, cv);			\
	} STMT_END
#define BSET_push_end(ary,cv)				\
	STMT_START {					\
	    av_unshift((PL_endav ? PL_endav : 		\
	    (PL_endav = newAV(), PL_endav)), 1);	\
	    av_store(PL_endav, 0, cv);			\
	} STMT_END
#define BSET_OBJ_STORE(obj, ix)			\
	((I32)ix > bstate->bs_obj_list_fill ?	\
	 bset_obj_store(aTHX_ bstate, obj, (I32)ix) : \
	 (bstate->bs_obj_list[ix] = obj),	\
	 bstate->bs_ix = ix+1)
#define BSET_OBJ_STOREX(obj)			\
	(bstate->bs_ix > bstate->bs_obj_list_fill ?	\
	 bset_obj_store(aTHX_ bstate, obj, bstate->bs_ix) : \
	 (bstate->bs_obj_list[bstate->bs_ix] = obj),	\
	 bstate->bs_ix++)

#define BSET_signal(cv, name)						\
	mg_set(*hv_store(GvHV(gv_fetchpv("SIG", TRUE, SVt_PVHV)),	\
		name, strlen(name), cv, 0))

#define BSET_xhv_name(hv, name)	hv_name_set((HV*)hv, name, strlen(name), 0)

/* NOTE: the bytecode header only sanity-checks the bytecode. If a script cares about
 * what version of Perl it's being called under, it should do a 'use 5.006_001' or
 * equivalent. However, since the header includes checks requiring an exact match in
 * ByteLoader versions (we can't guarantee forward compatibility), you don't 
 * need to specify one:
 * 	use ByteLoader;
 * is all you need.
 *	-- BKS, June 2000
*/

#define HEADER_FAIL(f)	\
	Perl_croak(aTHX_ "Invalid bytecode for this architecture: " f)
#define HEADER_FAIL1(f, arg1)	\
	Perl_croak(aTHX_ "Invalid bytecode for this architecture: " f, arg1)
#define HEADER_FAIL2(f, arg1, arg2)	\
	Perl_croak(aTHX_ "Invalid bytecode for this architecture: " f, arg1, arg2)

#define BYTECODE_HEADER_CHECK					\
	STMT_START {						\
	    U32 sz = 0;						\
	    strconst str;					\
								\
	    BGET_U32(sz); /* Magic: 'PLBC' */			\
	    if (sz != 0x43424c50) {				\
		HEADER_FAIL1("bad magic (want 0x43424c50, got %#x)", (int)sz);		\
	    }							\
	    BGET_strconst(str);	/* archname */			\
	    if (strNE(str, ARCHNAME)) {				\
		HEADER_FAIL2("wrong architecture (want %s, you have %s)",str,ARCHNAME);	\
	    }							\
	    BGET_strconst(str); /* ByteLoader version */	\
	    if (strNE(str, VERSION)) {				\
		HEADER_FAIL2("mismatched ByteLoader versions (want %s, you have %s)",	\
			str, VERSION);				\
	    }							\
	    BGET_U32(sz); /* ivsize */				\
	    if (sz != IVSIZE) {					\
		HEADER_FAIL("different IVSIZE");		\
	    }							\
	    BGET_U32(sz); /* ptrsize */				\
	    if (sz != PTRSIZE) {				\
		HEADER_FAIL("different PTRSIZE");		\
	    }							\
	} STMT_END
