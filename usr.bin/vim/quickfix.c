/*	$OpenBSD: quickfix.c,v 1.1.1.1 1996/09/07 21:40:25 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * quickfix.c: functions for quickfix mode, using a file with error messages
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

static void qf_free __ARGS((void));
static char_u *qf_types __ARGS((int, int));

/*
 * for each error the next struct is allocated and linked in a list
 */
struct qf_line
{
	struct qf_line	*qf_next;	/* pointer to next error in the list */
	struct qf_line	*qf_prev;	/* pointer to previous error in the list */
	linenr_t		 qf_lnum;	/* line number where the error occurred */
	int				 qf_fnum;	/* file number for the line */
	int				 qf_col;	/* column where the error occurred */
	int				 qf_nr;		/* error number */
	char_u			*qf_text;	/* description of the error */
	char_u			 qf_cleared;/* set to TRUE if line has been deleted */
	char_u			 qf_type;	/* type of the error (mostly 'E') */
	char_u			 qf_valid;	/* valid error message detected */
};

static struct qf_line *qf_start;		/* pointer to the first error */
static struct qf_line *qf_ptr;			/* pointer to the current error */

static int	qf_count = 0;		/* number of errors (0 means no error list) */
static int	qf_index;			/* current index in the error list */
static int	qf_nonevalid;		/* set to TRUE if not a single valid entry found */

#define MAX_ADDR	7			/* maximum number of % recognized, also adjust
									sscanf() below */

/*
 * Structure used to hold the info of one part of 'errorformat'
 */
struct eformat
{
	char_u			*fmtstr;		/* pre-formatted part of 'errorformat' */
#ifdef UTS2
	char_u			*(adr[MAX_ADDR]);	/* addresses used */
#else
	void			*(adr[MAX_ADDR]);
#endif
	int				adr_cnt;		/* number of addresses used */
	struct eformat	*next;			/* pointer to next (NULL if last) */
};

/*
 * Read the errorfile into memory, line by line, building the error list.
 * Return FAIL for error, OK for success.
 */
	int
qf_init()
{
	char_u 			*namebuf;
	char_u			*errmsg;
	int				col;
	int				type;
	int				valid;
	long			lnum;
	int				enr;
	FILE			*fd;
	struct qf_line	*qfp = NULL;
	struct qf_line	*qfprev = NULL;		/* init to make SASC shut up */
	char_u			*efmp;
	struct eformat	*fmt_first = NULL;
	struct eformat	*fmt_last = NULL;
	struct eformat	*fmt_ptr;
	char_u			*efm;
	int				maxlen;
	int				len;
	int				i, j;
	int				retval = FAIL;

	if (*p_ef == NUL)
	{
		emsg(e_errorf);
		return FAIL;
	}

	namebuf = alloc(CMDBUFFSIZE + 1);
	errmsg = alloc(CMDBUFFSIZE + 1);
	if (namebuf == NULL || errmsg == NULL)
		goto qf_init_end;

	if ((fd = fopen((char *)p_ef, "r")) == NULL)
	{
		emsg2(e_openerrf, p_ef);
		goto qf_init_end;
	}
	qf_free();
	qf_index = 0;

/*
 * Each part of the format string is copied and modified from p_efm to fmtstr.
 * Only a few % characters are allowed.
 */
	efm = p_efm;
	while (efm[0])
	{
		/*
		 * Allocate a new eformat structure and put it at the end of the list
		 */
		fmt_ptr = (struct eformat *)alloc((unsigned)sizeof(struct eformat));
		if (fmt_ptr == NULL)
			goto error2;
		if (fmt_first == NULL)		/* first one */
			fmt_first = fmt_ptr;
		else
			fmt_last->next = fmt_ptr;
		fmt_last = fmt_ptr;
		fmt_ptr->next = NULL;
		fmt_ptr->adr_cnt = 0;

		/*
		 * Isolate one part in the 'errorformat' option
		 */
		for (len = 0; efm[len] != NUL && efm[len] != ','; ++len)
			if (efm[len] == '\\' && efm[len + 1] != NUL)
				++len;

		/*
		 * Get some space to modify the format string into.
		 * Must be able to do the largest expansion (x3) MAX_ADDR times.
		 */
		maxlen = len + MAX_ADDR * 3 + 4;
		if ((fmt_ptr->fmtstr = alloc(maxlen)) == NULL)
			goto error2;

		for (i = 0; i < MAX_ADDR; ++i)
			fmt_ptr->adr[i] = NULL;

		for (efmp = efm, i = 0; efmp < efm + len; ++efmp, ++i)
		{
			if (efmp[0] != '%')				/* copy normal character */
			{
				if (efmp[0] == '\\' && efmp + 1 < efm + len)
					++efmp;
				fmt_ptr->fmtstr[i] = efmp[0];
			}
			else
			{
				fmt_ptr->fmtstr[i++] = '%';
				switch (efmp[1])
				{
				case 'f':		/* filename */
						fmt_ptr->adr[fmt_ptr->adr_cnt++] = namebuf;
						/* FALLTHROUGH */

				case 'm':		/* message */
						if (efmp[1] == 'm')
							fmt_ptr->adr[fmt_ptr->adr_cnt++] = errmsg;
						fmt_ptr->fmtstr[i++] = '[';
						fmt_ptr->fmtstr[i++] = '^';
#ifdef __EMX__
						/* don't allow spaces in filename. This fixes
						 * the broken sscanf() where an empty message
						 * is accepted as a valid conversion.
						 */
						if (efmp[1] == 'f')
							fmt_ptr->fmtstr[i++] = ' ';
#endif
						if (efmp[2] == '\\')		/* could be "%m\," */
							j = 3;
						else
							j = 2;
						if (efmp + j < efm + len)
							fmt_ptr->fmtstr[i++] = efmp[j];
						else
						{
							/*
							 * The %f or %m is the last one in the format,
							 * stop at the CR of NL at the end of the line.
							 */
#ifdef USE_CRNL
							fmt_ptr->fmtstr[i++] = '\r';
#endif
							fmt_ptr->fmtstr[i++] = '\n';
						}
						fmt_ptr->fmtstr[i] = ']';
						break;
				case 'c':		/* column */
						fmt_ptr->adr[fmt_ptr->adr_cnt++] = &col;
						fmt_ptr->fmtstr[i] = 'd';
						break;
				case 'l':		/* line */
						fmt_ptr->adr[fmt_ptr->adr_cnt++] = &lnum;
						fmt_ptr->fmtstr[i++] = 'l';
						fmt_ptr->fmtstr[i] = 'd';
						break;
				case 'n':		/* error number */
						fmt_ptr->adr[fmt_ptr->adr_cnt++] = &enr;
						fmt_ptr->fmtstr[i] = 'd';
						break;
				case 't':		/* error type */
						fmt_ptr->adr[fmt_ptr->adr_cnt++] = &type;
						fmt_ptr->fmtstr[i] = 'c';
						break;
				case '%':		/* %% */
				case '*':		/* %*: no assignment */
						fmt_ptr->fmtstr[i] = efmp[1];
						break;
				default:
						EMSG("invalid % in format string");
						goto error2;
				}
				if (fmt_ptr->adr_cnt == MAX_ADDR)
				{
					EMSG("too many % in format string");
					goto error2;
				}
				++efmp;
			}
			if (i >= maxlen - 6)
			{
				EMSG("invalid format string");
				goto error2;
			}
		}
		fmt_ptr->fmtstr[i] = NUL;

		/*
		 * Advance to next part
		 */
		efm = skip_to_option_part(efm + len);	/* skip comma and spaces */
	}
	if (fmt_first == NULL)		/* nothing found */
	{
		EMSG("'errorformat' contains no pattern");
		goto error2;
	}

	/*
	 * Read the lines in the error file one by one.
	 * Try to recognize one of the error formats in each line.
	 */
	while (fgets((char *)IObuff, CMDBUFFSIZE, fd) != NULL && !got_int)
	{
		if ((qfp = (struct qf_line *)alloc((unsigned)sizeof(struct qf_line)))
																	  == NULL)
			goto error2;

		IObuff[CMDBUFFSIZE] = NUL;	/* for very long lines */

		/*
		 * Try to match each part of 'errorformat' until we find a complete
		 * match or none matches.
		 */
		valid = TRUE;
		for (fmt_ptr = fmt_first; fmt_ptr != NULL; fmt_ptr = fmt_ptr->next)
		{
			namebuf[0] = NUL;
			errmsg[0] = NUL;
			lnum = 0;
			col = 0;
			enr = -1;
			type = 0;

			/*
			 * If first char of the format and message don't match, there is
			 * no need to try sscanf() on it... Somehow I believe there are
			 * very slow implementations of sscanf().
			 * -- Paul Slootman
			 */
			if (fmt_ptr->fmtstr[0] != '%' && fmt_ptr->fmtstr[0] != IObuff[0])
				continue;

			if (sscanf((char *)IObuff, (char *)fmt_ptr->fmtstr,
						fmt_ptr->adr[0], fmt_ptr->adr[1], fmt_ptr->adr[2],
						fmt_ptr->adr[3], fmt_ptr->adr[4], fmt_ptr->adr[5],
						fmt_ptr->adr[6]) == fmt_ptr->adr_cnt)
				break;
		}
		if (fmt_ptr == NULL)
		{
			namebuf[0] = NUL;			/* no match found, remove file name */
			lnum = 0;					/* don't jump to this line */
			valid = FALSE;
			STRCPY(errmsg, IObuff);		/* copy whole line to error message */
			if ((efmp = vim_strrchr(errmsg, '\n')) != NULL)
				*efmp = NUL;
#ifdef USE_CRNL
			if ((efmp = vim_strrchr(errmsg, '\r')) != NULL)
				*efmp = NUL;
#endif
		}

		if (namebuf[0] == NUL)			/* no file name */
			qfp->qf_fnum = 0;
		else
			qfp->qf_fnum = buflist_add(namebuf);
		if ((qfp->qf_text = strsave(errmsg)) == NULL)
			goto error1;
		qfp->qf_lnum = lnum;
		qfp->qf_col = col;
		qfp->qf_nr = enr;
		qfp->qf_type = type;
		qfp->qf_valid = valid;

		if (qf_count == 0)		/* first element in the list */
		{
			qf_start = qfp;
			qfp->qf_prev = qfp;	/* first element points to itself */
		}
		else
		{
			qfp->qf_prev = qfprev;
			qfprev->qf_next = qfp;
		}
		qfp->qf_next = qfp;		/* last element points to itself */
		qfp->qf_cleared = FALSE;
		qfprev = qfp;
		++qf_count;
		if (qf_index == 0 && qfp->qf_valid)		/* first valid entry */
		{
			qf_index = qf_count;
			qf_ptr = qfp;
		}
		line_breakcheck();
	}
	if (!ferror(fd))
	{
		if (qf_index == 0)		/* no valid entry found */
		{
			qf_ptr = qf_start;
			qf_index = 1;
			qf_nonevalid = TRUE;
		}
		else
			qf_nonevalid = FALSE;
		retval = OK;
		goto qf_init_ok;
	}
	emsg(e_readerrf);
error1:
	vim_free(qfp);
error2:
	qf_free();
qf_init_ok:
	fclose(fd);
	for (fmt_ptr = fmt_first; fmt_ptr != NULL; fmt_ptr = fmt_first)
	{
		fmt_first = fmt_ptr->next;
		vim_free(fmt_ptr->fmtstr);
		vim_free(fmt_ptr);
	}
qf_init_end:
	vim_free(namebuf);
	vim_free(errmsg);
	return retval;
}

/*
 * jump to a quickfix line
 * if dir == FORWARD go "errornr" valid entries forward
 * if dir == BACKWARD go "errornr" valid entries backward
 * else if "errornr" is zero, redisplay the same line
 * else go to entry "errornr"
 */
	void
qf_jump(dir, errornr)
	int		dir;
	int		errornr;
{
	struct qf_line	*old_qf_ptr;
	int				old_qf_index;
	static char_u	*e_no_more_errors = (char_u *)"No more errors";
	char_u			*err = e_no_more_errors;
	linenr_t		i;

	if (qf_count == 0)
	{
		emsg(e_quickfix);
		return;
	}

	old_qf_ptr = qf_ptr;
	old_qf_index = qf_index;
	if (dir == FORWARD)		/* next valid entry */
	{
		while (errornr--)
		{
			old_qf_ptr = qf_ptr;
			old_qf_index = qf_index;
			do
			{
				if (qf_index == qf_count || qf_ptr->qf_next == NULL)
				{
					qf_ptr = old_qf_ptr;
					qf_index = old_qf_index;
					if (err != NULL)
					{
						emsg(err);
						return;
					}
					errornr = 0;
					break;
				}
				++qf_index;
				qf_ptr = qf_ptr->qf_next;
			} while (!qf_nonevalid && !qf_ptr->qf_valid);
			err = NULL;
		}
	}
	else if (dir == BACKWARD)		/* previous valid entry */
	{
		while (errornr--)
		{
			old_qf_ptr = qf_ptr;
			old_qf_index = qf_index;
			do
			{
				if (qf_index == 1 || qf_ptr->qf_prev == NULL)
				{
					qf_ptr = old_qf_ptr;
					qf_index = old_qf_index;
					if (err != NULL)
					{
						emsg(err);
						return;
					}
					errornr = 0;
					break;
				}
				--qf_index;
				qf_ptr = qf_ptr->qf_prev;
			} while (!qf_nonevalid && !qf_ptr->qf_valid);
			err = NULL;
		}
	}
	else if (errornr != 0)		/* go to specified number */
	{
		while (errornr < qf_index && qf_index > 1 && qf_ptr->qf_prev != NULL)
		{
			--qf_index;
			qf_ptr = qf_ptr->qf_prev;
		}
		while (errornr > qf_index && qf_index < qf_count && qf_ptr->qf_next != NULL)
		{
			++qf_index;
			qf_ptr = qf_ptr->qf_next;
		}
	}

	/*
	 * If there is a file name, 
	 * read the wanted file if needed, and check autowrite etc.
	 */
	if (qf_ptr->qf_fnum == 0 || buflist_getfile(qf_ptr->qf_fnum,
											 (linenr_t)1, GETF_SETMARK) == OK)
	{
		/*
		 * Go to line with error, unless qf_lnum is 0.
		 */
		i = qf_ptr->qf_lnum;
		if (i > 0)
		{
			if (i > curbuf->b_ml.ml_line_count)
				i = curbuf->b_ml.ml_line_count;
			curwin->w_cursor.lnum = i;
		}
		if (qf_ptr->qf_col > 0)
		{
			curwin->w_cursor.col = qf_ptr->qf_col - 1;
			adjust_cursor();
		}
		else
			beginline(TRUE);
		cursupdate();
		smsg((char_u *)"(%d of %d)%s%s: %s", qf_index, qf_count, 
					qf_ptr->qf_cleared ? (char_u *)" (line deleted)" : (char_u *)"",
					qf_types(qf_ptr->qf_type, qf_ptr->qf_nr), qf_ptr->qf_text);
		/*
		 * if the message is short, redisplay after redrawing the screen
		 */
		if (linetabsize(IObuff) < ((int)p_ch - 1) * Columns + sc_col)
			keep_msg = IObuff;
	}
	else if (qf_ptr->qf_fnum != 0)
	{
		/*
		 * Couldn't open file, so put index back where it was.  This could
		 * happen if the file was readonly and we changed something - webb
		 */
		qf_ptr = old_qf_ptr;
		qf_index = old_qf_index;
  	}
}

/*
 * list all errors
 */
	void
qf_list(all)
	int all;		/* If not :cl!, only show recognised errors */
{
	BUF				*buf;
	char_u			*fname;
	struct qf_line	*qfp;
	int				i;

	if (qf_count == 0)
	{
		emsg(e_quickfix);
		return;
	}

	if (qf_nonevalid)
		all = TRUE;
	qfp = qf_start;
	set_highlight('d');		/* Same as for directories */
	for (i = 1; !got_int && i <= qf_count; ++i)
	{
		if (qfp->qf_valid || all)
		{
			msg_outchar('\n');
			start_highlight();
			fname = NULL;
			if (qfp->qf_fnum != 0 &&
								 (buf = buflist_findnr(qfp->qf_fnum)) != NULL)
				fname = buf->b_xfilename;
			if (fname == NULL)
				sprintf((char *)IObuff, "%2d", i);
			else
				sprintf((char *)IObuff, "%2d %s", i, fname);
			msg_outtrans(IObuff);
			stop_highlight();
			if (qfp->qf_lnum == 0)
				IObuff[0] = NUL;
			else if (qfp->qf_col == 0)
				sprintf((char *)IObuff, ":%ld", qfp->qf_lnum);
			else
				sprintf((char *)IObuff, ":%ld, col %d",
												   qfp->qf_lnum, qfp->qf_col);
			sprintf((char *)IObuff + STRLEN(IObuff), "%s: ",
										qf_types(qfp->qf_type, qfp->qf_nr));
			msg_outstr(IObuff);
			msg_prt_line(qfp->qf_text);
			flushbuf();					/* show one line at a time */
		}
		qfp = qfp->qf_next;
		mch_breakcheck();
	}
}

/*
 * free the error list
 */
	static void
qf_free()
{
	struct qf_line *qfp;

	while (qf_count)
	{
		qfp = qf_start->qf_next;
		vim_free(qf_start->qf_text);
		vim_free(qf_start);
		qf_start = qfp;
		--qf_count;
	}
}

/*
 * qf_mark_adjust: adjust marks
 */
   void
qf_mark_adjust(line1, line2, amount, amount_after)
	linenr_t	line1;
	linenr_t	line2;
	long		amount;
	long		amount_after;
{
	register int i;
	struct qf_line *qfp;

	if (qf_count)
		for (i = 0, qfp = qf_start; i < qf_count; ++i, qfp = qfp->qf_next)
			if (qfp->qf_fnum == curbuf->b_fnum)
			{
				if (qfp->qf_lnum >= line1 && qfp->qf_lnum <= line2)
				{
					if (amount == MAXLNUM)
						qfp->qf_cleared = TRUE;
					else
						qfp->qf_lnum += amount;
				}
				if (amount_after && qfp->qf_lnum > line2)
					qfp->qf_lnum += amount_after;
			}
}

/*
 * Make a nice message out of the error character and the error number:
 *	char	number		message
 *  e or E    0			"   error"
 *  w or W    0			" warning"
 *  other     0			""
 *  w or W    n			" warning n"
 *  other     n			"   error n"
 */
	static char_u *
qf_types(c, nr)
	int c, nr;
{
	static char_u	buf[20];
	char_u		*p1;

	p1 = (char_u *)"   error";
	if (c == 'W' || c == 'w')
		p1 =  (char_u *)" warning";
	else if (nr <= 0 && c != 'E' && c != 'e')
		p1 = (char_u *)"";

	if (nr <= 0)
		return p1;

	sprintf((char *)buf, "%s %3d", p1, nr);
	return buf;
}
