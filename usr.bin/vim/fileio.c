/*	$OpenBSD: fileio.c,v 1.2 1996/09/21 06:22:58 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * fileio.c: read from and write to a file
 */

#if defined MSDOS  ||  defined WIN32
# include <io.h>		/* for lseek(), must be before vim.h */
#endif

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef LATTICE
# include <proto/dos.h>		/* for Lock() and UnLock() */
#endif

#define BUFSIZE		8192			/* size of normal write buffer */
#define SBUFSIZE	256				/* size of emergency write buffer */

#ifdef VIMINFO
static void check_marks_read __ARGS((void));
#endif
static void msg_add_fname __ARGS((BUF *, char_u *));
static int msg_add_textmode __ARGS((int));
static void msg_add_lines __ARGS((int, long, long));
static void msg_add_eol __ARGS((void));
static int  write_buf __ARGS((int, char_u *, int));

static linenr_t	write_no_eol_lnum = 0; 	/* non-zero lnum when last line of
										   next binary write should not have
										   an end-of-line */

	void
filemess(buf, name, s)
	BUF			*buf;
	char_u		*name;
	char_u		*s;
{
	msg_add_fname(buf, name);		/* put file name in IObuff with quotes */
	STRCAT(IObuff, s);
	/*
	 * For the first message may have to start a new line.
	 * For further ones overwrite the previous one, reset msg_scroll before
	 * calling filemess().
	 */
	msg_start();
	msg_outtrans(IObuff);
	stop_highlight();
	msg_clr_eos();
	flushbuf();
}

/*
 * Read lines from file 'fname' into the buffer after line 'from'.
 *
 * 1. We allocate blocks with lalloc, as big as possible.
 * 2. Each block is filled with characters from the file with a single read().
 * 3. The lines are inserted in the buffer with ml_append().
 *
 * (caller must check that fname != NULL)
 *
 * lines_to_skip is the number of lines that must be skipped
 * lines_to_read is the number of lines that are appended
 * When not recovering lines_to_skip is 0 and lines_to_read MAXLNUM.
 *
 * return FAIL for failure, OK otherwise
 */
	int
readfile(fname, sfname, from, newfile, lines_to_skip, lines_to_read, filtering)
	char_u		   *fname;
	char_u		   *sfname;
	linenr_t		from;
	int				newfile;
	linenr_t		lines_to_skip;
	linenr_t		lines_to_read;
	int				filtering;
{
	int 				fd;
	register char_u 	c;
	register linenr_t	lnum = from;
	register char_u 	*ptr = NULL;			/* pointer into read buffer */
	register char_u		*buffer = NULL;			/* read buffer */
	char_u				*new_buffer = NULL;		/* init to shut up gcc */
	char_u				*line_start = NULL;		/* init to shut up gcc */
	colnr_t				len;
	register long		size;
	register char_u		*p;
	long				filesize = 0;
	int					split = 0;				/* number of split lines */
#define UNKNOWN		0x0fffffff					/* file size is unknown */
	linenr_t			linecnt;
	int 				error = FALSE;			/* errors encountered */
	int					tx_error = FALSE;		/* textmode, but no CR */
	long				linerest = 0;			/* remaining chars in line */
	int					firstpart = TRUE;		/* reading first part */
#ifdef UNIX
	int					perm;
#endif
	int					textmode;				/* accept CR-LF linebreak */
	struct stat			st;
	int					file_readonly;
	linenr_t			skip_count = lines_to_skip;
	linenr_t			read_count = lines_to_read;
	int					msg_save = msg_scroll;
	linenr_t			read_no_eol_lnum = 0; 	/* non-zero lnum when last
												   line of last read was
												   missing the eol */


#ifdef AUTOCMD
	write_no_eol_lnum = 0;		/* in case it was set by the previous read */
#endif

	/*
	 * If there is no file name yet, use the one for the read file.
	 * b_notedited is set to reflect this.
	 * Don't do this for a read from a filter.
	 * Only do this when 'cpoptions' contains the 'f' flag.
	 */
	if (curbuf->b_filename == NULL && !filtering &&
										vim_strchr(p_cpo, CPO_FNAMER) != NULL)
	{
		if (setfname(fname, sfname, FALSE) == OK)
			curbuf->b_notedited = TRUE;
	}

	if (shortmess(SHM_OVER) || curbuf->b_help)
		msg_scroll = FALSE;		/* overwrite previous file message */
	else
		msg_scroll = TRUE;		/* don't overwrite previous file message */
	if (sfname == NULL)
		sfname = fname;
	/*
	 * For Unix: Use the short filename whenever possible.
	 * Avoids problems with networks and when directory names are changed.
	 * Don't do this for MS-DOS, a "cd" in a sub-shell may have moved us to
	 * another directory, which we don't detect.
	 */
#if defined(UNIX) || defined(__EMX__)
	if (!did_cd)
		fname = sfname;
#endif

#ifdef UNIX
	/*
	 * On Unix it is possible to read a directory, so we have to
	 * check for it before the open().
	 */
	perm = getperm(fname);
# ifdef _POSIX_SOURCE
	if (perm >= 0 && !S_ISREG(perm)	 				/* not a regular file ... */
#  ifdef S_ISFIFO
				  && !S_ISFIFO(perm)				/* ... or fifo or socket */
#  endif
										   )
# else
	if (perm >= 0 && (perm & S_IFMT) != S_IFREG		/* not a regular file ... */
#  ifdef S_IFIFO
				  && (perm & S_IFMT) != S_IFIFO		/* ... or fifo ... */
#  endif
#  ifdef S_IFSOCK
				  && (perm & S_IFMT) != S_IFSOCK	/* ... or socket */
#  endif
											)
# endif
	{
# ifdef _POSIX_SOURCE
		if (S_ISDIR(perm))
# else
		if ((perm & S_IFMT) == S_IFDIR)
# endif
			filemess(curbuf, fname, (char_u *)"is a directory");
		else
			filemess(curbuf, fname, (char_u *)"is not a file");
		msg_scroll = msg_save;
		return FAIL;
	}
#endif

	/*
	 * When opening a new file we take the readonly flag from the file.
	 * Default is r/w, can be set to r/o below.
	 * Don't reset it when in readonly mode
	 */
	if (newfile && !readonlymode)			/* default: set file not readonly */
		curbuf->b_p_ro = FALSE;

	if (newfile)
	{
		if (stat((char *)fname, &st) >= 0)	/* remember time of file */
		{
			curbuf->b_mtime = st.st_mtime;
			curbuf->b_mtime_read = st.st_mtime;
#ifdef UNIX
			/*
			 * Set the protection bits of the swap file equal to the original
			 * file. This makes it possible for others to read the name of the
			 * original file from the swapfile.
			 */
			if (curbuf->b_ml.ml_mfp->mf_fname != NULL)
				(void)setperm(curbuf->b_ml.ml_mfp->mf_fname,
												  (st.st_mode & 0777) | 0600);
#endif
		}
		else
		{
			curbuf->b_mtime = 0;
			curbuf->b_mtime_read = 0;
		}
	}

/*
 * for UNIX: check readonly with perm and access()
 * for MSDOS and Amiga: check readonly by trying to open the file for writing
 */
	file_readonly = FALSE;
#if defined(UNIX) || defined(DJGPP) || defined(__EMX__)
	if (
# ifdef UNIX
		!(perm & 0222) ||
# endif
							access((char *)fname, W_OK))
		file_readonly = TRUE;
	fd = open((char *)fname, O_RDONLY | O_EXTRA);
#else
	if (!newfile || readonlymode || (fd =
								   open((char *)fname, O_RDWR | O_EXTRA)) < 0)
	{
		file_readonly = TRUE;
		fd = open((char *)fname, O_RDONLY | O_EXTRA);	/* try to open ro */
	}
#endif

	if (fd < 0)					 /* cannot open at all */
	{
#ifndef UNIX
		int		isdir_f;
#endif
		msg_scroll = msg_save;
#ifndef UNIX
	/*
	 * On MSDOS and Amiga we can't open a directory, check here.
	 */
		isdir_f = (mch_isdir(fname));
		/* replace with short name now, for the messages */
		if (!did_cd)
			fname = sfname;
		if (isdir_f)
			filemess(curbuf, fname, (char_u *)"is a directory");
		else
#endif
			if (newfile)
			{
#ifdef UNIX
				if (perm < 0)
#endif
				{
					filemess(curbuf, fname, (char_u *)"[New File]");
#ifdef AUTOCMD
					apply_autocmds(EVENT_BUFNEWFILE, fname, fname);
#endif
					return OK;		/* a new file is not an error */
				}
#ifdef UNIX
				else
					filemess(curbuf, fname, (char_u *)"[Permission Denied]");
#endif
			}

		return FAIL;
	}

	/*
	 * Only set the 'ro' flag for readonly files the first time they are
	 * loaded.
	 * Help files always get readonly mode
	 */
	if ((newfile && file_readonly) || curbuf->b_help)
		curbuf->b_p_ro = TRUE;

	if (newfile)
		curbuf->b_p_eol = TRUE;

#ifndef UNIX
	/* replace with short name now, for the messages */
	if (!did_cd)
		fname = sfname;
#endif
	++no_wait_return;							/* don't wait for return yet */

	/*
	 * Set '[ mark to the line above where the lines go (line 1 if zero).
	 */
	curbuf->b_op_start.lnum = ((from == 0) ? 1 : from);
	curbuf->b_op_start.col = 0;

#ifdef AUTOCMD
	{
		int	m = msg_scroll;
		int n = msg_scrolled;
		BUF	*old_curbuf = curbuf;

		/*
		 * The file must be closed again, the autocommands may want to change
		 * the file before reading it.
		 */
		close(fd);			/* ignore errors */

		/*
		 * The output from the autocommands should not overwrite anything and
		 * should not be overwritten: Set msg_scroll, restore its value if no
		 * output was done.
		 */
		msg_scroll = TRUE;
		if (filtering)
			apply_autocmds(EVENT_FILTERREADPRE, NULL, fname);
		else if (newfile)
			apply_autocmds(EVENT_BUFREADPRE, NULL, fname);
		else
			apply_autocmds(EVENT_FILEREADPRE, fname, fname);
		if (msg_scrolled == n)
			msg_scroll = m;

		/*
		 * Don't allow the autocommands to change the current buffer.
		 * Try to re-open the file.
		 */
		if (curbuf != old_curbuf ||
						   (fd = open((char *)fname, O_RDONLY | O_EXTRA)) < 0)
		{
			--no_wait_return;
			msg_scroll = msg_save;
			if (fd < 0)
				EMSG("*ReadPre autocommands made the file unreadable");
			else
				EMSG("*ReadPre autocommands must not change current buffer");
			return FAIL;
		}
	}
#endif

	if (!recoverymode && !filtering)
		filemess(curbuf, fname, (char_u *)"");	/* show that we are busy */

	msg_scroll = FALSE;							/* overwrite the file message */

	/*
	 * Set textmode and linecnt now, before the "retry" caused by 'textauto'
	 * and after the autocommands, which may change them.
	 */
	textmode = curbuf->b_p_tx;
	linecnt = curbuf->b_ml.ml_line_count;

retry:
	while (!error && !got_int)
	{
		/*
		 * We allocate as much space for the file as we can get, plus
		 * space for the old line plus room for one terminating NUL.
		 * The amount is limited by the fact that read() only can read
		 * upto max_unsigned characters (and other things).
		 */
#if SIZEOF_INT <= 2
		if (linerest >= 0x7ff0)
		{
			++split;
			*ptr = NL;				/* split line by inserting a NL */
			size = 1;
		}
		else
#endif
		{
#if SIZEOF_INT > 2
			size = 0x10000L;				/* use buffer >= 64K */
#else
			size = 0x7ff0L - linerest;		/* limit buffer to 32K */
#endif

			for ( ; size >= 10; size >>= 1)
			{
				if ((new_buffer = lalloc((long_u)(size + linerest + 1), FALSE)) != NULL)
					break;
			}
			if (new_buffer == NULL)
			{
				do_outofmem_msg();
				error = TRUE;
				break;
			}
			if (linerest)		/* copy characters from the previous buffer */
				vim_memmove(new_buffer, ptr - linerest, (size_t)linerest);
			vim_free(buffer);
			buffer = new_buffer;
			ptr = buffer + linerest;
			line_start = buffer;
			
			if ((size = read(fd, (char *)ptr, (size_t)size)) <= 0)
			{
				if (size < 0)				/* read error */
					error = TRUE;
				break;
			}
			filesize += size;				/* count the number of characters */

			/*
			 * when reading the first part of a file: guess EOL type
			 */
			if (firstpart && p_ta)
			{
				for (p = ptr; p < ptr + size; ++p)
					if (*p == NL)
					{
						if (p > ptr && p[-1] == CR)	/* found CR-NL */
							textmode = TRUE;
						else						/* found a single NL */
							textmode = FALSE;
							/* if editing a new file: may set p_tx */
						if (newfile)
							curbuf->b_p_tx = textmode;
						break;
					}
			}
		}

		firstpart = FALSE;

		/*
		 * This loop is executed once for every character read.
		 * Keep it fast!
		 */
		--ptr;
		while (++ptr, --size >= 0)
		{
			if ((c = *ptr) != NUL && c != NL)	/* catch most common case */
				continue;
			if (c == NUL)
				*ptr = NL;		/* NULs are replaced by newlines! */
			else
			{
				if (skip_count == 0)
				{
					*ptr = NUL;		/* end of line */
					len = ptr - line_start + 1;
					if (textmode)
					{
						if (ptr[-1] == CR)	/* remove CR */
						{
							ptr[-1] = NUL;
							--len;
						}
						/*
						 * Reading in textmode, but no CR-LF found!
						 * When 'textauto' set, delete all the lines read so
						 * far and start all over again.
						 * Otherwise give an error message later.
						 */
						else if (!tx_error)
						{
							if (p_ta && lseek(fd, 0L, SEEK_SET) == 0)
							{
								while (lnum > from)
									ml_delete(lnum--, FALSE);
								textmode = FALSE;
								if (newfile)
									curbuf->b_p_tx = FALSE;
								linerest = 0;
								filesize = 0;
								skip_count = lines_to_skip;
								read_count = lines_to_read;
								goto retry;
							}
							else
								tx_error = TRUE;
						}
					}
					if (ml_append(lnum, line_start, len, newfile) == FAIL)
					{
						error = TRUE;
						break;
					}
					++lnum;
					if (--read_count == 0)
					{
						error = TRUE;		/* break loop */
						line_start = ptr;	/* nothing left to write */
						break;
					}
				}
				else
					--skip_count;
				line_start = ptr + 1;
			}
		}
		linerest = ptr - line_start;
		mch_breakcheck();
	}

	/* not an error, max. number of lines reached */
	if (error && read_count == 0)
		error = FALSE;

	/*
	 * If we get EOF in the middle of a line, note the fact and
	 * complete the line ourselves.
	 * In textmode ignore a trailing CTRL-Z, unless 'binary' set.
	 */
	if (!error && !got_int && linerest != 0 &&
			!(!curbuf->b_p_bin && textmode &&
					*line_start == Ctrl('Z') && ptr == line_start + 1))
	{
		if (newfile)				/* remember for when writing */
			curbuf->b_p_eol = FALSE;
		*ptr = NUL;
		if (ml_append(lnum, line_start,
						(colnr_t)(ptr - line_start + 1), newfile) == FAIL)
			error = TRUE;
		else
			read_no_eol_lnum = ++lnum;
	}
	if (lnum != from && !newfile)	/* added at least one line */
		CHANGED;

	close(fd);						/* errors are ignored */
	vim_free(buffer);

	--no_wait_return;				/* may wait for return now */

	/* in recovery mode everything but autocommands are skipped */
	if (!recoverymode)
	{

		/* need to delete the last line, which comes from the empty buffer */
		if (newfile && !(curbuf->b_ml.ml_flags & ML_EMPTY))
		{
			ml_delete(curbuf->b_ml.ml_line_count, FALSE);
			--linecnt;
		}
		linecnt = curbuf->b_ml.ml_line_count - linecnt;
		if (filesize == 0)
			linecnt = 0;
		if (!newfile)
			mark_adjust(from + 1, MAXLNUM, (long)linecnt, 0L);

		if (got_int)
		{
			filemess(curbuf, fname, e_interr);
			msg_scroll = msg_save;
#ifdef VIMINFO
			check_marks_read();
#endif /* VIMINFO */
			return OK;			/* an interrupt isn't really an error */
		}

		if (!filtering)
		{
			msg_add_fname(curbuf, fname);	/* fname in IObuff with quotes */
			c = FALSE;

#ifdef UNIX
# ifdef S_ISFIFO
			if (S_ISFIFO(perm))						/* fifo or socket */
			{
				STRCAT(IObuff, "[fifo/socket]");
				c = TRUE;
			}
# else
#  ifdef S_IFIFO
			if ((perm & S_IFMT) == S_IFIFO)			/* fifo */
			{
				STRCAT(IObuff, "[fifo]");
				c = TRUE;
			}
#  endif
#  ifdef S_IFSOCK
			if ((perm & S_IFMT) == S_IFSOCK)		/* or socket */
			{
				STRCAT(IObuff, "[socket]");
				c = TRUE;
			}
#  endif
# endif
#endif
			if (curbuf->b_p_ro)
			{
				STRCAT(IObuff, shortmess(SHM_RO) ? "[RO]" : "[readonly]");
				c = TRUE;
			}
			if (read_no_eol_lnum)
			{
				msg_add_eol();
				c = TRUE;
			}
			if (tx_error)
			{
				STRCAT(IObuff, "[CR missing]");
				c = TRUE;
			}
			if (split)
			{
				STRCAT(IObuff, "[long lines split]");
				c = TRUE;
			}
			if (error)
			{
				STRCAT(IObuff, "[READ ERRORS]");
				c = TRUE;
			}
			if (msg_add_textmode(textmode))
				c = TRUE;
			msg_add_lines(c, (long)linecnt, filesize);
			msg_trunc(IObuff);
		}

		if (error && newfile)	/* with errors we should not write the file */
			curbuf->b_p_ro = TRUE;

		u_clearline();		/* cannot use "U" command after adding lines */

		if (from < curbuf->b_ml.ml_line_count)
		{
			curwin->w_cursor.lnum = from + 1;	/* cursor at first new line */
			beginline(TRUE);					/* on first non-blank */
		}

		/*
		 * Set '[ and '] marks to the newly read lines.
		 */
		curbuf->b_op_start.lnum = from + 1;
		curbuf->b_op_start.col = 0;
		curbuf->b_op_end.lnum = from + linecnt;
		curbuf->b_op_end.col = 0;
	}
	msg_scroll = msg_save;

#ifdef AUTOCMD
	{
		int	m = msg_scroll;
		int n = msg_scrolled;

		/*
		 * Trick: We remember if the last line of the read didn't have
		 * an eol for when writing it again.  This is required for
		 * ":autocmd FileReadPost *.gz set bin|'[,']!gunzip" to work.
		 */
		write_no_eol_lnum = read_no_eol_lnum;

		/*
		 * The output from the autocommands should not overwrite anything and
		 * should not be overwritten: Set msg_scroll, restore its value if no
		 * output was done.
		 */
		msg_scroll = TRUE;
		if (filtering)
			apply_autocmds(EVENT_FILTERREADPOST, NULL, fname);
		else if (newfile)
			apply_autocmds(EVENT_BUFREADPOST, NULL, fname);
		else
			apply_autocmds(EVENT_FILEREADPOST, fname, fname);
		if (msg_scrolled == n)
			msg_scroll = m;
	}
#endif

#ifdef VIMINFO
	check_marks_read();
#endif /* VIMINFO */

	if (recoverymode && error)
		return FAIL;
	return OK;
}

#ifdef VIMINFO
	static void
check_marks_read()
{
	if (!curbuf->b_marks_read && get_viminfo_parameter('\'') > 0)
	{
		read_viminfo(NULL, FALSE, TRUE, FALSE);
		curbuf->b_marks_read = TRUE;
	}
}
#endif /* VIMINFO */

/*
 * buf_write() - write to file 'fname' lines 'start' through 'end'
 *
 * We do our own buffering here because fwrite() is so slow.
 *
 * If forceit is true, we don't care for errors when attempting backups (jw).
 * In case of an error everything possible is done to restore the original file.
 * But when forceit is TRUE, we risk loosing it.
 * When reset_changed is TRUE and start == 1 and end ==
 * curbuf->b_ml.ml_line_count, reset curbuf->b_changed.
 *
 * This function must NOT use NameBuff (because it's called by autowrite()).
 *
 * return FAIL for failure, OK otherwise
 */
	int
buf_write(buf, fname, sfname, start, end, append, forceit,
													  reset_changed, filtering)
	BUF				*buf;
	char_u			*fname;
	char_u			*sfname;
	linenr_t		start, end;
	int				append;
	int				forceit;
	int				reset_changed;
	int				filtering;
{
	int 				fd;
	char_u			   *backup = NULL;
	char_u			   *ffname;
#ifdef AUTOCMD
	BUF				   *save_buf;
#endif
	register char_u	   *s;
	register char_u	   *ptr;
	register char_u		c;
	register int		len;
	register linenr_t	lnum;
	long				nchars;
	char_u				*errmsg = NULL;
	char_u				*buffer;
	char_u				smallbuf[SBUFSIZE];
	char_u				*backup_ext;
	int					bufsize;
	long 				perm = -1;			/* file permissions */
	int					retval = OK;
	int					newfile = FALSE;	/* TRUE if file doesn't exist yet */
	int					msg_save = msg_scroll;
	int					overwriting;		/* TRUE if writing over original */
	int					no_eol = FALSE;		/* no end-of-line written */
#if defined(UNIX) || defined(__EMX__XX) /*XXX fix me sometime? */
	struct stat			st_old;
	int					made_writable = FALSE;	/* 'w' bit has been set */
#endif
#ifdef AMIGA
	BPTR				flock;
#endif
											/* writing everything */
	int					whole = (start == 1 && end == buf->b_ml.ml_line_count);
#ifdef AUTOCMD
	linenr_t			old_line_count = buf->b_ml.ml_line_count;
#endif

	if (fname == NULL || *fname == NUL)		/* safety check */
		return FAIL;

	/*
	 * If there is no file name yet, use the one for the written file.
	 * b_notedited is set to reflect this (in case the write fails).
	 * Don't do this when the write is for a filter command.
	 * Only do this when 'cpoptions' contains the 'f' flag.
	 */
	if (reset_changed && whole && buf == curbuf &&
								   curbuf->b_filename == NULL && !filtering &&
										vim_strchr(p_cpo, CPO_FNAMEW) != NULL)
	{
		if (setfname(fname, sfname, FALSE) == OK)
			curbuf->b_notedited = TRUE;
	}

	if (sfname == NULL)
		sfname = fname;
	/*
	 * For Unix: Use the short filename whenever possible.
	 * Avoids problems with networks and when directory names are changed.
	 * Don't do this for MS-DOS, a "cd" in a sub-shell may have moved us to
	 * another directory, which we don't detect
	 */
	ffname = fname;							/* remember full fname */
#ifdef UNIX
	if (!did_cd)
		fname = sfname;
#endif

		/* make sure we have a valid backup extension to use */
	if (*p_bex == NUL)
		backup_ext = (char_u *)".bak";
	else
		backup_ext = p_bex;

	if (buf->b_filename != NULL && fnamecmp(ffname, buf->b_filename) == 0)
		overwriting = TRUE;
	else
		overwriting = FALSE;

	/*
	 * Disallow writing from .exrc and .vimrc in current directory for
	 * security reasons.
	 */
	if (secure)
	{
		secure = 2;
		emsg(e_curdir);
		return FAIL;
	}

	if (exiting)
		settmode(0);				/* when exiting allow typahead now */

	++no_wait_return;				/* don't wait for return yet */

	/*
	 * Set '[ and '] marks to the lines to be written.
	 */
	buf->b_op_start.lnum = start;
	buf->b_op_start.col = 0;
	buf->b_op_end.lnum = end;
	buf->b_op_end.col = 0;

#ifdef AUTOCMD
	/*
	 * Apply PRE aucocommands.
	 * Set curbuf to the buffer to be written.
	 * Careful: The autocommands may call buf_write() recursively!
	 */
	save_buf = curbuf;
	curbuf = buf;
	curwin->w_buffer = buf;
	if (append)
		apply_autocmds(EVENT_FILEAPPENDPRE, fname, fname);
	else if (filtering)
		apply_autocmds(EVENT_FILTERWRITEPRE, NULL, fname);
	else if (reset_changed && whole)
		apply_autocmds(EVENT_BUFWRITEPRE, fname, fname);
	else
		apply_autocmds(EVENT_FILEWRITEPRE, fname, fname);
	/*
	 * If the autocommands deleted or unloaded the buffer, give an error
	 * message.
	 */
	if (!buf_valid(buf) || buf->b_ml.ml_mfp == NULL)
	{
		--no_wait_return;
		msg_scroll = msg_save;
		EMSG("Autocommands deleted or unloaded buffer to be written");
		return FAIL;
	}
	/*
	 * If the autocommands didn't change the current buffer, go back to the
	 * original current buffer, if it still exists.
	 */
	if (curbuf == buf && buf_valid(save_buf))
	{
		curbuf = save_buf;
		curwin->w_buffer = save_buf;
	}
	
	/*
	 * The autocommands may have changed the number of lines in the file.
	 * When writing the whole file, adjust the end.
	 * When writing part of the file, assume that the autocommands only
	 * changed the number of lines that are to be written (tricky!).
	 */
	if (buf->b_ml.ml_line_count != old_line_count)
	{
		if (whole)											/* writing all */
			end = buf->b_ml.ml_line_count;
		else if (buf->b_ml.ml_line_count > old_line_count)	/* more lines */
			end += buf->b_ml.ml_line_count - old_line_count;
		else												/* less lines */
		{
			end -= old_line_count - buf->b_ml.ml_line_count;
			if (end < start)
			{
				--no_wait_return;
				msg_scroll = msg_save;
				EMSG("Autocommand changed number of lines in unexpected way");
				return FAIL;
			}
		}
	}
#endif

	if (shortmess(SHM_OVER))
		msg_scroll = FALSE;			/* overwrite previous file message */
	else
		msg_scroll = TRUE;			/* don't overwrite previous file message */
	if (!filtering)
		filemess(buf,
#ifndef UNIX
				did_cd ? fname : sfname,
#else
				fname,
#endif
					(char_u *)"");	/* show that we are busy */
	msg_scroll = FALSE;				/* always overwrite the file message now */

	buffer = alloc(BUFSIZE);
	if (buffer == NULL)				/* can't allocate big buffer, use small
									 * one (to be able to write when out of
									 * memory) */
	{
		buffer = smallbuf;
		bufsize = SBUFSIZE;
	}
	else
		bufsize = BUFSIZE;

#if defined(UNIX) && !defined(ARCHIE)
		/* get information about original file (if there is one) */
	st_old.st_dev = st_old.st_ino = 0;
	if (stat((char *)fname, &st_old))
		newfile = TRUE;
	else
	{
#ifdef _POSIX_SOURCE
		if (!S_ISREG(st_old.st_mode))      		/* not a file */
#else
		if ((st_old.st_mode & S_IFMT) != S_IFREG)	/* not a file */
#endif
		{
#ifdef _POSIX_SOURCE
			if (S_ISDIR(st_old.st_mode))
#else
			if ((st_old.st_mode & S_IFMT) == S_IFDIR)
#endif
				errmsg = (char_u *)"is a directory";
			else
				errmsg = (char_u *)"is not a file";
			goto fail;
		}
		if (buf->b_mtime_read != 0 &&
						  buf->b_mtime_read != st_old.st_mtime && overwriting)
		{
			msg_scroll = TRUE;		/* don't overwrite messages here */
			(void)set_highlight('e');	/* set highlight for error messages */
			msg_highlight = TRUE;
			/* don't use emsg() here, don't want to flush the buffers */
			MSG("WARNING: The file has been changed since reading it!!!");
			if (ask_yesno((char_u *)"Do you really want to write to it",
																 TRUE) == 'n')
			{
				retval = FAIL;
				goto fail;
			}
			msg_scroll = FALSE;		/* always overwrite the file message now */
		}
		perm = st_old.st_mode;
	}
/*
 * If we are not appending, the file exists, and the 'writebackup', 'backup'
 * or 'patchmode' option is set, try to make a backup copy of the file.
 */
	if (!append && perm >= 0 && (p_wb || p_bk || *p_pm != NUL) &&
						  (fd = open((char *)fname, O_RDONLY | O_EXTRA)) >= 0)
	{
		int				bfd, buflen;
		char_u			copybuf[BUFSIZE + 1], *wp;
		int				some_error = FALSE;
		struct stat		st_new;
		char_u			*dirp;
		char_u			*rootname;
#ifndef SHORT_FNAME
		int				did_set_shortname;
#endif

		/*
		 * Try to make the backup in each directory in the 'bdir' option.
		 *
		 * Unix semantics has it, that we may have a writable file, 
		 * that cannot be recreated with a simple open(..., O_CREAT, ) e.g:
		 *  - the directory is not writable, 
		 *  - the file may be a symbolic link, 
		 *  - the file may belong to another user/group, etc.
		 *
		 * For these reasons, the existing writable file must be truncated
		 * and reused. Creation of a backup COPY will be attempted.
		 */
		dirp = p_bdir;
		while (*dirp)
		{
			st_new.st_dev = st_new.st_ino = 0;
			st_new.st_gid = 0;

			/*
			 * Isolate one directory name, using an entry in 'bdir'.
			 */
			(void)copy_option_part(&dirp, copybuf, BUFSIZE, ",");
			rootname = get_file_in_dir(fname, copybuf);
			if (rootname == NULL)
			{
				some_error = TRUE;			/* out of memory */
				goto nobackup;
			}


#ifndef SHORT_FNAME
			did_set_shortname = FALSE;
#endif

			/*
			 * May try twice if 'shortname' not set.
			 */
			for (;;)
			{
				/*
				 * Make backup file name.
				 */
				backup = buf_modname(buf, rootname, backup_ext);
				if (backup == NULL)
				{
					some_error = TRUE;			/* out of memory */
					vim_free(rootname);
					goto nobackup;
				}

				/*
				 * Check if backup file already exists.
				 */
				if (!stat((char *)backup, &st_new))
				{
					/*
					 * Check if backup file is same as original file.
					 * May happen when modname gave the same file back.
					 * E.g. silly link, or filename-length reached.
					 * If we don't check here, we either ruin the file when
					 * copying or erase it after writing. jw.
					 */
					if (st_new.st_dev == st_old.st_dev &&
										   st_new.st_ino == st_old.st_ino)
					{
						vim_free(backup);
						backup = NULL;	/* there is no backup file to delete */
#ifndef SHORT_FNAME
						/*
						 * may try again with 'shortname' set
						 */
						if (!(buf->b_shortname || buf->b_p_sn))
						{
							buf->b_shortname = TRUE;
							did_set_shortname = TRUE;
							continue;
						}
							/* setting shortname didn't help */
						if (did_set_shortname)
							buf->b_shortname = FALSE;
#endif
						break;
					}

					/*
					 * If we are not going to keep the backup file, don't
					 * delete an existing one, try to use another name.
					 * Change one character, just before the extension.
					 */
					if (!p_bk)
					{
						wp = backup + STRLEN(backup) - 1 - STRLEN(backup_ext);
						if (wp < backup)		/* empty file name ??? */
							wp = backup;
						*wp = 'z';
						while (*wp > 'a' && !stat((char *)backup, &st_new))
							--*wp;
						/* They all exist??? Must be something wrong. */
						if (*wp == 'a')
						{
							vim_free(backup);
							backup = NULL;
						}
					}
				}
				break;
			}
			vim_free(rootname);

			/*
			 * Try to create the backup file
			 */
			if (backup != NULL)
			{
				/* remove old backup, if present */
				vim_remove(backup);
				bfd = open((char *)backup, O_WRONLY | O_CREAT | O_EXTRA, 0666);
				if (bfd < 0)
				{
					vim_free(backup);
					backup = NULL;
				}
				else
				{
					/* set file protection same as original file, but strip
					 * s-bit */
					(void)setperm(backup, perm & 0777);

					/*
					 * Try to set the group of the backup same as the original
					 * file. If this fails, set the protection bits for the
					 * group same as the protection bits for others.
					 */
					if (st_new.st_gid != st_old.st_gid &&
#ifdef HAVE_FCHOWN	/* sequent-ptx lacks fchown() */
										  fchown(bfd, -1, st_old.st_gid) != 0)
#else
										chown(backup, -1, st_old.st_gid) != 0)
#endif
						setperm(backup, (perm & 0707) | ((perm & 07) << 3));

					/* copy the file. */
					while ((buflen = read(fd, (char *)copybuf, BUFSIZE)) > 0)
					{
						if (write_buf(bfd, copybuf, buflen) == FAIL)
						{
							errmsg = (char_u *)"Can't write to backup file (use ! to override)";
							break;
						}
					}
					if (close(bfd) < 0 && errmsg == NULL)
						errmsg = (char_u *)"Close error for backup file (use ! to override)";
					if (buflen < 0)
						errmsg = (char_u *)"Can't read file for backup (use ! to override)";
					break;
				}
			}
		}
nobackup:
		close(fd);				/* ignore errors for closing read file */

		if (backup == NULL && errmsg == NULL)
			errmsg = (char_u *)"Cannot create backup file (use ! to override)";
		/* ignore errors when forceit is TRUE */
		if ((some_error || errmsg) && !forceit)
		{
			retval = FAIL;
			goto fail;
		}
		errmsg = NULL;
	}
	/* When using ":w!" and the file was read-only: make it writable */
	if (forceit && (st_old.st_uid == getuid()) && perm >= 0 && !(perm & 0200))
 	{
		perm |= 0200;	
		(void)setperm(fname, perm);
		made_writable = TRUE;
	}

#else /* end of UNIX, start of the rest */

/*
 * If we are not appending, the file exists, and the 'writebackup' or
 * 'backup' option is set, make a backup.
 * Do not make any backup, if "writebackup" and "backup" are 
 * both switched off. This helps when editing large files on
 * almost-full disks. (jw)
 */
	perm = getperm(fname);
	if (perm < 0)
		newfile = TRUE;
	else if (mch_isdir(fname))
	{
		errmsg = (char_u *)"is a directory";
		goto fail;
	}
	if (!append && perm >= 0 && (p_wb || p_bk || *p_pm != NUL))
	{
		char_u			*dirp;
		char_u			*p;
		char_u			*rootname;

		/*
		 * Form the backup file name - change path/fo.o.h to path/fo.o.h.bak
		 * Try all directories in 'backupdir', first one that works is used.
		 */
		dirp = p_bdir;
		while (*dirp)
		{
			/*
			 * Isolate one directory name and make the backup file name.
			 */
			(void)copy_option_part(&dirp, IObuff, IOSIZE, ",");
			rootname = get_file_in_dir(fname, IObuff);
			if (rootname == NULL)
				backup = NULL;
			else
			{
				backup = buf_modname(buf, rootname, backup_ext);
				vim_free(rootname);
			}

			if (backup != NULL)
			{
				/*
				 * If we are not going to keep the backup file, don't
				 * delete an existing one, try to use another name.
				 * Change one character, just before the extension.
				 */
				if (!p_bk && getperm(backup) >= 0)
				{
					p = backup + STRLEN(backup) - 1 - STRLEN(backup_ext);
					if (p < backup)		/* empty file name ??? */
						p = backup;
					*p = 'z';
					while (*p > 'a' && getperm(backup) >= 0)
						--*p;
					/* They all exist??? Must be something wrong! */
					if (*p == 'a')
					{
						vim_free(backup);
						backup = NULL;
					}
				}
			}
			if (backup != NULL)
			{

				/*
				 * Delete any existing backup and move the current version to
				 * the backup.  For safety, we don't remove the backup until
				 * the write has finished successfully. And if the 'backup'
				 * option is set, leave it around.
				 */
#ifdef AMIGA
				/*
				 * With MSDOS-compatible filesystems (crossdos, messydos) it is
				 * possible that the name of the backup file is the same as the
				 * original file. To avoid the chance of accidently deleting the
				 * original file (horror!) we lock it during the remove.
				 * This should not happen with ":w", because startscript()
				 * should detect this problem and set buf->b_shortname,
				 * causing modname to return a correct ".bak" filename. This
				 * problem does exist with ":w filename", but then the
				 * original file will be somewhere else so the backup isn't
				 * really important. If autoscripting is off the rename may
				 * fail.
				 */
				flock = Lock((UBYTE *)fname, (long)ACCESS_READ);
#endif
				vim_remove(backup);
#ifdef AMIGA
				if (flock)
					UnLock(flock);
#endif
				/*
				 * If the renaming of the original file to the backup file
				 * works, quit here.
				 */
				if (vim_rename(fname, backup) == 0)
					break;

				vim_free(backup);	/* don't do the rename below */
				backup = NULL;
			}
		}
		if (backup == NULL && !forceit)
		{
			errmsg = (char_u *)"Can't make backup file (use ! to override)";
			goto fail;
		}
	}
#endif /* UNIX */

	/* When using ":w!" and writing to the current file, readonly makes no
	 * sense, reset it */
	if (forceit && overwriting)
		buf->b_p_ro = FALSE;

	/*
	 * If the original file is being overwritten, there is a small chance that
	 * we crash in the middle of writing. Therefore the file is preserved now.
	 * This makes all block numbers positive so that recovery does not need
	 * the original file.
	 * Don't do this if there is a backup file and we are exiting.
	 */
	if (reset_changed && !newfile && !otherfile(ffname) &&
											!(exiting && backup != NULL))
		ml_preserve(buf, FALSE);

	/* 
	 * We may try to open the file twice: If we can't write to the
	 * file and forceit is TRUE we delete the existing file and try to create
	 * a new one. If this still fails we may have lost the original file!
	 * (this may happen when the user reached his quotum for number of files).
	 * Appending will fail if the file does not exist and forceit is FALSE.
	 */
	while ((fd = open((char *)fname, O_WRONLY | O_EXTRA | (append ?
					(forceit ? (O_APPEND | O_CREAT) : O_APPEND) :
					(O_CREAT | O_TRUNC)), 0666)) < 0)
 	{
		/*
		 * A forced write will try to create a new file if the old one is
		 * still readonly. This may also happen when the directory is
		 * read-only. In that case the vim_remove() will fail.
		 */
		if (!errmsg)
		{
			errmsg = (char_u *)"Can't open file for writing";
			if (forceit)
			{
#ifdef UNIX
				/* we write to the file, thus it should be marked
													writable after all */
				perm |= 0200;		
				made_writable = TRUE;
				if (st_old.st_uid != getuid() || st_old.st_gid != getgid())
					perm &= 0777;
#endif /* UNIX */
				if (!append)		/* don't remove when appending */
					vim_remove(fname);
				continue;
			}
		}
/*
 * If we failed to open the file, we don't need a backup. Throw it away.
 * If we moved or removed the original file try to put the backup in its place.
 */
 		if (backup != NULL)
		{
#ifdef UNIX
			struct stat st;

			/*
			 * There is a small chance that we removed the original, try
			 * to move the copy in its place.
			 * This may not work if the vim_rename() fails.
			 * In that case we leave the copy around.
			 */
			 							/* file does not exist */
			if (stat((char *)fname, &st) < 0)
										/* put the copy in its place */
				vim_rename(backup, fname);
										/* original file does exist */
			if (stat((char *)fname, &st) >= 0)
				vim_remove(backup);	/* throw away the copy */
#else
										/* try to put the original file back */
 			vim_rename(backup, fname);
#endif
		}
 		goto fail;
 	}
	errmsg = NULL;

	if (end > buf->b_ml.ml_line_count)
		end = buf->b_ml.ml_line_count;
	len = 0;
	s = buffer;
	nchars = 0;
	if (buf->b_ml.ml_flags & ML_EMPTY)
		start = end + 1;
	for (lnum = start; lnum <= end; ++lnum)
	{
		/*
		 * The next while loop is done once for each character written.
		 * Keep it fast!
		 */
		ptr = ml_get_buf(buf, lnum, FALSE) - 1;
		while ((c = *++ptr) != NUL)
		{
			if (c == NL)
				*s = NUL;		/* replace newlines with NULs */
			else
				*s = c;
			++s;
			if (++len != bufsize)
				continue;
			if (write_buf(fd, buffer, bufsize) == FAIL)
			{
				end = 0;				/* write error: break loop */
				break;
			}
			nchars += bufsize;
			s = buffer;
			len = 0;
		}
			/* write failed or last line has no EOL: stop here */
		if (end == 0 || (lnum == end && buf->b_p_bin &&
												(lnum == write_no_eol_lnum ||
						 (lnum == buf->b_ml.ml_line_count && !buf->b_p_eol))))
		{
			++lnum;				/* written the line, count it */
			no_eol = TRUE;
			break;
		}
		if (buf->b_p_tx)		/* write CR-NL */
		{
			*s = CR;
			++s;
			if (++len == bufsize)
			{
				if (write_buf(fd, buffer, bufsize) == FAIL)
				{
					end = 0;				/* write error: break loop */
					break;
				}
				nchars += bufsize;
				s = buffer;
				len = 0;
			}
		}
		*s = NL;
		++s;
		if (++len == bufsize && end)
		{
			if (write_buf(fd, buffer, bufsize) == FAIL)
			{
				end = 0;				/* write error: break loop */
				break;
			}
			nchars += bufsize;
			s = buffer;
			len = 0;
		}
	}
	if (len && end)
	{
		if (write_buf(fd, buffer, len) == FAIL)
			end = 0;				/* write error */
		nchars += len;
	}

	if (close(fd) != 0)
	{
		errmsg = (char_u *)"Close failed";
		goto fail;
	}
#ifdef UNIX
	if (made_writable)
		perm &= ~0200;			/* reset 'w' bit for security reasons */
#endif
	if (perm >= 0)
		(void)setperm(fname, perm);	/* set permissions of new file same as old file */

	if (end == 0)
	{
		errmsg = (char_u *)"write error (file system full?)";
		/*
		 * If we have a backup file, try to put it in place of the new file,
		 * because it is probably corrupt. This avoids loosing the original
		 * file when trying to make a backup when writing the file a second
		 * time.
		 * For unix this means copying the backup over the new file.
		 * For others this means renaming the backup file.
		 * If this is OK, don't give the extra warning message.
		 */
		if (backup != NULL)
		{
#ifdef UNIX
			char_u		copybuf[BUFSIZE + 1];
			int			bfd, buflen;

			if ((bfd = open((char *)backup, O_RDONLY | O_EXTRA)) >= 0)
			{
				if ((fd = open((char *)fname,
						  O_WRONLY | O_CREAT | O_TRUNC | O_EXTRA, 0666)) >= 0)
				{
					/* copy the file. */
					while ((buflen = read(bfd, (char *)copybuf, BUFSIZE)) > 0)
						if (write_buf(fd, copybuf, buflen) == FAIL)
							break;
					if (close(fd) >= 0 && buflen == 0)	/* success */
						end = 1;
				}
				close(bfd);		/* ignore errors for closing read file */
			}
#else
			if (vim_rename(backup, fname) == 0)
				end = 1;
#endif
		}
		goto fail;
	}

	lnum -= start;			/* compute number of written lines */
	--no_wait_return;		/* may wait for return now */

#ifndef UNIX
	/* use shortname now, for the messages */
	if (!did_cd)
		fname = sfname;
#endif
	if (!filtering)
	{
		msg_add_fname(buf, fname);		/* put fname in IObuff with quotes */
		c = FALSE;
		if (newfile)
		{
			STRCAT(IObuff, shortmess(SHM_NEW) ? "[New]" : "[New File]");
			c = TRUE;
		}
		if (no_eol)
		{
			msg_add_eol();
			c = TRUE;
		}
		if (msg_add_textmode(buf->b_p_tx))		/* may add [textmode] */
			c = TRUE;
		msg_add_lines(c, (long)lnum, nchars);	/* add line/char count */
		if (!shortmess(SHM_WRITE))
			STRCAT(IObuff, shortmess(SHM_WRI) ? " [w]" : " written");

		msg_trunc(IObuff);
	}

	if (reset_changed && whole)			/* when written everything */
	{
		UNCHANGED(buf);
		u_unchanged(buf);
	}

	/*
	 * If written to the current file, update the timestamp of the swap file
	 * and reset the 'notedited' flag. Also sets buf->b_mtime.
	 */
	if (!exiting && overwriting)
	{
		ml_timestamp(buf);
		buf->b_notedited = FALSE;
	}

	/*
	 * If we kept a backup until now, and we are in patch mode, then we make
	 * the backup file our 'original' file.
	 */
	if (*p_pm)
	{
	    char *org = (char *)buf_modname(buf, fname, p_pm);

		if (backup != NULL)
		{
		    struct stat st;

			/*
			 * If the original file does not exist yet
			 * the current backup file becomes the original file
			 */
		    if (org == NULL)
				EMSG("patchmode: can't save original file");
			else if (stat(org, &st) < 0)
			{
			    vim_rename(backup, (char_u *)org);
				vim_free(backup);			/* don't delete the file */
				backup = NULL;
			}
		}
	    /*
		 * If there is no backup file, remember that a (new) file was
		 * created.
		 */
	    else
		{
		    int empty_fd;

			if (org == NULL || (empty_fd =
									  open(org, O_CREAT | O_EXTRA, 0666)) < 0)
			  EMSG("patchmode: can't touch empty original file");
		    else
			  close(empty_fd);
		}
	    if (org != NULL)
		{
		    setperm((char_u *)org, getperm(fname) & 0777);
			vim_free(org);
		}
	}

	/*
	 * Remove the backup unless 'backup' option is set
	 */
	if (!p_bk && backup != NULL && vim_remove(backup) != 0)
		EMSG("Can't delete backup file");
	
	goto nofail;

fail:
	--no_wait_return;		/* may wait for return now */
nofail:

	vim_free(backup);
	if (buffer != smallbuf)
		vim_free(buffer);

	if (errmsg != NULL)
	{
		/* can't use emsg() here, do something alike */
		if (p_eb)
			beep_flush();			/* also includes flush_buffers() */
		else
			flush_buffers(FALSE);	/* flush internal buffers */
		(void)set_highlight('e');	/* set highlight mode for error messages */
		start_highlight();
		filemess(buf,
#ifndef UNIX
						did_cd ? fname : sfname,
#else
						fname,
#endif
													errmsg);
		retval = FAIL;
		if (end == 0)
		{
			MSG_OUTSTR("\nWARNING: Original file may be lost or damaged\n");
			MSG_OUTSTR("don't quit the editor until the file is successfully written!");
		}
	}
	msg_scroll = msg_save;

#ifdef AUTOCMD
	write_no_eol_lnum = 0;		/* in case it was set by the previous read */

	/*
	 * Apply POST autocommands.
	 * Careful: The autocommands may call buf_write() recursively!
	 */
	save_buf = curbuf;
	curbuf = buf;
	curwin->w_buffer = buf;
	if (append)
		apply_autocmds(EVENT_FILEAPPENDPOST, fname, fname);
	else if (filtering)
		apply_autocmds(EVENT_FILTERWRITEPOST, NULL, fname);
	else if (reset_changed && whole)
		apply_autocmds(EVENT_BUFWRITEPOST, fname, fname);
	else
		apply_autocmds(EVENT_FILEWRITEPOST, fname, fname);
	/*
	 * If the autocommands didn't change the current buffer, go back to the
	 * original current buffer, if it still exists.
	 */
	if (curbuf == buf && buf_valid(save_buf))
	{
		curbuf = save_buf;
		curwin->w_buffer = save_buf;
	}
#endif

	return retval;
}

/*
 * Put file name into IObuff with quotes.
 */
	static void
msg_add_fname(buf, fname)
	BUF		*buf;
	char_u	*fname;
{
		/* careful: home_replace calls vim_getenv(), which also uses IObuff! */
	home_replace(buf, fname, IObuff + 1, IOSIZE - 1);
	IObuff[0] = '"';
	STRCAT(IObuff, "\" ");
}

/*
 * Append message for text mode to IObuff.
 * Return TRUE if something appended.
 */
	static int
msg_add_textmode(textmode)
	int		textmode;
{
#ifdef USE_CRNL
	if (!textmode)
	{
		STRCAT(IObuff, shortmess(SHM_TEXT) ? "[notx]" : "[notextmode]");
		return TRUE;
	}
#else
	if (textmode)
	{
		STRCAT(IObuff, shortmess(SHM_TEXT) ? "[tx]" : "[textmode]");
		return TRUE;
	}
#endif
	return FALSE;
}

/*
 * Append line and character count to IObuff.
 */
	static void
msg_add_lines(insert_space, lnum, nchars)
	int		insert_space;
	long	lnum;
	long	nchars;
{
	char_u	*p;

	p = IObuff + STRLEN(IObuff);

	if (insert_space)
		*p++ = ' ';
	if (shortmess(SHM_LINES))
		sprintf((char *)p, "%ldL, %ldC", lnum, nchars);
	else
		sprintf((char *)p, "%ld line%s, %ld character%s",
			lnum, plural(lnum),
			nchars, plural(nchars));
}

/*
 * Append message for missing line separator to IObuff.
 */
	static void
msg_add_eol()
{
	STRCAT(IObuff, shortmess(SHM_LAST) ? "[noeol]" : "[Incomplete last line]");
}

/*
 * write_buf: call write() to write a buffer
 *
 * return FAIL for failure, OK otherwise
 */
	static int
write_buf(fd, buf, len)
	int		fd;
	char_u	*buf;
	int		len;
{
	int		wlen;

	while (len)
	{
		wlen = write(fd, (char *)buf, (size_t)len);
		if (wlen <= 0)				/* error! */
			return FAIL;
		len -= wlen;
		buf += wlen;
	}
	return OK;
}

/*
 * add extention to filename - change path/fo.o.h to path/fo.o.h.ext or
 * fo_o_h.ext for MSDOS or when shortname option set.
 *
 * Assumed that fname is a valid name found in the filesystem we assure that
 * the return value is a different name and ends in 'ext'.
 * "ext" MUST be at most 4 characters long if it starts with a dot, 3
 * characters otherwise.
 * Space for the returned name is allocated, must be freed later.
 */

	char_u *
modname(fname, ext)
	char_u *fname, *ext;
{
	return buf_modname(curbuf, fname, ext);
}

	char_u *
buf_modname(buf, fname, ext)
	BUF		*buf;
	char_u *fname, *ext;
{
	char_u				*retval;
	register char_u 	*s;
	register char_u		*e;
	register char_u		*ptr;
	register int		fnamelen, extlen;

	extlen = STRLEN(ext);

	/*
	 * if there is no filename we must get the name of the current directory
	 * (we need the full path in case :cd is used)
	 */
	if (fname == NULL || *fname == NUL)
	{
		retval = alloc((unsigned)(MAXPATHL + extlen + 3));
		if (retval == NULL)
			return NULL;
		if (mch_dirname(retval, MAXPATHL) == FAIL ||
											 (fnamelen = STRLEN(retval)) == 0)
		{
			vim_free(retval);
			return NULL;
		}
		if (!ispathsep(retval[fnamelen - 1]))
		{
			retval[fnamelen++] = PATHSEP;
			retval[fnamelen] = NUL;
		}
	}
	else
	{
		fnamelen = STRLEN(fname);
		retval = alloc((unsigned)(fnamelen + extlen + 2));
		if (retval == NULL)
			return NULL;
		STRCPY(retval, fname);
	}

	/*
	 * search backwards until we hit a '/', '\' or ':' replacing all '.'
	 * by '_' for MSDOS or when shortname option set and ext starts with a dot.
	 * Then truncate what is after the '/', '\' or ':' to 8 characters for
	 * MSDOS and 26 characters for AMIGA, a lot more for UNIX.
	 */
	for (ptr = retval + fnamelen; ptr >= retval; ptr--)
	{
		if (*ext == '.'
#ifdef USE_LONG_FNAME
					&& (!USE_LONG_FNAME || buf->b_p_sn || buf->b_shortname)
#else
# ifndef SHORT_FNAME
					&& (buf->b_p_sn || buf->b_shortname)
# endif
#endif
																)
			if (*ptr == '.')	/* replace '.' by '_' */
				*ptr = '_';
		if (ispathsep(*ptr))
			break;
	}
	ptr++;

	/* the filename has at most BASENAMELEN characters. */
#ifndef SHORT_FNAME
	if (STRLEN(ptr) > (unsigned)BASENAMELEN)
		ptr[BASENAMELEN] = '\0';
#endif

	s = ptr + STRLEN(ptr);

	/*
	 * For 8.3 filenames we may have to reduce the length.
	 */
#ifdef USE_LONG_FNAME
	if (!USE_LONG_FNAME || buf->b_p_sn || buf->b_shortname)
#else
# ifndef SHORT_FNAME
	if (buf->b_p_sn || buf->b_shortname)
# endif
#endif
	{
		/*
		 * If there is no file name, and the extension starts with '.', put a
		 * '_' before the dot, because just ".ext" is invalid.
		 */
		if (fname == NULL || *fname == NUL)
		{
			if (*ext == '.')
				*s++ = '_';
		}
		/*
		 * If the extension starts with '.', truncate the base name at 8
		 * characters
		 */
		else if (*ext == '.')
		{
			if (s - ptr > (size_t)8)
			{
				s = ptr + 8;
				*s = '\0';
			}
		}
		/*
		 * If the extension doesn't start with '.', and the file name
		 * doesn't have an extension yet, append a '.'
		 */
		else if ((e = vim_strchr(ptr, '.')) == NULL)
			*s++ = '.';
		/*
		 * If If the extension doesn't start with '.', and there already is an
		 * extension, it may need to be tructated
		 */
		else if ((int)STRLEN(e) + extlen > 4)
			s = e + 4 - extlen;
	}
#ifdef OS2
	/*
	 * If there is no file name, and the extension starts with '.', put a
	 * '_' before the dot, because just ".ext" may be invalid if it's on a
	 * FAT partition, and on HPFS it doesn't matter.
	 */
	else if ((fname == NULL || *fname == NUL) && *ext == '.')
		*s++ = '_';
#endif

	/*
	 * Append the extention.
	 * ext can start with '.' and cannot exceed 3 more characters.
	 */
	STRCPY(s, ext);

	/*
	 * Check that, after appending the extension, the file name is really
	 * different.
	 */
	if (fname != NULL && STRCMP(fname, retval) == 0)
	{
		/* we search for a character that can be replaced by '_' */
		while (--s >= ptr)
		{
			if (*s != '_')
			{
				*s = '_';
				break;
			}
		}
		if (s < ptr)	/* fname was "________.<ext>" how tricky! */
			*ptr = 'v';
	}
	return retval;
}

/* vim_fgets();
 *
 * Like fgets(), but if the file line is too long, it is truncated and the
 * rest of the line is thrown away.  Returns TRUE for end-of-file.
 * Note: do not pass IObuff as the buffer since this is used to read and
 * discard the extra part of any long lines.
 */
	int
vim_fgets(buf, size, fp)
	char_u		*buf;
	int			size;
	FILE		*fp;
{
	char *eof;

	buf[size - 2] = NUL;
	eof = fgets((char *)buf, size, fp);
	if (buf[size - 2] != NUL && buf[size - 2] != '\n')
	{
		buf[size - 1] = NUL;		/* Truncate the line */

		/* Now throw away the rest of the line: */
		do
		{
			IObuff[IOSIZE - 2] = NUL;
			fgets((char *)IObuff, IOSIZE, fp);
		} while (IObuff[IOSIZE - 2] != NUL && IObuff[IOSIZE - 2] != '\n');
	}
	return (eof == NULL);
}

/*
 * rename() only works if both files are on the same file system, this
 * function will (attempts to?) copy the file across if rename fails -- webb
 * Return -1 for failure, 0 for success.
 */
	int
vim_rename(from, to)
	char_u *from;
	char_u *to;
{
	int		fd_in;
	int		fd_out;
	int		n;
	char 	*errmsg = NULL;

	/*
	 * First delete the "to" file, this is required on some systems to make
	 * the rename() work, on other systems it makes sure that we don't have
	 * two files when the rename() fails.
	 */
	vim_remove(to);

	/*
	 * First try a normal rename, return if it works.
	 */
	if (rename((char *)from, (char *)to) == 0)
		return 0;

	/*
	 * Rename() failed, try copying the file.
	 */
	fd_in = open((char *)from, O_RDONLY | O_EXTRA);
	if (fd_in == -1)
		return -1;
	fd_out = open((char *)to, O_CREAT | O_TRUNC | O_WRONLY | O_EXTRA, 0666);
	if (fd_out == -1)
	{
		close(fd_in);
		return -1;
	}
	while ((n = read(fd_in, (char *)IObuff, (size_t)IOSIZE)) > 0)
		if (write(fd_out, (char *)IObuff, (size_t)n) != n)
		{
			errmsg = "writing to";
			break;
		}
	close(fd_in);
	if (close(fd_out) < 0)
		errmsg = "closing";
	if (n < 0)
	{
		errmsg = "reading";
		to = from;
	}
	if (errmsg != NULL)
	{
		sprintf((char *)IObuff, "Error %s '%s'", errmsg, to);
		emsg(IObuff);
		return -1;
	}
	vim_remove(from);
	return 0;
}

/*
 * Check if any not hidden buffer has been changed.
 * Postpone the check if there are characters in the stuff buffer, a global
 * command is being executed, a mapping is being executed or an autocommand is
 * busy.
 */
	void
check_timestamps()
{
	BUF		*buf;

	if (!stuff_empty() || global_busy || !typebuf_typed()
#ifdef AUTOCMD
						|| autocmd_busy
#endif
										)
		need_check_timestamps = TRUE;			/* check later */
	else
	{
		++no_wait_return;
		for (buf = firstbuf; buf != NULL; buf = buf->b_next)
			buf_check_timestamp(buf);
		--no_wait_return;
		need_check_timestamps = FALSE;
	}
}

/*
 * Check if buffer "buf" has been changed.
 */
	void
buf_check_timestamp(buf)
	BUF		*buf;
{
	struct stat		st;
	char_u			*path;

	if (	buf->b_filename != NULL &&
			buf->b_ml.ml_mfp != NULL &&
			!buf->b_notedited &&
			buf->b_mtime != 0 &&
			stat((char *)buf->b_filename, &st) >= 0 &&
			buf->b_mtime != st.st_mtime)
	{
		path = home_replace_save(buf, buf->b_xfilename);
		if (path != NULL)
		{
			EMSG2("Warning: File \"%s\" has changed since editing started",
																path);
			buf->b_mtime = st.st_mtime;
			vim_free(path);
		}
	}
}

/*
 * Adjust the line with missing eol, used for the next write.
 * Used for do_filter(), when the input lines for the filter are deleted.
 */
	void
write_lnum_adjust(offset)
	linenr_t	offset;
{
	if (write_no_eol_lnum)				/* only if there is a missing eol */
		write_no_eol_lnum += offset;
}

#ifdef USE_TMPNAM
extern char		*tmpnam __ARGS((char *));
#else
extern char		*mktemp __ARGS((char *));
#endif

/*
 * vim_tempname(): Return a unique name that can be used for a temp file.
 *
 * The temp file is NOT created.
 *
 * The returned pointer is to allocated memory.
 * The returned pointer is NULL if no valid name was found.
 */
	char_u	*
vim_tempname(extra_char)
	int		extra_char;		/* character to use in the name instead of '?' */
{
#ifdef USE_TMPNAM
	char_u			itmp[L_tmpnam];		/* use tmpnam() */
#else
	char_u			itmp[TEMPNAMELEN];
#endif
	char_u			*p;

#if defined(TEMPDIRNAMES)
	static char		*(tempdirs[]) = {TEMPDIRNAMES};
	static int		first_dir = 0;
	int				first_try = TRUE;
	int				i;

	/*
	 * Try a few places to put the temp file.
	 * To avoid waisting time with non-existing environment variables and
	 * directories, they are skipped next time.
	 */
	for (i = first_dir; i < sizeof(tempdirs) / sizeof(char *); ++i)
	{
		/* expand $TMP, leave room for '/', "v?XXXXXX" and NUL */
		expand_env((char_u *)tempdirs[i], itmp, TEMPNAMELEN - 10);
		if (mch_isdir(itmp))			/* directory exists */
		{
			if (first_try)
				first_dir = i;			/* start here next time */
			first_try = FALSE;
#ifdef BACKSLASH_IN_FILENAME
			/*
			 * really want a backslash here, because the filename will
			 * probably be used in a command line
			 */
			STRCAT(itmp, "\\");	
#else
			STRCAT(itmp, PATHSEPSTR);
#endif
			STRCAT(itmp, TEMPNAME);
			if ((p = vim_strchr(itmp, '?')) != NULL)
				*p = extra_char;
			if (*mktemp((char *)itmp) == NUL)
				continue;
			return strsave(itmp);
		}
	}
	return NULL;
#else

# ifndef USE_TMPNAM		 /* tmpnam() will make its own name */
	STRCPY(itmp, TEMPNAME);
# endif
	if ((p = vim_strchr(itmp, '?')) != NULL)
		*p = extra_char;
# ifdef USE_TMPNAM
	if (*tmpnam((char *)itmp) == NUL)
# else
	if (*mktemp((char *)itmp) == NUL)
# endif
		return NULL;
	return strsave(itmp);
#endif
}
