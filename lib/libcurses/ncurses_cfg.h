/*	$OpenBSD: ncurses_cfg.h,v 1.3 1998/01/17 16:27:37 millert Exp $	*/

/* include/ncurses_cfg.h.  Generated automatically by configure.  */
/******************************************************************************
 * Copyright 1997 by Thomas E. Dickey <dickey@clark.net>                      *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission. THE ABOVE LISTED      *
 * COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,  *
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO     *
 * EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY         *
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER       *
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF       *
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN        *
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                   *
 ******************************************************************************/
/*
 * Id: ncurses_cfg.hin,v 1.1 1997/12/24 12:26:32 tom Exp $
 *
 * This is a template-file used to generate the "ncurses_cfg.h" file.
 *
 * Rather than list every definition, the configuration script substitutes
 * the definitions that it finds using 'sed'.  You need a patch (971222)
 * to autoconf 2.12 to do this.
 */
#ifndef NC_CONFIG_H
#define NC_CONFIG_H

#define CC_HAS_INLINE_FUNCS	1
#define GCC_NORETURN	__attribute__((noreturn))
#define GCC_PRINTF	1
#define GCC_SCANF	1
#define GCC_UNUSED	__attribute__((unused))
#define HAVE_BUILTIN_H	1
#define HAVE_DIRENT_H	1
#define HAVE_FCNTL_H	1
#define HAVE_FORM_H	1
#define HAVE_GETTIMEOFDAY	1
#define HAVE_GETTTYNAM	1
#define HAVE_LIMITS_H	1
#define HAVE_LINK	1
#define HAVE_LOCALE_H	1
#define HAVE_LONG_FILE_NAMES	1
#define HAVE_MEMCCPY	1
#define HAVE_MENU_H	1
#define HAVE_NC_ALLOC_H	1
#define HAVE_PANEL_H	1
#define HAVE_POLL	1
#define HAVE_POLL_H	1
#define HAVE_REGEX_H_FUNCS	1
#define HAVE_SELECT	1
#define HAVE_SETBUF	1
#define HAVE_SETBUFFER	1
#define HAVE_SETVBUF	1
#define HAVE_SIGACTION	1
#define HAVE_SIGVEC	1
#define HAVE_SIZECHANGE	1
#define HAVE_STRDUP	1
#define HAVE_SYS_IOCTL_H	1
#define HAVE_SYS_PARAM_H	1
#define HAVE_SYS_SELECT_H	1
#define HAVE_SYS_TIMES_H	1
#define HAVE_SYS_TIME_H	1
#define HAVE_SYS_TIME_SELECT	1
#define HAVE_TCGETATTR	1
#define HAVE_TERMIOS_H	1
#define HAVE_TIMES	1
#define HAVE_TTYENT_H	1
#define HAVE_TYPEINFO	1
#define HAVE_UNISTD_H	1
#define HAVE_USLEEP	1
#define HAVE_VSNPRINTF	1
#define HAVE_VSSCANF	1
#define NCURSES_EXT_FUNCS	1
#define NDEBUG	1
#define PURE_TERMINFO	1
#define RETSIGTYPE	void
#define STDC_HEADERS	1
#define SYSTEM_NAME	"openbsd2.2"
#define TYPEOF_CHTYPE	long
#define USE_DATABASE	1
#define USE_SCROLL_HINTS	1

	/* The C compiler may not treat these properly but C++ has to */
#ifdef __cplusplus
#undef const
#undef inline
#else
#if defined(lint) || defined(TRACE)
#undef inline
#define inline /* nothing */
#endif
#endif

#endif /* NC_CONFIG_H */
