/*	$OpenBSD: macro.h,v 1.5 2004/02/01 22:26:41 vincent Exp $	*/

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

extern LINE	*maclhead;
extern LINE	*maclcur;
