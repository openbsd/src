/*	$OpenBSD: proto.h,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * proto.h: include the (automatically generated) function prototypes
 *
 * the proto/xxx.pro files are automatically generated when using Manx/Aztec C.
 * For other compilers you will have to edit them.
 */

#include "regexp.h"		/* for struct regexp */

/*
 * don't include these while generating prototypes, prevents problems when
 * files are missing
 */
#ifndef PROTO

/*
 * Machine-dependent routines.
 */
# ifdef AMIGA
#  include "proto/amiga.pro"
# endif
# if defined(UNIX) || defined(__EMX__)
#  include "proto/unix.pro"
#  ifndef HAVE_RENAME
	int rename __PARMS((const char *, const char *));
#  endif
# endif
# ifdef MSDOS
#  include "proto/msdos.pro"
# endif
# ifdef WIN32
#  include "proto/win32.pro"
# endif
# ifdef VMS
#  include "proto/vms.pro"
# endif

# include "proto/alloc.pro"
# include "proto/buffer.pro"
# include "proto/charset.pro"
# include "proto/cmdcmds.pro"
# include "proto/cmdline.pro"
# include "proto/csearch.pro"
# include "proto/digraph.pro"
# include "proto/edit.pro"
# include "proto/fileio.pro"
# include "proto/getchar.pro"
# include "proto/help.pro"
# include "proto/linefunc.pro"
# include "proto/main.pro"
# include "proto/mark.pro"
# ifndef MESSAGE
void smsg __PARMS((char_u *, ...));	/* cannot be produced automatically */
# endif
# include "proto/memfile.pro"
# include "proto/memline.pro"
# include "proto/message.pro"
# include "proto/misccmds.pro"
# include "proto/normal.pro"
# include "proto/ops.pro"
# include "proto/option.pro"
# include "proto/quickfix.pro"
# include "proto/regexp.pro"
# include "proto/regsub.pro"
# include "proto/screen.pro"
# include "proto/search.pro"
# include "proto/tables.pro"
# include "proto/tag.pro"
# include "proto/term.pro"
# if defined(HAVE_TGETENT) && (defined(AMIGA) || defined(VMS))
#  include "proto/termlib.pro"
# endif
# include "proto/undo.pro"
# include "proto/version.pro"
# include "proto/window.pro"

# ifdef USE_GUI
#  include "proto/gui.pro"
#  ifdef USE_GUI_MOTIF
#   include "proto/gui_motif.pro"
#  endif
#  ifdef USE_GUI_ATHENA
#   include "proto/gui_athena.pro"
#  endif
#  if defined(USE_GUI_MOTIF) || defined(USE_GUI_ATHENA)
#   include "proto/gui_x11.pro"
#  endif
#  if !defined(HAVE_SETENV) && !defined(HAVE_PUTENV)
extern int putenv __ARGS((char *string));		/* from pty.c */
#  endif
# endif	/* USE_GUI */
# if defined(USE_GUI)
extern int OpenPTY __ARGS((char **ttyn));		/* from pty.c */
# endif

#endif /* PROTO */
