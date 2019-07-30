/*    dump.c
 *
 *    Copyright (C) 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
 *    2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *  'You have talked long in your sleep, Frodo,' said Gandalf gently, 'and
 *   it has not been hard for me to read your mind and memory.'
 *
 *     [p.220 of _The Lord of the Rings_, II/i: "Many Meetings"]
 */

/* This file contains utility routines to dump the contents of SV and OP
 * structures, as used by command-line options like -Dt and -Dx, and
 * by Devel::Peek.
 *
 * It also holds the debugging version of the  runops function.

=head1 Display and Dump functions
 */

#include "EXTERN.h"
#define PERL_IN_DUMP_C
#include "perl.h"
#include "regcomp.h"

static const char* const svtypenames[SVt_LAST] = {
    "NULL",
    "IV",
    "NV",
    "PV",
    "INVLIST",
    "PVIV",
    "PVNV",
    "PVMG",
    "REGEXP",
    "PVGV",
    "PVLV",
    "PVAV",
    "PVHV",
    "PVCV",
    "PVFM",
    "PVIO"
};


static const char* const svshorttypenames[SVt_LAST] = {
    "UNDEF",
    "IV",
    "NV",
    "PV",
    "INVLST",
    "PVIV",
    "PVNV",
    "PVMG",
    "REGEXP",
    "GV",
    "PVLV",
    "AV",
    "HV",
    "CV",
    "FM",
    "IO"
};

struct flag_to_name {
    U32 flag;
    const char *name;
};

static void
S_append_flags(pTHX_ SV *sv, U32 flags, const struct flag_to_name *start,
	       const struct flag_to_name *const end)
{
    do {
	if (flags & start->flag)
	    sv_catpv(sv, start->name);
    } while (++start < end);
}

#define append_flags(sv, f, flags) \
    S_append_flags(aTHX_ (sv), (f), (flags), C_ARRAY_END(flags))

#define generic_pv_escape(sv,s,len,utf8) pv_escape( (sv), (s), (len), \
                              (len) * (4+UTF8_MAXBYTES) + 1, NULL, \
                              PERL_PV_ESCAPE_NONASCII | PERL_PV_ESCAPE_DWIM \
                              | ((utf8) ? PERL_PV_ESCAPE_UNI : 0) )

/*
=for apidoc pv_escape

Escapes at most the first C<count> chars of C<pv> and puts the results into
C<dsv> such that the size of the escaped string will not exceed C<max> chars
and will not contain any incomplete escape sequences.  The number of bytes
escaped will be returned in the C<STRLEN *escaped> parameter if it is not null.
When the C<dsv> parameter is null no escaping actually occurs, but the number
of bytes that would be escaped were it not null will be calculated.

If flags contains C<PERL_PV_ESCAPE_QUOTE> then any double quotes in the string
will also be escaped.

Normally the SV will be cleared before the escaped string is prepared,
but when C<PERL_PV_ESCAPE_NOCLEAR> is set this will not occur.

If C<PERL_PV_ESCAPE_UNI> is set then the input string is treated as UTF-8
if C<PERL_PV_ESCAPE_UNI_DETECT> is set then the input string is scanned
using C<is_utf8_string()> to determine if it is UTF-8.

If C<PERL_PV_ESCAPE_ALL> is set then all input chars will be output
using C<\x01F1> style escapes, otherwise if C<PERL_PV_ESCAPE_NONASCII> is set, only
non-ASCII chars will be escaped using this style; otherwise, only chars above
255 will be so escaped; other non printable chars will use octal or
common escaped patterns like C<\n>.
Otherwise, if C<PERL_PV_ESCAPE_NOBACKSLASH>
then all chars below 255 will be treated as printable and
will be output as literals.

If C<PERL_PV_ESCAPE_FIRSTCHAR> is set then only the first char of the
string will be escaped, regardless of max.  If the output is to be in hex,
then it will be returned as a plain hex
sequence.  Thus the output will either be a single char,
an octal escape sequence, a special escape like C<\n> or a hex value.

If C<PERL_PV_ESCAPE_RE> is set then the escape char used will be a C<"%"> and
not a C<"\\">.  This is because regexes very often contain backslashed
sequences, whereas C<"%"> is not a particularly common character in patterns.

Returns a pointer to the escaped text as held by C<dsv>.

=cut
*/
#define PV_ESCAPE_OCTBUFSIZE 32

char *
Perl_pv_escape( pTHX_ SV *dsv, char const * const str, 
                const STRLEN count, const STRLEN max, 
                STRLEN * const escaped, const U32 flags ) 
{
    const char esc = (flags & PERL_PV_ESCAPE_RE) ? '%' : '\\';
    const char dq = (flags & PERL_PV_ESCAPE_QUOTE) ? '"' : esc;
    char octbuf[PV_ESCAPE_OCTBUFSIZE] = "%123456789ABCDF";
    STRLEN wrote = 0;    /* chars written so far */
    STRLEN chsize = 0;   /* size of data to be written */
    STRLEN readsize = 1; /* size of data just read */
    bool isuni= flags & PERL_PV_ESCAPE_UNI ? 1 : 0; /* is this UTF-8 */
    const char *pv  = str;
    const char * const end = pv + count; /* end of string */
    octbuf[0] = esc;

    PERL_ARGS_ASSERT_PV_ESCAPE;

    if (dsv && !(flags & PERL_PV_ESCAPE_NOCLEAR)) {
	    /* This won't alter the UTF-8 flag */
	    sv_setpvs(dsv, "");
    }
    
    if ((flags & PERL_PV_ESCAPE_UNI_DETECT) && is_utf8_string((U8*)pv, count))
        isuni = 1;
    
    for ( ; (pv < end && (!max || (wrote < max))) ; pv += readsize ) {
        const UV u= (isuni) ? utf8_to_uvchr_buf((U8*)pv, (U8*) end, &readsize) : (U8)*pv;
        const U8 c = (U8)u & 0xFF;
        
        if ( ( u > 255 )
	  || (flags & PERL_PV_ESCAPE_ALL)
	  || (( ! isASCII(u) ) && (flags & (PERL_PV_ESCAPE_NONASCII|PERL_PV_ESCAPE_DWIM))))
	{
            if (flags & PERL_PV_ESCAPE_FIRSTCHAR) 
                chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                      "%"UVxf, u);
            else
                chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                      ((flags & PERL_PV_ESCAPE_DWIM) && !isuni)
                                      ? "%cx%02"UVxf
                                      : "%cx{%02"UVxf"}", esc, u);

        } else if (flags & PERL_PV_ESCAPE_NOBACKSLASH) {
            chsize = 1;            
        } else {         
            if ( (c == dq) || (c == esc) || !isPRINT(c) ) {
	        chsize = 2;
                switch (c) {
                
		case '\\' : /* FALLTHROUGH */
		case '%'  : if ( c == esc )  {
		                octbuf[1] = esc;  
		            } else {
		                chsize = 1;
		            }
		            break;
		case '\v' : octbuf[1] = 'v';  break;
		case '\t' : octbuf[1] = 't';  break;
		case '\r' : octbuf[1] = 'r';  break;
		case '\n' : octbuf[1] = 'n';  break;
		case '\f' : octbuf[1] = 'f';  break;
                case '"'  : 
                        if ( dq == '"' ) 
				octbuf[1] = '"';
                        else 
                            chsize = 1;
                        break;
		default:
                     if ( (flags & PERL_PV_ESCAPE_DWIM) && c != '\0' ) {
                        chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE,
                                      isuni ? "%cx{%02"UVxf"}" : "%cx%02"UVxf,
                                      esc, u);
                     }
                     else if ( (pv+readsize < end) && isDIGIT((U8)*(pv+readsize)) )
                            chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                                  "%c%03o", esc, c);
			else
                            chsize = my_snprintf( octbuf, PV_ESCAPE_OCTBUFSIZE, 
                                                  "%c%o", esc, c);
                }
            } else {
                chsize = 1;
            }
	}
	if ( max && (wrote + chsize > max) ) {
	    break;
        } else if (chsize > 1) {
            if (dsv)
                sv_catpvn(dsv, octbuf, chsize);
            wrote += chsize;
	} else {
	    /* If PERL_PV_ESCAPE_NOBACKSLASH is set then non-ASCII bytes
	       can be appended raw to the dsv. If dsv happens to be
	       UTF-8 then we need catpvf to upgrade them for us.
	       Or add a new API call sv_catpvc(). Think about that name, and
	       how to keep it clear that it's unlike the s of catpvs, which is
	       really an array of octets, not a string.  */
            if (dsv)
                Perl_sv_catpvf( aTHX_ dsv, "%c", c);
	    wrote++;
	}
        if ( flags & PERL_PV_ESCAPE_FIRSTCHAR ) 
            break;
    }
    if (escaped != NULL)
        *escaped= pv - str;
    return dsv ? SvPVX(dsv) : NULL;
}
/*
=for apidoc pv_pretty

Converts a string into something presentable, handling escaping via
C<pv_escape()> and supporting quoting and ellipses.

If the C<PERL_PV_PRETTY_QUOTE> flag is set then the result will be
double quoted with any double quotes in the string escaped.  Otherwise
if the C<PERL_PV_PRETTY_LTGT> flag is set then the result be wrapped in
angle brackets. 

If the C<PERL_PV_PRETTY_ELLIPSES> flag is set and not all characters in
string were output then an ellipsis C<...> will be appended to the
string.  Note that this happens AFTER it has been quoted.

If C<start_color> is non-null then it will be inserted after the opening
quote (if there is one) but before the escaped text.  If C<end_color>
is non-null then it will be inserted after the escaped text but before
any quotes or ellipses.

Returns a pointer to the prettified text as held by C<dsv>.

=cut           
*/

char *
Perl_pv_pretty( pTHX_ SV *dsv, char const * const str, const STRLEN count, 
  const STRLEN max, char const * const start_color, char const * const end_color, 
  const U32 flags ) 
{
    const U8 *quotes = (U8*)((flags & PERL_PV_PRETTY_QUOTE) ? "\"\"" :
                             (flags & PERL_PV_PRETTY_LTGT)  ? "<>" : NULL);
    STRLEN escaped;
    STRLEN max_adjust= 0;
    STRLEN orig_cur;
 
    PERL_ARGS_ASSERT_PV_PRETTY;
   
    if (!(flags & PERL_PV_PRETTY_NOCLEAR)) {
        /* This won't alter the UTF-8 flag */
        sv_setpvs(dsv, "");
    }
    orig_cur= SvCUR(dsv);

    if ( quotes )
        Perl_sv_catpvf(aTHX_ dsv, "%c", quotes[0]);
        
    if ( start_color != NULL ) 
        sv_catpv(dsv, start_color);

    if ((flags & PERL_PV_PRETTY_EXACTSIZE)) {
        if (quotes)
            max_adjust += 2;
        assert(max > max_adjust);
        pv_escape( NULL, str, count, max - max_adjust, &escaped, flags );
        if ( (flags & PERL_PV_PRETTY_ELLIPSES) && ( escaped < count ) )
            max_adjust += 3;
        assert(max > max_adjust);
    }

    pv_escape( dsv, str, count, max - max_adjust, &escaped, flags | PERL_PV_ESCAPE_NOCLEAR );

    if ( end_color != NULL ) 
        sv_catpv(dsv, end_color);

    if ( quotes )
        Perl_sv_catpvf(aTHX_ dsv, "%c", quotes[1]);
    
    if ( (flags & PERL_PV_PRETTY_ELLIPSES) && ( escaped < count ) )
	    sv_catpvs(dsv, "...");

    if ((flags & PERL_PV_PRETTY_EXACTSIZE)) {
        while( SvCUR(dsv) - orig_cur < max )
            sv_catpvs(dsv," ");
    }
 
    return SvPVX(dsv);
}

/*
=for apidoc pv_display

Similar to

  pv_escape(dsv,pv,cur,pvlim,PERL_PV_ESCAPE_QUOTE);

except that an additional "\0" will be appended to the string when
len > cur and pv[cur] is "\0".

Note that the final string may be up to 7 chars longer than pvlim.

=cut
*/

char *
Perl_pv_display(pTHX_ SV *dsv, const char *pv, STRLEN cur, STRLEN len, STRLEN pvlim)
{
    PERL_ARGS_ASSERT_PV_DISPLAY;

    pv_pretty( dsv, pv, cur, pvlim, NULL, NULL, PERL_PV_PRETTY_DUMP);
    if (len > cur && pv[cur] == '\0')
            sv_catpvs( dsv, "\\0");
    return SvPVX(dsv);
}

char *
Perl_sv_peek(pTHX_ SV *sv)
{
    dVAR;
    SV * const t = sv_newmortal();
    int unref = 0;
    U32 type;

    sv_setpvs(t, "");
  retry:
    if (!sv) {
	sv_catpv(t, "VOID");
	goto finish;
    }
    else if (sv == (const SV *)0x55555555 || ((char)SvTYPE(sv)) == 'U') {
        /* detect data corruption under memory poisoning */
	sv_catpv(t, "WILD");
	goto finish;
    }
    else if (sv == &PL_sv_undef || sv == &PL_sv_no || sv == &PL_sv_yes || sv == &PL_sv_placeholder) {
	if (sv == &PL_sv_undef) {
	    sv_catpv(t, "SV_UNDEF");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	else if (sv == &PL_sv_no) {
	    sv_catpv(t, "SV_NO");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 0 &&
		SvNVX(sv) == 0.0)
		goto finish;
	}
	else if (sv == &PL_sv_yes) {
	    sv_catpv(t, "SV_YES");
	    if (!(SvFLAGS(sv) & (SVf_ROK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		!(~SvFLAGS(sv) & (SVf_POK|SVf_NOK|SVf_READONLY|
				  SVp_POK|SVp_NOK)) &&
		SvCUR(sv) == 1 &&
		SvPVX_const(sv) && *SvPVX_const(sv) == '1' &&
		SvNVX(sv) == 1.0)
		goto finish;
	}
	else {
	    sv_catpv(t, "SV_PLACEHOLDER");
	    if (!(SvFLAGS(sv) & (SVf_OK|SVf_OOK|SVs_OBJECT|
				 SVs_GMG|SVs_SMG|SVs_RMG)) &&
		SvREADONLY(sv))
		goto finish;
	}
	sv_catpv(t, ":");
    }
    else if (SvREFCNT(sv) == 0) {
	sv_catpv(t, "(");
	unref++;
    }
    else if (DEBUG_R_TEST_) {
	int is_tmp = 0;
	SSize_t ix;
	/* is this SV on the tmps stack? */
	for (ix=PL_tmps_ix; ix>=0; ix--) {
	    if (PL_tmps_stack[ix] == sv) {
		is_tmp = 1;
		break;
	    }
	}
	if (SvREFCNT(sv) > 1)
	    Perl_sv_catpvf(aTHX_ t, "<%"UVuf"%s>", (UV)SvREFCNT(sv),
		    is_tmp ? "T" : "");
	else if (is_tmp)
	    sv_catpv(t, "<T>");
    }

    if (SvROK(sv)) {
	sv_catpv(t, "\\");
	if (SvCUR(t) + unref > 10) {
	    SvCUR_set(t, unref + 3);
	    *SvEND(t) = '\0';
	    sv_catpv(t, "...");
	    goto finish;
	}
	sv = SvRV(sv);
	goto retry;
    }
    type = SvTYPE(sv);
    if (type == SVt_PVCV) {
        SV * const tmp = newSVpvs_flags("", SVs_TEMP);
        GV* gvcv = CvGV(sv);
        Perl_sv_catpvf(aTHX_ t, "CV(%s)", gvcv
                       ? generic_pv_escape( tmp, GvNAME(gvcv), GvNAMELEN(gvcv), GvNAMEUTF8(gvcv))
                       : "");
	goto finish;
    } else if (type < SVt_LAST) {
	sv_catpv(t, svshorttypenames[type]);

	if (type == SVt_NULL)
	    goto finish;
    } else {
	sv_catpv(t, "FREED");
	goto finish;
    }

    if (SvPOKp(sv)) {
	if (!SvPVX_const(sv))
	    sv_catpv(t, "(null)");
	else {
	    SV * const tmp = newSVpvs("");
	    sv_catpv(t, "(");
	    if (SvOOK(sv)) {
		STRLEN delta;
		SvOOK_offset(sv, delta);
		Perl_sv_catpvf(aTHX_ t, "[%s]", pv_display(tmp, SvPVX_const(sv)-delta, delta, 0, 127));
	    }
	    Perl_sv_catpvf(aTHX_ t, "%s)", pv_display(tmp, SvPVX_const(sv), SvCUR(sv), SvLEN(sv), 127));
	    if (SvUTF8(sv))
		Perl_sv_catpvf(aTHX_ t, " [UTF8 \"%s\"]",
			       sv_uni_display(tmp, sv, 6 * SvCUR(sv),
					      UNI_DISPLAY_QQ));
	    SvREFCNT_dec_NN(tmp);
	}
    }
    else if (SvNOKp(sv)) {
	STORE_LC_NUMERIC_UNDERLYING_SET_STANDARD();
	Perl_sv_catpvf(aTHX_ t, "(%"NVgf")",SvNVX(sv));
	RESTORE_LC_NUMERIC_UNDERLYING();
    }
    else if (SvIOKp(sv)) {
	if (SvIsUV(sv))
	    Perl_sv_catpvf(aTHX_ t, "(%"UVuf")", (UV)SvUVX(sv));
	else
            Perl_sv_catpvf(aTHX_ t, "(%"IVdf")", (IV)SvIVX(sv));
    }
    else
	sv_catpv(t, "()");

  finish:
    while (unref--)
	sv_catpv(t, ")");
    if (TAINTING_get && sv && SvTAINTED(sv))
	sv_catpv(t, " [tainted]");
    return SvPV_nolen(t);
}

/*
=head1 Debugging Utilities
*/

void
Perl_dump_indent(pTHX_ I32 level, PerlIO *file, const char* pat, ...)
{
    va_list args;
    PERL_ARGS_ASSERT_DUMP_INDENT;
    va_start(args, pat);
    dump_vindent(level, file, pat, &args);
    va_end(args);
}

void
Perl_dump_vindent(pTHX_ I32 level, PerlIO *file, const char* pat, va_list *args)
{
    PERL_ARGS_ASSERT_DUMP_VINDENT;
    PerlIO_printf(file, "%*s", (int)(level*PL_dumpindent), "");
    PerlIO_vprintf(file, pat, *args);
}

/*
=for apidoc dump_all

Dumps the entire optree of the current program starting at C<PL_main_root> to 
C<STDERR>.  Also dumps the optrees for all visible subroutines in
C<PL_defstash>.

=cut
*/

void
Perl_dump_all(pTHX)
{
    dump_all_perl(FALSE);
}

void
Perl_dump_all_perl(pTHX_ bool justperl)
{
    PerlIO_setlinebuf(Perl_debug_log);
    if (PL_main_root)
	op_dump(PL_main_root);
    dump_packsubs_perl(PL_defstash, justperl);
}

/*
=for apidoc dump_packsubs

Dumps the optrees for all visible subroutines in C<stash>.

=cut
*/

void
Perl_dump_packsubs(pTHX_ const HV *stash)
{
    PERL_ARGS_ASSERT_DUMP_PACKSUBS;
    dump_packsubs_perl(stash, FALSE);
}

void
Perl_dump_packsubs_perl(pTHX_ const HV *stash, bool justperl)
{
    I32	i;

    PERL_ARGS_ASSERT_DUMP_PACKSUBS_PERL;

    if (!HvARRAY(stash))
	return;
    for (i = 0; i <= (I32) HvMAX(stash); i++) {
        const HE *entry;
	for (entry = HvARRAY(stash)[i]; entry; entry = HeNEXT(entry)) {
	    GV * gv = (GV *)HeVAL(entry);
            if (SvROK(gv) && SvTYPE(SvRV(gv)) == SVt_PVCV)
                /* unfake a fake GV */
                (void)CvGV(SvRV(gv));
	    if (SvTYPE(gv) != SVt_PVGV || !GvGP(gv))
		continue;
	    if (GvCVu(gv))
		dump_sub_perl(gv, justperl);
	    if (GvFORM(gv))
		dump_form(gv);
	    if (HeKEY(entry)[HeKLEN(entry)-1] == ':') {
		const HV * const hv = GvHV(gv);
		if (hv && (hv != PL_defstash))
		    dump_packsubs_perl(hv, justperl); /* nested package */
	    }
	}
    }
}

void
Perl_dump_sub(pTHX_ const GV *gv)
{
    PERL_ARGS_ASSERT_DUMP_SUB;
    dump_sub_perl(gv, FALSE);
}

void
Perl_dump_sub_perl(pTHX_ const GV *gv, bool justperl)
{
    STRLEN len;
    SV * const sv = newSVpvs_flags("", SVs_TEMP);
    SV *tmpsv;
    const char * name;

    PERL_ARGS_ASSERT_DUMP_SUB_PERL;

    if (justperl && (CvISXSUB(GvCV(gv)) || !CvROOT(GvCV(gv))))
	return;

    tmpsv = newSVpvs_flags("", SVs_TEMP);
    gv_fullname3(sv, gv, NULL);
    name = SvPV_const(sv, len);
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "\nSUB %s = ",
                     generic_pv_escape(tmpsv, name, len, SvUTF8(sv)));
    if (CvISXSUB(GvCV(gv)))
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "(xsub 0x%"UVxf" %d)\n",
	    PTR2UV(CvXSUB(GvCV(gv))),
	    (int)CvXSUBANY(GvCV(gv)).any_i32);
    else if (CvROOT(GvCV(gv)))
	op_dump(CvROOT(GvCV(gv)));
    else
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "<undef>\n");
}

void
Perl_dump_form(pTHX_ const GV *gv)
{
    SV * const sv = sv_newmortal();

    PERL_ARGS_ASSERT_DUMP_FORM;

    gv_fullname3(sv, gv, NULL);
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "\nFORMAT %s = ", SvPVX_const(sv));
    if (CvROOT(GvFORM(gv)))
	op_dump(CvROOT(GvFORM(gv)));
    else
	Perl_dump_indent(aTHX_ 0, Perl_debug_log, "<undef>\n");
}

void
Perl_dump_eval(pTHX)
{
    op_dump(PL_eval_root);
}

void
Perl_do_pmop_dump(pTHX_ I32 level, PerlIO *file, const PMOP *pm)
{
    char ch;

    PERL_ARGS_ASSERT_DO_PMOP_DUMP;

    if (!pm) {
	Perl_dump_indent(aTHX_ level, file, "{}\n");
	return;
    }
    Perl_dump_indent(aTHX_ level, file, "{\n");
    level++;
    if (pm->op_pmflags & PMf_ONCE)
	ch = '?';
    else
	ch = '/';
    if (PM_GETRE(pm))
	Perl_dump_indent(aTHX_ level, file, "PMf_PRE %c%.*s%c%s\n",
	     ch,(int)RX_PRELEN(PM_GETRE(pm)), RX_PRECOMP(PM_GETRE(pm)), ch,
	     (pm->op_private & OPpRUNTIME) ? " (RUNTIME)" : "");
    else
	Perl_dump_indent(aTHX_ level, file, "PMf_PRE (RUNTIME)\n");
    if (pm->op_type != OP_PUSHRE && pm->op_pmreplrootu.op_pmreplroot) {
	Perl_dump_indent(aTHX_ level, file, "PMf_REPL = ");
	op_dump(pm->op_pmreplrootu.op_pmreplroot);
    }
    if (pm->op_code_list) {
	if (pm->op_pmflags & PMf_CODELIST_PRIVATE) {
	    Perl_dump_indent(aTHX_ level, file, "CODE_LIST =\n");
	    do_op_dump(level, file, pm->op_code_list);
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "CODE_LIST = 0x%"UVxf"\n",
				    PTR2UV(pm->op_code_list));
    }
    if (pm->op_pmflags || (PM_GETRE(pm) && RX_CHECK_SUBSTR(PM_GETRE(pm)))) {
	SV * const tmpsv = pm_description(pm);
	Perl_dump_indent(aTHX_ level, file, "PMFLAGS = (%s)\n", SvCUR(tmpsv) ? SvPVX_const(tmpsv) + 1 : "");
	SvREFCNT_dec_NN(tmpsv);
    }

    Perl_dump_indent(aTHX_ level-1, file, "}\n");
}

const struct flag_to_name pmflags_flags_names[] = {
    {PMf_CONST, ",CONST"},
    {PMf_KEEP, ",KEEP"},
    {PMf_GLOBAL, ",GLOBAL"},
    {PMf_CONTINUE, ",CONTINUE"},
    {PMf_RETAINT, ",RETAINT"},
    {PMf_EVAL, ",EVAL"},
    {PMf_NONDESTRUCT, ",NONDESTRUCT"},
    {PMf_HAS_CV, ",HAS_CV"},
    {PMf_CODELIST_PRIVATE, ",CODELIST_PRIVATE"},
    {PMf_IS_QR, ",IS_QR"}
};

static SV *
S_pm_description(pTHX_ const PMOP *pm)
{
    SV * const desc = newSVpvs("");
    const REGEXP * const regex = PM_GETRE(pm);
    const U32 pmflags = pm->op_pmflags;

    PERL_ARGS_ASSERT_PM_DESCRIPTION;

    if (pmflags & PMf_ONCE)
	sv_catpv(desc, ",ONCE");
#ifdef USE_ITHREADS
    if (SvREADONLY(PL_regex_pad[pm->op_pmoffset]))
        sv_catpv(desc, ":USED");
#else
    if (pmflags & PMf_USED)
        sv_catpv(desc, ":USED");
#endif

    if (regex) {
        if (RX_ISTAINTED(regex))
            sv_catpv(desc, ",TAINTED");
        if (RX_CHECK_SUBSTR(regex)) {
            if (!(RX_INTFLAGS(regex) & PREGf_NOSCAN))
                sv_catpv(desc, ",SCANFIRST");
            if (RX_EXTFLAGS(regex) & RXf_CHECK_ALL)
                sv_catpv(desc, ",ALL");
        }
        if (RX_EXTFLAGS(regex) & RXf_SKIPWHITE)
            sv_catpv(desc, ",SKIPWHITE");
    }

    append_flags(desc, pmflags, pmflags_flags_names);
    return desc;
}

void
Perl_pmop_dump(pTHX_ PMOP *pm)
{
    do_pmop_dump(0, Perl_debug_log, pm);
}

/* Return a unique integer to represent the address of op o.
 * If it already exists in PL_op_sequence, just return it;
 * otherwise add it.
 *  *** Note that this isn't thread-safe */

STATIC UV
S_sequence_num(pTHX_ const OP *o)
{
    dVAR;
    SV     *op,
          **seq;
    const char *key;
    STRLEN  len;
    if (!o)
	return 0;
    op = newSVuv(PTR2UV(o));
    sv_2mortal(op);
    key = SvPV_const(op, len);
    if (!PL_op_sequence)
	PL_op_sequence = newHV();
    seq = hv_fetch(PL_op_sequence, key, len, 0);
    if (seq)
	return SvUV(*seq);
    (void)hv_store(PL_op_sequence, key, len, newSVuv(++PL_op_seq), 0);
    return PL_op_seq;
}





const struct flag_to_name op_flags_names[] = {
    {OPf_KIDS, ",KIDS"},
    {OPf_PARENS, ",PARENS"},
    {OPf_REF, ",REF"},
    {OPf_MOD, ",MOD"},
    {OPf_STACKED, ",STACKED"},
    {OPf_SPECIAL, ",SPECIAL"}
};


void
Perl_do_op_dump(pTHX_ I32 level, PerlIO *file, const OP *o)
{
    UV      seq;
    const OPCODE optype = o->op_type;

    PERL_ARGS_ASSERT_DO_OP_DUMP;

    Perl_dump_indent(aTHX_ level, file, "{\n");
    level++;
    seq = sequence_num(o);
    if (seq)
	PerlIO_printf(file, "%-4"UVuf, seq);
    else
	PerlIO_printf(file, "????");
    PerlIO_printf(file,
		  "%*sTYPE = %s  ===> ",
		  (int)(PL_dumpindent*level-4), "", OP_NAME(o));
    if (o->op_next)
	PerlIO_printf(file,
			o->op_type == OP_NULL ? "(%"UVuf")\n" : "%"UVuf"\n",
				sequence_num(o->op_next));
    else
	PerlIO_printf(file, "NULL\n");
    if (o->op_targ) {
	if (optype == OP_NULL) {
	    Perl_dump_indent(aTHX_ level, file, "  (was %s)\n", PL_op_name[o->op_targ]);
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "TARG = %ld\n", (long)o->op_targ);
    }
#ifdef DUMPADDR
    Perl_dump_indent(aTHX_ level, file, "ADDR = 0x%"UVxf" => 0x%"UVxf"\n", (UV)o, (UV)o->op_next);
#endif

    if (o->op_flags || o->op_slabbed || o->op_savefree || o->op_static) {
        SV * const tmpsv = newSVpvs("");
        switch (o->op_flags & OPf_WANT) {
        case OPf_WANT_VOID:
            sv_catpv(tmpsv, ",VOID");
            break;
        case OPf_WANT_SCALAR:
            sv_catpv(tmpsv, ",SCALAR");
            break;
        case OPf_WANT_LIST:
            sv_catpv(tmpsv, ",LIST");
            break;
        default:
            sv_catpv(tmpsv, ",UNKNOWN");
            break;
        }
        append_flags(tmpsv, o->op_flags, op_flags_names);
        if (o->op_slabbed)  sv_catpvs(tmpsv, ",SLABBED");
        if (o->op_savefree) sv_catpvs(tmpsv, ",SAVEFREE");
        if (o->op_static)   sv_catpvs(tmpsv, ",STATIC");
        if (o->op_folded)   sv_catpvs(tmpsv, ",FOLDED");
        if (o->op_moresib)  sv_catpvs(tmpsv, ",MORESIB");
        Perl_dump_indent(aTHX_ level, file, "FLAGS = (%s)\n",
                         SvCUR(tmpsv) ? SvPVX_const(tmpsv) + 1 : "");
    }

    if (o->op_private) {
        U16 oppriv = o->op_private;
        I16 op_ix = PL_op_private_bitdef_ix[o->op_type];
        SV * tmpsv = NULL;

        if (op_ix != -1) {
            U16 stop = 0;
            tmpsv = newSVpvs("");
            for (; !stop; op_ix++) {
                U16 entry = PL_op_private_bitdefs[op_ix];
                U16 bit = (entry >> 2) & 7;
                U16 ix = entry >> 5;

                stop = (entry & 1);

                if (entry & 2) {
                    /* bitfield */
                    I16 const *p = &PL_op_private_bitfields[ix];
                    U16 bitmin = (U16) *p++;
                    I16 label = *p++;
                    I16 enum_label;
                    U16 mask = 0;
                    U16 i;
                    U16 val;

                    for (i = bitmin; i<= bit; i++)
                        mask |= (1<<i);
                    bit = bitmin;
                    val = (oppriv & mask);

                    if (   label != -1
                        && PL_op_private_labels[label] == '-'
                        && PL_op_private_labels[label+1] == '\0'
                    )
                        /* display as raw number */
                        continue;

                    oppriv -= val;
                    val >>= bit;
                    enum_label = -1;
                    while (*p != -1) {
                        if (val == *p++) {
                            enum_label = *p;
                            break;
                        }
                        p++;
                    }
                    if (val == 0 && enum_label == -1)
                        /* don't display anonymous zero values */
                        continue;

                    sv_catpv(tmpsv, ",");
                    if (label != -1) {
                        sv_catpv(tmpsv, &PL_op_private_labels[label]);
                        sv_catpv(tmpsv, "=");
                    }
                    if (enum_label == -1)
                        Perl_sv_catpvf(aTHX_ tmpsv, "0x%"UVxf, (UV)val);
                    else
                        sv_catpv(tmpsv, &PL_op_private_labels[enum_label]);

                }
                else {
                    /* bit flag */
                    if (   oppriv & (1<<bit)
                        && !(PL_op_private_labels[ix] == '-'
                             && PL_op_private_labels[ix+1] == '\0'))
                    {
                        oppriv -= (1<<bit);
                        sv_catpv(tmpsv, ",");
                        sv_catpv(tmpsv, &PL_op_private_labels[ix]);
                    }
                }
            }
            if (oppriv) {
                sv_catpv(tmpsv, ",");
                Perl_sv_catpvf(aTHX_ tmpsv, "0x%"UVxf, (UV)oppriv);
            }
        }
	if (tmpsv && SvCUR(tmpsv)) {
            Perl_dump_indent(aTHX_ level, file, "PRIVATE = (%s)\n", SvPVX_const(tmpsv) + 1);
	} else
            Perl_dump_indent(aTHX_ level, file, "PRIVATE = (0x%"UVxf")\n",
                             (UV)oppriv);
    }

    switch (optype) {
    case OP_AELEMFAST:
    case OP_GVSV:
    case OP_GV:
#ifdef USE_ITHREADS
	Perl_dump_indent(aTHX_ level, file, "PADIX = %" IVdf "\n", (IV)cPADOPo->op_padix);
#else
	if ( ! (o->op_flags & OPf_SPECIAL)) { /* not lexical */
	    if (cSVOPo->op_sv) {
      STRLEN len;
      const char * name;
      SV * const tmpsv  = newSVpvs_flags("", SVs_TEMP);
      SV * const tmpsv2 = newSVpvs_flags("", SVs_TEMP);
		gv_fullname3(tmpsv, MUTABLE_GV(cSVOPo->op_sv), NULL);
      name = SvPV_const(tmpsv, len);
		Perl_dump_indent(aTHX_ level, file, "GV = %s\n",
                       generic_pv_escape( tmpsv2, name, len, SvUTF8(tmpsv)));
	    }
	    else
		Perl_dump_indent(aTHX_ level, file, "GV = NULL\n");
	}
#endif
	break;

    case OP_MULTIDEREF:
    {
        UNOP_AUX_item *items = cUNOP_AUXo->op_aux;
        UV i, count = items[-1].uv;

	Perl_dump_indent(aTHX_ level, file, "ARGS = \n");
        for (i=0; i < count;  i++)
            Perl_dump_indent(aTHX_ level+1, file, "%"UVuf" => 0x%"UVxf"\n",
                                    i, items[i].uv);
	break;
    }

    case OP_CONST:
    case OP_HINTSEVAL:
    case OP_METHOD_NAMED:
    case OP_METHOD_SUPER:
    case OP_METHOD_REDIR:
    case OP_METHOD_REDIR_SUPER:
#ifndef USE_ITHREADS
	/* with ITHREADS, consts are stored in the pad, and the right pad
	 * may not be active here, so skip */
	Perl_dump_indent(aTHX_ level, file, "SV = %s\n", SvPEEK(cMETHOPx_meth(o)));
#endif
	break;
    case OP_NULL:
	if (o->op_targ != OP_NEXTSTATE && o->op_targ != OP_DBSTATE)
	    break;
	/* FALLTHROUGH */
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	if (CopLINE(cCOPo))
	    Perl_dump_indent(aTHX_ level, file, "LINE = %"UVuf"\n",
			     (UV)CopLINE(cCOPo));
    if (CopSTASHPV(cCOPo)) {
        SV* tmpsv = newSVpvs_flags("", SVs_TEMP);
        HV *stash = CopSTASH(cCOPo);
        const char * const hvname = HvNAME_get(stash);
        
	    Perl_dump_indent(aTHX_ level, file, "PACKAGE = \"%s\"\n",
                           generic_pv_escape(tmpsv, hvname,
                              HvNAMELEN(stash), HvNAMEUTF8(stash)));
    }
  if (CopLABEL(cCOPo)) {
       SV* tmpsv = newSVpvs_flags("", SVs_TEMP);
       STRLEN label_len;
       U32 label_flags;
       const char *label = CopLABEL_len_flags(cCOPo,
                                                &label_len, &label_flags);
       Perl_dump_indent(aTHX_ level, file, "LABEL = \"%s\"\n",
                           generic_pv_escape( tmpsv, label, label_len,
                                      (label_flags & SVf_UTF8)));
   }
        Perl_dump_indent(aTHX_ level, file, "SEQ = %u\n",
                         (unsigned int)cCOPo->cop_seq);
	break;
    case OP_ENTERLOOP:
	Perl_dump_indent(aTHX_ level, file, "REDO ===> ");
	if (cLOOPo->op_redoop)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOOPo->op_redoop));
	else
	    PerlIO_printf(file, "DONE\n");
	Perl_dump_indent(aTHX_ level, file, "NEXT ===> ");
	if (cLOOPo->op_nextop)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOOPo->op_nextop));
	else
	    PerlIO_printf(file, "DONE\n");
	Perl_dump_indent(aTHX_ level, file, "LAST ===> ");
	if (cLOOPo->op_lastop)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOOPo->op_lastop));
	else
	    PerlIO_printf(file, "DONE\n");
	break;
    case OP_COND_EXPR:
    case OP_RANGE:
    case OP_MAPWHILE:
    case OP_GREPWHILE:
    case OP_OR:
    case OP_AND:
	Perl_dump_indent(aTHX_ level, file, "OTHER ===> ");
	if (cLOGOPo->op_other)
	    PerlIO_printf(file, "%"UVuf"\n", sequence_num(cLOGOPo->op_other));
	else
	    PerlIO_printf(file, "DONE\n");
	break;
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
	do_pmop_dump(level, file, cPMOPo);
	break;
    case OP_LEAVE:
    case OP_LEAVEEVAL:
    case OP_LEAVESUB:
    case OP_LEAVESUBLV:
    case OP_LEAVEWRITE:
    case OP_SCOPE:
	if (o->op_private & OPpREFCOUNTED)
	    Perl_dump_indent(aTHX_ level, file, "REFCNT = %"UVuf"\n", (UV)o->op_targ);
	break;
    default:
	break;
    }
    if (o->op_flags & OPf_KIDS) {
	OP *kid;
	for (kid = cUNOPo->op_first; kid; kid = OpSIBLING(kid))
	    do_op_dump(level, file, kid);
    }
    Perl_dump_indent(aTHX_ level-1, file, "}\n");
}

/*
=for apidoc op_dump

Dumps the optree starting at OP C<o> to C<STDERR>.

=cut
*/

void
Perl_op_dump(pTHX_ const OP *o)
{
    PERL_ARGS_ASSERT_OP_DUMP;
    do_op_dump(0, Perl_debug_log, o);
}

void
Perl_gv_dump(pTHX_ GV *gv)
{
    STRLEN len;
    const char* name;
    SV *sv, *tmp = newSVpvs_flags("", SVs_TEMP);

    if (!gv) {
	PerlIO_printf(Perl_debug_log, "{}\n");
	return;
    }
    sv = sv_newmortal();
    PerlIO_printf(Perl_debug_log, "{\n");
    gv_fullname3(sv, gv, NULL);
    name = SvPV_const(sv, len);
    Perl_dump_indent(aTHX_ 1, Perl_debug_log, "GV_NAME = %s",
                     generic_pv_escape( tmp, name, len, SvUTF8(sv) ));
    if (gv != GvEGV(gv)) {
	gv_efullname3(sv, GvEGV(gv), NULL);
        name = SvPV_const(sv, len);
        Perl_dump_indent(aTHX_ 1, Perl_debug_log, "-> %s",
                     generic_pv_escape( tmp, name, len, SvUTF8(sv) ));
    }
    (void)PerlIO_putc(Perl_debug_log, '\n');
    Perl_dump_indent(aTHX_ 0, Perl_debug_log, "}\n");
}


/* map magic types to the symbolic names
 * (with the PERL_MAGIC_ prefixed stripped)
 */

static const struct { const char type; const char *name; } magic_names[] = {
#include "mg_names.inc"
	/* this null string terminates the list */
	{ 0,                         NULL },
};

void
Perl_do_magic_dump(pTHX_ I32 level, PerlIO *file, const MAGIC *mg, I32 nest, I32 maxnest, bool dumpops, STRLEN pvlim)
{
    PERL_ARGS_ASSERT_DO_MAGIC_DUMP;

    for (; mg; mg = mg->mg_moremagic) {
 	Perl_dump_indent(aTHX_ level, file,
			 "  MAGIC = 0x%"UVxf"\n", PTR2UV(mg));
 	if (mg->mg_virtual) {
            const MGVTBL * const v = mg->mg_virtual;
	    if (v >= PL_magic_vtables
		&& v < PL_magic_vtables + magic_vtable_max) {
		const U32 i = v - PL_magic_vtables;
	        Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = &PL_vtbl_%s\n", PL_magic_vtable_names[i]);
	    }
	    else
	        Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = 0x%"UVxf"\n", PTR2UV(v));
        }
	else
	    Perl_dump_indent(aTHX_ level, file, "    MG_VIRTUAL = 0\n");

	if (mg->mg_private)
	    Perl_dump_indent(aTHX_ level, file, "    MG_PRIVATE = %d\n", mg->mg_private);

	{
	    int n;
	    const char *name = NULL;
	    for (n = 0; magic_names[n].name; n++) {
		if (mg->mg_type == magic_names[n].type) {
		    name = magic_names[n].name;
		    break;
		}
	    }
	    if (name)
		Perl_dump_indent(aTHX_ level, file,
				"    MG_TYPE = PERL_MAGIC_%s\n", name);
	    else
		Perl_dump_indent(aTHX_ level, file,
				"    MG_TYPE = UNKNOWN(\\%o)\n", mg->mg_type);
	}

        if (mg->mg_flags) {
            Perl_dump_indent(aTHX_ level, file, "    MG_FLAGS = 0x%02X\n", mg->mg_flags);
	    if (mg->mg_type == PERL_MAGIC_envelem &&
		mg->mg_flags & MGf_TAINTEDDIR)
	        Perl_dump_indent(aTHX_ level, file, "      TAINTEDDIR\n");
	    if (mg->mg_type == PERL_MAGIC_regex_global &&
		mg->mg_flags & MGf_MINMATCH)
	        Perl_dump_indent(aTHX_ level, file, "      MINMATCH\n");
	    if (mg->mg_flags & MGf_REFCOUNTED)
	        Perl_dump_indent(aTHX_ level, file, "      REFCOUNTED\n");
            if (mg->mg_flags & MGf_GSKIP)
	        Perl_dump_indent(aTHX_ level, file, "      GSKIP\n");
	    if (mg->mg_flags & MGf_COPY)
	        Perl_dump_indent(aTHX_ level, file, "      COPY\n");
	    if (mg->mg_flags & MGf_DUP)
	        Perl_dump_indent(aTHX_ level, file, "      DUP\n");
	    if (mg->mg_flags & MGf_LOCAL)
	        Perl_dump_indent(aTHX_ level, file, "      LOCAL\n");
	    if (mg->mg_type == PERL_MAGIC_regex_global &&
		mg->mg_flags & MGf_BYTES)
	        Perl_dump_indent(aTHX_ level, file, "      BYTES\n");
        }
	if (mg->mg_obj) {
	    Perl_dump_indent(aTHX_ level, file, "    MG_OBJ = 0x%"UVxf"\n",
	        PTR2UV(mg->mg_obj));
            if (mg->mg_type == PERL_MAGIC_qr) {
		REGEXP* const re = (REGEXP *)mg->mg_obj;
		SV * const dsv = sv_newmortal();
                const char * const s
		    = pv_pretty(dsv, RX_WRAPPED(re), RX_WRAPLEN(re),
                    60, NULL, NULL,
                    ( PERL_PV_PRETTY_QUOTE | PERL_PV_ESCAPE_RE | PERL_PV_PRETTY_ELLIPSES |
                    (RX_UTF8(re) ? PERL_PV_ESCAPE_UNI : 0))
                );
		Perl_dump_indent(aTHX_ level+1, file, "    PAT = %s\n", s);
		Perl_dump_indent(aTHX_ level+1, file, "    REFCNT = %"IVdf"\n",
			(IV)RX_REFCNT(re));
            }
            if (mg->mg_flags & MGf_REFCOUNTED)
		do_sv_dump(level+2, file, mg->mg_obj, nest+1, maxnest, dumpops, pvlim); /* MG is already +1 */
	}
        if (mg->mg_len)
	    Perl_dump_indent(aTHX_ level, file, "    MG_LEN = %ld\n", (long)mg->mg_len);
        if (mg->mg_ptr) {
	    Perl_dump_indent(aTHX_ level, file, "    MG_PTR = 0x%"UVxf, PTR2UV(mg->mg_ptr));
	    if (mg->mg_len >= 0) {
		if (mg->mg_type != PERL_MAGIC_utf8) {
		    SV * const sv = newSVpvs("");
		    PerlIO_printf(file, " %s", pv_display(sv, mg->mg_ptr, mg->mg_len, 0, pvlim));
		    SvREFCNT_dec_NN(sv);
		}
            }
	    else if (mg->mg_len == HEf_SVKEY) {
		PerlIO_puts(file, " => HEf_SVKEY\n");
		do_sv_dump(level+2, file, MUTABLE_SV(((mg)->mg_ptr)), nest+1,
			   maxnest, dumpops, pvlim); /* MG is already +1 */
		continue;
	    }
	    else if (mg->mg_len == -1 && mg->mg_type == PERL_MAGIC_utf8);
	    else
		PerlIO_puts(
		  file,
		 " ???? - " __FILE__
		 " does not know how to handle this MG_LEN"
		);
            (void)PerlIO_putc(file, '\n');
        }
	if (mg->mg_type == PERL_MAGIC_utf8) {
	    const STRLEN * const cache = (STRLEN *) mg->mg_ptr;
	    if (cache) {
		IV i;
		for (i = 0; i < PERL_MAGIC_UTF8_CACHESIZE; i++)
		    Perl_dump_indent(aTHX_ level, file,
				     "      %2"IVdf": %"UVuf" -> %"UVuf"\n",
				     i,
				     (UV)cache[i * 2],
				     (UV)cache[i * 2 + 1]);
	    }
	}
    }
}

void
Perl_magic_dump(pTHX_ const MAGIC *mg)
{
    do_magic_dump(0, Perl_debug_log, mg, 0, 0, FALSE, 0);
}

void
Perl_do_hv_dump(pTHX_ I32 level, PerlIO *file, const char *name, HV *sv)
{
    const char *hvname;

    PERL_ARGS_ASSERT_DO_HV_DUMP;

    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && (hvname = HvNAME_get(sv)))
    {
	/* we have to use pv_display and HvNAMELEN_get() so that we display the real package
           name which quite legally could contain insane things like tabs, newlines, nulls or
           other scary crap - this should produce sane results - except maybe for unicode package
           names - but we will wait for someone to file a bug on that - demerphq */
        SV * const tmpsv = newSVpvs_flags("", SVs_TEMP);
        PerlIO_printf(file, "\t\"%s\"\n",
                              generic_pv_escape( tmpsv, hvname,
                                   HvNAMELEN(sv), HvNAMEUTF8(sv)));
    }
    else
        (void)PerlIO_putc(file, '\n');
}

void
Perl_do_gv_dump(pTHX_ I32 level, PerlIO *file, const char *name, GV *sv)
{
    PERL_ARGS_ASSERT_DO_GV_DUMP;

    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && GvNAME(sv)) {
        SV * const tmpsv = newSVpvs("");
        PerlIO_printf(file, "\t\"%s\"\n",
                              generic_pv_escape( tmpsv, GvNAME(sv), GvNAMELEN(sv), GvNAMEUTF8(sv) ));
    }
    else
        (void)PerlIO_putc(file, '\n');
}

void
Perl_do_gvgv_dump(pTHX_ I32 level, PerlIO *file, const char *name, GV *sv)
{
    PERL_ARGS_ASSERT_DO_GVGV_DUMP;

    Perl_dump_indent(aTHX_ level, file, "%s = 0x%"UVxf, name, PTR2UV(sv));
    if (sv && GvNAME(sv)) {
       SV *tmp = newSVpvs_flags("", SVs_TEMP);
	const char *hvname;
        HV * const stash = GvSTASH(sv);
	PerlIO_printf(file, "\t");
   /* TODO might have an extra \" here */
	if (stash && (hvname = HvNAME_get(stash))) {
            PerlIO_printf(file, "\"%s\" :: \"",
                                  generic_pv_escape(tmp, hvname,
                                      HvNAMELEN(stash), HvNAMEUTF8(stash)));
        }
        PerlIO_printf(file, "%s\"\n",
                              generic_pv_escape( tmp, GvNAME(sv), GvNAMELEN(sv), GvNAMEUTF8(sv)));
    }
    else
        (void)PerlIO_putc(file, '\n');
}

const struct flag_to_name first_sv_flags_names[] = {
    {SVs_TEMP, "TEMP,"},
    {SVs_OBJECT, "OBJECT,"},
    {SVs_GMG, "GMG,"},
    {SVs_SMG, "SMG,"},
    {SVs_RMG, "RMG,"},
    {SVf_IOK, "IOK,"},
    {SVf_NOK, "NOK,"},
    {SVf_POK, "POK,"}
};

const struct flag_to_name second_sv_flags_names[] = {
    {SVf_OOK, "OOK,"},
    {SVf_FAKE, "FAKE,"},
    {SVf_READONLY, "READONLY,"},
    {SVf_PROTECT, "PROTECT,"},
    {SVf_BREAK, "BREAK,"},
    {SVp_IOK, "pIOK,"},
    {SVp_NOK, "pNOK,"},
    {SVp_POK, "pPOK,"}
};

const struct flag_to_name cv_flags_names[] = {
    {CVf_ANON, "ANON,"},
    {CVf_UNIQUE, "UNIQUE,"},
    {CVf_CLONE, "CLONE,"},
    {CVf_CLONED, "CLONED,"},
    {CVf_CONST, "CONST,"},
    {CVf_NODEBUG, "NODEBUG,"},
    {CVf_LVALUE, "LVALUE,"},
    {CVf_METHOD, "METHOD,"},
    {CVf_WEAKOUTSIDE, "WEAKOUTSIDE,"},
    {CVf_CVGV_RC, "CVGV_RC,"},
    {CVf_DYNFILE, "DYNFILE,"},
    {CVf_AUTOLOAD, "AUTOLOAD,"},
    {CVf_HASEVAL, "HASEVAL,"},
    {CVf_SLABBED, "SLABBED,"},
    {CVf_NAMED, "NAMED,"},
    {CVf_LEXICAL, "LEXICAL,"},
    {CVf_ISXSUB, "ISXSUB,"}
};

const struct flag_to_name hv_flags_names[] = {
    {SVphv_SHAREKEYS, "SHAREKEYS,"},
    {SVphv_LAZYDEL, "LAZYDEL,"},
    {SVphv_HASKFLAGS, "HASKFLAGS,"},
    {SVf_AMAGIC, "OVERLOAD,"},
    {SVphv_CLONEABLE, "CLONEABLE,"}
};

const struct flag_to_name gp_flags_names[] = {
    {GVf_INTRO, "INTRO,"},
    {GVf_MULTI, "MULTI,"},
    {GVf_ASSUMECV, "ASSUMECV,"},
};

const struct flag_to_name gp_flags_imported_names[] = {
    {GVf_IMPORTED_SV, " SV"},
    {GVf_IMPORTED_AV, " AV"},
    {GVf_IMPORTED_HV, " HV"},
    {GVf_IMPORTED_CV, " CV"},
};

/* NOTE: this structure is mostly duplicative of one generated by
 * 'make regen' in regnodes.h - perhaps we should somehow integrate
 * the two. - Yves */
const struct flag_to_name regexp_extflags_names[] = {
    {RXf_PMf_MULTILINE,   "PMf_MULTILINE,"},
    {RXf_PMf_SINGLELINE,  "PMf_SINGLELINE,"},
    {RXf_PMf_FOLD,        "PMf_FOLD,"},
    {RXf_PMf_EXTENDED,    "PMf_EXTENDED,"},
    {RXf_PMf_EXTENDED_MORE, "PMf_EXTENDED_MORE,"},
    {RXf_PMf_KEEPCOPY,    "PMf_KEEPCOPY,"},
    {RXf_PMf_NOCAPTURE,   "PMf_NOCAPURE,"},
    {RXf_IS_ANCHORED,     "IS_ANCHORED,"},
    {RXf_NO_INPLACE_SUBST, "NO_INPLACE_SUBST,"},
    {RXf_EVAL_SEEN,       "EVAL_SEEN,"},
    {RXf_CHECK_ALL,       "CHECK_ALL,"},
    {RXf_MATCH_UTF8,      "MATCH_UTF8,"},
    {RXf_USE_INTUIT_NOML, "USE_INTUIT_NOML,"},
    {RXf_USE_INTUIT_ML,   "USE_INTUIT_ML,"},
    {RXf_INTUIT_TAIL,     "INTUIT_TAIL,"},
    {RXf_SPLIT,           "SPLIT,"},
    {RXf_COPY_DONE,       "COPY_DONE,"},
    {RXf_TAINTED_SEEN,    "TAINTED_SEEN,"},
    {RXf_TAINTED,         "TAINTED,"},
    {RXf_START_ONLY,      "START_ONLY,"},
    {RXf_SKIPWHITE,       "SKIPWHITE,"},
    {RXf_WHITE,           "WHITE,"},
    {RXf_NULL,            "NULL,"},
};

/* NOTE: this structure is mostly duplicative of one generated by
 * 'make regen' in regnodes.h - perhaps we should somehow integrate
 * the two. - Yves */
const struct flag_to_name regexp_core_intflags_names[] = {
    {PREGf_SKIP,            "SKIP,"},
    {PREGf_IMPLICIT,        "IMPLICIT,"},
    {PREGf_NAUGHTY,         "NAUGHTY,"},
    {PREGf_VERBARG_SEEN,    "VERBARG_SEEN,"},
    {PREGf_CUTGROUP_SEEN,   "CUTGROUP_SEEN,"},
    {PREGf_USE_RE_EVAL,     "USE_RE_EVAL,"},
    {PREGf_NOSCAN,          "NOSCAN,"},
    {PREGf_GPOS_SEEN,       "GPOS_SEEN,"},
    {PREGf_GPOS_FLOAT,      "GPOS_FLOAT,"},
    {PREGf_ANCH_MBOL,       "ANCH_MBOL,"},
    {PREGf_ANCH_SBOL,       "ANCH_SBOL,"},
    {PREGf_ANCH_GPOS,       "ANCH_GPOS,"},
};

/* Perl_do_sv_dump():
 *
 * level:   amount to indent the output
 * sv:      the object to dump
 * nest:    the current level of recursion
 * maxnest: the maximum allowed level of recursion
 * dumpops: if true, also dump the ops associated with a CV
 * pvlim:   limit on the length of any strings that are output
 * */

void
Perl_do_sv_dump(pTHX_ I32 level, PerlIO *file, SV *sv, I32 nest, I32 maxnest, bool dumpops, STRLEN pvlim)
{
    SV *d;
    const char *s;
    U32 flags;
    U32 type;

    PERL_ARGS_ASSERT_DO_SV_DUMP;

    if (!sv) {
	Perl_dump_indent(aTHX_ level, file, "SV = 0\n");
	return;
    }

    flags = SvFLAGS(sv);
    type = SvTYPE(sv);

    /* process general SV flags */

    d = Perl_newSVpvf(aTHX_
		   "(0x%"UVxf") at 0x%"UVxf"\n%*s  REFCNT = %"IVdf"\n%*s  FLAGS = (",
		   PTR2UV(SvANY(sv)), PTR2UV(sv),
		   (int)(PL_dumpindent*level), "", (IV)SvREFCNT(sv),
		   (int)(PL_dumpindent*level), "");

    if ((flags & SVs_PADSTALE))
	    sv_catpv(d, "PADSTALE,");
    if ((flags & SVs_PADTMP))
	    sv_catpv(d, "PADTMP,");
    append_flags(d, flags, first_sv_flags_names);
    if (flags & SVf_ROK)  {	
    				sv_catpv(d, "ROK,");
	if (SvWEAKREF(sv))	sv_catpv(d, "WEAKREF,");
    }
    if (flags & SVf_IsCOW && type != SVt_PVHV) sv_catpvs(d, "IsCOW,");
    append_flags(d, flags, second_sv_flags_names);
    if (flags & SVp_SCREAM && type != SVt_PVHV && !isGV_with_GP(sv)
			   && type != SVt_PVAV) {
	if (SvPCS_IMPORTED(sv))
				sv_catpv(d, "PCS_IMPORTED,");
	else
				sv_catpv(d, "SCREAM,");
    }

    /* process type-specific SV flags */

    switch (type) {
    case SVt_PVCV:
    case SVt_PVFM:
	append_flags(d, CvFLAGS(sv), cv_flags_names);
	break;
    case SVt_PVHV:
	append_flags(d, flags, hv_flags_names);
	break;
    case SVt_PVGV:
    case SVt_PVLV:
	if (isGV_with_GP(sv)) {
	    append_flags(d, GvFLAGS(sv), gp_flags_names);
	}
	if (isGV_with_GP(sv) && GvIMPORTED(sv)) {
	    sv_catpv(d, "IMPORT");
	    if (GvIMPORTED(sv) == GVf_IMPORTED)
		sv_catpv(d, "ALL,");
	    else {
		sv_catpv(d, "(");
		append_flags(d, GvFLAGS(sv), gp_flags_imported_names);
		sv_catpv(d, " ),");
	    }
	}
	/* FALLTHROUGH */
    default:
    evaled_or_uv:
	if (SvEVALED(sv))	sv_catpv(d, "EVALED,");
	if (SvIsUV(sv) && !(flags & SVf_ROK))	sv_catpv(d, "IsUV,");
	break;
    case SVt_PVMG:
	if (SvTAIL(sv))		sv_catpv(d, "TAIL,");
	if (SvVALID(sv))	sv_catpv(d, "VALID,");
	/* FALLTHROUGH */
	goto evaled_or_uv;
    case SVt_PVAV:
	break;
    }
    /* SVphv_SHAREKEYS is also 0x20000000 */
    if ((type != SVt_PVHV) && SvUTF8(sv))
        sv_catpv(d, "UTF8");

    if (*(SvEND(d) - 1) == ',') {
        SvCUR_set(d, SvCUR(d) - 1);
	SvPVX(d)[SvCUR(d)] = '\0';
    }
    sv_catpv(d, ")");
    s = SvPVX_const(d);

    /* dump initial SV details */

#ifdef DEBUG_LEAKING_SCALARS
    Perl_dump_indent(aTHX_ level, file,
	"ALLOCATED at %s:%d %s %s (parent 0x%"UVxf"); serial %"UVuf"\n",
	sv->sv_debug_file ? sv->sv_debug_file : "(unknown)",
	sv->sv_debug_line,
	sv->sv_debug_inpad ? "for" : "by",
	sv->sv_debug_optype ? PL_op_name[sv->sv_debug_optype]: "(none)",
	PTR2UV(sv->sv_debug_parent),
	sv->sv_debug_serial
    );
#endif
    Perl_dump_indent(aTHX_ level, file, "SV = ");

    /* Dump SV type */

    if (type < SVt_LAST) {
	PerlIO_printf(file, "%s%s\n", svtypenames[type], s);

	if (type ==  SVt_NULL) {
	    SvREFCNT_dec_NN(d);
	    return;
	}
    } else {
	PerlIO_printf(file, "UNKNOWN(0x%"UVxf") %s\n", (UV)type, s);
	SvREFCNT_dec_NN(d);
	return;
    }

    /* Dump general SV fields */

    if ((type >= SVt_PVIV && type != SVt_PVAV && type != SVt_PVHV
	 && type != SVt_PVCV && type != SVt_PVFM && type != SVt_PVIO
	 && type != SVt_REGEXP && !isGV_with_GP(sv) && !SvVALID(sv))
	|| (type == SVt_IV && !SvROK(sv))) {
	if (SvIsUV(sv)
	                             )
	    Perl_dump_indent(aTHX_ level, file, "  UV = %"UVuf, (UV)SvUVX(sv));
	else
	    Perl_dump_indent(aTHX_ level, file, "  IV = %"IVdf, (IV)SvIVX(sv));
	(void)PerlIO_putc(file, '\n');
    }

    if ((type >= SVt_PVNV && type != SVt_PVAV && type != SVt_PVHV
		&& type != SVt_PVCV && type != SVt_PVFM  && type != SVt_REGEXP
		&& type != SVt_PVIO && !isGV_with_GP(sv) && !SvVALID(sv))
	       || type == SVt_NV) {
	STORE_LC_NUMERIC_UNDERLYING_SET_STANDARD();
	Perl_dump_indent(aTHX_ level, file, "  NV = %.*" NVgf "\n", NV_DIG, SvNVX(sv));
	RESTORE_LC_NUMERIC_UNDERLYING();
    }

    if (SvROK(sv)) {
	Perl_dump_indent(aTHX_ level, file, "  RV = 0x%"UVxf"\n", PTR2UV(SvRV(sv)));
	if (nest < maxnest)
	    do_sv_dump(level+1, file, SvRV(sv), nest+1, maxnest, dumpops, pvlim);
    }

    if (type < SVt_PV) {
	SvREFCNT_dec_NN(d);
	return;
    }

    if ((type <= SVt_PVLV && !isGV_with_GP(sv))
     || (type == SVt_PVIO && IoFLAGS(sv) & IOf_FAKE_DIRP)) {
	const bool re = isREGEXP(sv);
	const char * const ptr =
	    re ? RX_WRAPPED((REGEXP*)sv) : SvPVX_const(sv);
	if (ptr) {
	    STRLEN delta;
	    if (SvOOK(sv)) {
		SvOOK_offset(sv, delta);
		Perl_dump_indent(aTHX_ level, file,"  OFFSET = %"UVuf"\n",
				 (UV) delta);
	    } else {
		delta = 0;
	    }
	    Perl_dump_indent(aTHX_ level, file,"  PV = 0x%"UVxf" ", PTR2UV(ptr));
	    if (SvOOK(sv)) {
		PerlIO_printf(file, "( %s . ) ",
			      pv_display(d, ptr - delta, delta, 0,
					 pvlim));
	    }
            if (type == SVt_INVLIST) {
		PerlIO_printf(file, "\n");
                /* 4 blanks indents 2 beyond the PV, etc */
                _invlist_dump(file, level, "    ", sv);
            }
            else {
                PerlIO_printf(file, "%s", pv_display(d, ptr, SvCUR(sv),
                                                     re ? 0 : SvLEN(sv),
                                                     pvlim));
                if (SvUTF8(sv)) /* the 6?  \x{....} */
                    PerlIO_printf(file, " [UTF8 \"%s\"]",
                                         sv_uni_display(d, sv, 6 * SvCUR(sv),
                                                        UNI_DISPLAY_QQ));
                PerlIO_printf(file, "\n");
            }
	    Perl_dump_indent(aTHX_ level, file, "  CUR = %"IVdf"\n", (IV)SvCUR(sv));
	    if (!re)
		Perl_dump_indent(aTHX_ level, file, "  LEN = %"IVdf"\n",
				       (IV)SvLEN(sv));
#ifdef PERL_COPY_ON_WRITE
	    if (SvIsCOW(sv) && SvLEN(sv))
		Perl_dump_indent(aTHX_ level, file, "  COW_REFCNT = %d\n",
				       CowREFCNT(sv));
#endif
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "  PV = 0\n");
    }

    if (type >= SVt_PVMG) {
	if (SvMAGIC(sv))
		do_magic_dump(level, file, SvMAGIC(sv), nest+1, maxnest, dumpops, pvlim);
	if (SvSTASH(sv))
	    do_hv_dump(level, file, "  STASH", SvSTASH(sv));

	if ((type == SVt_PVMG || type == SVt_PVLV) && SvVALID(sv)) {
	    Perl_dump_indent(aTHX_ level, file, "  USEFUL = %"IVdf"\n", (IV)BmUSEFUL(sv));
	}
    }

    /* Dump type-specific SV fields */

    switch (type) {
    case SVt_PVAV:
	Perl_dump_indent(aTHX_ level, file, "  ARRAY = 0x%"UVxf, PTR2UV(AvARRAY(sv)));
	if (AvARRAY(sv) != AvALLOC(sv)) {
	    PerlIO_printf(file, " (offset=%"IVdf")\n", (IV)(AvARRAY(sv) - AvALLOC(sv)));
	    Perl_dump_indent(aTHX_ level, file, "  ALLOC = 0x%"UVxf"\n", PTR2UV(AvALLOC(sv)));
	}
	else
            (void)PerlIO_putc(file, '\n');
	Perl_dump_indent(aTHX_ level, file, "  FILL = %"IVdf"\n", (IV)AvFILLp(sv));
	Perl_dump_indent(aTHX_ level, file, "  MAX = %"IVdf"\n", (IV)AvMAX(sv));
	Perl_dump_indent(aTHX_ level, file, "  ARYLEN = 0x%"UVxf"\n",
				   SvMAGIC(sv) ? PTR2UV(AvARYLEN(sv)) : 0);
	sv_setpvs(d, "");
	if (AvREAL(sv))	sv_catpv(d, ",REAL");
	if (AvREIFY(sv))	sv_catpv(d, ",REIFY");
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = (%s)\n",
			 SvCUR(d) ? SvPVX_const(d) + 1 : "");
	if (nest < maxnest && av_tindex(MUTABLE_AV(sv)) >= 0) {
	    SSize_t count;
	    for (count = 0; count <=  av_tindex(MUTABLE_AV(sv)) && count < maxnest; count++) {
		SV** const elt = av_fetch(MUTABLE_AV(sv),count,0);

		Perl_dump_indent(aTHX_ level + 1, file, "Elt No. %"IVdf"\n", (IV)count);
		if (elt)
		    do_sv_dump(level+1, file, *elt, nest+1, maxnest, dumpops, pvlim);
	    }
	}
	break;
    case SVt_PVHV: {
	U32 usedkeys;
        if (SvOOK(sv)) {
            struct xpvhv_aux *const aux = HvAUX(sv);
            Perl_dump_indent(aTHX_ level, file, "  AUX_FLAGS = %"UVuf"\n",
                             (UV)aux->xhv_aux_flags);
        }
	Perl_dump_indent(aTHX_ level, file, "  ARRAY = 0x%"UVxf, PTR2UV(HvARRAY(sv)));
	usedkeys = HvUSEDKEYS(sv);
	if (HvARRAY(sv) && usedkeys) {
	    /* Show distribution of HEs in the ARRAY */
	    int freq[200];
#define FREQ_MAX ((int)(C_ARRAY_LENGTH(freq) - 1))
	    int i;
	    int max = 0;
	    U32 pow2 = 2, keys = usedkeys;
	    NV theoret, sum = 0;

	    PerlIO_printf(file, "  (");
	    Zero(freq, FREQ_MAX + 1, int);
	    for (i = 0; (STRLEN)i <= HvMAX(sv); i++) {
		HE* h;
		int count = 0;
                for (h = HvARRAY(sv)[i]; h; h = HeNEXT(h))
		    count++;
		if (count > FREQ_MAX)
		    count = FREQ_MAX;
	        freq[count]++;
	        if (max < count)
		    max = count;
	    }
	    for (i = 0; i <= max; i++) {
		if (freq[i]) {
		    PerlIO_printf(file, "%d%s:%d", i,
				  (i == FREQ_MAX) ? "+" : "",
				  freq[i]);
		    if (i != max)
			PerlIO_printf(file, ", ");
		}
            }
	    (void)PerlIO_putc(file, ')');
	    /* The "quality" of a hash is defined as the total number of
	       comparisons needed to access every element once, relative
	       to the expected number needed for a random hash.

	       The total number of comparisons is equal to the sum of
	       the squares of the number of entries in each bucket.
	       For a random hash of n keys into k buckets, the expected
	       value is
				n + n(n-1)/2k
	    */

	    for (i = max; i > 0; i--) { /* Precision: count down. */
		sum += freq[i] * i * i;
            }
	    while ((keys = keys >> 1))
		pow2 = pow2 << 1;
	    theoret = usedkeys;
	    theoret += theoret * (theoret-1)/pow2;
	    (void)PerlIO_putc(file, '\n');
	    Perl_dump_indent(aTHX_ level, file, "  hash quality = %.1"NVff"%%", theoret/sum*100);
	}
	(void)PerlIO_putc(file, '\n');
	Perl_dump_indent(aTHX_ level, file, "  KEYS = %"IVdf"\n", (IV)usedkeys);
        {
            STRLEN count = 0;
            HE **ents = HvARRAY(sv);

            if (ents) {
                HE *const *const last = ents + HvMAX(sv);
                count = last + 1 - ents;
                
                do {
                    if (!*ents)
                        --count;
                } while (++ents <= last);
            }

            if (SvOOK(sv)) {
                struct xpvhv_aux *const aux = HvAUX(sv);
                Perl_dump_indent(aTHX_ level, file, "  FILL = %"UVuf
                                 " (cached = %"UVuf")\n",
                                 (UV)count, (UV)aux->xhv_fill_lazy);
            } else {
                Perl_dump_indent(aTHX_ level, file, "  FILL = %"UVuf"\n",
                                 (UV)count);
            }
        }
	Perl_dump_indent(aTHX_ level, file, "  MAX = %"IVdf"\n", (IV)HvMAX(sv));
        if (SvOOK(sv)) {
	    Perl_dump_indent(aTHX_ level, file, "  RITER = %"IVdf"\n", (IV)HvRITER_get(sv));
	    Perl_dump_indent(aTHX_ level, file, "  EITER = 0x%"UVxf"\n", PTR2UV(HvEITER_get(sv)));
#ifdef PERL_HASH_RANDOMIZE_KEYS
	    Perl_dump_indent(aTHX_ level, file, "  RAND = 0x%"UVxf, (UV)HvRAND_get(sv));
            if (HvRAND_get(sv) != HvLASTRAND_get(sv) && HvRITER_get(sv) != -1 ) {
                PerlIO_printf(file, " (LAST = 0x%"UVxf")", (UV)HvLASTRAND_get(sv));
            }
#endif
            (void)PerlIO_putc(file, '\n');
        }
	{
	    MAGIC * const mg = mg_find(sv, PERL_MAGIC_symtab);
	    if (mg && mg->mg_obj) {
		Perl_dump_indent(aTHX_ level, file, "  PMROOT = 0x%"UVxf"\n", PTR2UV(mg->mg_obj));
	    }
	}
	{
	    const char * const hvname = HvNAME_get(sv);
	    if (hvname) {
          SV* tmpsv = newSVpvs_flags("", SVs_TEMP);
     Perl_dump_indent(aTHX_ level, file, "  NAME = \"%s\"\n",
                                       generic_pv_escape( tmpsv, hvname,
                                           HvNAMELEN(sv), HvNAMEUTF8(sv)));
        }
	}
	if (SvOOK(sv)) {
	    AV * const backrefs
		= *Perl_hv_backreferences_p(aTHX_ MUTABLE_HV(sv));
	    struct mro_meta * const meta = HvAUX(sv)->xhv_mro_meta;
	    if (HvAUX(sv)->xhv_name_count)
		Perl_dump_indent(aTHX_
		 level, file, "  NAMECOUNT = %"IVdf"\n",
		 (IV)HvAUX(sv)->xhv_name_count
		);
	    if (HvAUX(sv)->xhv_name_u.xhvnameu_name && HvENAME_HEK_NN(sv)) {
		const I32 count = HvAUX(sv)->xhv_name_count;
		if (count) {
		    SV * const names = newSVpvs_flags("", SVs_TEMP);
		    /* The starting point is the first element if count is
		       positive and the second element if count is negative. */
		    HEK *const *hekp = HvAUX(sv)->xhv_name_u.xhvnameu_names
			+ (count < 0 ? 1 : 0);
		    HEK *const *const endp = HvAUX(sv)->xhv_name_u.xhvnameu_names
			+ (count < 0 ? -count : count);
		    while (hekp < endp) {
			if (HEK_LEN(*hekp)) {
             SV *tmp = newSVpvs_flags("", SVs_TEMP);
			    Perl_sv_catpvf(aTHX_ names, ", \"%s\"",
                              generic_pv_escape(tmp, HEK_KEY(*hekp), HEK_LEN(*hekp), HEK_UTF8(*hekp)));
			} else {
			    /* This should never happen. */
			    sv_catpvs(names, ", (null)");
			}
			++hekp;
		    }
		    Perl_dump_indent(aTHX_
		     level, file, "  ENAME = %s\n", SvPV_nolen(names)+2
		    );
		}
		else {
                    SV * const tmp = newSVpvs_flags("", SVs_TEMP);
                    const char *const hvename = HvENAME_get(sv);
		    Perl_dump_indent(aTHX_
		     level, file, "  ENAME = \"%s\"\n",
                     generic_pv_escape(tmp, hvename,
                                       HvENAMELEN_get(sv), HvENAMEUTF8(sv)));
                }
	    }
	    if (backrefs) {
		Perl_dump_indent(aTHX_ level, file, "  BACKREFS = 0x%"UVxf"\n",
				 PTR2UV(backrefs));
		do_sv_dump(level+1, file, MUTABLE_SV(backrefs), nest+1, maxnest,
			   dumpops, pvlim);
	    }
	    if (meta) {
		SV* tmpsv = newSVpvs_flags("", SVs_TEMP);
		Perl_dump_indent(aTHX_ level, file, "  MRO_WHICH = \"%s\" (0x%"UVxf")\n",
				 generic_pv_escape( tmpsv, meta->mro_which->name,
                                meta->mro_which->length,
                                (meta->mro_which->kflags & HVhek_UTF8)),
				 PTR2UV(meta->mro_which));
		Perl_dump_indent(aTHX_ level, file, "  CACHE_GEN = 0x%"UVxf"\n",
				 (UV)meta->cache_gen);
		Perl_dump_indent(aTHX_ level, file, "  PKG_GEN = 0x%"UVxf"\n",
				 (UV)meta->pkg_gen);
		if (meta->mro_linear_all) {
		    Perl_dump_indent(aTHX_ level, file, "  MRO_LINEAR_ALL = 0x%"UVxf"\n",
				 PTR2UV(meta->mro_linear_all));
		do_sv_dump(level+1, file, MUTABLE_SV(meta->mro_linear_all), nest+1, maxnest,
			   dumpops, pvlim);
		}
		if (meta->mro_linear_current) {
		    Perl_dump_indent(aTHX_ level, file, "  MRO_LINEAR_CURRENT = 0x%"UVxf"\n",
				 PTR2UV(meta->mro_linear_current));
		do_sv_dump(level+1, file, MUTABLE_SV(meta->mro_linear_current), nest+1, maxnest,
			   dumpops, pvlim);
		}
		if (meta->mro_nextmethod) {
		    Perl_dump_indent(aTHX_ level, file, "  MRO_NEXTMETHOD = 0x%"UVxf"\n",
				 PTR2UV(meta->mro_nextmethod));
		do_sv_dump(level+1, file, MUTABLE_SV(meta->mro_nextmethod), nest+1, maxnest,
			   dumpops, pvlim);
		}
		if (meta->isa) {
		    Perl_dump_indent(aTHX_ level, file, "  ISA = 0x%"UVxf"\n",
				 PTR2UV(meta->isa));
		do_sv_dump(level+1, file, MUTABLE_SV(meta->isa), nest+1, maxnest,
			   dumpops, pvlim);
		}
	    }
	}
	if (nest < maxnest) {
	    HV * const hv = MUTABLE_HV(sv);
	    STRLEN i;
	    HE *he;

	    if (HvARRAY(hv)) {
		int count = maxnest - nest;
		for (i=0; i <= HvMAX(hv); i++) {
		    for (he = HvARRAY(hv)[i]; he; he = HeNEXT(he)) {
			U32 hash;
			SV * keysv;
			const char * keypv;
			SV * elt;
                        STRLEN len;

			if (count-- <= 0) goto DONEHV;

			hash = HeHASH(he);
			keysv = hv_iterkeysv(he);
			keypv = SvPV_const(keysv, len);
			elt = HeVAL(he);

                        Perl_dump_indent(aTHX_ level+1, file, "Elt %s ", pv_display(d, keypv, len, 0, pvlim));
                        if (SvUTF8(keysv))
                            PerlIO_printf(file, "[UTF8 \"%s\"] ", sv_uni_display(d, keysv, 6 * SvCUR(keysv), UNI_DISPLAY_QQ));
			if (HvEITER_get(hv) == he)
			    PerlIO_printf(file, "[CURRENT] ");
                        PerlIO_printf(file, "HASH = 0x%"UVxf"\n", (UV) hash);
                        do_sv_dump(level+1, file, elt, nest+1, maxnest, dumpops, pvlim);
                    }
		}
	      DONEHV:;
	    }
	}
	break;
    } /* case SVt_PVHV */

    case SVt_PVCV:
	if (CvAUTOLOAD(sv)) {
	    SV* tmpsv = newSVpvs_flags("", SVs_TEMP);
       STRLEN len;
	    const char *const name =  SvPV_const(sv, len);
	    Perl_dump_indent(aTHX_ level, file, "  AUTOLOAD = \"%s\"\n",
			     generic_pv_escape(tmpsv, name, len, SvUTF8(sv)));
	}
	if (SvPOK(sv)) {
       SV* tmpsv = newSVpvs_flags("", SVs_TEMP);
       const char *const proto = CvPROTO(sv);
	    Perl_dump_indent(aTHX_ level, file, "  PROTOTYPE = \"%s\"\n",
			     generic_pv_escape(tmpsv, proto, CvPROTOLEN(sv),
                                SvUTF8(sv)));
	}
	/* FALLTHROUGH */
    case SVt_PVFM:
	do_hv_dump(level, file, "  COMP_STASH", CvSTASH(sv));
	if (!CvISXSUB(sv)) {
	    if (CvSTART(sv)) {
		Perl_dump_indent(aTHX_ level, file,
				 "  START = 0x%"UVxf" ===> %"IVdf"\n",
				 PTR2UV(CvSTART(sv)),
				 (IV)sequence_num(CvSTART(sv)));
	    }
	    Perl_dump_indent(aTHX_ level, file, "  ROOT = 0x%"UVxf"\n",
			     PTR2UV(CvROOT(sv)));
	    if (CvROOT(sv) && dumpops) {
		do_op_dump(level+1, file, CvROOT(sv));
	    }
	} else {
	    SV * const constant = cv_const_sv((const CV *)sv);

	    Perl_dump_indent(aTHX_ level, file, "  XSUB = 0x%"UVxf"\n", PTR2UV(CvXSUB(sv)));

	    if (constant) {
		Perl_dump_indent(aTHX_ level, file, "  XSUBANY = 0x%"UVxf
				 " (CONST SV)\n",
				 PTR2UV(CvXSUBANY(sv).any_ptr));
		do_sv_dump(level+1, file, constant, nest+1, maxnest, dumpops,
			   pvlim);
	    } else {
		Perl_dump_indent(aTHX_ level, file, "  XSUBANY = %"IVdf"\n",
				 (IV)CvXSUBANY(sv).any_i32);
	    }
	}
	if (CvNAMED(sv))
	    Perl_dump_indent(aTHX_ level, file, "  NAME = \"%s\"\n",
				   HEK_KEY(CvNAME_HEK((CV *)sv)));
	else do_gvgv_dump(level, file, "  GVGV::GV", CvGV(sv));
	Perl_dump_indent(aTHX_ level, file, "  FILE = \"%s\"\n", CvFILE(sv));
	Perl_dump_indent(aTHX_ level, file, "  DEPTH = %"IVdf"\n", (IV)CvDEPTH(sv));
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = 0x%"UVxf"\n", (UV)CvFLAGS(sv));
	Perl_dump_indent(aTHX_ level, file, "  OUTSIDE_SEQ = %"UVuf"\n", (UV)CvOUTSIDE_SEQ(sv));
	if (!CvISXSUB(sv)) {
	    Perl_dump_indent(aTHX_ level, file, "  PADLIST = 0x%"UVxf"\n", PTR2UV(CvPADLIST(sv)));
	    if (nest < maxnest) {
		do_dump_pad(level+1, file, CvPADLIST(sv), 0);
	    }
	}
	else
	    Perl_dump_indent(aTHX_ level, file, "  HSCXT = 0x%p\n", CvHSCXT(sv));
	{
	    const CV * const outside = CvOUTSIDE(sv);
	    Perl_dump_indent(aTHX_ level, file, "  OUTSIDE = 0x%"UVxf" (%s)\n",
			PTR2UV(outside),
			(!outside ? "null"
			 : CvANON(outside) ? "ANON"
			 : (outside == PL_main_cv) ? "MAIN"
			 : CvUNIQUE(outside) ? "UNIQUE"
			 : CvGV(outside) ?
			     generic_pv_escape(
			         newSVpvs_flags("", SVs_TEMP),
			         GvNAME(CvGV(outside)),
			         GvNAMELEN(CvGV(outside)),
			         GvNAMEUTF8(CvGV(outside)))
			 : "UNDEFINED"));
	}
	if (CvOUTSIDE(sv)
	 && (nest < maxnest && (CvCLONE(sv) || CvCLONED(sv))))
	    do_sv_dump(level+1, file, MUTABLE_SV(CvOUTSIDE(sv)), nest+1, maxnest, dumpops, pvlim);
	break;

    case SVt_PVGV:
    case SVt_PVLV:
	if (type == SVt_PVLV) {
	    Perl_dump_indent(aTHX_ level, file, "  TYPE = %c\n", LvTYPE(sv));
	    Perl_dump_indent(aTHX_ level, file, "  TARGOFF = %"IVdf"\n", (IV)LvTARGOFF(sv));
	    Perl_dump_indent(aTHX_ level, file, "  TARGLEN = %"IVdf"\n", (IV)LvTARGLEN(sv));
	    Perl_dump_indent(aTHX_ level, file, "  TARG = 0x%"UVxf"\n", PTR2UV(LvTARG(sv)));
	    Perl_dump_indent(aTHX_ level, file, "  FLAGS = %"IVdf"\n", (IV)LvFLAGS(sv));
	    if (isALPHA_FOLD_NE(LvTYPE(sv), 't'))
		do_sv_dump(level+1, file, LvTARG(sv), nest+1, maxnest,
		    dumpops, pvlim);
	}
	if (isREGEXP(sv)) goto dumpregexp;
	if (!isGV_with_GP(sv))
	    break;
       {
          SV* tmpsv = newSVpvs_flags("", SVs_TEMP);
          Perl_dump_indent(aTHX_ level, file, "  NAME = \"%s\"\n",
                    generic_pv_escape(tmpsv, GvNAME(sv),
                                      GvNAMELEN(sv),
                                      GvNAMEUTF8(sv)));
       }
	Perl_dump_indent(aTHX_ level, file, "  NAMELEN = %"IVdf"\n", (IV)GvNAMELEN(sv));
	do_hv_dump (level, file, "  GvSTASH", GvSTASH(sv));
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = 0x%"UVxf"\n", (UV)GvFLAGS(sv));
	Perl_dump_indent(aTHX_ level, file, "  GP = 0x%"UVxf"\n", PTR2UV(GvGP(sv)));
	if (!GvGP(sv))
	    break;
	Perl_dump_indent(aTHX_ level, file, "    SV = 0x%"UVxf"\n", PTR2UV(GvSV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    REFCNT = %"IVdf"\n", (IV)GvREFCNT(sv));
	Perl_dump_indent(aTHX_ level, file, "    IO = 0x%"UVxf"\n", PTR2UV(GvIOp(sv)));
	Perl_dump_indent(aTHX_ level, file, "    FORM = 0x%"UVxf"  \n", PTR2UV(GvFORM(sv)));
	Perl_dump_indent(aTHX_ level, file, "    AV = 0x%"UVxf"\n", PTR2UV(GvAV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    HV = 0x%"UVxf"\n", PTR2UV(GvHV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    CV = 0x%"UVxf"\n", PTR2UV(GvCV(sv)));
	Perl_dump_indent(aTHX_ level, file, "    CVGEN = 0x%"UVxf"\n", (UV)GvCVGEN(sv));
	Perl_dump_indent(aTHX_ level, file, "    GPFLAGS = 0x%"UVxf
					    " (%s)\n",
			       (UV)GvGPFLAGS(sv),
			       "");
	Perl_dump_indent(aTHX_ level, file, "    LINE = %"IVdf"\n", (IV)GvLINE(sv));
	Perl_dump_indent(aTHX_ level, file, "    FILE = \"%s\"\n", GvFILE(sv));
	do_gv_dump (level, file, "    EGV", GvEGV(sv));
	break;
    case SVt_PVIO:
	Perl_dump_indent(aTHX_ level, file, "  IFP = 0x%"UVxf"\n", PTR2UV(IoIFP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  OFP = 0x%"UVxf"\n", PTR2UV(IoOFP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  DIRP = 0x%"UVxf"\n", PTR2UV(IoDIRP(sv)));
	Perl_dump_indent(aTHX_ level, file, "  LINES = %"IVdf"\n", (IV)IoLINES(sv));
	Perl_dump_indent(aTHX_ level, file, "  PAGE = %"IVdf"\n", (IV)IoPAGE(sv));
	Perl_dump_indent(aTHX_ level, file, "  PAGE_LEN = %"IVdf"\n", (IV)IoPAGE_LEN(sv));
	Perl_dump_indent(aTHX_ level, file, "  LINES_LEFT = %"IVdf"\n", (IV)IoLINES_LEFT(sv));
        if (IoTOP_NAME(sv))
            Perl_dump_indent(aTHX_ level, file, "  TOP_NAME = \"%s\"\n", IoTOP_NAME(sv));
	if (!IoTOP_GV(sv) || SvTYPE(IoTOP_GV(sv)) == SVt_PVGV)
	    do_gv_dump (level, file, "  TOP_GV", IoTOP_GV(sv));
	else {
	    Perl_dump_indent(aTHX_ level, file, "  TOP_GV = 0x%"UVxf"\n",
			     PTR2UV(IoTOP_GV(sv)));
	    do_sv_dump (level+1, file, MUTABLE_SV(IoTOP_GV(sv)), nest+1,
			maxnest, dumpops, pvlim);
	}
	/* Source filters hide things that are not GVs in these three, so let's
	   be careful out there.  */
        if (IoFMT_NAME(sv))
            Perl_dump_indent(aTHX_ level, file, "  FMT_NAME = \"%s\"\n", IoFMT_NAME(sv));
	if (!IoFMT_GV(sv) || SvTYPE(IoFMT_GV(sv)) == SVt_PVGV)
	    do_gv_dump (level, file, "  FMT_GV", IoFMT_GV(sv));
	else {
	    Perl_dump_indent(aTHX_ level, file, "  FMT_GV = 0x%"UVxf"\n",
			     PTR2UV(IoFMT_GV(sv)));
	    do_sv_dump (level+1, file, MUTABLE_SV(IoFMT_GV(sv)), nest+1,
			maxnest, dumpops, pvlim);
	}
        if (IoBOTTOM_NAME(sv))
            Perl_dump_indent(aTHX_ level, file, "  BOTTOM_NAME = \"%s\"\n", IoBOTTOM_NAME(sv));
	if (!IoBOTTOM_GV(sv) || SvTYPE(IoBOTTOM_GV(sv)) == SVt_PVGV)
	    do_gv_dump (level, file, "  BOTTOM_GV", IoBOTTOM_GV(sv));
	else {
	    Perl_dump_indent(aTHX_ level, file, "  BOTTOM_GV = 0x%"UVxf"\n",
			     PTR2UV(IoBOTTOM_GV(sv)));
	    do_sv_dump (level+1, file, MUTABLE_SV(IoBOTTOM_GV(sv)), nest+1,
			maxnest, dumpops, pvlim);
	}
	if (isPRINT(IoTYPE(sv)))
            Perl_dump_indent(aTHX_ level, file, "  TYPE = '%c'\n", IoTYPE(sv));
	else
            Perl_dump_indent(aTHX_ level, file, "  TYPE = '\\%o'\n", IoTYPE(sv));
	Perl_dump_indent(aTHX_ level, file, "  FLAGS = 0x%"UVxf"\n", (UV)IoFLAGS(sv));
	break;
    case SVt_REGEXP:
      dumpregexp:
	{
	    struct regexp * const r = ReANY((REGEXP*)sv);

#define SV_SET_STRINGIFY_REGEXP_FLAGS(d,flags,names) STMT_START { \
            sv_setpv(d,"");                                 \
            append_flags(d, flags, names);     \
            if (SvCUR(d) > 0 && *(SvEND(d) - 1) == ',') {       \
                SvCUR_set(d, SvCUR(d) - 1);                 \
                SvPVX(d)[SvCUR(d)] = '\0';                  \
            }                                               \
} STMT_END
            SV_SET_STRINGIFY_REGEXP_FLAGS(d,r->compflags,regexp_extflags_names);
            Perl_dump_indent(aTHX_ level, file, "  COMPFLAGS = 0x%"UVxf" (%s)\n",
                                (UV)(r->compflags), SvPVX_const(d));

            SV_SET_STRINGIFY_REGEXP_FLAGS(d,r->extflags,regexp_extflags_names);
	    Perl_dump_indent(aTHX_ level, file, "  EXTFLAGS = 0x%"UVxf" (%s)\n",
                                (UV)(r->extflags), SvPVX_const(d));

            Perl_dump_indent(aTHX_ level, file, "  ENGINE = 0x%"UVxf" (%s)\n",
                                PTR2UV(r->engine), (r->engine == &PL_core_reg_engine) ? "STANDARD" : "PLUG-IN" );
            if (r->engine == &PL_core_reg_engine) {
                SV_SET_STRINGIFY_REGEXP_FLAGS(d,r->intflags,regexp_core_intflags_names);
                Perl_dump_indent(aTHX_ level, file, "  INTFLAGS = 0x%"UVxf" (%s)\n",
                                (UV)(r->intflags), SvPVX_const(d));
            } else {
                Perl_dump_indent(aTHX_ level, file, "  INTFLAGS = 0x%"UVxf"\n",
				(UV)(r->intflags));
            }
#undef SV_SET_STRINGIFY_REGEXP_FLAGS
	    Perl_dump_indent(aTHX_ level, file, "  NPARENS = %"UVuf"\n",
				(UV)(r->nparens));
	    Perl_dump_indent(aTHX_ level, file, "  LASTPAREN = %"UVuf"\n",
				(UV)(r->lastparen));
	    Perl_dump_indent(aTHX_ level, file, "  LASTCLOSEPAREN = %"UVuf"\n",
				(UV)(r->lastcloseparen));
	    Perl_dump_indent(aTHX_ level, file, "  MINLEN = %"IVdf"\n",
				(IV)(r->minlen));
	    Perl_dump_indent(aTHX_ level, file, "  MINLENRET = %"IVdf"\n",
				(IV)(r->minlenret));
	    Perl_dump_indent(aTHX_ level, file, "  GOFS = %"UVuf"\n",
				(UV)(r->gofs));
	    Perl_dump_indent(aTHX_ level, file, "  PRE_PREFIX = %"UVuf"\n",
				(UV)(r->pre_prefix));
	    Perl_dump_indent(aTHX_ level, file, "  SUBLEN = %"IVdf"\n",
				(IV)(r->sublen));
	    Perl_dump_indent(aTHX_ level, file, "  SUBOFFSET = %"IVdf"\n",
				(IV)(r->suboffset));
	    Perl_dump_indent(aTHX_ level, file, "  SUBCOFFSET = %"IVdf"\n",
				(IV)(r->subcoffset));
	    if (r->subbeg)
		Perl_dump_indent(aTHX_ level, file, "  SUBBEG = 0x%"UVxf" %s\n",
			    PTR2UV(r->subbeg),
			    pv_display(d, r->subbeg, r->sublen, 50, pvlim));
	    else
		Perl_dump_indent(aTHX_ level, file, "  SUBBEG = 0x0\n");
	    Perl_dump_indent(aTHX_ level, file, "  MOTHER_RE = 0x%"UVxf"\n",
				PTR2UV(r->mother_re));
	    if (nest < maxnest && r->mother_re)
		do_sv_dump(level+1, file, (SV *)r->mother_re, nest+1,
			   maxnest, dumpops, pvlim);
	    Perl_dump_indent(aTHX_ level, file, "  PAREN_NAMES = 0x%"UVxf"\n",
				PTR2UV(r->paren_names));
	    Perl_dump_indent(aTHX_ level, file, "  SUBSTRS = 0x%"UVxf"\n",
				PTR2UV(r->substrs));
	    Perl_dump_indent(aTHX_ level, file, "  PPRIVATE = 0x%"UVxf"\n",
				PTR2UV(r->pprivate));
	    Perl_dump_indent(aTHX_ level, file, "  OFFS = 0x%"UVxf"\n",
				PTR2UV(r->offs));
	    Perl_dump_indent(aTHX_ level, file, "  QR_ANONCV = 0x%"UVxf"\n",
				PTR2UV(r->qr_anoncv));
#ifdef PERL_ANY_COW
	    Perl_dump_indent(aTHX_ level, file, "  SAVED_COPY = 0x%"UVxf"\n",
				PTR2UV(r->saved_copy));
#endif
	}
	break;
    }
    SvREFCNT_dec_NN(d);
}

/*
=for apidoc sv_dump

Dumps the contents of an SV to the C<STDERR> filehandle.

For an example of its output, see L<Devel::Peek>.

=cut
*/

void
Perl_sv_dump(pTHX_ SV *sv)
{
    PERL_ARGS_ASSERT_SV_DUMP;

    if (SvROK(sv))
	do_sv_dump(0, Perl_debug_log, sv, 0, 4, 0, 0);
    else
	do_sv_dump(0, Perl_debug_log, sv, 0, 0, 0, 0);
}

int
Perl_runops_debug(pTHX)
{
    if (!PL_op) {
	Perl_ck_warner_d(aTHX_ packWARN(WARN_DEBUGGING), "NULL OP IN RUN");
	return 0;
    }

    DEBUG_l(Perl_deb(aTHX_ "Entering new RUNOPS level\n"));
    do {
#ifdef PERL_TRACE_OPS
        ++PL_op_exec_cnt[PL_op->op_type];
#endif
	if (PL_debug) {
            ENTER;
            SAVETMPS;
	    if (PL_watchaddr && (*PL_watchaddr != PL_watchok))
		PerlIO_printf(Perl_debug_log,
			      "WARNING: %"UVxf" changed from %"UVxf" to %"UVxf"\n",
			      PTR2UV(PL_watchaddr), PTR2UV(PL_watchok),
			      PTR2UV(*PL_watchaddr));
	    if (DEBUG_s_TEST_) {
		if (DEBUG_v_TEST_) {
		    PerlIO_printf(Perl_debug_log, "\n");
		    deb_stack_all();
		}
		else
		    debstack();
	    }


	    if (DEBUG_t_TEST_) debop(PL_op);
	    if (DEBUG_P_TEST_) debprof(PL_op);
            FREETMPS;
            LEAVE;
	}

        PERL_DTRACE_PROBE_OP(PL_op);
    } while ((PL_op = PL_op->op_ppaddr(aTHX)));
    DEBUG_l(Perl_deb(aTHX_ "leaving RUNOPS level\n"));
    PERL_ASYNC_CHECK();

    TAINT_NOT;
    return 0;
}


/* print the names of the n lexical vars starting at pad offset off */

STATIC void
S_deb_padvar(pTHX_ PADOFFSET off, int n, bool paren)
{
    PADNAME *sv;
    CV * const cv = deb_curcv(cxstack_ix);
    PADNAMELIST *comppad = NULL;
    int i;

    if (cv) {
        PADLIST * const padlist = CvPADLIST(cv);
        comppad = PadlistNAMES(padlist);
    }
    if (paren)
        PerlIO_printf(Perl_debug_log, "(");
    for (i = 0; i < n; i++) {
        if (comppad && (sv = padnamelist_fetch(comppad, off + i)))
            PerlIO_printf(Perl_debug_log, "%"PNf, PNfARG(sv));
        else
            PerlIO_printf(Perl_debug_log, "[%"UVuf"]",
                    (UV)(off+i));
        if (i < n - 1)
            PerlIO_printf(Perl_debug_log, ",");
    }
    if (paren)
        PerlIO_printf(Perl_debug_log, ")");
}


/* append to the out SV, the name of the lexical at offset off in the CV
 * cv */

static void
S_append_padvar(pTHX_ PADOFFSET off, CV *cv, SV *out, int n,
        bool paren, bool is_scalar)
{
    PADNAME *sv;
    PADNAMELIST *namepad = NULL;
    int i;

    if (cv) {
        PADLIST * const padlist = CvPADLIST(cv);
        namepad = PadlistNAMES(padlist);
    }

    if (paren)
        sv_catpvs_nomg(out, "(");
    for (i = 0; i < n; i++) {
        if (namepad && (sv = padnamelist_fetch(namepad, off + i)))
        {
            STRLEN cur = SvCUR(out);
            Perl_sv_catpvf(aTHX_ out, "[%"UTF8f,
                                 UTF8fARG(1, PadnameLEN(sv) - 1,
                                          PadnamePV(sv) + 1));
            if (is_scalar)
                SvPVX(out)[cur] = '$';
        }
        else
            Perl_sv_catpvf(aTHX_ out, "[%"UVuf"]", (UV)(off+i));
        if (i < n - 1)
            sv_catpvs_nomg(out, ",");
    }
    if (paren)
        sv_catpvs_nomg(out, "(");
}


static void
S_append_gv_name(pTHX_ GV *gv, SV *out)
{
    SV *sv;
    if (!gv) {
        sv_catpvs_nomg(out, "<NULLGV>");
        return;
    }
    sv = newSV(0);
    gv_fullname4(sv, gv, NULL, FALSE);
    Perl_sv_catpvf(aTHX_ out, "$%"SVf, SVfARG(sv));
    SvREFCNT_dec_NN(sv);
}

#ifdef USE_ITHREADS
#  define ITEM_SV(item) (comppad ? \
    *av_fetch(comppad, (item)->pad_offset, FALSE) : NULL);
#else
#  define ITEM_SV(item) UNOP_AUX_item_sv(item)
#endif


/* return a temporary SV containing a stringified representation of
 * the op_aux field of a MULTIDEREF op, associated with CV cv
 */

SV*
Perl_multideref_stringify(pTHX_ const OP *o, CV *cv)
{
    UNOP_AUX_item *items = cUNOP_AUXo->op_aux;
    UV actions = items->uv;
    SV *sv;
    bool last = 0;
    bool is_hash = FALSE;
    int derefs = 0;
    SV *out = newSVpvn_flags("",0,SVs_TEMP);
#ifdef USE_ITHREADS
    PAD *comppad;

    if (cv) {
        PADLIST *padlist = CvPADLIST(cv);
        comppad = PadlistARRAY(padlist)[1];
    }
    else
        comppad = NULL;
#endif

    PERL_ARGS_ASSERT_MULTIDEREF_STRINGIFY;

    while (!last) {
        switch (actions & MDEREF_ACTION_MASK) {

        case MDEREF_reload:
            actions = (++items)->uv;
            continue;
            NOT_REACHED; /* NOTREACHED */

        case MDEREF_HV_padhv_helem:
            is_hash = TRUE;
            /* FALLTHROUGH */
        case MDEREF_AV_padav_aelem:
            derefs = 1;
            S_append_padvar(aTHX_ (++items)->pad_offset, cv, out, 1, 0, 1);
            goto do_elem;
            NOT_REACHED; /* NOTREACHED */

        case MDEREF_HV_gvhv_helem:
            is_hash = TRUE;
            /* FALLTHROUGH */
        case MDEREF_AV_gvav_aelem:
            derefs = 1;
            items++;
            sv = ITEM_SV(items);
            S_append_gv_name(aTHX_ (GV*)sv, out);
            goto do_elem;
            NOT_REACHED; /* NOTREACHED */

        case MDEREF_HV_gvsv_vivify_rv2hv_helem:
            is_hash = TRUE;
            /* FALLTHROUGH */
        case MDEREF_AV_gvsv_vivify_rv2av_aelem:
            items++;
            sv = ITEM_SV(items);
            S_append_gv_name(aTHX_ (GV*)sv, out);
            goto do_vivify_rv2xv_elem;
            NOT_REACHED; /* NOTREACHED */

        case MDEREF_HV_padsv_vivify_rv2hv_helem:
            is_hash = TRUE;
            /* FALLTHROUGH */
        case MDEREF_AV_padsv_vivify_rv2av_aelem:
            S_append_padvar(aTHX_ (++items)->pad_offset, cv, out, 1, 0, 1);
            goto do_vivify_rv2xv_elem;
            NOT_REACHED; /* NOTREACHED */

        case MDEREF_HV_pop_rv2hv_helem:
        case MDEREF_HV_vivify_rv2hv_helem:
            is_hash = TRUE;
            /* FALLTHROUGH */
        do_vivify_rv2xv_elem:
        case MDEREF_AV_pop_rv2av_aelem:
        case MDEREF_AV_vivify_rv2av_aelem:
            if (!derefs++)
                sv_catpvs_nomg(out, "->");
        do_elem:
            if ((actions & MDEREF_INDEX_MASK)== MDEREF_INDEX_none) {
                sv_catpvs_nomg(out, "->");
                last = 1;
                break;
            }

            sv_catpvn_nomg(out, (is_hash ? "{" : "["), 1);
            switch (actions & MDEREF_INDEX_MASK) {
            case MDEREF_INDEX_const:
                if (is_hash) {
                    items++;
                    sv = ITEM_SV(items);
                    if (!sv)
                        sv_catpvs_nomg(out, "???");
                    else {
                        STRLEN cur;
                        char *s;
                        s = SvPV(sv, cur);
                        pv_pretty(out, s, cur, 30,
                                    NULL, NULL,
                                    (PERL_PV_PRETTY_NOCLEAR
                                    |PERL_PV_PRETTY_QUOTE
                                    |PERL_PV_PRETTY_ELLIPSES));
                    }
                }
                else
                    Perl_sv_catpvf(aTHX_ out, "%"IVdf, (++items)->iv);
                break;
            case MDEREF_INDEX_padsv:
                S_append_padvar(aTHX_ (++items)->pad_offset, cv, out, 1, 0, 1);
                break;
            case MDEREF_INDEX_gvsv:
                items++;
                sv = ITEM_SV(items);
                S_append_gv_name(aTHX_ (GV*)sv, out);
                break;
            }
            sv_catpvn_nomg(out, (is_hash ? "}" : "]"), 1);

            if (actions & MDEREF_FLAG_last)
                last = 1;
            is_hash = FALSE;

            break;

        default:
            PerlIO_printf(Perl_debug_log, "UNKNOWN(%d)",
                (int)(actions & MDEREF_ACTION_MASK));
            last = 1;
            break;

        } /* switch */

        actions >>= MDEREF_SHIFT;
    } /* while */
    return out;
}


I32
Perl_debop(pTHX_ const OP *o)
{
    PERL_ARGS_ASSERT_DEBOP;

    if (CopSTASH_eq(PL_curcop, PL_debstash) && !DEBUG_J_TEST_)
	return 0;

    Perl_deb(aTHX_ "%s", OP_NAME(o));
    switch (o->op_type) {
    case OP_CONST:
    case OP_HINTSEVAL:
	/* With ITHREADS, consts are stored in the pad, and the right pad
	 * may not be active here, so check.
	 * Looks like only during compiling the pads are illegal.
	 */
#ifdef USE_ITHREADS
	if ((((SVOP*)o)->op_sv) || !IN_PERL_COMPILETIME)
#endif
	    PerlIO_printf(Perl_debug_log, "(%s)", SvPEEK(cSVOPo_sv));
	break;
    case OP_GVSV:
    case OP_GV:
	if (cGVOPo_gv && isGV(cGVOPo_gv)) {
	    SV * const sv = newSV(0);
	    gv_fullname3(sv, cGVOPo_gv, NULL);
	    PerlIO_printf(Perl_debug_log, "(%s)", SvPV_nolen_const(sv));
	    SvREFCNT_dec_NN(sv);
	}
	else if (cGVOPo_gv) {
	    SV * const sv = newSV(0);
	    assert(SvROK(cGVOPo_gv));
	    assert(SvTYPE(SvRV(cGVOPo_gv)) == SVt_PVCV);
	    PerlIO_printf(Perl_debug_log, "(cv ref: %s)",
		    SvPV_nolen_const(cv_name((CV *)SvRV(cGVOPo_gv),sv,0)));
	    SvREFCNT_dec_NN(sv);
	}
	else
	    PerlIO_printf(Perl_debug_log, "(NULL)");
	break;

    case OP_PADSV:
    case OP_PADAV:
    case OP_PADHV:
        S_deb_padvar(aTHX_ o->op_targ, 1, 1);
        break;

    case OP_PADRANGE:
        S_deb_padvar(aTHX_ o->op_targ,
                        o->op_private & OPpPADRANGE_COUNTMASK, 1);
        break;

    case OP_MULTIDEREF:
        PerlIO_printf(Perl_debug_log, "(%"SVf")",
            SVfARG(multideref_stringify(o, deb_curcv(cxstack_ix))));
        break;

    default:
	break;
    }
    PerlIO_printf(Perl_debug_log, "\n");
    return 0;
}

STATIC CV*
S_deb_curcv(pTHX_ I32 ix)
{
    PERL_SI *si = PL_curstackinfo;
    for (; ix >=0; ix--) {
        const PERL_CONTEXT * const cx = &(si->si_cxstack)[ix];

        if (CxTYPE(cx) == CXt_SUB || CxTYPE(cx) == CXt_FORMAT)
            return cx->blk_sub.cv;
        else if (CxTYPE(cx) == CXt_EVAL && !CxTRYBLOCK(cx))
            return cx->blk_eval.cv;
        else if (ix == 0 && si->si_type == PERLSI_MAIN)
            return PL_main_cv;
        else if (ix == 0 && CxTYPE(cx) == CXt_NULL
               && si->si_type == PERLSI_SORT)
        {
            /* fake sort sub; use CV of caller */
            si = si->si_prev;
            ix = si->si_cxix + 1;
        }
    }
    return NULL;
}

void
Perl_watch(pTHX_ char **addr)
{
    PERL_ARGS_ASSERT_WATCH;

    PL_watchaddr = addr;
    PL_watchok = *addr;
    PerlIO_printf(Perl_debug_log, "WATCHING, %"UVxf" is currently %"UVxf"\n",
	PTR2UV(PL_watchaddr), PTR2UV(PL_watchok));
}

STATIC void
S_debprof(pTHX_ const OP *o)
{
    PERL_ARGS_ASSERT_DEBPROF;

    if (!DEBUG_J_TEST_ && CopSTASH_eq(PL_curcop, PL_debstash))
	return;
    if (!PL_profiledata)
	Newxz(PL_profiledata, MAXO, U32);
    ++PL_profiledata[o->op_type];
}

void
Perl_debprofdump(pTHX)
{
    unsigned i;
    if (!PL_profiledata)
	return;
    for (i = 0; i < MAXO; i++) {
	if (PL_profiledata[i])
	    PerlIO_printf(Perl_debug_log,
			  "%5lu %s\n", (unsigned long)PL_profiledata[i],
                                       PL_op_name[i]);
    }
}


/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
