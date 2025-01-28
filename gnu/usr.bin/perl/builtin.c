/*    builtin.c
 *
 *    Copyright (C) 2021 by Paul Evans and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/* This file contains the code that implements functions in perl's "builtin::"
 * namespace
 */

#include "EXTERN.h"
#define PERL_IN_BUILTIN_C
#include "perl.h"

#include "XSUB.h"

/* copied from op.c */
#define SHORTVER(maj,min) (((maj) << 8) | (min))

struct BuiltinFuncDescriptor {
    const char *name;
    U16 since_ver;
    XSUBADDR_t xsub;
    OP *(*checker)(pTHX_ OP *, GV *, SV *);
    IV ckval;
    bool is_experimental;
};

#define warn_experimental_builtin(name) S_warn_experimental_builtin(aTHX_ name)
static void S_warn_experimental_builtin(pTHX_ const char *name)
{
    /* diag_listed_as: Built-in function '%s' is experimental */
    Perl_ck_warner_d(aTHX_ packWARN(WARN_EXPERIMENTAL__BUILTIN),
                     "Built-in function 'builtin::%s' is experimental", name);
}

/* These three utilities might want to live elsewhere to be reused from other
 * code sometime
 */
void
Perl_prepare_export_lexical(pTHX)
{
    assert(PL_compcv);

    /* We need to have PL_comppad / PL_curpad set correctly for lexical importing */
    ENTER;
    SAVESPTR(PL_comppad_name); PL_comppad_name = PadlistNAMES(CvPADLIST(PL_compcv));
    SAVECOMPPAD();
    PL_comppad      = PadlistARRAY(CvPADLIST(PL_compcv))[1];
    PL_curpad       = PadARRAY(PL_comppad);
}

#define export_lexical(name, sv)  S_export_lexical(aTHX_ name, sv)
static void S_export_lexical(pTHX_ SV *name, SV *sv)
{
    PADOFFSET off = pad_add_name_sv(name, padadd_STATE, 0, 0);
    SvREFCNT_dec(PL_curpad[off]);
    PL_curpad[off] = SvREFCNT_inc(sv);
}

void
Perl_finish_export_lexical(pTHX)
{
    intro_my();

    LEAVE;
}


XS(XS_builtin_true);
XS(XS_builtin_true)
{
    dXSARGS;
    if(items)
        croak_xs_usage(cv, "");
    EXTEND(SP, 1);
    XSRETURN_YES;
}

XS(XS_builtin_false);
XS(XS_builtin_false)
{
    dXSARGS;
    if(items)
        croak_xs_usage(cv, "");
    EXTEND(SP, 1);
    XSRETURN_NO;
}

XS(XS_builtin_inf);
XS(XS_builtin_inf)
{
    dXSARGS;
    if(items)
        croak_xs_usage(cv, "");
    EXTEND(SP, 1);
    XSRETURN_NV(NV_INF);
}

XS(XS_builtin_nan);
XS(XS_builtin_nan)
{
    dXSARGS;
    if(items)
        croak_xs_usage(cv, "");
    EXTEND(SP, 1);
    XSRETURN_NV(NV_NAN);
}

enum {
    BUILTIN_CONST_FALSE,
    BUILTIN_CONST_TRUE,
    BUILTIN_CONST_INF,
    BUILTIN_CONST_NAN,
};

static OP *ck_builtin_const(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    const struct BuiltinFuncDescriptor *builtin = NUM2PTR(const struct BuiltinFuncDescriptor *, SvUV(ckobj));

    if(builtin->is_experimental)
        warn_experimental_builtin(builtin->name);

    SV *prototype = newSVpvs("");
    SAVEFREESV(prototype);

    assert(entersubop->op_type == OP_ENTERSUB);

    entersubop = ck_entersub_args_proto(entersubop, namegv, prototype);

    SV *constval;
    switch(builtin->ckval) {
        case BUILTIN_CONST_FALSE: constval = &PL_sv_no; break;
        case BUILTIN_CONST_TRUE:  constval = &PL_sv_yes; break;
        case BUILTIN_CONST_INF:   constval = newSVnv(NV_INF); break;
        case BUILTIN_CONST_NAN:   constval = newSVnv(NV_NAN); break;
        default:
            DIE(aTHX_ "panic: unrecognised builtin_const value %" IVdf,
                      builtin->ckval);
            break;
    }

    op_free(entersubop);

    return newSVOP(OP_CONST, 0, constval);
}

XS(XS_builtin_func1_scalar);
XS(XS_builtin_func1_scalar)
{
    dXSARGS;
    dXSI32;

    if(items != 1)
        croak_xs_usage(cv, "arg");

    switch(ix) {
        case OP_IS_BOOL:
            warn_experimental_builtin(PL_op_name[ix]);
            Perl_pp_is_bool(aTHX);
            break;

        case OP_IS_WEAK:
            Perl_pp_is_weak(aTHX);
            break;

        case OP_BLESSED:
            Perl_pp_blessed(aTHX);
            break;

        case OP_REFADDR:
            Perl_pp_refaddr(aTHX);
            break;

        case OP_REFTYPE:
            Perl_pp_reftype(aTHX);
            break;

        case OP_CEIL:
            Perl_pp_ceil(aTHX);
            break;

        case OP_FLOOR:
            Perl_pp_floor(aTHX);
            break;

        case OP_IS_TAINTED:
            Perl_pp_is_tainted(aTHX);
            break;

        case OP_STRINGIFY:
            {
                /* we could only call pp_stringify if we're sure there is a TARG
                   and if the XSUB is called from call_sv() or goto it may not
                   have one.
                */
                dXSTARG;
                sv_copypv(TARG, *PL_stack_sp);
                SvSETMAGIC(TARG);
                rpp_replace_1_1_NN(TARG);
            }
            break;

        default:
            Perl_die(aTHX_ "panic: unhandled opcode %" IVdf
                           " for xs_builtin_func1_scalar()", (IV) ix);
    }

    XSRETURN(1);
}

XS(XS_builtin_trim);
XS(XS_builtin_trim)
{
    dXSARGS;

    if (items != 1) {
        croak_xs_usage(cv, "arg");
    }

    dXSTARG;
    SV *source = TOPs;
    STRLEN len;
    const U8 *start;
    SV *dest;

    SvGETMAGIC(source);

    if (SvOK(source))
        start = (const U8*)SvPV_nomg_const(source, len);
    else {
        if (ckWARN(WARN_UNINITIALIZED))
            report_uninit(source);
        start = (const U8*)"";
        len = 0;
    }

    if (DO_UTF8(source)) {
        const U8 *end = start + len;

        /* Find the first non-space */
        while(len) {
            STRLEN thislen;
            if (!isSPACE_utf8_safe(start, end))
                break;
            start += (thislen = UTF8SKIP(start));
            len -= thislen;
        }

        /* Find the final non-space */
        STRLEN thislen;
        const U8 *cur_end = end;
        while ((thislen = is_SPACE_utf8_safe_backwards(cur_end, start))) {
            cur_end -= thislen;
        }
        len -= (end - cur_end);
    }
    else if (len) {
        while(len) {
            if (!isSPACE_L1(*start))
                break;
            start++;
            len--;
        }

        while(len) {
            if (!isSPACE_L1(start[len-1]))
                break;
            len--;
        }
    }

    dest = TARG;

    if (SvPOK(dest) && (dest == source)) {
        sv_chop(dest, (const char *)start);
        SvCUR_set(dest, len);
    }
    else {
        SvUPGRADE(dest, SVt_PV);
        SvGROW(dest, len + 1);

        Copy(start, SvPVX(dest), len, U8);
        SvPVX(dest)[len] = '\0';
        SvPOK_on(dest);
        SvCUR_set(dest, len);

        if (DO_UTF8(source))
            SvUTF8_on(dest);
        else
            SvUTF8_off(dest);

        if (SvTAINTED(source))
            SvTAINT(dest);
    }

    SvSETMAGIC(dest);

    SETs(dest);

    XSRETURN(1);
}

XS(XS_builtin_export_lexically);
XS(XS_builtin_export_lexically)
{
    dXSARGS;

    warn_experimental_builtin("export_lexically");

    if(!PL_compcv)
        Perl_croak(aTHX_
                "export_lexically can only be called at compile time");

    if(items % 2)
        Perl_croak(aTHX_ "Odd number of elements in export_lexically");

    for(int i = 0; i < items; i += 2) {
        SV *name = ST(i);
        SV *ref  = ST(i+1);

        if(!SvROK(ref))
            /* diag_listed_as: Expected %s reference in export_lexically */
            Perl_croak(aTHX_ "Expected a reference in export_lexically");

        char sigil = SvPVX(name)[0];
        SV *rv = SvRV(ref);

        const char *bad = NULL;
        switch(sigil) {
            default:
                /* overwrites the pointer on the stack; but this is fine, the
                 * caller's value isn't modified */
                ST(i) = name = sv_2mortal(Perl_newSVpvf(aTHX_ "&%" SVf, SVfARG(name)));

                /* FALLTHROUGH */
            case '&':
                if(SvTYPE(rv) != SVt_PVCV)
                    bad = "a CODE";
                break;

            case '$':
                /* Permit any of SVt_NULL to SVt_PVMG. Technically this also
                 * includes SVt_INVLIST but it isn't thought possible for pureperl
                 * code to ever manage to see one of those. */
                if(SvTYPE(rv) > SVt_PVMG)
                    bad = "a SCALAR";
                break;

            case '@':
                if(SvTYPE(rv) != SVt_PVAV)
                    bad = "an ARRAY";
                break;

            case '%':
                if(SvTYPE(rv) != SVt_PVHV)
                    bad = "a HASH";
                break;
        }

        if(bad)
            Perl_croak(aTHX_ "Expected %s reference in export_lexically", bad);
    }

    prepare_export_lexical();

    for(int i = 0; i < items; i += 2) {
        SV *name = ST(i);
        SV *ref  = ST(i+1);

        export_lexical(name, SvRV(ref));
    }

    finish_export_lexical();
}

XS(XS_builtin_func1_void);
XS(XS_builtin_func1_void)
{
    dXSARGS;
    dXSI32;

    if(items != 1)
        croak_xs_usage(cv, "arg");

    switch(ix) {
        case OP_WEAKEN:
            Perl_pp_weaken(aTHX);
            break;

        case OP_UNWEAKEN:
            Perl_pp_unweaken(aTHX);
            break;

        default:
            Perl_die(aTHX_ "panic: unhandled opcode %" IVdf
                           " for xs_builtin_func1_void()", (IV) ix);
    }

    XSRETURN(0);
}

XS(XS_builtin_created_as_string)
{
    dXSARGS;

    if(items != 1)
        croak_xs_usage(cv, "arg");

    SV *arg = ST(0);
    SvGETMAGIC(arg);

    /* SV was created as string if it has POK and isn't bool */
    ST(0) = boolSV(SvPOK(arg) && !SvIsBOOL(arg));
    XSRETURN(1);
}

XS(XS_builtin_created_as_number)
{
    dXSARGS;

    if(items != 1)
        croak_xs_usage(cv, "arg");

    SV *arg = ST(0);
    SvGETMAGIC(arg);

    /* SV was created as number if it has NOK or IOK but not POK and is not bool */
    ST(0) = boolSV(SvNIOK(arg) && !SvPOK(arg) && !SvIsBOOL(arg));
    XSRETURN(1);
}

static OP *ck_builtin_func1(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    const struct BuiltinFuncDescriptor *builtin = NUM2PTR(const struct BuiltinFuncDescriptor *, SvUV(ckobj));

    if(builtin->is_experimental)
        warn_experimental_builtin(builtin->name);

    SV *prototype = newSVpvs("$");
    SAVEFREESV(prototype);

    assert(entersubop->op_type == OP_ENTERSUB);

    entersubop = ck_entersub_args_proto(entersubop, namegv, prototype);

    OPCODE opcode = builtin->ckval;
    if(!opcode)
        return entersubop;

    OP *parent = entersubop, *pushop, *argop;

    pushop = cUNOPx(entersubop)->op_first;
    if (!OpHAS_SIBLING(pushop)) {
        pushop = cUNOPx(pushop)->op_first;
    }

    argop = OpSIBLING(pushop);

    if (!argop || !OpHAS_SIBLING(argop) || OpHAS_SIBLING(OpSIBLING(argop)))
        return entersubop;

    (void)op_sibling_splice(parent, pushop, 1, NULL);

    U8 wantflags = entersubop->op_flags & OPf_WANT;

    op_free(entersubop);

    if(opcode == OP_STRINGIFY)
        /* Even though pp_stringify only looks at TOPs and conceptually works
         * on a single argument, it happens to be a LISTOP. I've no idea why
         */
        return newLISTOPn(opcode, wantflags,
            argop,
            NULL);
    else {
        OP * const op = newUNOP(opcode, wantflags, argop);

        /* since these pp funcs can be called from XS, and XS may be called
           without a normal ENTERSUB, we need to indicate to them that a targ
           has been allocated.
        */
        if (op->op_targ)
            op->op_private |= OPpENTERSUB_HASTARG;

        return op;
    }
}

XS(XS_builtin_indexed)
{
    dXSARGS;

    switch(GIMME_V) {
        case G_VOID:
            Perl_ck_warner(aTHX_ packWARN(WARN_VOID),
                "Useless use of %s in void context", "builtin::indexed");
            XSRETURN(0);

        case G_SCALAR:
            Perl_ck_warner(aTHX_ packWARN(WARN_SCALAR),
                "Useless use of %s in scalar context", "builtin::indexed");
            ST(0) = sv_2mortal(newSViv(items * 2));
            XSRETURN(1);

        case G_LIST:
            break;
    }

    SSize_t retcount = items * 2;
    EXTEND(SP, retcount);

    /* Copy from [items-1] down to [0] so we don't have to make
     * temporary copies */
    for(SSize_t index = items - 1; index >= 0; index--) {
        /* Copy, not alias */
        ST(index * 2 + 1) = sv_mortalcopy(ST(index));
        ST(index * 2)     = sv_2mortal(newSViv(index));
    }

    XSRETURN(retcount);
}

XS(XS_builtin_load_module);
XS(XS_builtin_load_module)
{
    dXSARGS;
    if (items != 1)
        croak_xs_usage(cv, "arg");
    SV *module_name = newSVsv(ST(0));
    if (!SvPOK(module_name)) {
        SvREFCNT_dec(module_name);
        croak_xs_usage(cv, "defined string");
    }
    load_module(PERL_LOADMOD_NOIMPORT, module_name, NULL, NULL);
    /* The loaded module's name is left intentionally on the stack for the
     * caller's benefit, and becomes load_module's return value. */
    XSRETURN(1);
}

/* These pp_ funcs all need to use dXSTARG */

PP(pp_refaddr)
{
    dXSTARG;
    SV *arg = *PL_stack_sp;

    SvGETMAGIC(arg);

    if(SvROK(arg))
        sv_setuv_mg(TARG, PTR2UV(SvRV(arg)));
    else
        sv_setsv(TARG, &PL_sv_undef);

    rpp_replace_1_1_NN(TARG);
    return NORMAL;
}

PP(pp_reftype)
{
    dXSTARG;
    SV *arg = *PL_stack_sp;

    SvGETMAGIC(arg);

    if(SvROK(arg))
        sv_setpv_mg(TARG, sv_reftype(SvRV(arg), FALSE));
    else
        sv_setsv(TARG, &PL_sv_undef);

    rpp_replace_1_1_NN(TARG);
    return NORMAL;
}

PP(pp_ceil)
{
    dXSTARG;
    TARGn(Perl_ceil(SvNVx(*PL_stack_sp)), 1);
    rpp_replace_1_1_NN(TARG);
    return NORMAL;
}

PP(pp_floor)
{
    dXSTARG;
    TARGn(Perl_floor(SvNVx(*PL_stack_sp)), 1);
    rpp_replace_1_1_NN(TARG);
    return NORMAL;
}

static OP *ck_builtin_funcN(pTHX_ OP *entersubop, GV *namegv, SV *ckobj)
{
    const struct BuiltinFuncDescriptor *builtin = NUM2PTR(const struct BuiltinFuncDescriptor *, SvUV(ckobj));

    if(builtin->is_experimental)
        warn_experimental_builtin(builtin->name);

    SV *prototype = newSVpvs("@");
    SAVEFREESV(prototype);

    assert(entersubop->op_type == OP_ENTERSUB);

    entersubop = ck_entersub_args_proto(entersubop, namegv, prototype);
    return entersubop;
}

static const char builtin_not_recognised[] = "'%" SVf "' is not recognised as a builtin function";

#define NO_BUNDLE SHORTVER(255,255)

static const struct BuiltinFuncDescriptor builtins[] = {
    /* constants */
    { "true",  SHORTVER(5,39), &XS_builtin_true,   &ck_builtin_const, BUILTIN_CONST_TRUE,  false },
    { "false", SHORTVER(5,39), &XS_builtin_false,  &ck_builtin_const, BUILTIN_CONST_FALSE, false },
    { "inf",        NO_BUNDLE, &XS_builtin_inf,    &ck_builtin_const, BUILTIN_CONST_INF,   true },
    { "nan",        NO_BUNDLE, &XS_builtin_nan,    &ck_builtin_const, BUILTIN_CONST_NAN,   true },

    /* unary functions */
    { "is_bool",         NO_BUNDLE, &XS_builtin_func1_scalar, &ck_builtin_func1, OP_IS_BOOL,    true  },
    { "weaken",     SHORTVER(5,39), &XS_builtin_func1_void,   &ck_builtin_func1, OP_WEAKEN,     false },
    { "unweaken",   SHORTVER(5,39), &XS_builtin_func1_void,   &ck_builtin_func1, OP_UNWEAKEN,   false },
    { "is_weak",    SHORTVER(5,39), &XS_builtin_func1_scalar, &ck_builtin_func1, OP_IS_WEAK,    false },
    { "blessed",    SHORTVER(5,39), &XS_builtin_func1_scalar, &ck_builtin_func1, OP_BLESSED,    false },
    { "refaddr",    SHORTVER(5,39), &XS_builtin_func1_scalar, &ck_builtin_func1, OP_REFADDR,    false },
    { "reftype",    SHORTVER(5,39), &XS_builtin_func1_scalar, &ck_builtin_func1, OP_REFTYPE,    false },
    { "ceil",       SHORTVER(5,39), &XS_builtin_func1_scalar, &ck_builtin_func1, OP_CEIL,       false },
    { "floor",      SHORTVER(5,39), &XS_builtin_func1_scalar, &ck_builtin_func1, OP_FLOOR,      false },
    { "is_tainted", SHORTVER(5,39), &XS_builtin_func1_scalar, &ck_builtin_func1, OP_IS_TAINTED, false },
    { "trim",       SHORTVER(5,39), &XS_builtin_trim,         &ck_builtin_func1, 0,             false },
    { "stringify",       NO_BUNDLE, &XS_builtin_func1_scalar, &ck_builtin_func1, OP_STRINGIFY,  true },

    { "created_as_string", NO_BUNDLE, &XS_builtin_created_as_string, &ck_builtin_func1, 0, true },
    { "created_as_number", NO_BUNDLE, &XS_builtin_created_as_number, &ck_builtin_func1, 0, true },

    { "load_module", NO_BUNDLE, &XS_builtin_load_module, &ck_builtin_func1, 0, true },

    /* list functions */
    { "indexed",          SHORTVER(5,39), &XS_builtin_indexed,          &ck_builtin_funcN, 0, false },
    { "export_lexically",      NO_BUNDLE, &XS_builtin_export_lexically, NULL,              0, true },

    { NULL, 0, NULL, NULL, 0, false }
};

static bool S_parse_version(const char *vstr, const char *vend, UV *vmajor, UV *vminor)
{
    /* Parse a string like "5.35" to yield 5 and 35. Ignores an optional
     * trailing third component e.g. "5.35.7". Returns false on parse errors.
     */

    const char *end = vend;
    if (!grok_atoUV(vstr, vmajor, &end))
        return FALSE;

    vstr = end;
    if (*vstr++ != '.')
        return FALSE;

    end = vend;
    if (!grok_atoUV(vstr, vminor, &end))
        return FALSE;

    if(*vminor > 255)
        return FALSE;

    vstr = end;

    if(vstr[0] == '.') {
        vstr++;

        UV _dummy;
        if(!grok_atoUV(vstr, &_dummy, &end))
            return FALSE;
        if(_dummy > 255)
            return FALSE;

        vstr = end;
    }

    if(vstr != vend)
        return FALSE;

    return TRUE;
}

#define import_sym(sym)  S_import_sym(aTHX_ sym)
static void S_import_sym(pTHX_ SV *sym)
{
    SV *ampname = sv_2mortal(Perl_newSVpvf(aTHX_ "&%" SVf, SVfARG(sym)));
    SV *fqname = sv_2mortal(Perl_newSVpvf(aTHX_ "builtin::%" SVf, SVfARG(sym)));

    CV *cv = get_cv(SvPV_nolen(fqname), SvUTF8(fqname) ? SVf_UTF8 : 0);
    if(!cv)
        Perl_croak(aTHX_ builtin_not_recognised, sym);

    export_lexical(ampname, (SV *)cv);
}

#define cv_is_builtin(cv)  S_cv_is_builtin(aTHX_ cv)
static bool S_cv_is_builtin(pTHX_ CV *cv)
{
    char *file = CvFILE(cv);
    return file && strEQ(file, __FILE__);
}

void
Perl_import_builtin_bundle(pTHX_ U16 ver)
{
    SV *ampname = sv_newmortal();

    for(int i = 0; builtins[i].name; i++) {
        sv_setpvf(ampname, "&%s", builtins[i].name);

        bool want = (builtins[i].since_ver <= ver);

        bool got = false;
        PADOFFSET off = pad_findmy_sv(ampname, 0);
        CV *cv;
        if(off != NOT_IN_PAD &&
                SvTYPE((cv = (CV *)PL_curpad[off])) == SVt_PVCV &&
                cv_is_builtin(cv))
            got = true;

        if(!got && want) {
            import_sym(newSVpvn_flags(builtins[i].name, strlen(builtins[i].name), SVs_TEMP));
        }
    }
}

XS(XS_builtin_import);
XS(XS_builtin_import)
{
    dXSARGS;

    if(!PL_compcv)
        Perl_croak(aTHX_
                "builtin::import can only be called at compile time");

    prepare_export_lexical();

    for(int i = 1; i < items; i++) {
        SV *sym = ST(i);
        STRLEN symlen;
        const char *sympv = SvPV(sym, symlen);
        if(strEQ(sympv, "import"))
            Perl_croak(aTHX_ builtin_not_recognised, sym);

        if(sympv[0] == ':') {
            UV vmajor, vminor;
            if(!S_parse_version(sympv + 1, sympv + symlen, &vmajor, &vminor))
                Perl_croak(aTHX_ "Invalid version bundle %" SVf_QUOTEDPREFIX, sym);

            U16 want_ver = SHORTVER(vmajor, vminor);

            if(want_ver < SHORTVER(5,39) ||
                    /* round up devel version to next major release; e.g. 5.39 => 5.40 */
                    want_ver > SHORTVER(PERL_REVISION, PERL_VERSION + (PERL_VERSION % 2)))
                Perl_croak(aTHX_ "Builtin version bundle \"%s\" is not supported by Perl " PERL_VERSION_STRING,
                        sympv);

            import_builtin_bundle(want_ver);

            continue;
        }

        import_sym(sym);
    }

    finish_export_lexical();
}

void
Perl_boot_core_builtin(pTHX)
{
    I32 i;
    for(i = 0; builtins[i].name; i++) {
        const struct BuiltinFuncDescriptor *builtin = &builtins[i];

        const char *proto = NULL;
        if(builtin->checker == &ck_builtin_const)
            proto = "";
        else if(builtin->checker == &ck_builtin_func1)
            proto = "$";
        else if(builtin->checker == &ck_builtin_funcN)
            proto = "@";

        SV *name = newSVpvs_flags("builtin::", SVs_TEMP);
        sv_catpv(name, builtin->name);
        CV *cv = newXS_flags(SvPV_nolen(name), builtin->xsub, __FILE__, proto, 0);
        XSANY.any_i32 = builtin->ckval;

        if (   builtin->xsub == &XS_builtin_func1_void
            || builtin->xsub == &XS_builtin_func1_scalar)
        {
            /* these XS functions just call out to the relevant pp()
             * functions, so they must operate with a reference-counted
             * stack if the pp() do too.
             */
                CvXS_RCSTACK_on(cv);
        }

        if(builtin->checker) {
            cv_set_call_checker_flags(cv, builtin->checker, newSVuv(PTR2UV(builtin)), 0);
        }
    }

    newXS_flags("builtin::import", &XS_builtin_import, __FILE__, NULL, 0);
}

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
