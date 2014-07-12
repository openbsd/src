/*
 * (c) Thomas Pornin 1999 - 2002
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef UCPP__TUNE__
#define UCPP__TUNE__

#ifdef UCPP_CONFIG
#include "config.h"
#else

/* ====================================================================== */
/*
 * The LOW_MEM macro triggers the use of macro storage which uses less
 * memory. It actually also improves performance on large, modern machines
 * (due to less cache pressure). This option implies no limitation (except
 * on the number of arguments a macro may, which is then limited to 32766)
 * so it is on by default. Non-LOW_MEM code is considered deprecated.
 */
#define LOW_MEM

/* ====================================================================== */
/*
 * Define AMIGA for systems using "drive letters" at the beginning of
 * some paths; define MSDOS on systems with drive letters and using
 * backslashes to seperate directory components.
 */
/* #define AMIGA */
/* #define MSDOS */

/* ====================================================================== */
/*
 * Define this if your compiler does not know the strftime() function;
 * TurboC 2.01 under Msdos does not know strftime().
 */
/* #define NOSTRFTIME */

/* ====================================================================== */
/*
 * Buffering: there are two levels of buffering on input and output streams:
 * the standard libc buffering (manageable with setbuf() and setvbuf())
 * and some buffering provided by ucpp itself. The ucpp buffering uses
 * two buffers, of size respectively INPUT_BUF_MEMG and OUTPUT_BUF_MEMG
 * (as defined below).
 * You can disable one or both of these bufferings by defining the macros
 * NO_LIBC_BUF and NO_UCPP_BUF.
 */
/* #define NO_LIBC_BUF */
/* #define NO_UCPP_BUF */

/*
 * On Unix stations, the system call mmap() might be used on input files.
 * This option is a subclause of ucpp internal buffering. On one station,
 * a 10% speed improvement was observed. Do not define this unless the
 * host architecture has the following characteristics:
 *  -- Posix / Single Unix compliance
 *  -- Text files correspond one to one with memory representation
 * If a file is not seekable or not mmapable, ucpp will revert to the
 * standard fread() solution.
 *
 * This feature is still considered beta quality. On some systems where
 * files can be bigger than memory address space (mainly, 32-bit systems
 * with files bigger than 4 GB), this option makes ucpp fail to operate
 * on those extremely large files.
 */
#define UCPP_MMAP

/*
 * Performance issues:
 * -- On memory-starved systems, such as Minix-i86, do not use ucpp
 * buffering; keep only libc buffering.
 * -- If you do not use libc buffering, activate the UCPP_MMAP option.
 * Note that the UCPP_MMAP option is ignored if ucpp buffering is not
 * activated.
 *
 * On an Athlon 1200 running FreeBSD 4.7, the best performances are
 * achieved when libc buffering is activated and/or UCPP_MMAP is on.
 */

/* ====================================================================== */
/*
 * Define this if you want ucpp to generate tokenized PRAGMA tokens;
 * otherwise, it will generate raw string contents. This setting is
 * irrelevant to the stand-alone version of ucpp.
 */
#define PRAGMA_TOKENIZE

/*
 * Define this to the special character that marks the end of tokens with
 * a string value inside a tokenized PRAGMA token. The #pragma and _Pragma()
 * directives which use this character will be a bit more difficult to
 * decode (but ucpp will not mind). 0 cannot be used. '\n' is fine because
 * it cannot appear inside a #pragma or _Pragma(), since newlines cannot be
 * embedded inside tokens, neither directly nor by macro substitution and
 * stringization. Besides, '\n' is portable.
 */
#define PRAGMA_TOKEN_END	((unsigned char)'\n')

/*
 * Define this if you want ucpp to include encountered #pragma directives
 * in its output in non-lexer mode; _Pragma() are translated to equivalent
 * #pragma directives.
 */
#define PRAGMA_DUMP

/*
 * According to my interpretation of the C99 standard, _Pragma() are
 * evaluated wherever macro expansion could take place. However, Neil Booth,
 * whose mother language is English (contrary to me) and who is well aware
 * of the C99 standard (and especially the C preprocessor) told me that
 * it was unclear whether _Pragma() are evaluated inside directives such
 * as #if, #include and #line. If you want to disable the evaluation of
 * _Pragma() inside such directives, define the following macro.
 */
/* #define NO_PRAGMA_IN_DIRECTIVE */

/*
 * The C99 standard mandates that the operator `##' must yield a single,
 * valid token, lest undefined behaviour befall upon thy head. Hence,
 * for instance, `+ ## +=' is forbidden, because `++=' is not a valid
 * token (although it is a valid list of two tokens, `++' and `=').
 * However, ucpp only emits a warning for such sin, and unmerges the
 * tokens (thus emitting `+' then `+=' for that example). When ucpp
 * produces text output, those two tokens will be separated by a space
 * character so that the basic rule of text output is preserved: when
 * parsed again, text output yields the exact same stream of tokens.
 * That extra space is virtual: it does not count as a true whitespace
 * token for stringization.
 *
 * However, it might be desirable, for some uses other than preprocessing
 * C source code, not to emit that extra space at all. To make ucpp behave
 * that way, define the DSHARP_TOKEN_MERGE macro. Please note that this
 * can trigger spurious token merging. For instance, with that macro
 * activated, `+ ## +=' will be output as `++=' which, if preprocessed
 * again, will read as `++' followed by `='.
 *
 * All this is irrelevant to lexer mode; and trying to merge incompatible
 * tokens is a shooting offence, anyway.
 */
/* #define DSHARP_TOKEN_MERGE */

/* ====================================================================== */
/*
 * Define INMACRO_FLAG to include two flags to the structure lexer_state,
 * that tell whether tokens come from a macro-replacement, and count those
 * macro-replacements.
 */
/* #define INMACRO_FLAG */

/* ====================================================================== */
/*
 * Paths where files are looked for by default, when #include is used.
 * Typical path is /usr/local/include and /usr/include, in that order.
 * If you want to set up no path, define the macro to 0.
 *
 * For Linux, get gcc includes too, or you will miss things like stddef.h.
 * The exact path varies much, depending on the distribution.
 */
#define STD_INCLUDE_PATH	"/usr/local/include", "/usr/include"

/* ====================================================================== */
/*
 * Arithmetic code for evaluation of #if expressions. Evaluation
 * uses either a native machine type, or an emulated two's complement
 * type. Division by 0 and overflow on division are considered as errors
 * and reported as such. If ARITHMETIC_CHECKS is defined, all other
 * operations that imply undefined or implementation-defined behaviour
 * are reported as warnings but otherwise performed nonetheless.
 *
 * For native type evaluation, the following macros should be defined:
 *   NATIVE_SIGNED           the native signed type
 *   NATIVE_UNSIGNED         the native corresponding unsigned type
 *   NATIVE_UNSIGNED_BITS    the native unsigned type width, in bits
 *   NATIVE_SIGNED_MIN       the native signed type minimum value
 *   NATIVE_SIGNED_MAX       the native signed type maximum value
 *
 * The code in the arith.c file performs some tricky detection
 * operations on the native type representation and possible existence
 * of a trap representation. These operations assume a C99-compliant
 * compiler; on a C90-only compiler, the operations are valid but may
 * yield incorrect results. You may force those settings with some
 * more macros: see the comments in arith.c (look for "ARCH_DEFINED").
 * Remember that this is mostly a non-issue, unless you are building
 * ucpp with a pre-C99 cross-compiler and either the host or target
 * architecture uses a non-two's complement representation of signed
 * integers. Such a combination is pretty rare nowadays, so the best
 * you can do is forgetting completely this paragraph and live in peace.
 *
 *
 * If you do not have a handy native type (for instance, you compile ucpp
 * with a C90 compiler which lacks the "long long" type, or you compile
 * ucpp for a cross-compiler which should support an evaluation integer
 * type of a size that is not available on the host machine), you may use
 * a simulated type. The type uses two's complement representation and
 * may have any width from 2 bits to twice the underlying native type
 * width, inclusive (odd widths are allowed). To use an emulated type,
 * make sure that NATIVE_SIGNED is not defined, and define the following
 * macros:
 *   SIMUL_ARITH_SUBTYPE     the native underlying type to use
 *   SIMUL_SUBTYPE_BITS      the native underlying type width
 *   SIMUL_NUMBITS           the emulated type width
 *
 * Undefined and implementation-defined behaviours are warned upon, if
 * ARITHMETIC_CHECKS is defined. Results are truncated to the type
 * width; shift count for the << and >> operators is reduced modulo the
 * emulatd type width; right shifting of a signed negative value performs
 * sign extension (the result is left-padded with bits set to 1).
 */

/*
 * For native type evaluation with a 64-bit "long long" type.
 */
#define NATIVE_SIGNED           long long
#define NATIVE_UNSIGNED         unsigned long long
#define NATIVE_UNSIGNED_BITS    64
#define NATIVE_SIGNED_MIN       (-9223372036854775807LL - 1)
#define NATIVE_SIGNED_MAX       9223372036854775807LL

/*
 * For emulation of a 64-bit type using a native 32-bit "unsigned long"
 * type.
#undef NATIVE_SIGNED
#define SIMUL_ARITH_SUBTYPE     unsigned long
#define SIMUL_SUBTYPE_BITS      32
#define SIMUL_NUMBITS           64
 */

/*
 * Comment out the following line if you want to deactivate arithmetic
 * checks (warnings upon undefined and implementation-defined
 * behaviour). Arithmetic checks slow down a bit arithmetic operations,
 * especially multiplications, but this should not be an issue with
 * typical C source code.
 */
#define ARITHMETIC_CHECKS

/* ====================================================================== */
/*
 * To force signedness of wide character constants, define WCHAR_SIGNEDNESS
 * to 0 for unsigned, 1 for signed. By default, wide character constants
 * are signed if the native `char' type is signed, and unsigned otherwise.
#define WCHAR_SIGNEDNESS	0
 */

/*
 * Standard assertions. They should include one cpu() assertion, one machine()
 * assertion (identical to cpu()), and one or more system() assertions.
 *
 * for Linux/PC:      cpu(i386),  machine(i386),  system(unix), system(linux)
 * for Linux/Alpha:   cpu(alpha), machine(alpha), system(unix), system(linux)
 * for Sparc/Solaris: cpu(sparc), machine(sparc), system(unix), system(solaris)
 *
 * These are only suggestions. On Solaris, machine() should be defined
 * for i386 or sparc (standard system header use such an assertion). For
 * cross-compilation, define assertions related to the target architecture.
 *
 * If you want no standard assertion, define STD_ASSERT to 0.
 */
/*
#define STD_ASSERT	"cpu(i386)", "machine(i386)", "system(unix)", \
			"system(freebsd)"
*/

/* ====================================================================== */
/*
 * System predefined macros. Nothing really mandatory, but some programs
 * might rely on those.
 * Each string must be either "name" or "name=token-list". If you want
 * no predefined macro, define STD_MACROS to 0.
 */
/*
#define STD_MACROS	"__FreeBSD=4", "__unix", "__i386", \
			"__FreeBSD__=4", "__unix__", "__i386__"
*/

/* ====================================================================== */
/*
 * Default flags; HANDLE_ASSERTIONS is required for Solaris system headers.
 * See cpp.h for the definition of these flags.
 */
#define DEFAULT_CPP_FLAGS	(DISCARD_COMMENTS | WARN_STANDARD \
				| WARN_PRAGMA | FAIL_SHARP | MACRO_VAARG \
				| CPLUSPLUS_COMMENTS | LINE_NUM | TEXT_OUTPUT \
				| KEEP_OUTPUT | HANDLE_TRIGRAPHS \
				| HANDLE_ASSERTIONS)
#define DEFAULT_LEXER_FLAGS	(DISCARD_COMMENTS | WARN_STANDARD | FAIL_SHARP \
				| MACRO_VAARG | CPLUSPLUS_COMMENTS | LEXER \
				| HANDLE_TRIGRAPHS | HANDLE_ASSERTIONS)

/* ====================================================================== */
/*
 * Define this to use sigsetjmp()/siglongjmp() instead of setjmp()/longjmp().
 * This is non-ANSI, but it improves performance on some POSIX system.
 * On typical C source code, such improvement is completely negligeable.
 */
/* #define POSIX_JMP */

/* ====================================================================== */
/*
 * Maximum value (plus one) of a character handled by the lexer; 128 is
 * alright for ASCII native source code, but 256 is needed for EBCDIC.
 * 256 is safe in both cases; you will have big problems if you set
 * this value to INT_MAX or above. On Minix-i86 or Msdos (small memory
 * model), define MAX_CHAR_VAL to 128.
 *
 * Set MAX_CHAR_VAL to a power of two to increase lexing speed. Beware
 * that lexer.c defines a static array of size MSTATE * MAX_CHAR_VAL
 * values of type int (MSTATE is defined in lexer.c and is about 40).
 */
#define MAX_CHAR_VAL	128

/*
 * If you want some extra character to be considered as whitespace,
 * define this macro to that space. On ISO-8859-1 machines, 160 is
 * the code for the unbreakable space.
 */
/* #define UNBREAKABLE_SPACE	160 */

/*
 * If you want whitespace tokens contents to be recorded (making them
 * tokens with a string content), define this. The macro STRING_TOKEN
 * will be adjusted accordingly.
 * Without this option, whitespace tokens are not even returned by the
 * lex() function. This is irrelevant for the non-lexer mode (almost --
 * it might slow down a bit ucpp, and with this option, comments will be
 * kept inside #pragma directives).
 */
/* #define SEMPER_FIDELIS */

#endif
/* End of options overridable by UCPP_CONFIG and config.h */

/* ====================================================================== */
/*
 * Some constants used for memory increment granularity. Increasing these
 * values reduces the number of calls to malloc() but increases memory
 * consumption.
 *
 * Values should be powers of 2.
 */

/* for cpp.c */
#define COPY_LINE_LENGTH	80
#define INPUT_BUF_MEMG		8192
#define OUTPUT_BUF_MEMG		8192
#define TOKEN_NAME_MEMG		64	/* must be at least 4 */
#define TOKEN_LIST_MEMG		32
#define INCPATH_MEMG		16
#define GARBAGE_LIST_MEMG	32
#define LS_STACK_MEMG		4
#define FNAME_MEMG		32

/* ====================================================================== */

/* To protect the innocent. */
#if defined(NO_UCPP_BUF) && defined(UCPP_MMAP)
#undef UCPP_MMAP
#endif

#if defined(UCPP_MMAP) || defined(POSIX_JMP)
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE	1
#endif
#endif

/*
 * C90 does not know about the "inline" keyword, but C99 does know,
 * and some C90 compilers know it as an extension. This part detects
 * these occurrences.
 */

#ifndef INLINE

#if __STDC__ && __STDC_VERSION__ >= 199901L
/* this is a C99 compiler, keep inline unchanged */
#elif defined(__GNUC__)
/* this is GNU gcc; modify inline. The semantics is not identical to C99
   but the differences are irrelevant as long as inline functions are static */
#undef inline
#define inline __inline__
#elif defined(__DECC) && defined(__linux__)
/* this is Compaq C under Linux, use __inline__ */
#undef inline
#define inline __inline__
#else
/* unknown compiler -> deactivate inline */
#undef inline
#define inline
#endif

#else
/* INLINE has been set, use its value */
#undef inline
#define inline INLINE
#endif

#endif
