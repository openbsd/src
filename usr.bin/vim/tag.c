/*	$OpenBSD: tag.c,v 1.3 1996/10/14 03:55:28 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * Code to handle tags and the tag stack
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

static int get_tagfname __ARGS((int, char_u	*));

#ifdef EMACS_TAGS
static int parse_tag_line __ARGS((char_u *, int,
					  char_u **, char_u **, char_u **, char_u **, char_u **));
static int jumpto_tag __ARGS((char_u *, int, char_u *, char_u *, int));
#else
static int parse_tag_line __ARGS((char_u *,
					  char_u **, char_u **, char_u **, char_u **, char_u **));
static int jumpto_tag __ARGS((char_u *, char_u *, int));
#endif
static int test_for_static __ARGS((char_u **, char_u *, char_u *, char_u *));
static char_u *expand_rel_name __ARGS((char_u *fname, char_u *tag_fname));
static void simplify_filename __ARGS((char_u *filename));
#ifdef EMACS_TAGS
static int test_for_current __ARGS((int, char_u *, char_u *, char_u *));
#else
static int test_for_current __ARGS((char_u *, char_u *, char_u *));
#endif

static char_u *bottommsg = (char_u *)"at bottom of tag stack";
static char_u *topmsg = (char_u *)"at top of tag stack";

/*
 * Jump to tag; handling of tag stack
 *
 * *tag != NUL (:tag): jump to new tag, add to tag stack
 * type == 1 (:pop) || type == 2 (CTRL-T): jump to old position
 * type == 0 (:tag): jump to old tag
 */
	void
do_tag(tag, type, count, forceit)
	char_u	*tag;
	int		type;
	int		count;
	int		forceit;		/* :ta with ! */
{
	int 			i;
	struct taggy	*tagstack = curwin->w_tagstack;
	int				tagstackidx = curwin->w_tagstackidx;
	int				tagstacklen = curwin->w_tagstacklen;
	int				oldtagstackidx = tagstackidx;

	if (*tag != NUL)						/* new pattern, add to the stack */
	{
		/*
		 * if last used entry is not at the top, delete all tag stack entries
		 * above it.
		 */
		while (tagstackidx < tagstacklen)
			vim_free(tagstack[--tagstacklen].tagname);

				/* if tagstack is full: remove oldest entry */
		if (++tagstacklen > TAGSTACKSIZE)
		{
			tagstacklen = TAGSTACKSIZE;
			vim_free(tagstack[0].tagname);
			for (i = 1; i < tagstacklen; ++i)
				tagstack[i - 1] = tagstack[i];
			--tagstackidx;
		}

		/*
		 * put the tag name in the tag stack
		 * the position is added below
		 */
		tagstack[tagstackidx].tagname = strsave(tag);
	}
	else if (tagstacklen == 0)					/* empty stack */
	{
		EMSG("tag stack empty");
		goto end_do_tag;
	}
	else if (type)								/* go to older position */
	{
		if ((tagstackidx -= count) < 0)
		{
			emsg(bottommsg);
			if (tagstackidx + count == 0)
			{
				/* We did ^T (or <num>^T) from the bottom of the stack */
				tagstackidx = 0;
				goto end_do_tag;
			}
			/* We weren't at the bottom of the stack, so jump all the way to
			 * the bottom.
			 */
			tagstackidx = 0;
		}
		else if (tagstackidx >= tagstacklen)	/* must have been count == 0 */
		{
			emsg(topmsg);
			goto end_do_tag;
		}
		if (tagstack[tagstackidx].fmark.fnum != curbuf->b_fnum)
		{
			/*
			 * Jump to other file. If this fails (e.g. because the file was
			 * changed) keep original position in tag stack.
			 */
			if (buflist_getfile(tagstack[tagstackidx].fmark.fnum,
					tagstack[tagstackidx].fmark.mark.lnum,
					GETF_SETMARK, forceit) == FAIL)
			{
				tagstackidx = oldtagstackidx;	/* back to old position */
				goto end_do_tag;
			}
		}
		else
			curwin->w_cursor.lnum = tagstack[tagstackidx].fmark.mark.lnum;
		curwin->w_cursor.col = tagstack[tagstackidx].fmark.mark.col;
		curwin->w_set_curswant = TRUE;
		goto end_do_tag;
	}
	else									/* go to newer pattern */
	{
		if ((tagstackidx += count - 1) >= tagstacklen)
		{
			/*
			 * beyond the last one, just give an error message and go to the
			 * last one
			 */
			tagstackidx = tagstacklen - 1;
			emsg(topmsg);
		}
		else if (tagstackidx < 0)			/* must have been count == 0 */
		{
			emsg(bottommsg);
			tagstackidx = 0;
			goto end_do_tag;
		}
	}
	/*
	 * For :tag [arg], remember position before the jump
	 */
	if (type == 0)
	{
		tagstack[tagstackidx].fmark.mark = curwin->w_cursor;
		tagstack[tagstackidx].fmark.fnum = curbuf->b_fnum;
	}
	/* curwin will change in the call to find_tags() if ^W^] was used -- webb */
	curwin->w_tagstackidx = tagstackidx;
	curwin->w_tagstacklen = tagstacklen;
	if (find_tags(tagstack[tagstackidx].tagname, NULL, NULL, NULL,
														  FALSE, forceit) > 0)
		++tagstackidx;

end_do_tag:
	curwin->w_tagstackidx = tagstackidx;
	curwin->w_tagstacklen = tagstacklen;
}

/*
 * Print the tag stack
 */
	void
do_tags()
{
	int				i;
	char_u			*name;
	struct taggy	*tagstack = curwin->w_tagstack;
	int				tagstackidx = curwin->w_tagstackidx;
	int				tagstacklen = curwin->w_tagstacklen;

	set_highlight('t');		/* Highlight title */
	start_highlight();
	MSG_OUTSTR("\n  # TO tag      FROM line in file");
	stop_highlight();
	for (i = 0; i < tagstacklen; ++i)
	{
		if (tagstack[i].tagname != NULL)
		{
			name = fm_getname(&(tagstack[i].fmark));
			if (name == NULL)		/* file name not available */
				continue;

			msg_outchar('\n');
			sprintf((char *)IObuff, "%c%2d %-15s %4ld  %s",
				i == tagstackidx ? '>' : ' ',
				i + 1,
				tagstack[i].tagname,
				tagstack[i].fmark.mark.lnum,
				name);
			msg_outtrans(IObuff);
		}
		flushbuf();					/* show one line at a time */
	}
	if (tagstackidx == tagstacklen)		/* idx at top of stack */
		MSG_OUTSTR("\n>");
}

/*
 * find_tags() - goto tag or search for tags in tags files
 *
 * If "tag" is not NULL, search for a single tag and jump to it.
 *   return FAIL for failure, OK for success
 * If "tag" is NULL, find all tags matching the regexp given with 'prog'.
 *   return FAIL if search completely failed, OK otherwise.
 *
 * There is a priority in which type of tag is recognized. It is computed
 * from the PRI_ defines below.
 *
 *  6.  A static or global tag with a full matching tag for the current file.
 *  5.  A global tag with a full matching tag for another file.
 *  4.  A static tag with a full matching tag for another file.
 *  2.  A static or global tag with an ignore-case matching tag for the
 *  	current file.
 *  1.  A global tag with an ignore-case matching tag for another file.
 *  0.  A static tag with an ignore-case matching tag for another file.
 *
 *  Tags in an emacs-style tags file are always global.
 */
#define PRI_GLOBAL			1		/* global or emacs tag */
#define PRI_CURRENT			2		/* tag for current file */
#define PRI_FULL_MATCH		4		/* case of tag matches */

	int
find_tags(tag, prog, num_file, file, help_only, forceit)
	char_u		*tag;				/* NULL or tag to search for */
	regexp		*prog;				/* regexp program or NULL */
	int			*num_file;			/* return value: number of matches found */
	char_u		***file;			/* return value: array of matches found */
	int			help_only;			/* if TRUE: only tags for help files */
	int			forceit;			/* :ta with ! */
{
	FILE	   *fp;
	char_u	   *lbuf;					/* line buffer */
	char_u	   *tag_fname;				/* name of tag file */
	int			first_file;				/* trying first tag file */
	char_u	   *tagname, *tagname_end;	/* name of tag in current line */
	char_u	   *fname, *fname_end;		/* fname in current line */
	int			did_open = FALSE;		/* did open a tag file */
	int			stop_searching = FALSE;	/* stop when match found or error */
	int			retval = FAIL;			/* return value */
	int			is_static;				/* current tag line is static */
	int			is_current;				/* file name matches */
	register char_u	*p;
#ifdef EMACS_TAGS
	char_u	   *ebuf;					/* aditional buffer for etag fname */
	int			is_etag;				/* current file is emaces style */
#endif

/*
 * Variables used only when "tag" != NULL
 */
	int			taglen = 0;				/* init for GCC */
	int			cmplen;
	int			full_match;
	int			icase_match;
	int			priority;				/* priority of current line */

	char_u		*bestmatch_line = NULL;	/* saved line for best match found so
											far */
	char_u		*bestmatch_tag_fname = NULL;
										/* copy of tag_fname for best match */
	int			bestmatch_priority = 0; /* best match priority */

#ifdef EMACS_TAGS
	/*
	 * Stack for included emacs-tags file.
	 * It has a fixed size, to truncate cyclic includes. jw
	 */
# define INCSTACK_SIZE 42
	struct
	{
	    FILE	*fp;
		char_u	*tag_fname;
	} incstack[INCSTACK_SIZE];

	int			incstack_idx = 0;			/* index in incstack */
	char_u	   *bestmatch_ebuf = NULL;		/* copy of ebuf for best match */
	int			bestmatch_is_etag = FALSE;	/* copy of is_etag for best match */
#endif

/*
 * Variables used when "tag" == NULL
 */
	char_u	**matches = NULL;			/* array of matches found */
	char_u	**new_matches;
	int		match_limit = 100;			/* max. number of matches stored */
	int		match_count = 0;			/* number of matches found */
	int		i;
	int		help_save;

	help_save = curbuf->b_help;
/*
 * Allocate memory for the buffers that are used
 */
	lbuf = alloc(LSIZE);
#ifdef EMACS_TAGS
	ebuf = alloc(LSIZE);
#endif
	tag_fname = alloc(LSIZE + 1);
									/* check for out of memory situation */
	if ((tag == NULL && prog == NULL) || lbuf == NULL || tag_fname == NULL
#ifdef EMACS_TAGS
														 || ebuf == NULL
#endif
																		)
		goto findtag_end;
	if (tag == NULL)
	{
		matches = (char_u **)alloc((unsigned)(match_limit * sizeof(char_u *)));
		if (matches == NULL)
			goto findtag_end;
	}

/*
 * Initialize a few variables
 */
	if (tag != NULL)
	{
		taglen = STRLEN(tag);
		if (p_tl != 0 && taglen > p_tl)		/* adjust for 'taglength' */
			taglen = p_tl;
	}
	else if (help_only)				/* want tags from help file */
		curbuf->b_help = TRUE;

/*
 * Try tag file names from tags option one by one.
 */
	for (first_file = TRUE; get_tagfname(first_file, tag_fname) == OK;
															first_file = FALSE)
	{
		/*
		 * A file that doesn't exist is silently ignored.
		 */
		if ((fp = fopen((char *)tag_fname, "r")) == NULL)
			continue;
		did_open = TRUE;	/* remember that we found at least one file */

#ifdef EMACS_TAGS
		is_etag = 0;		/* default is: not emacs style */
#endif
		/*
		 * Read and parse the lines in the file one by one
		 */
		while (!got_int)
		{
			line_breakcheck();

			if (vim_fgets(lbuf, LSIZE, fp))
#ifdef EMACS_TAGS
				if (incstack_idx)		/* this was an included file */
				{
					--incstack_idx;
					fclose(fp);			/* end of this file ... */
					fp = incstack[incstack_idx].fp;
					STRCPY(tag_fname, incstack[incstack_idx].tag_fname);
					vim_free(incstack[incstack_idx].tag_fname);
					is_etag = 1;		/* (only etags can include) */
					continue;			/* ... continue with parent file */
				}
				else
#endif
					break;							/* end of file */

#ifdef EMACS_TAGS
			/*
			 * Emacs tags line with CTRL-L: New file name on next line.
			 * The file name is followed by a ','.
			 */
			if (*lbuf == Ctrl('L'))		/* remember etag filename in ebuf */
			{
				is_etag = 1;	
				if (!vim_fgets(ebuf, LSIZE, fp))
				{
					for (p = ebuf; *p && *p != ','; p++)
						;
					*p = NUL;

					/* 
					 * atoi(p+1) is the number of bytes before the next ^L
					 * unless it is an include statement.
					 */
					if (STRNCMP(p + 1, "include", 7) == 0 &&
												 incstack_idx < INCSTACK_SIZE)
					{
						if ((incstack[incstack_idx].tag_fname =
												  strsave(tag_fname)) != NULL)
						{
							incstack[incstack_idx].fp = fp;
							if ((fp = fopen((char *)ebuf, "r")) == NULL)
							{
								fp = incstack[incstack_idx].fp;
								vim_free(incstack[incstack_idx].tag_fname);
							}
							else
							{
								STRCPY(tag_fname, ebuf);
								++incstack_idx;
							}
							is_etag = 0;	/* we can include anything */
						}
					}
				}
				continue;
			}
#endif

			/*
			 * Figure out where the different strings are in this line.
			 * For "normal" tags: Do a quick check if the tag matches.
			 * This speeds up tag searching a lot!
			 */
			if (tag != NULL
#ifdef EMACS_TAGS
							&& !is_etag
#endif
										)
			{
				tagname = lbuf;
				fname = NULL;
				for (tagname_end = lbuf; *tagname_end &&
									!vim_iswhite(*tagname_end); ++tagname_end)
				{
					if (*tagname_end == ':')
					{
						if (fname == NULL)
							fname = skipwhite(skiptowhite(tagname_end));
						if (fnamencmp(lbuf, fname, tagname_end - lbuf) == 0 &&
									   vim_iswhite(fname[tagname_end - lbuf]))
							tagname = tagname_end + 1;
					}
				}

				/*
				 * Skip this line if the lenght of the tag is different.
				 */
				cmplen = tagname_end - tagname;
				if (p_tl != 0 && cmplen > p_tl)		/* adjust for 'taglength' */
					cmplen = p_tl;
				if (taglen != cmplen)
					continue;

				/*
				 * Skip this line if the tag does not match (ignoring case).
				 */
				if (vim_strnicmp(tagname, tag, (size_t)cmplen))
					continue;

				/*
				 * This is a useful tag, isolate the filename.
				 */
				if (fname == NULL)
					fname = skipwhite(skiptowhite(tagname_end));
				fname_end = skiptowhite(fname);
				if (*fname_end == NUL)
					i = FAIL;
				else
					i = OK;
			}
			else
				i = parse_tag_line(lbuf,
#ifdef EMACS_TAGS
								   is_etag,
#endif
							&tagname, &tagname_end, &fname, &fname_end, NULL);
			if (i == FAIL)
			{
				EMSG2("Format error in tags file \"%s\"", tag_fname);
				stop_searching = TRUE;
				break;
			}

#ifdef EMACS_TAGS
			is_static = FALSE;
			if (!is_etag)		/* emacs tags are never static */
#endif
				is_static = test_for_static(&tagname, tagname_end, 
															fname, fname_end);
#ifdef EMACS_TAGS
			if (is_etag)
				fname = ebuf;
#endif
			/*
			 * "tag" == NULL: find tags matching regexp "prog"
			 */
			if (tag == NULL)
			{
				*tagname_end = NUL;
				if (vim_regexec(prog, tagname, TRUE))
				{
					is_current = test_for_current(
#ifdef EMACS_TAGS
							is_etag,
#endif
									 fname, fname_end, tag_fname);
					if (!is_static || is_current)
					{
						/*
						 * Found a match, add it to matches[].
						 * May need to make matches[] larger.
						 */
						if (match_count == match_limit)
						{
							match_limit += 100;
							new_matches = (char_u **)alloc(
									(unsigned)(match_limit * sizeof(char_u *)));
							if (new_matches == NULL)
							{
								/* Out of memory! Just forget about the rest
								 * of the matches. */
								retval = OK;
								stop_searching = TRUE;
								break;
							}
							for (i = 0; i < match_count; i++)
								new_matches[i] = matches[i];
							vim_free(matches);
							matches = new_matches;
						}
						if (help_only)
						{
							int		len;

							/*
							 * Append the help-heuristic number after the
							 * tagname, for sorting it later.
							 */
							len = STRLEN(tagname);
							p = alloc(len + 10);
							if (p != NULL)
							{
								STRCPY(p, tagname);
								sprintf((char *)p + len + 1, "%06d",
													   help_heuristic(tagname,
										   (int)(prog->startp[0] - tagname)));
							}
							matches[match_count++] = p;
						}
						else
							matches[match_count++] = strsave(tagname);
					}
				}
			}
			/*
			 * "tag" != NULL: find tag and jump to it
			 */
			else
			{
				/*
				 * If tag length does not match, skip the rest
				 */
				cmplen = tagname_end - tagname;
				if (p_tl != 0 && cmplen > p_tl)		/* adjust for 'taglength' */
					cmplen = p_tl;
				if (taglen == cmplen)
				{
					/*
					 * Check for match (ignoring case).
					 */
					icase_match = (vim_strnicmp(tagname, tag,
														(size_t)cmplen) == 0);
					if (icase_match)	/* Tag matches somehow */
					{
						/*
						 * If it's a full match for the current file, jump to
						 * it now.
						 */
						full_match = (STRNCMP(tagname, tag, cmplen) == 0);
						is_current = test_for_current(
#ifdef EMACS_TAGS
								is_etag,
#endif
										 fname, fname_end, tag_fname);
						if (full_match && is_current)
						{
							retval = jumpto_tag(lbuf,
#ifdef EMACS_TAGS
													is_etag, ebuf,
#endif
														  tag_fname, forceit);
							stop_searching = TRUE;
							break;
						}

						/*
						 * If the priority of the current line is higher than
						 * the best match so far, store it as the best match
						 */
						if (full_match)
							priority = PRI_FULL_MATCH;
						else
							priority = 0;
						if (is_current)
							priority += PRI_CURRENT;
						if (!is_static)
							priority += PRI_GLOBAL;

						if (priority > bestmatch_priority)
						{
							vim_free(bestmatch_line);
							bestmatch_line = strsave(lbuf);
							vim_free(bestmatch_tag_fname);
							bestmatch_tag_fname = strsave(tag_fname);
							bestmatch_priority = priority;
#ifdef EMACS_TAGS
							bestmatch_is_etag = is_etag;
							if (is_etag)
							{
								vim_free(bestmatch_ebuf);
								bestmatch_ebuf = strsave(ebuf);
							}
#endif
						}
					}
				}
			}
		}
		fclose(fp);
#ifdef EMACS_TAGS
		while (incstack_idx)
		{
			--incstack_idx;
			fclose(incstack[incstack_idx].fp);
			vim_free(incstack[incstack_idx].tag_fname);
		}
#endif
		if (stop_searching)
			break;

		/*
		 * Stop searching if a tag was found in the current tags file and
		 * we got a global match with matching case or 'ignorecase' is set.
		 */
		if (tag != NULL && bestmatch_line != NULL &&
			   bestmatch_priority >= (p_ic ? 0 : PRI_FULL_MATCH) + PRI_GLOBAL)
			break;
	}

	if (!stop_searching)
	{
		if (!did_open)						/* never opened any tags file */
			EMSG("No tags file");
		else if (tag == NULL)
		{
			retval = OK;		/* It's OK even when no tag found */
		}
		else
		{
			/*
			 * If we didn't find a static full match, use the best match found.
			 */
			if (bestmatch_line != NULL)
			{
				if (bestmatch_priority < PRI_FULL_MATCH)
				{
					MSG("Only found tag with different case!");
					if (!msg_scrolled)
					{
						flushbuf();
						mch_delay(1000L, TRUE);
					}
				}
				retval = jumpto_tag(bestmatch_line,
#ifdef EMACS_TAGS
						bestmatch_is_etag, bestmatch_ebuf,
#endif
						bestmatch_tag_fname, forceit);
			}
			else
				EMSG("tag not found");
		}
	}

findtag_end:
	vim_free(lbuf);
	vim_free(tag_fname);
	vim_free(bestmatch_line);
	vim_free(bestmatch_tag_fname);
#ifdef EMACS_TAGS
	vim_free(ebuf);
	vim_free(bestmatch_ebuf);
#endif

	if (tag == NULL)
	{
		if (retval == FAIL)			/* free all matches found */
			while (match_count > 0)
				vim_free(matches[--match_count]);
		if (match_count == 0)		/* nothing found, free matches[] */
		{
			vim_free(matches);
			matches = NULL;
		}
		*file = matches;
		*num_file = match_count;
	}
	curbuf->b_help = help_save;

	return retval;
}

/*
 * Get the next name of a tag file from the tag file list.
 * For help files, use "vim_tags" file only.
 *
 * Return FAIL if no more tag file names, OK otherwise.
 */
	static int
get_tagfname(first, buf)
	int		first;			/* TRUE when first file name is wanted */
	char_u	*buf;			/* pointer to buffer of LSIZE chars */
{
	static char_u	*np = NULL;
	char_u			*fname;
	size_t			path_len, fname_len;
	/*
	 * A list is made of the files that have been visited.
	 */
	struct visited
	{
		struct visited	*v_next;
#if defined(UNIX)
		struct stat		v_st;
#else
		char_u			v_fname[1];		/* actually longer */
#endif
	};
	static struct visited	*first_visited = NULL;
	struct visited			*vp;
#if defined(UNIX)
	struct stat				st;
#endif

	if (first)
	{
		np = p_tags;
		while (first_visited != NULL)
		{
			vp = first_visited->v_next;
			vim_free(first_visited);
			first_visited = vp;
		}
	}

	if (np == NULL)			/* tried allready (or bogus call) */
		return FAIL;

	/*
	 * For a help window only try the file 'vim_tags' in the same
	 * directory as 'helpfile'.
	 */
	if (curbuf->b_help)
	{
		path_len = gettail(p_hf) - p_hf;
		if (path_len + 9 >= LSIZE)
			return FAIL;
		vim_memmove(buf, p_hf, path_len);
		STRCPY(buf + path_len, "vim_tags");

		np = NULL;				/* try only once */
	}

	else
	{
		/*
		 * Loop until we have found a file name that can be used.
		 */
		for (;;)
		{
			if (*np == NUL)			/* tried all possibilities */
				return FAIL;

			/*
			 * Copy next file name into buf.
			 */
			(void)copy_option_part(&np, buf, LSIZE, " ,");

			/*
			 * Tag file name starting with "./": Replace '.' with path of
			 * current file.
			 */
			if (buf[0] == '.' && ispathsep(buf[1]))
			{
				if (curbuf->b_filename == NULL)	/* skip if no filename */
					continue;

				path_len = gettail(curbuf->b_filename) - curbuf->b_filename;
				fname = buf + 1;
				while (ispathsep(*fname))		/* skip '/' and the like */
					++fname;
				fname_len = STRLEN(fname);
				if (fname_len + path_len + 1 > LSIZE)
					continue;
				vim_memmove(buf + path_len, fname, fname_len + 1);
				vim_memmove(buf, curbuf->b_filename, path_len);
			}

			/*
			 * Check if this tags file has been used already.
			 * If file doesn't exist, skip it.
			 */
#if defined(UNIX)
			if (stat((char *)buf, &st) < 0)
#else
			if (FullName(buf, NameBuff, MAXPATHL, TRUE) == FAIL)
#endif
				continue;

			for (vp = first_visited; vp != NULL; vp = vp->v_next)
#if defined(UNIX)
				if (vp->v_st.st_dev == st.st_dev &&
												 vp->v_st.st_ino == st.st_ino)
#else
				if (fnamecmp(vp->v_fname, NameBuff) == 0)
#endif
					break;

			if (vp != NULL)			/* already visited, skip it */
				continue;

			/*
			 * Found the next name.  Add it to the list of visited files.
			 */
#if defined(UNIX)
			vp = (struct visited *)alloc((unsigned)sizeof(struct visited));
#else
			vp = (struct visited *)alloc((unsigned)(sizeof(struct visited) +
														   STRLEN(NameBuff)));
#endif
			if (vp != NULL)
			{
#if defined(UNIX)
				vp->v_st = st;
#else
				STRCPY(vp->v_fname, NameBuff);
#endif
				vp->v_next = first_visited;
				first_visited = vp;
			}
			break;
		}
	}
	return OK;
}

/*
 * Parse one line from the tags file. Find start/end of tag name, start/end of
 * file name and start of search pattern.
 *
 * If is_etag is TRUE, fname and fname_end are not set.
 * If command == NULL it is not set.
 *
 * Return FAIL if there is a format error in this line, OK otherwise.
 */
	static int
parse_tag_line(lbuf,
#ifdef EMACS_TAGS
					is_etag,
#endif
							  tagname, tagname_end, fname, fname_end, command)
	char_u		*lbuf;
#ifdef EMACS_TAGS
	int			is_etag;
#endif
	char_u		**tagname;
	char_u		**tagname_end;
	char_u		**fname;
	char_u		**fname_end;
	char_u		**command;
{
	char_u		*p;

#ifdef EMACS_TAGS
	char_u		*p_7f;

	if (is_etag)
	{
		/*
		 * There are two formats for an emacs tag line:
		 * 1:  struct EnvBase ^?EnvBase^A139,4627
		 * 2: #define	ARPB_WILD_WORLD ^?153,5194
		 */
		p_7f = vim_strchr(lbuf, 0x7f);
		if (p_7f == NULL)
			return FAIL;
										/* find start of line number */
		for (p = p_7f + 1; *p < '0' || *p > '9'; ++p)
			if (*p == NUL)
				return FAIL;
		if (command != NULL)
			*command = p;

								/* first format: explicit tagname given */
		if (p[-1] == Ctrl('A'))
		{
			*tagname = p_7f + 1;
			*tagname_end = p - 1;
		}
		else
								/* second format: isolate tagname */
		{
			/* find end of tagname */
			for (p = p_7f - 1; *p == ' ' || *p == '\t' || 
												  *p == '(' || *p == ';'; --p)
				if (p == lbuf)
					return FAIL;
			*tagname_end = p + 1;
			while (p >= lbuf && *p != ' ' && *p != '\t')
				--p;
			*tagname = p + 1;
		}
	}
	else
	{
#endif
			/* Isolate the tagname, from lbuf up to the first white */
		*tagname = lbuf;
		p = skiptowhite(lbuf);
		if (*p == NUL)
			return FAIL;
		*tagname_end = p;

			/* Isolate file name, from first to second white space */
		p = skipwhite(p);
		*fname = p;
		p = skiptowhite(p);
		if (*p == NUL)
			return FAIL;
		*fname_end = p;

			/* find start of search command, after second white space */
		if (command != NULL)
		{
			p = skipwhite(p);
			if (*p == NUL)
				return FAIL;
			*command = p;
		}
#ifdef EMACS_TAGS
	}
#endif

	return OK;
}

/*
 * Check if tagname is a static tag
 *
 * Static tags produced by the ctags program have the
 * format: 'file:tag  file  /pattern'.
 * This is only recognized when both occurences of 'file'
 * are the same, to avoid recognizing "string::string" or
 * ":exit".
 *
 * Return TRUE if it is a static tag and adjust *tagname to the real tag.
 * Return FALSE if it is not a static tag.
 */
	static int
test_for_static(tagname, tagname_end, fname, fname_end)
	char_u		**tagname;
	char_u		*tagname_end;
	char_u		*fname;
	char_u		*fname_end;
{
	char_u		*p;

	p = *tagname + (fname_end - fname);
	if (p < tagname_end && *p == ':' &&
							fnamencmp(*tagname, fname, fname_end - fname) == 0)
	{
		*tagname = p + 1;
		return TRUE;
	}
	return FALSE;
}

/*
 * Jump to a tag that has been found in one of the tag files
 */
	static int
jumpto_tag(lbuf,
#ifdef EMACS_TAGS
				is_etag, etag_fname,
#endif
									tag_fname, forceit)
	char_u		*lbuf;			/* line from the tags file for this tag */
#ifdef EMACS_TAGS
	int			is_etag;		/* TRUE if it's from an emacs tags file */
	char_u		*etag_fname;	/* file name for tag if is_etag is TRUE */
#endif
	char_u		*tag_fname;		/* file name of the tags file itself */
	int			forceit;		/* :ta with ! */
{
	int			save_secure;
	int			save_p_ws, save_p_scs, save_p_ic;
	char_u		*str;
	char_u		*pbuf;					/* search pattern buffer */
	char_u		*p;
	char_u		*expanded_fname = NULL;
	char_u		*tagname, *tagname_end;
	char_u		*fname, *fname_end;
	char_u		*orig_fname;
	int			retval = FAIL;
	int			getfile_result;
	int			search_options;

	pbuf = alloc(LSIZE);

	if (pbuf == NULL
#ifdef EMACS_TAGS
					|| (is_etag && etag_fname == NULL)
#endif
														|| tag_fname == NULL)
		goto erret;

	/*
	 * find the search pattern (caller should check it is there)
	 */
	if (parse_tag_line(lbuf,
#ifdef EMACS_TAGS
							   is_etag,
#endif
					&tagname, &tagname_end, &fname, &fname_end, &str) == FAIL)
		goto erret;
	orig_fname = fname;		/* remember for test_for_static() below */

#ifdef EMACS_TAGS
	if (is_etag)
		fname = etag_fname;
	else
#endif
		*fname_end = NUL;

	/*
	 * If the command is a string like "/^function fname"
	 * scan through the search string. If we see a magic
	 * char, we have to quote it. This lets us use "real"
	 * implementations of ctags.
	 */
	if (*str == '/' || *str == '?')
	{
		p = pbuf;
		*p++ = *str++;			/* copy the '/' or '?' */
		if (*str == '^')
			*p++ = *str++;			/* copy the '^' */

		while (*str)
		{
			switch (*str)
			{
						/* Always remove '\' before '('.
						 * Remove a '\' befor '*' if 'nomagic'.
						 * Otherwise just copy the '\' and don't look at the
						 * next character
						 */
			case '\\':	if (str[1] == '(' || (!p_magic && str[1] == '*'))
							++str;
						else
							*p++ = *str++;
						break;

			case '\r':
			case '\n':	*str = pbuf[0];	/* copy '/' or '?' */
						str[1] = NUL;	/* delete NL after CR */
						break;

						/*
						 * if string ends in search character: skip it
						 * else escape it with '\'
						 */
			case '/':
			case '?':	if (*str != pbuf[0])	/* not the search char */
							break;
												/* last char */
						if (str[1] == '\n' || str[1] == '\r')
						{
							++str;
							continue;
						}
			case '[':
						if (!p_magic)
							break;
			case '^':
			case '*':
			case '~':
			case '.':	*p++ = '\\';
						break;
			}
			*p++ = *str++;
		}
	}
	else		/* not a search command, just copy it */
	{
		for (p = pbuf; *str && *str != '\n' && *str != '\r'; )
		{
#ifdef EMACS_TAGS
			if (is_etag && *str == ',')		/* stop at ',' after line number */
				break;
#endif
			*p++ = *str++;
		}
	}
	*p = NUL;

	/*
	 * expand filename (for environment variables)
	 */
	expanded_fname = ExpandOne((char_u *)fname, NULL, WILD_LIST_NOTFOUND,
															WILD_EXPAND_FREE);
	if (expanded_fname != NULL)
		fname = expanded_fname;

	/*
	 * if 'tagrelative' option set, may change file name
	 */
	fname = expand_rel_name(fname, tag_fname);

	/*
	 * check if file for tag exists before abandoning current file
	 */
	if (getperm(fname) < 0)
	{
		EMSG2("File \"%s\" does not exist", fname);
		goto erret;
	}

	++RedrawingDisabled;
	/*
	 * if it was a CTRL-W CTRL-] command split window now
	 */
	if (postponed_split)
		win_split(0, FALSE);
	/*
	 * A :ta from a help file will keep the b_help flag set.
	 */
	keep_help_flag = curbuf->b_help;
	getfile_result = getfile(0, fname, NULL, TRUE, (linenr_t)0, forceit);

	if (getfile_result <= 0)			/* got to the right file */
	{
		curwin->w_set_curswant = TRUE;
		postponed_split = FALSE;

		save_secure = secure;
		secure = 1;
		/*
		 * If 'cpoptions' contains 't', store the search pattern for the "n"
		 * command.  If 'cpoptions' does not contain 't', the search pattern
		 * is not stored.
		 */
		if (vim_strchr(p_cpo, CPO_TAGPAT) != NULL)
			search_options = 0;
		else
			search_options = SEARCH_KEEP;

		/*
		 * if the command is a search, try here
		 *
		 * Rather than starting at line one, just turn wrap-scan
		 * on temporarily, this ensures that tags on line 1 will
		 * be found, and makes sure our guess searches search the
		 * whole file when repeated -- webb.
		 * Also reset 'smartcase' for the search, since the search
		 * pattern was not typed by the user.
		 */
		if (pbuf[0] == '/' || pbuf[0] == '?')
		{
			save_p_ws = p_ws;
			save_p_ic = p_ic;
			save_p_scs = p_scs;
			p_ws = TRUE;	/* Switch wrap-scan on temporarily */
			p_ic = FALSE;	/* don't ignore case now */
			p_scs = FALSE;
			add_to_history(1, pbuf + 1);	/* put pattern in search history */

			if (do_search(pbuf[0], pbuf + 1, (long)1, search_options))
				retval = OK;
			else
			{
				register int notfound = FALSE;

				/*
				 * try again, ignore case now
				 */
				p_ic = TRUE;
				if (!do_search(pbuf[0], pbuf + 1, (long)1, search_options))
				{
					/*
					 * Failed to find pattern, take a guess: "^func  ("
					 */
					(void)test_for_static(&tagname, tagname_end,
														orig_fname, fname_end);
					*tagname_end = NUL;
					sprintf((char *)pbuf, "^%s[ \t]*(", tagname);
					if (!do_search('/', pbuf, (long)1, search_options))
					{
						/* Guess again: "^char * func  (" */
						sprintf((char *)pbuf, "^[#a-zA-Z_].*%s[ \t]*(",
																	 tagname);
						if (!do_search('/', pbuf, (long)1, search_options))
							notfound = TRUE;
					}
				}
				if (notfound)
					EMSG("Can't find tag pattern");
				else
				{
					MSG("Couldn't find tag, just guessing!");
					if (!msg_scrolled)
					{
						flushbuf();
						mch_delay(1000L, TRUE);
					}
					retval = OK;
				}
			}
			p_ws = save_p_ws;
			p_ic = save_p_ic;
			p_scs = save_p_scs;
		}
		else
		{							/* start command in line 1 */
			curwin->w_cursor.lnum = 1;
			do_cmdline(pbuf, TRUE, TRUE);
			retval = OK;
		}

		if (secure == 2)			/* done something that is not allowed */
			wait_return(TRUE);
		secure = save_secure;

		/*
		 * Print the file message after redraw if jumped to another file.
		 * Don't do this for help files (avoid a hit-return message).
		 */
		if (getfile_result == -1)
		{
			if (!curbuf->b_help)
				need_fileinfo = TRUE;
			retval = OK;			/* always return OK if jumped to another
									   file (at least we found the file!) */
		}

		/*
		 * For a help buffer: Put the cursor line at the top of the window,
		 * the help subject will be below it.
		 */
		if (curbuf->b_help)
		{
			curwin->w_topline = curwin->w_cursor.lnum;
			comp_Botline(curwin);
			cursupdate();		/* take care of 'scrolloff' */
			updateScreen(NOT_VALID);
		}
		--RedrawingDisabled;
	}
	else
	{
		--RedrawingDisabled;
		if (postponed_split)			/* close the window */
		{
			close_window(curwin, FALSE);
			postponed_split = FALSE;
		}
	}

erret:
	vim_free(pbuf);
	vim_free(expanded_fname);

	return retval;
}

/*
 * If 'tagrelative' option set, change fname (name of file containing tag)
 * according to tag_fname (name of tag file containing fname).
 */
	static char_u *
expand_rel_name(fname, tag_fname)
	char_u		*fname;
	char_u		*tag_fname;
{
	char_u		*p;

	if ((p_tr || curbuf->b_help) && !isFullName(fname) &&
									   (p = gettail(tag_fname)) != tag_fname)
	{
		STRCPY(NameBuff, tag_fname);
		STRNCPY(NameBuff + (p - tag_fname), fname,
												 MAXPATHL - (p - tag_fname));
		/*
		 * Translate names like "src/a/../b/file.c" into "src/b/file.c".
		 */
		simplify_filename(NameBuff);
		fname = NameBuff;
	}
	return fname;
}

/*
 * Moves the tail part of the path (including the terminating NUL) pointed to
 * by "tail" to the new location pointed to by "here". This should accomodate
 * an overlapping move.
 */
#define movetail(here, tail)  vim_memmove(here, tail, STRLEN(tail) + (size_t)1)

/*
 * For MS-DOS we should check for backslash too, but that is complicated.
 */
#define DIR_SEP		'/'			/* the directory separator character */

/*
 * Converts a filename into a canonical form. It simplifies a filename into
 * its simplest form by stripping out unneeded components, if any.  The
 * resulting filename is simplified in place and will either be the same
 * length as that supplied, or shorter.
 */
	static void
simplify_filename(filename)
	char_u *filename;
{
	int		absolute = FALSE;
	int		components = 0;
	char_u	*p, *tail;

	p = filename;
	if (*p == DIR_SEP)
	{
		absolute = TRUE;
		++p;
	}
	do
	{
		/*  Always leave "p" pointing to character following next "/". */
		if (*p == DIR_SEP)
			movetail(p, p+1);				/* strip duplicate "/" */
		else if (STRNCMP(p, "./", 2) == 0)
			movetail(p, p+2);				/* strip "./" */
		else if (STRNCMP(p, "../", 3) == 0)
		{
			if (components > 0)				/* strip any prev. component */
			{
				*(p - 1) = 0;				/* delete "/" before  "../" */
				tail  = p + 2;				/* skip to "/" of "../" */
				p = vim_strrchr(filename, DIR_SEP);	 /* find preceding sep. */
				if (p != NULL)				/* none found */
					++p;					/* skip to char after "/" */
				else
				{
					++tail;					/* strip leading "/" from tail*/
					p = filename;			/* go back to beginning */
					if (absolute)			/* skip over any leading "/" */
						++p;
				}
				movetail(p, tail);			/* strip previous component */
				--components;
			}
			else if (absolute)				/* no parent to root... */
				movetail(p, p+3);			/*   so strip "../" */
			else							/* leading series of "../" */
			{
				p = vim_strchr(p, DIR_SEP);	/* skip to next "/" */
				if (p != NULL)
					++p;					/* skip to char after "/" */
			}
		}
		else
		{
			++components;					/* simple path component */
			p = vim_strchr(p, DIR_SEP);			/* skip to next "/" */
			if (p != NULL)
				++p;						/* skip to char after "/" */
		}
	} while (p != NULL && *p != NUL);
}

/*
 * Check if we have a tag for the current file.
 * This is a bit slow, because of the full path compare in fullpathcmp().
 * Return TRUE if tag for file "fname" if tag file "tag_fname" is for current
 * file.
 */
	static int
#ifdef EMACS_TAGS
test_for_current(is_etag, fname, fname_end, tag_fname)
	int		is_etag;
#else
test_for_current(fname, fname_end, tag_fname)
#endif
	char_u	*fname;
	char_u	*fname_end;
	char_u	*tag_fname;
{
	int		c;
	int		retval;

	if (curbuf->b_filename == NULL)
		retval = FALSE;
	else
	{
#ifdef EMACS_TAGS
		if (is_etag)
			c = 0;			/* to shut up GCC */
		else
#endif
		{
			c = *fname_end;
			*fname_end = NUL;
		}
		retval = (fullpathcmp(expand_rel_name(fname, tag_fname),
											curbuf->b_xfilename) == FPC_SAME);
#ifdef EMACS_TAGS
		if (!is_etag)
#endif
			*fname_end = c;
	}

	return retval;
}
