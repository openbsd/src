#define ST(off) stack_base[ax + (off)]

#ifdef CAN_PROTOTYPE
#define XS(name) void name(CV* cv)
#else
#define XS(name) void name(cv) CV* cv;
#endif

#define dXSARGS				\
	dSP; dMARK;			\
	I32 ax = mark - stack_base + 1;	\
	I32 items = sp - mark

#define XSANY CvXSUBANY(cv)

#define dXSI32 I32 ix = XSANY.any_i32

#define XSRETURN(off) stack_sp = stack_base + ax + ((off) - 1); return

/* Simple macros to put new mortal values onto the stack.   */
/* Typically used to return values from XS functions.       */
#define XST_mIV(i,v)  (ST(i) = sv_2mortal(newSViv(v))  )
#define XST_mNV(i,v)  (ST(i) = sv_2mortal(newSVnv(v))  )
#define XST_mPV(i,v)  (ST(i) = sv_2mortal(newSVpv(v,0)))
#define XST_mNO(i)    (ST(i) = &sv_no   )
#define XST_mYES(i)   (ST(i) = &sv_yes  )
#define XST_mUNDEF(i) (ST(i) = &sv_undef)
 
#define XSRETURN_IV(v) STMT_START { XST_mIV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_NV(v) STMT_START { XST_mNV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_PV(v) STMT_START { XST_mPV(0,v);  XSRETURN(1); } STMT_END
#define XSRETURN_NO    STMT_START { XST_mNO(0);    XSRETURN(1); } STMT_END
#define XSRETURN_YES   STMT_START { XST_mYES(0);   XSRETURN(1); } STMT_END
#define XSRETURN_UNDEF STMT_START { XST_mUNDEF(0); XSRETURN(1); } STMT_END
#define XSRETURN_EMPTY STMT_START {                XSRETURN(0); } STMT_END

#define newXSproto(a,b,c,d)	sv_setpv((SV*)newXS(a,b,c), d)

#ifdef XS_VERSION
# define XS_VERSION_BOOTCHECK \
    STMT_START {							\
	char *vn = "", *module = SvPV(ST(0),na);			\
	if (items >= 2)	 /* version supplied as bootstrap arg */	\
	    Sv = ST(1);							\
	else {								\
	    /* XXX GV_ADDWARN */					\
	    Sv = perl_get_sv(form("%s::%s", module,			\
				  vn = "XS_VERSION"), FALSE);		\
	    if (!Sv || !SvOK(Sv))					\
		Sv = perl_get_sv(form("%s::%s", module,			\
				      vn = "VERSION"), FALSE);		\
	}								\
	if (Sv && (!SvOK(Sv) || strNE(XS_VERSION, SvPV(Sv, na))))	\
	    croak("%s object version %s does not match $%s::%s %_",	\
		  module, XS_VERSION, module, vn, Sv);			\
    } STMT_END
#else
# define XS_VERSION_BOOTCHECK
#endif
