/*	$OpenBSD: version.c,v 1.4 1996/10/15 08:08:00 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 * vi:set comments=sbl\:*\ -,mb\:*,el\:*\ -,sr\:/\*,mb\:*,el\:*\/,fb\:- :
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
 * interesting.  Differences between version 3.0 and 4.x can be found in
 * "../doc/vim_40.txt".
 *
 * Changes between version 4.4 BETA and 4.5:
 * - Ignore CR in line from tags file, also when there is no search command.
 * - Fixed small cindent problem, when using 'cino' with non-zero after '}'.
 * - Corrected error message for security violation for tag file commands.
 * - Fixed bug: When 'shell' set to "sh", "!echo text >x<Tab>" would create
 *   the file "x*".  Now completion of file names starts after a ">", "<" and
 *   following "&" and "!" characters.
 * - Added a few more changes for QNX.
 * - Fixed: when 'showmode' was not set, CTRL-X submode (error) messages would
 *   not be shown correctly.
 * - MSDOS: Fixed computation of mouse area again, didn't work for 50 lines
 *   screen.
 * - Fixed: Cursor was not positioned after ":move" command.
 * - Fixed a few compiler warnings for Athena on Sun OS 5.2.
 * - Added 'w' flag to 'cpoptions', to fix vi incompatibility for "cw" on a
 *   row of blanks.
 * - Fixed making a core dump on certain signals.
 * - Fixed check for Sun OS 4.x.x for xxd.c.
 * - Fixed problem with expanding two-character directory names for Win32
 *   version.
 * - Fixed: highlight mode for completion sub-messages was always 'r', now it
 *   depends on the type of message: 'e' for errors, 'w' for warnings.
 * - Fixed: 'cindentkeys' were not checked when inserting text from a mapping.
 * - Fixed: a ":global" that requires input, could not be broken with CTRL-C.
 * - Fixed: "1H" and "1L" were off by one line.
 * - Included version 1.5 of ctags.
 * Last minute fixes:
 * - Fixed: When using ^X^C in insert mode and then entering insert mode
 *   again, the ^X mode message is shown when it shouldn't.
 * - Fixed: In GUI, when reading the output from an external command, there
 *   was no check for an error, which could result in an endless loop.
 * - Small correction in src/ctags/ctags.c for MS-DOS.
 * - Fixed for Unix: Would never detect a triple signal.
 * - Removed ":mfstat" command, it's for debugging only.
 *
 * Changes between version 4.3 BETA and 4.4 BETA:
 * - Moved outputting newline from getout() to mch_windexit().  Helps when
 *   switching display pages in xterm after an error message for ":!".
 * - Fixed problem: Not executing BufEnter autocommands for first buffer.
 * - Fixed Makefile: "make shadow" didn't make ctags and xxd directories.  Now
 *   passes CC and CFLAGS to ctags and xxd makefiles.
 * - Removed use of #elif, some old compilers don't understand it.
 * - Included version 1.4 of ctags.  New Makefile.bcc, supports wildcards for
 *   16 bit DOS version.
 * - Fixed mouse positioning in wrong column for MSDOS 16 and 32 bit versions.
 * - Fixed: Delay in updating Visual area when using "/pat".
 * - Fixed: With some shells gvim could be killed with CTRL-C in the shell
 *   where it was started.
 * - Fixed: For abbreviations entered with ":noreab" only the first two
 *   characters were not re-mapped instead of all.
 * - Added help tags for search pattern special characters.  Adjusted
 *   doctags.c to insert a backslash before a backslash.
 * - Fixed Vi incompatibility: If the rhs of a mapping starts with the lhs,
 *   remapping is only disabled for the first character, not the whole lhs.
 * - Fixed: Default padding character was a space, which caused trouble on
 *   some systems.  Now it's a NUL.
 * - Fixed: With GUI Athena the scrollbar could get stuck at the bottom.
 * - Fixed: When using :imenu to insert more than one line of text, only the
 *   first line could be undone.
 * - Fixed: Word completion (CTRL-N) in Insert mode, when there was no
 *   matching word, the "Pattern not found" message was not shown.
 * - Fixed: Pattern completion (CTRL-X I) in Insert mode; the file name shown
 *   was overwritten with the mode message.
 * - Added ":if" and ":endif" commands.  Everything in between them is
 *   ingored.  Just to allow for future expansion that is backwards compatible.
 * - Fixed: Starting Vim without a file, then ":e file", ":sp" and ":q"
 *   unloaded the buffer.
 * - Fixed: execution of autocommands could not be interrupted.
 * - Fixed: "ga" on an empty line gave a misleading message, now it prints
 *   "empty line".
 * - Fixed: With 'number' set mouse positioning was wrong when lines wrap, and
 *   in the GUI horizontal scrolling didn't work properly.
 * - Removed "src/tags" from the source distribution; you can generate it
 *   yourself now that ctags is included.
 * - Included "macros/life", macros to run Conway's game of life.
 * - Fixed using "set go=r" in gvimrc problem for Motif GUI.
 * - Fixed problems when using autocommands with ":next" et. al..  Made
 *   "line1", "line2" and "forceit" local variables, instead of global.  Lots
 *   of function have to pass it as an argument, which is required to avoid
 *   the vars to get mixed up with recursive Ex commands.
 * - Removed the use of "want_start" in search.c.  Fixes bug when using a
 *   search string that starts with "^" and contains "\|".
 *
 * Changes between version 4.2 and 4.3 BETA:
 * - Moved ctags, tee and xxd sources from the binary to the source archive.
 * - OS/2: Adjusted ExpandWildCards again, fixed alloc/free error.
 * - Fixed: "Nothing in register ^@", ^@ for buffer 0 is now "
 * - Fixed: Was outputting CR CR LF instead of CR LF for termios.
 * - Fixed: cindent didn't handle "} else" and "} while (cond);".
 * - Fixed: Was using killpg(0, SIGINT) to interrupt external commands in the
 *   GUI, which isn't documented on all systems.  Use kill(0, SIGINT) instead.
 * - Updated version number that is compared with for the ":version" command.
 * - Fixed: ":0put" inserted text below line 1; now it inserts above line 1.
 * - Fixed: "/t/e" would not find the last character in a line.
 * - Fixed: Unloading the current buffer could load another unloaded buffer.
 *   Use a loaded buffer if there is one.
 * - Improved: ":dis" now shows "^J" at the end of linewise registers.
 * - Fixed: Using ":q" to close a window didn't execute the BufEnter
 *   autocommands for the new current window.
 * - Forbid the reading of a file after *ReadPre autocommands, this could give
 *   unpredictable results.
 * - Fixed: ":sball" didn't work correctly with autocommands that contain a
 *   ":normal" command.
 * - Fixed: was accepting a mapping after CTRL-W count.
 * - Fixed: a '[' in a regexp was special, made "/[z[]/" fail.
 * - Amiga, DICE: included a few patches to amiga.c.
 * - Fixed: Could crash when completing a setting, e.g. ":set <t_K<Tab>"
 * - Fixed: Using "new fname" in a vimrc caused a window with a non-existing
 *   buffer.
 * - Added support for keypad keys <kHome>, <kEnd>, <kPageUp> and <kPageDown>.
 *   They only work when they send a different code from <Home>, etc..
 * - Swapped the arguments to LINKIT in Makefile, was making a link from Vim,
 *   instead of to Vim.
 * - Fixed: Not all parts of the swap file were cleared before using them,
 *   could include any data in the swap file (mostly the password file!).
 * - Fixed: Could get an extra swap file when using ":au BufLeave xx bd xx".
 * - Fixed: ":set comment=n:" didn't give an error message; formatting would
 *   cause a hang.
 * - Use off_t for lseek; FreeBSD and others use long long instead of long.
 * - Fixed: ":/pat" didn't match at first column in the next line.
 * - Fixed: CTRL-F at end of file with 'scrolloff' non-zero would make the
 *   screen jump up and down and didn't beep when no more scrolling was
 *   possible.  When last two lines didn't fit on the screen together, the
 *   last one was never shown.
 * - When Vim is not compiled with AUTOCMD, "<afile>" is not included.
 * - Fixed: ":au BufWritePre xx bunload" caused empty file to be written, now
 *   it gives an error message.
 * - Added "<Bar>", to be used in mappings where a '|' is needed.
 * - Moved "Changing readonly file" message, In insert mode, to after the mode
 *   message, it would otherwise be hidden.
 * - Fixed: Putting a temp file in current directory for MS-DOS causes
 *   problems on readonly devices.  Try several directories to put the temp
 *   file in.
 * - Changed default for Unix 'errorformat' to include a few more compilers.
 * - Fixed: When exiting because of a non-existing file after the "-e"
 *   argument, there was no newline.
 * - When writing part of a buffer to a file, don't add a end-of-line for the
 *   last line, if 'binary' is set and the previous read didn't have an
 *   end-of-line for the same line.  For FileWritePre autocommands that filter
 *   the lines through gzip.
 * - Fixed: When not writing the eol for the last line, this was not visible,
 *   and the line count was one too low.
 * - Fixed: BufNewFile autocommands that do ":r file" sometimes didn't work,
 *   because the cursor was in an invalid line.
 * - Fixed: a *ReadFile autocommand that changed the file to be read didn't
 *   work, because the file was already opened.
 * - Fixed: When doing ":bdel", buf_copy_options() could copy options from
 *   already freed memory.  Would cause any combination of strange settings.
 * - Check for errors while reading the viminfo file.  When there are more
 *   than 10 errors, quit reading it.  When there is any error, don't
 *   overwrite it with a new viminfo file.  Prevents trashing a file when
 *   accidently doing "vim -i file" instead of "vim -v file".
 * - Added "ZQ", alias for ":q!".  Elvis compatible.
 * - Fixed: "vim -g" would crash when .gvimrc contains ":set go=r".
 * - Fixed: ":set go&" didn't work, the default contained an illegal 'A'.
 * - Added 'titlelen' option: percentage of 'columns' to use for the title.
 *   Reduces problems with truncating long path names.
 * - Added "tee" for OS/2.  Very useful for ":make".
 * - Fixed: Setting 'title' in the gvimrc messed up the title of the xterm
 *   where Vim was started when doing ":gui".
 * - Fixed: When expanding "~/file" with CTRL-X CTRL-F in insert mode, the
 *   "~/" would get expanded into a full path.  For "~user/file" the problem
 *   still exists though.
 * - Fixed: ":set path=../b" didn't work as expected, any path starting with a
 *   dot would get expanded to the directory of the current file.
 * - Fixed: Any dir name in 'directory' and 'backupdir' starting with '.' was
 *   considered to be in the current directory, also "..".  Now using "./dir"
 *   means using a directory relative to where the file is.
 * - Fixed: ":all", ":ball" and "-o" command line option would execute
 *   Buf/Win Enter/Leave autocommands for first buffer a few times.  Now
 *   they are only done when really entering a buffer/window.
 * - Fixed: ":all", change in first buffer, ":all" would give an error message
 *   for not writing the file.
 * - Added 'shellcmdflag' and 'shellquote' options, mainly for Win32 when
 *   using different kinds of shell.
 * - Fixed: "unmenu *" in .gvimrc caused a crash on some machines.
 * - Fixed: ":buf" (go to current buffer) should not do anything.  It executed
 *   autocommands and set the previous context mark.
 * - Fixed: "*``" moved the cursor back to the start of the word, instead of
 *   where the cursor was within or before the word.
 * - Fixed: ":e %:p:h" removed the head of the path ("/" for unix, "d:\" for
 *   DOS, "drive:" for Amiga.
 * - Fixed: for the Win32 version, 'term' must be "win32", don't init it with
 *   $TERM.
 * - Fixed: Filename completion with <Tab>, when there are several matches,
 *   but only one without ignored suffix, next <Tab> obtained second match,
 *   not the one after the previous one.  Now the files without matching
 *   suffix are put in front of the list.
 * - Fixed: DJGPP version of system() was eating file descriptors, after a few
 *   filter commands there would be an "Out of file handles" error.
 * - Fixed: for MS-DOS: ":n doc\*.txt" didn't work, it became "doc*.txt".
 * - Added: MS-DOS and WIN32 now expand $ENV in a filename. ":e $VIM/_vimrc"
 *   works now.
 * - Fixed: MS-DOS: after ":mode 1" mouse didn't move correctly.  Now it
 *   mostly works for the display modes up to 0x13.
 * - Fixed: In Insert mode, the message from  "^O:set tw" would be overwritten
 *   by "--INSERT--".  Now there is a 10 second delay to be able to read the
 *   message.
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

#include "version.h"

char		   *Version = "VIM 4.5";
#ifdef HAVE_DATE_TIME
char		   *longVersion = "VIM - Vi IMproved 4.5 (1996 Oct 12, compiled " __DATE__ " " __TIME__ ")";
#else
char		   *longVersion = "VIM - Vi IMproved 4.5 (1996 Oct 12)";
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
		if (found_version > VIM_VERSION_MAJOR * 100 + VIM_VERSION_MINOR)
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
