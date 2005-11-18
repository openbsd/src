/*	$OpenBSD: macro.h,v 1.7 2005/11/18 20:56:53 deraadt Exp $	*/

/* This file is in the public domain. */

/* definitions for keyboard macros */

#define MAXMACRO 256		/* maximum functs in a macro */

extern int inmacro;
extern int macrodef;
extern int macrocount;

union macrodef {
	PF	m_funct;
	int	m_count;	/* for count-prefix	 */
};

extern union macrodef macro[MAXMACRO];

extern struct line	*maclhead;
extern struct line	*maclcur;
