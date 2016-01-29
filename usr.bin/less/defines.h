/*
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * This file is made available under the terms of the Less License.
 */

/*
 * LESSKEYFILE is the filename of the default lesskey output file
 * (in the HOME directory).
 * LESSKEYFILE_SYS is the filename of the system-wide lesskey output file.
 * DEF_LESSKEYINFILE is the filename of the default lesskey input
 * (in the HOME directory).
 * LESSHISTFILE is the filename of the history file
 * (in the HOME directory).
 */
#define	LESSKEYFILE		".less"
#define	LESSKEYFILE_SYS		SYSDIR "/sysless"
#define	DEF_LESSKEYINFILE	".lesskey"
#define	LESSHISTFILE		"-"
#define	TGETENT_OK  1		/* "OK" from curses.h */

/*
 * Default shell metacharacters and meta-escape character.
 */
#define	DEF_METACHARS	"; *?\t\n'\"()<>[]|&^`#\\$%=~"

#define	CMDBUF_SIZE	2048	/* Buffer for multichar commands */
#define	UNGOT_SIZE	200	/* Max chars to unget() */
#define	LINEBUF_SIZE	1024	/* Initial max size of line in input file */
#define	OUTBUF_SIZE	1024	/* Output buffer */
#define	PROMPT_SIZE	2048	/* Max size of prompt string */
#define	TERMBUF_SIZE	2048	/* Termcap buffer for tgetent */
#define	TERMSBUF_SIZE	1024	/* Buffer to hold termcap strings */
#define	TAGLINE_SIZE	1024	/* Max size of line in tags file */
#define	TABSTOP_MAX	128	/* Max number of custom tab stops */
#define	EDIT_PGM	"vi"	/* Editor program */
