/*	$OpenBSD: version.c,v 1.1.1.1 1996/09/07 21:40:24 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"

/*
 * Vim originated from Stevie version 3.6 (Fish disk 217) by GRWalter (Fred)
 * It has been changed beyond recognition since then.
 *
 * All the remarks about older versions have been removed, they are not very
 * interesting.  Differences between version 3.0 and 4.0 can be found in
 * "../doc/vim_40.txt".
 *
 * Changes between version 4.1 and 4.2:
 * - Included ctags version 1.3.
 * - Included xxd.c version 1.4 (only one change since 1.3m)
 * - Unix: Adjusted check for child finished again.  Now use != ECHILD
 *   instead of == EINTR.
 * - Fixed: loading compressed files (with autocommands) didn't work when
 *   'textmode' was previously set.
 * - Fixed: When executing a shell, a window resize was not recognized for
 *   Unix.
 * - Fixed: For GUI, when executing an external command, a window resize
 *   caused the buffer to be redrawn.
 * - Fixed: Error message for not being able to open the .viminfo file could
 *   report the wrong filename.
 * - MS-DOS, Win32, OS/2: When $HOME is not set, use "C:/".
 * - OS/2: Improved handling of wildcards again.  Didn't ignore case, didn't
 *   work with backslashes.
 * - Fixed: ":s/pat<Tab>" could cause a core dump or other problems.
 * - Fixed: When entering a hidden buffer with ":e", the BufEnter autocommands
 *   were not executed.
 * - Fixed: Ignoring a CTRL-Z at end-of-file was only done on MS-DOS.  Now it
 *   also works on other systems, but only in textmode.
 * - Fixed: In the GUI special characters were not passed on to external
 *   commands, typeahead was truncated.
 * - Added "gq" as an alias to "Q".  Should be used when "Q" is made to be Vi
 *   compatible: go to Ex mode.
 * - Fixed: "gu" in Visual mode could not be redone with "." correctly.
 * - Fixed: ":normal" command made any typeahead executed right away, causing
 *   unpredictable problems.
 * - Fixed: ":normal atest^[" didn't update the screen.
 * - Fixed: Redoing blockwise visual delete at the end of the file could cause
 *   "invalid line number" errors.
 * - Fixed: vim_rename() didn't open the output file in binary mode, could
 *   cause the .viminfo file to contain CR-LF on OS/2.
 * - Fixed: OS/2 was using /tmp/xxx for temporary file name, would fail if
 *   there is no /tmp directory.  Now use $TMP/xxx, defaulting to c:/ if $TMP
 *   is not set.
 * - Fixed: When USE_TMPNAM was defined, was using the wrong array size for
 *   the viminfo temporary file.
 *
 * Changes between version 4.0 and 4.1:
 *
 * - Included xxd.c version 1.3m.
 * - Included ctags version 1.2.
 * - Included tools/efm_filt.er.
 * - Included changes for port to Atari MiNT, including makefile.mint.
 * - Included a few changes for OS/2: Improved executing external commands,
 *   depending on the shell; Handle resize after executing an external
 *   command; Handle wildcard expansion for more than one argument (e.g.
 *   ":n *.c *.h").
 * - Include a lot of small changes to the docs.
 * - Fixed: GUI version would busy-loop and mappings didn't work.  Was caused
 *   by gui_mch_wait_for_chars() not working properly.  This fix was the main
 *   reason for releasing 4.1.
 * - Fixed: setting 'term' while GUI is active was possible, and caused
 *   problems.
 * - Fixed: When the "to" part of a mapping or menu command was long (more
 *   than 24 chars on 32 bit MS-DOS, 128 chars on other systems), any <> were
 *   not translated and CTRL-Vs not removed.
 * - Fixed: 'modified' option was included for ":mkvimrc", it shouldn't.
 * - Included a few changes for that Atari MiNT port (vt52 builtin term
 *   entry).
 * - Fixed: on MS-DOS a file name pattern for an autocommand that contains
 *   "\*" or "\?" didn't work.
 * - On MS-DOS and Amiga, ignore case when matching the file name pattern for
 *   autocommands.
 * - Fixed: using :set to show the value of two options gave an error message
 *   (e.g. ":set so sj").
 * - Fixed: Non-printable characters in a file name caused trouble when
 *   displayed in a status line.
 * - Pack the MS-DOS zip files with Infozip, under Unix.  Preserves the long
 *   filenames and case.  Files with two dots don't work though, the first dot
 *   is replaced with an underscore.
 * - Fixed: Pasting more than one line with the mouse in insert mode, didn't
 *   put the cursor after the last pasted character.
 * - When pasting linewise text, put the '] mark on the last character of the
 *   last line, instead of the first character of the last line.
 * - Fixed: on some Unix systems, when resizing the window while in a external
 *   command (e.g., ":!cat"), Vim would stop waiting for the child, causing
 *   trouble, because the child is still running.
 * - Fixed: resizing the window while executing an external command, and
 *   't_ti' and 't_te' are defined to swap display buffers, Vim would redraw
 *   in the wrong display buffer after the "hit RETURN" message.
 * - Fixed: "va", "vA", "Vp", "VP", "Vs" and "VS" didn't set the cursor
 *   position used for up/down movements (e.g., when using "j" after them).
 * - Fixed: in GUI version, after using "cw" visual selection by dragging the
 *   mouse didn't work.
 * - Fixed: setting 'ttyscroll' to 0 caused scrolling of message to stop
 *   working.
 * - Fixed: the "WARNING: file changed" message caused buffers to be flushed
 *   and subsequent commands not to be executed.
 * - Fixed: in Insert mode, the message from "^O^G" would be
 *   overwritten by the mode message if 'showmode' set.
 * - Fixed: Using ":bdel" when there is only one buffer with two windows,
 *   could cause a crash.
 * - Changed: the '<' flag in 'cpoptions' now only switches off the
 *   recognizing of the <> form of key codes.  The 'k' flag is now used for
 *   the recognizing of raw key codes.
 * - Fixed: Typing ':' at the --more-- prompt, when displaying autocommands,
 *   caused extra linefeeds to be produced.
 * - Fixed: Using 'tagrelative' and ":set tags=./../tags", filenames would
 *   contain "../" as many times as CTRL-] would be used.  These are removed
 *   now.
 * - Fixed: Extremely long error message could cause a crash (e.g., when
 *   using ":help ^A<CR>").
 * - Added check for negative value of 'textwidth'.
 * - Fixed: On MS-DOS, getting the value of $HOME would cause the current
 *   directory for the drive to be changed.
 */

/*
 * Version[] is copied into the swap file (max. length is 10 chars).
 * longVersion[] is used for the ":version" command and "Vim -h".
 * Don't forget to update the numbers in version.h for Win32!!!
 */

char		   *Version = "VIM 4.2";
#ifdef HAVE_DATE_TIME
char		   *longVersion = "VIM - Vi IMproved 4.2 (1996 June 17, compiled " __DATE__ " " __TIME__ ")";
#else
char		   *longVersion = "VIM - Vi IMproved 4.2 (1996 June 17)";
#endif

static void version_msg __ARGS((char *s));

	void
do_version(arg)
	char_u	*arg;
{
	long		n;

	if (*arg != NUL)
	{
		found_version = getdigits(&arg) * 100;
		if (*arg == '.' && isdigit(arg[1]))
		{
			/* "4.1"  -> 401, "4.10" -> 410 */
			n = arg[1] - '0';
			if (isdigit(arg[2]))
				found_version += (arg[2] - '0') + n * 10;
			else
				found_version += n;
		}
		if (found_version > 400)
		{
			MSG("Warning: Found newer version command");
			if (sourcing_name != NULL)
			{
				MSG_OUTSTR(" in: \"");
				msg_outstr(sourcing_name);
				MSG_OUTSTR("\" line: ");
				msg_outnum((long)sourcing_lnum);
			}
		}
	}
	else
	{
		msg_outchar('\n');
		MSG(longVersion);
#ifdef WIN32
		MSG_OUTSTR("\nWindows NT / Windows 95 version");
#endif
#ifdef MSDOS
# ifdef DJGPP
		MSG_OUTSTR("\n32 bit MS-DOS version");
# else
		MSG_OUTSTR("\n16 bit MS-DOS version");
# endif
#endif
		MSG_OUTSTR("\nCompiled with (+) or without (-):\n");
#ifdef AMIGA			/* only for Amiga systems */
# ifdef NO_ARP
		version_msg("-ARP ");
# else
		version_msg("+ARP ");
# endif
#endif
#ifdef AUTOCMD
		version_msg("+autocmd ");
#else
		version_msg("-autocmd ");
#endif
#ifdef NO_BUILTIN_TCAPS
		version_msg("-builtin_terms ");
#endif
#ifdef SOME_BUILTIN_TCAPS
		version_msg("+builtin_terms ");
#endif
#ifdef ALL_BUILTIN_TCAPS
		version_msg("++builtin_terms ");
#endif
#ifdef CINDENT
		version_msg("+cindent ");
#else
		version_msg("-cindent ");
#endif
#ifdef COMPATIBLE
		version_msg("+compatible ");
#else
		version_msg("-compatible ");
#endif
#ifdef DEBUG
		version_msg("+debug ");
#endif
#ifdef DIGRAPHS
		version_msg("+digraphs ");
#else
		version_msg("-digraphs ");
#endif
#ifdef EMACS_TAGS
		version_msg("+emacs_tags ");
#else
		version_msg("-emacs_tags ");
#endif
			/* only interesting on Unix systems */
#if !defined(USE_SYSTEM) && defined(UNIX)
		version_msg("+fork() ");
#endif
#ifdef UNIX
# ifdef USE_GUI_MOTIF
		version_msg("+GUI_Motif ");
# else
#  ifdef USE_GUI_ATHENA
		version_msg("+GUI_Athena ");
#  else
		version_msg("-GUI ");
#  endif
# endif
#endif
#ifdef INSERT_EXPAND
		version_msg("+insert_expand ");
#else
		version_msg("-insert_expand ");
#endif
#ifdef HAVE_LANGMAP
		version_msg("+langmap ");
#else
		version_msg("-langmap ");
#endif
#ifdef LISPINDENT
		version_msg("+lispindent ");
#else
		version_msg("-lispindent ");
#endif
#ifdef RIGHTLEFT
		version_msg("+rightleft ");
#else
		version_msg("-rightleft ");
#endif
#ifdef SMARTINDENT
		version_msg("+smartindent ");
#else
		version_msg("-smartindent ");
#endif
			/* only interesting on Unix systems */
#if defined(USE_SYSTEM) && (defined(UNIX) || defined(__EMX__))
		version_msg("+system() ");
#endif
#if defined(UNIX) || defined(__EMX__)
/* only unix (or OS/2 with EMX!) can have terminfo instead of termcap */
# ifdef TERMINFO
		version_msg("+terminfo ");
# else
		version_msg("-terminfo ");
# endif
#else				/* unix always includes termcap support */
# ifdef HAVE_TGETENT
		version_msg("+tgetent ");
# else
		version_msg("-tgetent ");
# endif
#endif
#ifdef VIMINFO
		version_msg("+viminfo ");
#else
		version_msg("-viminfo ");
#endif
#ifdef WRITEBACKUP
		version_msg("+writebackup ");
#else
		version_msg("-writebackup ");
#endif
#ifdef UNIX
# if defined(WANT_X11) && defined(HAVE_X11)
		version_msg("+X11 ");
# else
		version_msg("-X11 ");
# endif
#endif
		msg_outchar('\n');
#ifdef USR_VIMRC_FILE
		version_msg("user vimrc file: \"");
		version_msg(USR_VIMRC_FILE);
		version_msg("\" ");
#endif
#ifdef USR_EXRC_FILE
		version_msg("user exrc file: \"");
		version_msg(USR_EXRC_FILE);
		version_msg("\" ");
#endif
#ifdef USE_GUI
		version_msg("user gvimrc file: \"");
		version_msg(USR_GVIMRC_FILE);
		version_msg("\" ");
#endif
#if defined(HAVE_CONFIG_H) || defined(OS2)
		msg_outchar('\n');
		version_msg("system vimrc file: \"");
		version_msg((char *)sys_vimrc_fname);
		version_msg("\"");
		msg_outchar('\n');
		version_msg("system compatrc file: \"");
		version_msg((char *)sys_compatrc_fname);
		version_msg("\"");
# ifdef USE_GUI
		msg_outchar('\n');
		version_msg("system gvimrc file: \"");
		version_msg((char *)sys_gvimrc_fname);
		MSG_OUTSTR("\"");
# endif
		msg_outchar('\n');
		version_msg("Compilation: ");
		version_msg((char *)all_cflags);
#endif
	}
}

/*
 * Output a string for the version message.  If it's going to wrap, output a
 * newline, unless the message is too long to fit on the screen anyway.
 */
	static void
version_msg(s)
	char		*s;
{
	int		len = strlen(s);

	if (len < (int)Columns && msg_col + len >= (int)Columns)
		msg_outchar('\n');
	MSG_OUTSTR(s);
}
