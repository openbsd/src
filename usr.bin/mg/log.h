/*      $OpenBSD: log.h,v 1.5 2019/07/18 10:50:24 lum Exp $   */

/* This file is in the public domain. */

/*
 * Specifically for mg logging functionality.
 *
 */
int	 mglog(PF, KEYMAP *);
int	 mgloginit(void);
int	 mglog_execbuf(	const char* const,
			const char* const,
			const char* const,
			const char* const,
	     		const int,
			const int,
			const char* const,
			const char* const,
			const char* const
			);

int	 mglog_isvar(	const char* const,
			const char* const,
			const int
			);

char 			*mglogpath_lines;
char 			*mglogpath_undo;
char 			*mglogpath_window;
char 			*mglogpath_key;
char			*mglogpath_interpreter;
