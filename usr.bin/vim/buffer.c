/*	$OpenBSD: buffer.c,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * buffer.c: functions for dealing with the buffer structure
 */

/*
 * The buffer list is a double linked list of all buffers.
 * Each buffer can be in one of these states:
 * never loaded: b_neverloaded == TRUE, only the file name is valid
 *   not loaded: b_ml.ml_mfp == NULL, no memfile allocated
 *       hidden: b_nwindows == 0, loaded but not displayed in a window
 *       normal: loaded and displayed in a window
 *
 * Instead of storing file names all over the place, each file name is
 * stored in the buffer list. It can be referenced by a number.
 *
 * The current implementation remembers all file names ever used.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

static void		enter_buffer __ARGS((BUF *));
static void		free_buf_options __ARGS((BUF *));
static char_u	*buflist_match __ARGS((regexp *prog, BUF *buf));
static void		buflist_setlnum __ARGS((BUF *, linenr_t));
static linenr_t buflist_findlnum __ARGS((BUF *));
static void		append_arg_number __ARGS((char_u *, int));

/*
 * Open current buffer, that is: open the memfile and read the file into memory
 * return FAIL for failure, OK otherwise
 */
 	int
open_buffer()
{
	int		retval = OK;

	/*
	 * The 'readonly' flag is only set when b_neverloaded is being reset.
	 * When re-entering the same buffer, it should not change, because the
	 * user may have reset the flag by hand.
	 */
	if (readonlymode && curbuf->b_filename != NULL && curbuf->b_neverloaded)
		curbuf->b_p_ro = TRUE;

	if (ml_open() == FAIL)
	{
		/*
		 * There MUST be a memfile, otherwise we can't do anything
		 * If we can't create one for the current buffer, take another buffer
		 */
		close_buffer(NULL, curbuf, FALSE, FALSE);
		for (curbuf = firstbuf; curbuf != NULL; curbuf = curbuf->b_next)
			if (curbuf->b_ml.ml_mfp != NULL)
				break;
		/*
		 * if there is no memfile at all, exit
		 * This is OK, since there are no changes to loose.
		 */
		if (curbuf == NULL)
		{
			EMSG("Cannot allocate buffer, exiting...");
			getout(2);
		}
		EMSG("Cannot allocate buffer, using other one...");
		enter_buffer(curbuf);
		return FAIL;
	}
	if (curbuf->b_filename != NULL)
		retval = readfile(curbuf->b_filename, curbuf->b_sfilename,
							  (linenr_t)0, TRUE, (linenr_t)0, MAXLNUM, FALSE);
	else
	{
		MSG("Empty Buffer");
		msg_col = 0;
		msg_didout = FALSE;		/* overwrite this message whenever you like */
	}

	/* if first time loading this buffer, init chartab */
	if (curbuf->b_neverloaded)
		init_chartab();

	/*
	 * Reset the Changed flag first, autocmds may change the buffer.
	 * Apply the automatic commands, before processing the modelines.
	 * So the modelines have priority over auto commands.
	 */
	if (retval != FAIL)
		UNCHANGED(curbuf);

#ifdef AUTOCMD
	apply_autocmds(EVENT_BUFENTER, NULL, NULL);
#endif

	if (retval != FAIL)
	{
		do_modelines();
		curbuf->b_neverloaded = FALSE;
	}

	return retval;
}

/*
 * Close the link to a buffer. If "free_buf" is TRUE free the buffer if it
 * becomes unreferenced. The caller should get a new buffer very soon!
 * if 'del_buf' is TRUE, remove the buffer from the buffer list.
 */
	void
close_buffer(win, buf, free_buf, del_buf)
	WIN		*win;			/* if not NULL, set b_last_cursor */
	BUF		*buf;
	int		free_buf;
	int		del_buf;
{
	if (buf->b_nwindows > 0)
		--buf->b_nwindows;
	if (buf->b_nwindows == 0 && win != NULL)
		set_last_cursor(win);	/* may set b_last_cursor */
	if (buf->b_nwindows > 0 || !free_buf)
	{
		if (buf == curbuf)
			u_sync();		/* sync undo before going to another buffer */
		return;
	}

	buf_freeall(buf);		/* free all things allocated for this buffer */
	/*
	 * If there is no file name, remove the buffer from the list
	 */
	if (buf->b_filename == NULL || del_buf)
	{
		vim_free(buf->b_filename);
		vim_free(buf->b_sfilename);
		if (buf->b_prev == NULL)
			firstbuf = buf->b_next;
		else
			buf->b_prev->b_next = buf->b_next;
		if (buf->b_next == NULL)
			lastbuf = buf->b_prev;
		else
			buf->b_next->b_prev = buf->b_prev;
		free_buf_options(buf);
	}
	else
		buf_clear(buf);
}

/*
 * buf_clear() - make buffer empty
 */
	void
buf_clear(buf)
	BUF		*buf;
{
	buf->b_ml.ml_line_count = 1;
	buf->b_changed = FALSE;
#ifndef SHORT_FNAME
	buf->b_shortname = FALSE;
#endif
	buf->b_p_eol = TRUE;
	buf->b_ml.ml_mfp = NULL;
	buf->b_ml.ml_flags = ML_EMPTY;				/* empty buffer */
}

/*
 * buf_freeall() - free all things allocated for the buffer
 */
	void
buf_freeall(buf)
	BUF		*buf;
{
	u_blockfree(buf);				/* free the memory allocated for undo */
	ml_close(buf, TRUE);			/* close and delete the memline/memfile */
	buf->b_ml.ml_line_count = 0;	/* no lines in buffer */
	u_clearall(buf);				/* reset all undo information */
}

/*
 * do_bufdel() - delete or unload buffer(s)
 *
 * addr_count == 0:	":bdel" - delete current buffer
 * addr_count == 1: ":N bdel" or ":bdel N [N ..] - first delete
 *					buffer "end_bnr", then any other arguments.
 * addr_count == 2: ":N,N bdel" - delete buffers in range
 *
 * command can be DOBUF_UNLOAD (":bunload") or DOBUF_DEL (":bdel")
 *
 * Returns error message or NULL
 */
	char_u *
do_bufdel(command, arg, addr_count, start_bnr, end_bnr, forceit)
	int		command;
	char_u	*arg;		/* pointer to extra arguments */
	int		addr_count;
	int		start_bnr;	/* first buffer number in a range */
	int		end_bnr;	/* buffer number or last buffer number in a range */
	int		forceit;
{
	int		do_current = 0;		/* delete current buffer? */
	int		deleted = 0;		/* number of buffers deleted */
	char_u	*errormsg = NULL;	/* return value */
	int		bnr;				/* buffer number */
	char_u	*p;

	if (addr_count == 0)
		(void)do_buffer(command, DOBUF_CURRENT, FORWARD, 0, forceit);
	else
	{
		if (addr_count == 2)
		{
			if (*arg)			/* both range and argument is not allowed */
				return e_trailing;
			bnr = start_bnr;
		}
		else	/* addr_count == 1 */
			bnr = end_bnr;

		for ( ;!got_int; mch_breakcheck())
		{
			/*
			 * delete the current buffer last, otherwise when the
			 * current buffer is deleted, the next buffer becomes
			 * the current one and will be loaded, which may then
			 * also be deleted, etc.
			 */
			if (bnr == curbuf->b_fnum)
				do_current = bnr;
			else if (do_buffer(command, DOBUF_FIRST, FORWARD, (int)bnr,
					forceit) == OK)
				++deleted;

			/*
			 * find next buffer number to delete/unload
			 */
			if (addr_count == 2)
			{
				if (++bnr > end_bnr)
					break;
			}
			else	/* addr_count == 1 */
			{
				arg = skipwhite(arg);
				if (*arg == NUL)
					break;
				if (!isdigit(*arg))
				{
					p = skiptowhite_esc(arg);
					bnr = buflist_findpat(arg, p);
					if (bnr < 0)			/* failed */
						break;
					arg = p;
				}
				else
					bnr = getdigits(&arg);
			}
		}
		if (!got_int && do_current && do_buffer(command, DOBUF_FIRST,
				FORWARD, do_current, forceit) == OK)
			++deleted;

		if (deleted == 0)
		{
			sprintf((char *)IObuff, "No buffers were %s",
					command == DOBUF_UNLOAD ? "unloaded" : "deleted");
			errormsg = IObuff;
		}
		else
			smsg((char_u *)"%d buffer%s %s", deleted,
					plural((long)deleted),
					command == DOBUF_UNLOAD ? "unloaded" : "deleted");
	}

	return errormsg;
}

/*
 * Implementation of the command for the buffer list
 *
 * action == DOBUF_GOTO		go to specified buffer
 * action == DOBUF_SPLIT	split window and go to specified buffer
 * action == DOBUF_UNLOAD	unload specified buffer(s)
 * action == DOBUF_DEL		delete specified buffer(s)
 *
 * start == DOBUF_CURRENT	go to "count" buffer from current buffer
 * start == DOBUF_FIRST		go to "count" buffer from first buffer
 * start == DOBUF_LAST		go to "count" buffer from last buffer
 * start == DOBUF_MOD		go to "count" modified buffer from current buffer
 *
 * Return FAIL or OK.
 */
	int
do_buffer(action, start, dir, count, forceit)
	int		action;
	int		start;
	int		dir;		/* FORWARD or BACKWARD */
	int		count;		/* buffer number or number of buffers */
	int		forceit;	/* TRUE for :bdelete! */
{
	BUF		*buf;
	BUF		*delbuf;
	int		retval;

	switch (start)
	{
		case DOBUF_FIRST:	buf = firstbuf;	break;
		case DOBUF_LAST:	buf = lastbuf;	break;
		default:			buf = curbuf;	break;
	}
	if (start == DOBUF_MOD)			/* find next modified buffer */
	{
		while (count-- > 0)
		{
			do
			{
				buf = buf->b_next;
				if (buf == NULL)
					buf = firstbuf;
			}
			while (buf != curbuf && !buf->b_changed);
		}
		if (!buf->b_changed)
		{
			EMSG("No modified buffer found");
			return FAIL;
		}
	}
	else if (start == DOBUF_FIRST && count)	/* find specified buffer number */
	{
		while (buf != NULL && buf->b_fnum != count)
			buf = buf->b_next;
	}
	else
	{
		while (count-- > 0)
		{
			if (dir == FORWARD)
			{
				buf = buf->b_next;
				if (buf == NULL)
					buf = firstbuf;
			}
			else
			{
				buf = buf->b_prev;
				if (buf == NULL)
					buf = lastbuf;
			}
		}
	}

	if (buf == NULL)		/* could not find it */
	{
		if (start == DOBUF_FIRST)
		{
											/* don't warn when deleting */
			if (action != DOBUF_UNLOAD && action != DOBUF_DEL)
				EMSGN("Cannot go to buffer %ld", count);
		}
		else if (dir == FORWARD)
			EMSG("Cannot go beyond last buffer");
		else
			EMSG("Cannot go before first buffer");
		return FAIL;
	}

	/*
	 * delete buffer buf from memory and/or the list
	 */
	if (action == DOBUF_UNLOAD || action == DOBUF_DEL)
	{
		if (!forceit && buf->b_changed)
		{
			EMSGN("No write since last change for buffer %ld (use ! to override)",
						buf->b_fnum);
			return FAIL;
		}

		/*
		 * If deleting last buffer, make it empty.
		 * The last buffer cannot be unloaded.
		 */
		if (firstbuf->b_next == NULL)
		{
			if (action == DOBUF_UNLOAD)
			{
				EMSG("Cannot unload last buffer");
				return FAIL;
			}
			/* Close any other windows on this buffer */
			close_others(FALSE);
			buf = curbuf;
			setpcmark();
			retval = do_ecmd(0, NULL, NULL, NULL, FALSE, (linenr_t)1, FALSE);
			/*
			 * The do_ecmd() may create a new buffer, then we have to delete
			 * the old one.  But do_ecmd() may have done that already, check
			 * if the buffer still exists (it will be the first or second in
			 * the buffer list).
			 */
			if (buf != curbuf && (buf == firstbuf || buf == firstbuf->b_next))
				close_buffer(NULL, buf, TRUE, TRUE);
			return retval;
		}

		/*
		 * If the deleted buffer is the current one, close the current window
		 * (unless it's the only window).
		 */
		while (buf == curbuf && firstwin != lastwin)
			close_window(curwin, FALSE);

		/*
		 * If the buffer to be deleted is not current one, delete it here.
		 */
		if (buf != curbuf)
		{
			close_windows(buf);
			close_buffer(NULL, buf, TRUE, action == DOBUF_DEL);
			return OK;
		}

		/*
		 * Deleting the current buffer: Need to find another buffer to go to.
		 * There must be another, otherwise it would have been handled above.
		 */
		if (curbuf->b_next != NULL)
			buf = curbuf->b_next;
		else
			buf = curbuf->b_prev;
	}

	/*
	 * make buf current buffer
	 */
	setpcmark();
	if (action == DOBUF_SPLIT)		/* split window first */
	{
		if (win_split(0, FALSE) == FAIL)
			return FAIL;
	}
	curwin->w_alt_fnum = curbuf->b_fnum; /* remember alternate file */
	buflist_altlnum();					 /* remember curpos.lnum */

#ifdef AUTOCMD
	apply_autocmds(EVENT_BUFLEAVE, NULL, NULL);
#endif
	delbuf = curbuf;		/* close_windows() may change curbuf */
	if (action == DOBUF_UNLOAD || action == DOBUF_DEL)
		close_windows(curbuf);
	close_buffer(NULL, delbuf, action == DOBUF_UNLOAD || action == DOBUF_DEL,
														 action == DOBUF_DEL);
	enter_buffer(buf);
	return OK;
}

/*
 * enter a new current buffer.
 * (old curbuf must have been freed already)
 */
	static void
enter_buffer(buf)
	BUF		*buf;
{
	buf_copy_options(curbuf, buf, TRUE);
	curwin->w_buffer = buf;
	curbuf = buf;
	++curbuf->b_nwindows;
	if (curbuf->b_ml.ml_mfp == NULL)	/* need to load the file */
		open_buffer();
	else
	{
		need_fileinfo = TRUE;			/* display file info after redraw */
		buf_check_timestamp(curbuf);	/* check if file has changed */
#ifdef AUTOCMD
		apply_autocmds(EVENT_BUFENTER, NULL, NULL);
#endif
	}
	buflist_getlnum();					/* restore curpos.lnum */
	check_arg_idx();					/* check for valid arg_idx */
	maketitle();
	scroll_cursor_halfway(FALSE);		/* redisplay at correct position */
	updateScreen(NOT_VALID);
}

/*
 * functions for dealing with the buffer list
 */

/*
 * Add a file name to the buffer list. Return a pointer to the buffer.
 * If the same file name already exists return a pointer to that buffer.
 * If it does not exist, or if fname == NULL, a new entry is created.
 * If use_curbuf is TRUE, may use current buffer.
 * This is the ONLY way to create a new buffer.
 */
	BUF *
buflist_new(fname, sfname, lnum, use_curbuf)
	char_u		*fname;
	char_u		*sfname;
	linenr_t	lnum;
	int			use_curbuf;
{
	static int	top_file_num = 1;			/* highest file number */
	BUF			*buf;

	fname_expand(&fname, &sfname);

/*
 * If file name already exists in the list, update the entry
 */
	if (fname != NULL && (buf = buflist_findname(fname)) != NULL)
	{
		if (lnum != 0)
			buflist_setlnum(buf, lnum);
		/* copy the options now, if 'cpo' doesn't have 's' and not done
		 * already */
		buf_copy_options(curbuf, buf, FALSE);
		return buf;
	}

/*
 * If the current buffer has no name and no contents, use the current buffer.
 * Otherwise: Need to allocate a new buffer structure.
 *
 * This is the ONLY place where a new buffer structure is allocated!
 */
	if (use_curbuf && curbuf != NULL && curbuf->b_filename == NULL &&
				curbuf->b_nwindows <= 1 &&
				(curbuf->b_ml.ml_mfp == NULL || bufempty()))
		buf = curbuf;
	else
	{
		buf = (BUF *)alloc((unsigned)sizeof(BUF));
		if (buf == NULL)
			return NULL;
		(void)vim_memset(buf, 0, sizeof(BUF));
	}

	if (fname != NULL)
	{
		buf->b_filename = strsave(fname);
		buf->b_sfilename = strsave(sfname);
	}
	if (buf->b_winlnum == NULL)
		buf->b_winlnum = (WINLNUM *)alloc((unsigned)sizeof(WINLNUM));
	if ((fname != NULL && (buf->b_filename == NULL ||
						 buf->b_sfilename == NULL)) || buf->b_winlnum == NULL)
	{
		vim_free(buf->b_filename);
		buf->b_filename = NULL;
		vim_free(buf->b_sfilename);
		buf->b_sfilename = NULL;
		if (buf != curbuf)
		{
			vim_free(buf->b_winlnum);
			free_buf_options(buf);
		}
		return NULL;
	}

	if (buf == curbuf)
	{
		buf_freeall(buf);		/* free all things allocated for this buffer */
		buf->b_nwindows = 0;
	}
	else
	{
		/*
		 * Copy the options from the current buffer.
		 */
		buf_copy_options(curbuf, buf, FALSE);

		/*
		 * put new buffer at the end of the buffer list
		 */
		buf->b_next = NULL;
		if (firstbuf == NULL)			/* buffer list is empty */
		{
			buf->b_prev = NULL;
			firstbuf = buf;
		}
		else							/* append new buffer at end of list */
		{
			lastbuf->b_next = buf;
			buf->b_prev = lastbuf;
		}
		lastbuf = buf;

		buf->b_fnum = top_file_num++;
		if (top_file_num < 0)			/* wrap around (may cause duplicates) */
		{
			EMSG("Warning: List of file names overflow");
			mch_delay(3000L, TRUE);		/* make sure it is noticed */
			top_file_num = 1;
		}

		buf->b_winlnum->wl_lnum = lnum;
		buf->b_winlnum->wl_next = NULL;
		buf->b_winlnum->wl_prev = NULL;
		buf->b_winlnum->wl_win = curwin;
	}

	if (did_cd)
		buf->b_xfilename = buf->b_filename;
	else
		buf->b_xfilename = buf->b_sfilename;
	buf->b_u_synced = TRUE;
	buf->b_neverloaded = TRUE;
	buf_clear(buf);
	clrallmarks(buf);				/* clear marks */
	fmarks_check_names(buf);		/* check file marks for this file */

	return buf;
}

/*
 * Free the memory for a BUF structure and its options
 */
	static void
free_buf_options(buf)
	BUF		*buf;
{
	free_string_option(buf->b_p_fo);
	free_string_option(buf->b_p_isk);
	free_string_option(buf->b_p_com);
#ifdef CINDENT
	free_string_option(buf->b_p_cink);
	free_string_option(buf->b_p_cino);
#endif
#if defined(CINDENT) || defined(SMARTINDENT)
	free_string_option(buf->b_p_cinw);
#endif
	vim_free(buf);
}

/*
 * get alternate file n
 * set linenr to lnum or altlnum if lnum == 0
 * if (options & GETF_SETMARK) call setpcmark()
 * if (options & GETF_ALT) we are jumping to an alternate file.
 *
 * return FAIL for failure, OK for success
 */
	int
buflist_getfile(n, lnum, options)
	int			n;
	linenr_t	lnum;
	int			options;
{
	BUF		*buf;

	buf = buflist_findnr(n);
	if (buf == NULL)
	{
		if ((options & GETF_ALT) && n == 0)
			emsg(e_noalt);
		else
			EMSGN("buffer %ld not found", n);
		return FAIL;
	}

	/* if alternate file is the current buffer, nothing to do */
	if (buf == curbuf)
		return OK;

	/* altlnum may be changed by getfile(), get it now */
	if (lnum == 0)
		lnum = buflist_findlnum(buf);
	++RedrawingDisabled;
	if (getfile(buf->b_fnum, NULL, NULL, (options & GETF_SETMARK), lnum) <= 0)
	{
		--RedrawingDisabled;
		return OK;
	}
	--RedrawingDisabled;
	return FAIL;
}

/*
 * go to the last know line number for the current buffer
 */
	void
buflist_getlnum()
{
	linenr_t	lnum;

	curwin->w_cursor.lnum = 1;
	curwin->w_cursor.col = 0;
	lnum = buflist_findlnum(curbuf);
	if (lnum != 0 && lnum <= curbuf->b_ml.ml_line_count)
		curwin->w_cursor.lnum = lnum;
}

/*
 * find file in buffer list by name (it has to be for the current window)
 * 'fname' must have a full path.
 */
	BUF *
buflist_findname(fname)
	char_u		*fname;
{
	BUF			*buf;

	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		if (buf->b_filename != NULL && fnamecmp(fname, buf->b_filename) == 0)
			return (buf);
	return NULL;
}

/*
 * Find file in buffer list by a regexppattern.
 * Return fnum of the found buffer, < 0 for error.
 */
	int
buflist_findpat(pattern, pattern_end)
	char_u		*pattern;
	char_u		*pattern_end;		/* pointer to first char after pattern */
{
	BUF			*buf;
	regexp		*prog;
	int			fnum = -1;
	char_u		*pat;
	char_u		*match;
	int			attempt;
	char_u		*p;

	if (pattern_end == pattern + 1 && (*pattern == '%' || *pattern == '#'))
	{
		if (*pattern == '%')
			fnum = curbuf->b_fnum;
		else
			fnum = curwin->w_alt_fnum;
	}

	/*
	 * Try four ways of matching:
	 * attempt == 0: without '^' or '$' (at any position)
	 * attempt == 1: with '^' at start (only at postion 0)
	 * attempt == 2: with '$' at end (only match at end)
	 * attempt == 3: with '^' at start and '$' at end (only full match)
	 */
	else for (attempt = 0; attempt <= 3; ++attempt)
	{
		/* may add '^' and '$' */
		pat = file_pat_to_reg_pat(pattern, pattern_end, NULL);
		if (pat == NULL)
			return -1;
		if (attempt < 2)
		{
			p = pat + STRLEN(pat) - 1;
			if (p > pat && *p == '$')				/* remove '$' */
				*p = NUL;
		}
		p = pat;
		if (*p == '^' && !(attempt & 1))			/* remove '^' */
			++p;
		prog = vim_regcomp(p);
		vim_free(pat);
		if (prog == NULL)
			return -1;

		for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		{
			match = buflist_match(prog, buf);
			if (match != NULL)
			{
				if (fnum >= 0)			/* already found a match */
				{
					fnum = -2;
					break;
				}
				fnum = buf->b_fnum;		/* remember first match */
			}
		}
		vim_free(prog);
		if (fnum >= 0)					/* found one match */
			break;
	}

	if (fnum == -2)
		EMSG2("More than one match for %s", pattern);
	if (fnum < 1)
		EMSG2("No matching buffer for %s", pattern);
	return fnum;
}

/*
 * Find all buffer names that match.
 * For command line expansion of ":buf" and ":sbuf".
 * Return OK if matches found, FAIL otherwise.
 */
	int
ExpandBufnames(pat, num_file, file, options)
	char_u		*pat;
	int			*num_file;
	char_u		***file;
	int			options;
{
	int			count = 0;
	BUF			*buf;
	int			round;
	char_u		*p;
	int			attempt;
	regexp		*prog;

	*num_file = 0;					/* return values in case of FAIL */
	*file = NULL;

	/*
	 * attempt == 1: try match with    '^', match at start
	 * attempt == 2: try match without '^', match anywhere
	 */
	for (attempt = 1; attempt <= 2; ++attempt)
	{
		if (attempt == 2)
		{
			if (*pat != '^')		/* there's no '^', no need to try again */
				break;
			++pat;					/* skip the '^' */
		}
		prog = vim_regcomp(pat);
		if (prog == NULL)
			return FAIL;

		/*
		 * round == 1: Count the matches.
		 * round == 2: Build the array to keep the matches.
		 */
		for (round = 1; round <= 2; ++round)
		{
			count = 0;
			for (buf = firstbuf; buf != NULL; buf = buf->b_next)
			{
				p = buflist_match(prog, buf);
				if (p != NULL)
				{
					if (round == 1)
						++count;
					else
					{
						if (options & WILD_HOME_REPLACE)
							p = home_replace_save(buf, p);
						else
							p = strsave(p);
						(*file)[count++] = p;
					}
				}
			}
			if (count == 0)		/* no match found, break here */
				break;
			if (round == 1)
			{
				*file = (char_u **)alloc((unsigned)(count * sizeof(char_u *)));
				if (*file == NULL)
				{
					vim_free(prog);
					return FAIL;
				}
			}
		}
		vim_free(prog);
		if (count)				/* match(es) found, break here */
			break;
	}

	*num_file = count;
	return (count == 0 ? FAIL : OK);
}

/*
 * Check for a match on the file name for buffer "buf" with regex prog "prog".
 */
	static char_u *
buflist_match(prog, buf)
	regexp		*prog;
	BUF			*buf;
{
	char_u	*match = NULL;

	if (buf->b_sfilename != NULL &&
							   vim_regexec(prog, buf->b_sfilename, TRUE) != 0)
		match = buf->b_sfilename;
	else if (buf->b_filename != NULL)
	{
		if (vim_regexec(prog, buf->b_filename, TRUE) != 0)
			match = buf->b_filename;
		else
		{
			home_replace(NULL, buf->b_filename, NameBuff, MAXPATHL);
			if (vim_regexec(prog, NameBuff, TRUE) != 0)
				match = buf->b_filename;
		}
	}
	return match;
}

/*
 * find file in buffer name list by number
 */
	BUF	*
buflist_findnr(nr)
	int			nr;
{
	BUF			*buf;

	if (nr == 0)
		nr = curwin->w_alt_fnum;
	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		if (buf->b_fnum == nr)
			return (buf);
	return NULL;
}

/*
 * get name of file 'n' in the buffer list
 */
 	char_u *
buflist_nr2name(n, fullname, helptail)
	int n;
	int fullname;
	int helptail;			/* for help buffers return tail only */
{
	BUF		*buf;
	char_u	*fname;

	buf = buflist_findnr(n);
	if (buf == NULL)
		return NULL;
	if (fullname)
		fname = buf->b_filename;
	else
		fname = buf->b_xfilename;
	home_replace(helptail ? buf : NULL, fname, NameBuff, MAXPATHL);
	return NameBuff;
}

/*
 * set the lnum for the buffer 'buf' and the current window
 */
	static void
buflist_setlnum(buf, lnum)
	BUF			*buf;
	linenr_t	lnum;
{
	WINLNUM		*wlp;
	
	for (wlp = buf->b_winlnum; wlp != NULL; wlp = wlp->wl_next)
		if (wlp->wl_win == curwin)
			break;
	if (wlp == NULL)			/* make new entry */
	{
		wlp = (WINLNUM *)alloc((unsigned)sizeof(WINLNUM));
		if (wlp == NULL)
			return;
		wlp->wl_win = curwin;
	}
	else						/* remove entry from list */
	{
		if (wlp->wl_prev)
			wlp->wl_prev->wl_next = wlp->wl_next;
		else
			buf->b_winlnum = wlp->wl_next;
		if (wlp->wl_next)
			wlp->wl_next->wl_prev = wlp->wl_prev;
	}
	wlp->wl_lnum = lnum;
/*
 * insert entry in front of the list
 */
	wlp->wl_next = buf->b_winlnum;
	buf->b_winlnum = wlp;
	wlp->wl_prev = NULL;
	if (wlp->wl_next)
		wlp->wl_next->wl_prev = wlp;

	return;
}

/*
 * find the lnum for the buffer 'buf' for the current window
 */
	static linenr_t
buflist_findlnum(buf)
	BUF		*buf;
{
	WINLNUM 	*wlp;

	for (wlp = buf->b_winlnum; wlp != NULL; wlp = wlp->wl_next)
		if (wlp->wl_win == curwin)
			break;

	if (wlp == NULL)		/* if no lnum for curwin, use the first in the list */
		wlp = buf->b_winlnum;

	if (wlp)
		return wlp->wl_lnum;
	else
		return (linenr_t)1;
}

/*
 * list all know file names (for :files and :buffers command)
 */
	void
buflist_list()
{
	BUF			*buf;
	int			len;

	for (buf = firstbuf; buf != NULL && !got_int; buf = buf->b_next)
	{
		msg_outchar('\n');
		if (buf->b_xfilename == NULL)
			STRCPY(NameBuff, "No File");
		else
			/* careful: home_replace calls vim_getenv(), which uses IObuff! */
			home_replace(buf, buf->b_xfilename, NameBuff, MAXPATHL);

		sprintf((char *)IObuff, "%3d %c%c%c \"",
				buf->b_fnum,
				buf == curbuf ? '%' :
						(curwin->w_alt_fnum == buf->b_fnum ? '#' : ' '),
				buf->b_ml.ml_mfp == NULL ? '-' :
						(buf->b_nwindows == 0 ? 'h' : ' '),
				buf->b_changed ? '+' : ' ');

		len = STRLEN(IObuff);
		STRNCPY(IObuff + len, NameBuff, IOSIZE - 20 - len);

		len = STRLEN(IObuff);
		IObuff[len++] = '"';
		/*
		 * try to put the "line" strings in column 40
		 */
		do
		{
			IObuff[len++] = ' ';
		} while (len < 40 && len < IOSIZE - 18);
		sprintf((char *)IObuff + len, "line %ld",
				buf == curbuf ? curwin->w_cursor.lnum :
								(long)buflist_findlnum(buf));
		msg_outtrans(IObuff);
		flushbuf();			/* output one line at a time */
		mch_breakcheck();
	}
}

/*
 * get file name and line number for file 'fnum'
 * used by DoOneCmd() for translating '%' and '#'
 * return FAIL if not found, OK for success
 */
	int
buflist_name_nr(fnum, fname, lnum)
	int			fnum;
	char_u		**fname;
	linenr_t	*lnum;
{
	BUF			*buf;

	buf = buflist_findnr(fnum);
	if (buf == NULL || buf->b_filename == NULL)
		return FAIL;

	if (did_cd)
		*fname = buf->b_filename;
	else
		*fname = buf->b_sfilename;
	*lnum = buflist_findlnum(buf);

	return OK;
}

/*
 * Set the current file name to 's', short file name to 'ss'.
 * The file name with the full path is also remembered, for when :cd is used.
 * Returns FAIL for failure (file name already in use by other buffer)
 * 		OK otherwise.
 */
	int
setfname(fname, sfname, message)
	char_u *fname, *sfname;
	int		message;
{
	BUF		*buf;

	if (fname == NULL || *fname == NUL)
	{
		vim_free(curbuf->b_filename);
		vim_free(curbuf->b_sfilename);
		curbuf->b_filename = NULL;
		curbuf->b_sfilename = NULL;
	}
	else
	{
		fname_expand(&fname, &sfname);
#ifdef USE_FNAME_CASE
# ifdef USE_LONG_FNAME
		if (USE_LONG_FNAME)
# endif
			fname_case(sfname);		/* set correct case for short filename */
#endif
		/*
		 * if the file name is already used in another buffer:
		 * - if the buffer is loaded, fail
		 * - if the buffer is not loaded, delete it from the list
		 */
		buf = buflist_findname(fname);
		if (buf != NULL && buf != curbuf)
		{
			if (buf->b_ml.ml_mfp != NULL)		/* it's loaded, fail */
			{
				if (message)
					EMSG("Buffer with this name already exists");
				return FAIL;
			}
			close_buffer(NULL, buf, TRUE, TRUE);	/* delete from the list */
		}
		fname = strsave(fname);
		sfname = strsave(sfname);
		if (fname == NULL || sfname == NULL)
		{
			vim_free(sfname);
			vim_free(fname);
			return FAIL;
		}
		vim_free(curbuf->b_filename);
		vim_free(curbuf->b_sfilename);
		curbuf->b_filename = fname;
		curbuf->b_sfilename = sfname;
	}
	if (did_cd)
		curbuf->b_xfilename = curbuf->b_filename;
	else
		curbuf->b_xfilename = curbuf->b_sfilename;

#ifndef SHORT_FNAME
	curbuf->b_shortname = FALSE;
#endif
	/*
	 * If the file name changed, also change the name of the swapfile
	 */
	if (curbuf->b_ml.ml_mfp != NULL)
		ml_setname();

	check_arg_idx();			/* check file name for arg list */
	maketitle();				/* set window title */
	status_redraw_all();		/* status lines need to be redrawn */
	fmarks_check_names(curbuf);	/* check named file marks */
	ml_timestamp(curbuf);		/* reset timestamp */
	return OK;
}

/*
 * set alternate file name for current window
 *
 * used by dowrite() and do_ecmd()
 */
	void
setaltfname(fname, sfname, lnum)
	char_u		*fname;
	char_u		*sfname;
	linenr_t	lnum;
{
	BUF		*buf;

	buf = buflist_new(fname, sfname, lnum, FALSE);
	if (buf != NULL)
		curwin->w_alt_fnum = buf->b_fnum;
}

/*
 * add a file name to the buflist and return its number
 *
 * used by qf_init(), main() and doarglist()
 */
	int
buflist_add(fname)
	char_u		*fname;
{
	BUF		*buf;

	buf = buflist_new(fname, NULL, (linenr_t)0, FALSE);
	if (buf != NULL)
		return buf->b_fnum;
	return 0;
}

/*
 * set alternate lnum for current window
 */
	void
buflist_altlnum()
{
	buflist_setlnum(curbuf, curwin->w_cursor.lnum);
}

/*
 * return nonzero if 'fname' is not the same file as current file
 * fname must have a full path (expanded by FullName)
 */
	int
otherfile(fname)
	char_u	*fname;
{									/* no name is different */
	if (fname == NULL || *fname == NUL || curbuf->b_filename == NULL)
		return TRUE;
	return fnamecmp(fname, curbuf->b_filename);
}

	void
fileinfo(fullname, shorthelp, dont_truncate)
	int fullname;
	int shorthelp;
	int	dont_truncate;
{
	char_u		*name;
	int			n;
	char_u		*p;
	char_u		*buffer;

	buffer = alloc(IOSIZE);
	if (buffer == NULL)
		return;

	if (fullname > 1)		/* 2 CTRL-G: include buffer number */
	{
		sprintf((char *)buffer, "buf %d: ", curbuf->b_fnum);
		p = buffer + STRLEN(buffer);
	}
	else
		p = buffer;

	*p++ = '"';
	if (curbuf->b_filename == NULL)
		STRCPY(p, "No File");
	else
	{
		if (!fullname && curbuf->b_sfilename != NULL)
			name = curbuf->b_sfilename;
		else
			name = curbuf->b_filename;
		home_replace(shorthelp ? curbuf : NULL, name, p,
												(int)(IOSIZE - (p - buffer)));
	}

	sprintf((char *)buffer + STRLEN(buffer),
			"\"%s%s%s%s",
			curbuf->b_changed ? (shortmess(SHM_MOD) ?
												" [+]" : " [Modified]") : " ",
			curbuf->b_notedited ? "[Not edited]" : "",
			curbuf->b_p_ro ? (shortmess(SHM_RO) ? "[RO]" : "[readonly]") : "",
			(curbuf->b_changed || curbuf->b_notedited || curbuf->b_p_ro) ?
																	" " : "");
	n = (int)(((long)curwin->w_cursor.lnum * 100L) /
											(long)curbuf->b_ml.ml_line_count);
	if (curbuf->b_ml.ml_flags & ML_EMPTY)
	{
		STRCPY(buffer + STRLEN(buffer), no_lines_msg);
	}
	else if (p_ru)
	{
		/* Current line and column are already on the screen -- webb */
		sprintf((char *)buffer + STRLEN(buffer),
			"%ld line%s --%d%%--",
			(long)curbuf->b_ml.ml_line_count,
			plural((long)curbuf->b_ml.ml_line_count),
			n);
	}
	else
	{
		sprintf((char *)buffer + STRLEN(buffer),
			"line %ld of %ld --%d%%-- col ",
			(long)curwin->w_cursor.lnum,
			(long)curbuf->b_ml.ml_line_count,
			n);
		col_print(buffer + STRLEN(buffer),
				   (int)curwin->w_cursor.col + 1, (int)curwin->w_virtcol + 1);
	}

	append_arg_number(buffer, !shortmess(SHM_FILE));

	if (dont_truncate)
		msg(buffer);
	else
		msg_trunc(buffer);

	vim_free(buffer);
}

/*
 * Give some info about the position of the cursor (for "g CTRL-G").
 */
	void
cursor_pos_info()
{
	char_u		*p;
	char_u		buf1[20];
	char_u		buf2[20];
	linenr_t	lnum;
	long		char_count = 0;
	long		char_count_cursor = 0;
	int		eol_size;

	/*
	 * Compute the length of the file in characters.
	 */
	if (curbuf->b_ml.ml_flags & ML_EMPTY)
	{
		MSG(no_lines_msg);
	}
	else
	{
		if (curbuf->b_p_tx)
			eol_size = 2;
		else
			eol_size = 1;
		for (lnum = 1; lnum <= curbuf->b_ml.ml_line_count; ++lnum)
		{
			if (lnum == curwin->w_cursor.lnum)
				char_count_cursor = char_count + curwin->w_cursor.col + 1;
			char_count += STRLEN(ml_get(lnum)) + eol_size;
		}
		if (!curbuf->b_p_eol && curbuf->b_p_bin)
			char_count -= eol_size;

		p = ml_get_curline();
		col_print(buf1, (int)curwin->w_cursor.col + 1, (int)curwin->w_virtcol + 1);
		col_print(buf2, (int)STRLEN(p), linetabsize(p));

		sprintf((char *)IObuff, "Col %s of %s; Line %ld of %ld; Char %ld of %ld",
				(char *)buf1, (char *)buf2,
				(long)curwin->w_cursor.lnum, (long)curbuf->b_ml.ml_line_count,
				char_count_cursor, char_count);
		msg(IObuff);
	}
}

	void
col_print(buf, col, vcol)
	char_u	*buf;
	int		col;
	int		vcol;
{
	if (col == vcol)
		sprintf((char *)buf, "%d", col);
	else
		sprintf((char *)buf, "%d-%d", col, vcol);
}

/*
 * put filename in title bar of window and in icon title
 */

static char_u *lasttitle = NULL;
static char_u *lasticon = NULL;

	void
maketitle()
{
	char_u		*t_name;
	char_u		*i_name;

	if (curbuf->b_filename == NULL)
	{
		t_name = (char_u *)"";
		i_name = (char_u *)"No File";
	}
	else
	{
		home_replace(curbuf, curbuf->b_filename, IObuff, IOSIZE);
		append_arg_number(IObuff, FALSE);
		t_name = IObuff;
		i_name = gettail(curbuf->b_filename);	/* use filename only for icon */
	}

	vim_free(lasttitle);
	if (p_title && (lasttitle = alloc((unsigned)(strsize(t_name) + 7))) != NULL)
	{
		STRCPY(lasttitle, "VIM - ");
		while (*t_name)
			STRCAT(lasttitle, transchar(*t_name++));
	}
	else
		lasttitle = NULL;

	vim_free(lasticon);
	if (p_icon && (lasticon = alloc((unsigned)(strsize(i_name) + 1))) != NULL)
	{
		*lasticon = NUL;
		while (*i_name)
			STRCAT(lasticon, transchar(*i_name++));
	}
	else
		lasticon = NULL;

	resettitle();
}

/*
 * Append (file 2 of 8) to 'buf'.
 */
	static void
append_arg_number(buf, add_file)
	char_u	*buf;
	int		add_file;		/* Add "file" before the arg number */
{
	if (arg_count <= 1)		/* nothing to do */
		return;

	buf += STRLEN(buf);		/* go to the end of the buffer */
	*buf++ = ' ';
	*buf++ = '(';
	if (add_file)
	{
		STRCPY(buf, "file ");
		buf += 5;
	}
	sprintf((char *)buf, curwin->w_arg_idx_invalid ? "(%d) of %d)" :
								 "%d of %d)", curwin->w_arg_idx + 1, arg_count);
}

/*
 * Put current window title back (used after calling a shell)
 */
	void
resettitle()
{
	mch_settitle(lasttitle, lasticon);
}

/*
 * If fname is not a full path, make it a full path
 */
	char_u	*
fix_fname(fname)
	char_u	*fname;
{
	if (fname != NameBuff)			/* if not already expanded */
	{
		if (!isFullName(fname))
		{
			(void)FullName(fname, NameBuff, MAXPATHL, FALSE);
			fname = NameBuff;
		}
#ifdef USE_FNAME_CASE
		else
# ifdef USE_LONG_FNAME
			if (USE_LONG_FNAME)
# endif
		{
			STRNCPY(NameBuff, fname, MAXPATHL);	/* make copy, it may change */
			fname = NameBuff;
			fname_case(fname);			/* set correct case for filename */
		}
#endif
	}
	return fname;
}

/*
 * make fname a full file name, set sfname to fname if not NULL
 */
	void
fname_expand(fname, sfname)
	char_u		**fname;
	char_u		**sfname;
{
	if (*fname == NULL)			/* if no file name given, nothing to do */
		return;
	if (*sfname == NULL)		/* if no short file name given, use fname */
		*sfname = *fname;
	*fname = fix_fname(*fname);	/* expand to full path */
}

/*
 * do_arg_all: open up to 'count' windows, one for each argument
 */
	void
do_arg_all(count)
	int count;
{
	int		i;

	if (arg_count <= 1)
	{
		/* Don't give this obvious error message. We don't want it when the
		 * ":all" command is in the .vimrc. */
		/* EMSG("Argument list contains less than 2 files"); */
		return;
	}
	/*
	 * 1. close all but first window
	 * 2. make the desired number of windows
	 * 3. start editing in the windows
	 */
	setpcmark();
	close_others(FALSE);
	curwin->w_arg_idx = 0;
	if (count > arg_count || count <= 0)
		count = arg_count;
	count = make_windows(count);
	for (i = 0; i < count; ++i)
	{
												/* edit file i */
		(void)do_ecmd(0, arg_files[i], NULL, NULL, TRUE, (linenr_t)1, FALSE);
		curwin->w_arg_idx = i;
		if (i == arg_count - 1)
			arg_had_last = TRUE;
		if (curwin->w_next == NULL)				/* just checking */
			break;
		win_enter(curwin->w_next, FALSE);
	}
	win_enter(firstwin, FALSE);					/* back to first window */
}

/*
 * do_arg_all: open a window for each buffer
 *
 * 'count' is the maximum number of windows to open.
 * when 'all' is TRUE, also load inactive buffers
 */
	void
do_buffer_all(count, all)
	int		count;
	int		all;
{
	int		buf_count;
	BUF		*buf;
	int		i;

/*
 * count number of desired windows
 */
	buf_count = 0; 
	for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		if (all || buf->b_ml.ml_mfp != NULL)
			++buf_count;

	if (buf_count == 0)				/* Cannot happen? */
	{
		EMSG("No relevant entries in buffer list");
		return;
	}

	/*
	 * 1. close all but first window
	 * 2. make the desired number of windows
	 * 3. stuff commands to fill the windows
	 */
	close_others(FALSE);
	curwin->w_arg_idx = 0;
	if (buf_count > count)
		buf_count = count;
	buf_count = make_windows(buf_count);
	buf = firstbuf;
	for (i = 0; i < buf_count; ++i)
	{
		for ( ; buf != NULL; buf = buf->b_next)
			if (all || buf->b_ml.ml_mfp != NULL)
				break;
		if (buf == NULL)			/* Cannot happen? */
			break;
		if (i != 0)
			stuffReadbuff((char_u *)"\n\027\027:");	/* CTRL-W CTRL-W */
		stuffReadbuff((char_u *)":buf ");			/* edit Nth buffer */
		stuffnumReadbuff((long)buf->b_fnum);
		buf = buf->b_next;
	}
	stuffReadbuff((char_u *)"\n100\027k");		/* back to first window */
}

/*
 * do_modelines() - process mode lines for the current file
 *
 * Returns immediately if the "ml" option isn't set.
 */
static int 	chk_modeline __ARGS((linenr_t));

	void
do_modelines()
{
	linenr_t		lnum;
	int 			nmlines;

	if (!curbuf->b_p_ml || (nmlines = (int)p_mls) == 0)
		return;

	for (lnum = 1; lnum <= curbuf->b_ml.ml_line_count && lnum <= nmlines;
																	   ++lnum)
		if (chk_modeline(lnum) == FAIL)
			nmlines = 0;

	for (lnum = curbuf->b_ml.ml_line_count; lnum > 0 && lnum > nmlines &&
						  lnum > curbuf->b_ml.ml_line_count - nmlines; --lnum)
		if (chk_modeline(lnum) == FAIL)
			nmlines = 0;
}

/*
 * chk_modeline() - check a single line for a mode string
 * Return FAIL if an error encountered.
 */
	static int
chk_modeline(lnum)
	linenr_t lnum;
{
	register char_u	*s;
	register char_u	*e;
	char_u			*linecopy;			/* local copy of any modeline found */
	int				prev;
	int				end;
	int				retval = OK;
	char_u			*save_sourcing_name;

	prev = -1;
	for (s = ml_get(lnum); *s != NUL; ++s)
	{
		if (prev == -1 || vim_isspace(prev))
		{
			if ((prev != -1 && STRNCMP(s, "ex:", (size_t)3) == 0) ||
				 			   STRNCMP(s, "vi:", (size_t)3) == 0 ||
							   STRNCMP(s, "vim:", (size_t)4) == 0)
				break;
		}
		prev = *s;
	}

	if (*s)
	{
		do								/* skip over "ex:", "vi:" or "vim:" */
			++s;
		while (s[-1] != ':');

		s = linecopy = strsave(s);		/* copy the line, it will change */
		if (linecopy == NULL)
			return FAIL;

		sourcing_lnum = lnum;			/* prepare for emsg() */
		save_sourcing_name = sourcing_name;
		sourcing_name = (char_u *)"modelines";

		end = FALSE;
		while (end == FALSE)
		{
			s = skipwhite(s);
			if (*s == NUL)
				break;

			/*
			 * Find end of set command: ':' or end of line.
			 */
			for (e = s; (*e != ':' || *(e - 1) == '\\') && *e != NUL; ++e)
				;
			if (*e == NUL)
				end = TRUE;

			/*
			 * If there is a "set" command, require a terminating ':' and
			 * ignore the stuff after the ':'.
			 * "vi:set opt opt opt: foo" -- foo not interpreted
			 * "vi:opt opt opt: foo" -- foo interpreted
			 */
			if (STRNCMP(s, "set ", (size_t)4) == 0)
			{
				if (*e != ':')			/* no terminating ':'? */
					break;
				end = TRUE;
				s += 4;
			}

			*e = NUL;					/* truncate the set command */
			if (do_set(s) == FAIL)		/* stop if error found */
			{
				retval = FAIL;
				break;
			}
			s = e + 1;					/* advance to next part */
		}

		sourcing_lnum = 0;
		sourcing_name = save_sourcing_name;

		vim_free(linecopy);
	}
	return retval;
}
