/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Regular expression library.
 * Define exactly one of the following to be 1:
 * HAVE_POSIX_REGCOMP: POSIX regcomp() and regex.h
 * HAVE_RE_COMP: BSD re_comp()
 * HAVE_REGCMP: System V regcmp()
 * HAVE_V8_REGCOMP: Henry Spencer V8 regcomp() and regexp.h
 * NO_REGEX: pattern matching is supported, but without metacharacters.
 */
#undef HAVE_POSIX_REGCOMP
#undef HAVE_RE_COMP
#undef HAVE_REGCMP
#undef HAVE_V8_REGCOMP
#undef NO_REGEX

/* Define HAVE_VOID if your compiler supports the "void" type. */
#undef HAVE_VOID

/* Define HAVE_TIME_T if your system supports the "time_t" type. */
#undef HAVE_TIME_T

/* Define HAVE_STRERROR if you have the strerror() function. */
#undef HAVE_STRERROR

/* Define HAVE_FILENO if you have the fileno() macro. */
#undef HAVE_FILENO

/* Define HAVE_ERRNO if you have the errno variable */
#undef HAVE_ERRNO

/* Define HAVE_SYS_ERRLIST if you have the sys_errlist[] variable */
#undef HAVE_SYS_ERRLIST

/* Define HAVE_OSPEED if your termcap library has the ospeed variable */
#undef HAVE_OSPEED
/* Define MUST_DEFINE_OSPEED if you have ospeed but it is not defined
 * in termcap.h. */
#undef MUST_DEFINE_OSPEED

/* Define HAVE_LOCALE if you have locale.h and setlocale. */
#undef HAVE_LOCALE

/* Define HAVE_TERMIOS_FUNCS if you have tcgetattr/tcsetattr */
#undef HAVE_TERMIOS_FUNCS

/* Define HAVE_UPPER_LOWER if you have isupper, islower, toupper, tolower */
#undef HAVE_UPPER_LOWER
