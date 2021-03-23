/*      $OpenBSD: interpreter.c,v 1.13 2021/03/23 15:22:25 lum Exp $	*/
/*
 * This file is in the public domain.
 *
 * Author: Mark Lumsden <mark@showcomplex.com>
 */

/*
 * This file attempts to add some 'scripting' functionality into mg.
 *
 * The initial goal is to give mg the ability to use it's existing functions
 * and structures in a linked-up way. Hopefully resulting in user definable
 * functions. The syntax is 'scheme' like but currently it is not a scheme
 * interpreter.
 *
 * At the moment there is no manual page reference to this file. The code below
 * is liable to change, so use at your own risk!
 *
 * If you do want to do some testing, you can add some lines to your .mg file
 * like:
 * 
 * 1. Give multiple arguments to a function that usually would accept only one:
 * (find-file "a.txt" "b.txt" "c.txt")
 *
 * 2. Define a single value variable:
 * (define myfile "d.txt")
 *
 * 3. Define a list:
 * (define myfiles(list "e.txt" "f.txt"))
 *
 * 4. Use the previously defined variable or list:
 * (find-file myfiles)
 *
 * To do:
 * 1. multiline parsing - currently only single lines supported.
 * 2. parsing for '(' and ')' throughout whole string and evaluate correctly.
 * 3. conditional execution.
 * 4. deal with special characters in a string: "x\" x" etc
 * 5. do symbol names need more complex regex patterns? A-Za-z- at the moment. 
 * 6. oh so many things....
 * [...]
 * n. implement user definable functions.
 * 
 */
#include <sys/queue.h>

#include <limits.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"
#include "funmap.h"

#ifdef  MGLOG
#include "kbd.h"
#include "log.h"
#endif

static int	 multiarg(char *);
static int	 isvar(char **, char **, int);
static int	 foundvar(char *);
static int	 doregex(char *, char *);
static int	 parseexp(char *);
static void	 clearexp(void);

TAILQ_HEAD(exphead, expentry) ehead;
struct expentry {
	TAILQ_ENTRY(expentry) eentry;
	char	*exp;		/* The string found between paraenthesis. */
	int	 par1;		/* Parenthesis at start of string (=1	  */
	int	 par2;		/* Parenthesis at end of string   )=2     */
	int	 expctr;	/* An incremental counter:+1 for each exp */
	int	 blkid;		/* Which block are we in?		  */
};

/*
 * Structure for variables during buffer evaluation.
 */
struct varentry {
	SLIST_ENTRY(varentry) entry;
	char	*name;
	char	*vals;
	int	 count;
};
SLIST_HEAD(vlisthead, varentry) varhead = SLIST_HEAD_INITIALIZER(varhead);

/*
 * Structure for scheme keywords. 
 */
#define NUMSCHKEYS	4
#define MAXLENSCHKEYS	17	/* 17 = longest keyword (16)  + 1 */

char scharkey[NUMSCHKEYS][MAXLENSCHKEYS] =
	{ 
		"define",
	  	"list",
	  	"if",
	  	"lambda"
	};


/*
 * Line has a '(' as the first non-white char.
 * Do some very basic parsing of line.
 * Multi-line not supported at the moment, To do.
 */
int
foundparen(char *funstr)
{
	struct expentry *e1 = NULL, *e2 = NULL;
	char		*p, *valp, *endp = NULL, *regs;
	char		 expbuf[BUFSIZE], tmpbuf[BUFSIZE];
	int     	 ret, pctr, fndstart, expctr, blkid, fndchr, fndend;
	int		 inquote;

	pctr = fndstart = expctr = fndchr = fndend = inquote = 0;
	blkid = 1;
	/*
	 * Check for blocks of code with opening and closing ().
	 * One block = (cmd p a r a m)
	 * Two blocks = (cmd p a r a m s)(hola)
	 * Two blocks = (cmd p a r (list a m s))(hola)
	 * Only single line at moment, but more for multiline.
	 */
	p = funstr;

	/*
	 * Currently can't do () or (( at the moment,
	 * just drop out - stops a segv. TODO.
	 */
	regs = "[(]+[\t ]*[)]+";
        if (doregex(regs, funstr))
		return(dobeep_msg("Empty lists not supported at moment"));
	regs = "[(]+[\t ]*[(]+";
        if (doregex(regs, funstr))
		return(dobeep_msg("Multiple consecutive left parantheses "\
		    "found."));
	/*
	 * load expressions into a list called 'expentry', to be processd
	 * when all are obtained.
	 * Not really live code at the moment. Just part of the process of
	 * working out what needs to be done.
	 */
	TAILQ_INIT(&ehead);

	while (*p != '\0') {
		if (*p == '(') {
			if (fndstart == 1) {
				if (endp == NULL)
					*p = '\0';
				else
					*endp = '\0';
				e1->par2 = 1;
	                        if ((e1->exp = strndup(valp, BUFSIZE)) ==
				    NULL) {
					cleanup();
        	                        return(dobeep_msg("strndup error"));
				}
			}
			if ((e1 = malloc(sizeof(struct expentry))) == NULL) {
				cleanup();
                               	return (dobeep_msg("malloc Error"));
			}
                       	TAILQ_INSERT_HEAD(&ehead, e1, eentry);
			e1->exp = NULL;
                       	e1->expctr = ++expctr;
			e1->blkid = blkid;
                       	e1->par1 = 1; 
			fndstart = 1;
			fndend = 0;
			fndchr = 0;
			endp = NULL;
			pctr++;
		} else if (*p == ')') {
			if (inquote == 1) {
				cleanup();
				return(dobeep_msg("Opening and closing quote "\
				    "char error"));
			}
			if (endp == NULL)
				*p = '\0';
			else
				*endp = '\0';
			if ((e1->exp = strndup(valp, BUFSIZE)) == NULL) {
				cleanup();
				return(dobeep_msg("strndup error"));
			}
			fndstart = 0;
			pctr--;
		} else if (*p != ' ' && *p != '\t') {
			if (fndchr == 0) {
				valp = p;
				fndchr = 1;
			}
			if (*p == '"') {
				if (inquote == 0)
					inquote = 1;
				else
					inquote = 0;
			}
			fndend = 0;
			endp = NULL;
		} else if (fndend == 0 && (*p == ' ' || *p == '\t')) {
			*p = ' ';
			fndend = 1;
			endp = p;
		} else if (*p == '\t')
			if (inquote == 0)
				*p = ' ';
		if (pctr == 0)
			blkid++;
		p++;
	}

	if (pctr != 0) {
		cleanup();
		return(dobeep_msg("Opening and closing parentheses error"));
	}
	/*
	 * Join expressions together for the moment, to progess.
	 * This needs to be totally redone and
	 * iterate in-to-out, evaluating as we go. Eventually.
	 */
	expbuf[0] = tmpbuf[0] = '\0';
	TAILQ_FOREACH_SAFE(e1, &ehead, eentry, e2) {
		if (strlcpy(tmpbuf, expbuf, sizeof(tmpbuf)) >= sizeof(tmpbuf))
			return (dobeep_msg("strlcpy error"));
		expbuf[0] = '\0';
		if (strlcpy(expbuf, e1->exp, sizeof(expbuf)) >= sizeof(expbuf))
                	return (dobeep_msg("strlcat error"));
		if (*tmpbuf != '\0')
			if (strlcat(expbuf, " ", sizeof(expbuf)) >=
			    sizeof(expbuf))
				return (dobeep_msg("strlcat error"));
		if (strlcat(expbuf, tmpbuf, sizeof(expbuf)) >= sizeof(expbuf))
			return (dobeep_msg("strlcat error"));
#ifdef MGLOG
		mglog_misc("exp|%s|\n", e1->exp);
#endif
	}
	
	ret = parseexp(expbuf);
	if (ret == FALSE)
		cleanup();
	else
		clearexp();	/* leave lists but remove expressions */

	return (ret);
}

/*
 * At the moment, only parsing list defines. Much more to do.
 * Also only use basic chars for symbol names like ones found in
 * mg functions.
 */
static int
parseexp(char *funstr)
{
	char    *regs;

        /* Does the line have a list 'define' like: */
        /* (define alist(list 1 2 3 4)) */
        regs = "^define[ ]+[A-Za-z-]+[ ]+list[ ]+.*[ ]*";
        if (doregex(regs, funstr))
                return(foundvar(funstr));

	/* Does the line have a variable 'define' like: */
	/* (define i (function-name j)) */
	regs = "^define[ ]+[A-Za-z-]+[ ]+[A-Za-z-]+[ ]+.*$";
	if (doregex(regs, funstr))
		return(foundvar(funstr));

        /* Does the line have a incorrect variable 'define' like: */
        /* (define i y z) */
        regs = "^define[ ]+[A-Za-z-]+[ ]+.*[ ]+.*$";
        if (doregex(regs, funstr))
                return(dobeep_msg("Invalid use of define."));

        /* Does the line have a single variable 'define' like: */
        /* (define i 0) */
        regs = "^define[ ]+[A-Za-z-]+[ ]+.*$";
        if (doregex(regs, funstr))
                return(foundvar(funstr));

        /* Does the line have an unrecognised 'define' */
        regs = "^define[\t ]+";
        if (doregex(regs, funstr))
                return(dobeep_msg("Invalid use of define"));

	return(multiarg(funstr));
}

/*
 * Pass a list of arguments to a function.
 */
static int
multiarg(char *funstr)
{
	PF	 funcp;
	char	 excbuf[BUFSIZE], argbuf[BUFSIZE];
	char	 contbuf[BUFSIZE], varbuf[BUFSIZE];
	char	*cmdp = NULL, *argp, *fendp = NULL, *endp, *p, *v, *s = " ";
	char	*regs;
	int	 spc, numparams, numspc;
	int	 inlist, sizof, fin, inquote;

	/* mg function name regex */	
        if (doregex("^[A-Za-z-]+$", funstr))
		return(excline(funstr));

	cmdp = funstr;
	fendp = strchr(cmdp, ' ');
	*fendp = '\0';
	/*
	 * If no extant mg command found, just return.
	 */
	if ((funcp = name_function(cmdp)) == NULL)
		return (dobeep_msgs("Unknown command: ", cmdp));

	numparams = numparams_function(funcp);
	if (numparams == 0)
		return (dobeep_msgs("Command takes no arguments: ", cmdp));

	/* now find the first argument */
	p = fendp + 1;
	p = skipwhite(p);

	if (strlcpy(argbuf, p, sizeof(argbuf)) >= sizeof(argbuf))
		return (dobeep_msg("strlcpy error"));
	argp = argbuf;
	numspc = spc = 1; /* initially fake a space so we find first argument */
	inlist = fin = inquote = 0;

	for (p = argbuf; *p != '\0'; p++) {
		if (*(p + 1) == '\0')
			fin = 1;

		if (*p != ' ') {
			if (*p == '"') {
				if (inquote == 1)
					inquote = 0;	
				else
					inquote = 1;
			}
			if (spc == 1)
				argp = p;
			spc = 0;
		}
		if ((*p == ' ' && inquote == 0) || fin) {
			if (spc == 1)
				continue;

			if (*p == ' ') {
				*p = '\0';		/* terminate arg string */
			}
			endp = p + 1;
			excbuf[0] = '\0';
			varbuf[0] = '\0';
			contbuf[0] = '\0';			
			sizof = sizeof(varbuf);
			v = varbuf;
			regs = "[\"]+.*[\"]+";
       			if (doregex(regs, argp))
				;			/* found quotes */
			else if (isvar(&argp, &v, sizof)) {
				(void)(strlcat(varbuf, " ",
                                    sizof) >= sizof);

				*p = ' ';

				(void)(strlcpy(contbuf, endp,
				    sizeof(contbuf)) >= sizeof(contbuf));

				(void)(strlcat(varbuf, contbuf,
				    sizof) >= sizof);
				
				argbuf[0] = ' ';
				argbuf[1] = '\0';
				(void)(strlcat(argbuf, varbuf,
				    sizof) >= sizof);

				p = argp = argbuf;
				spc = 1;
				fin = 0;
				continue;
			} else {
				const char *errstr;
				int iters;

				iters = strtonum(argp, 0, INT_MAX, &errstr);
				if (errstr != NULL)
					return (dobeep_msgs("Var not found:",
					    argp));
			}

			if (strlcpy(excbuf, cmdp, sizeof(excbuf))
			    >= sizeof(excbuf))
				return (dobeep_msg("strlcpy error"));
			if (strlcat(excbuf, s, sizeof(excbuf))
			    >= sizeof(excbuf))
				return (dobeep_msg("strlcat error"));
			if (strlcat(excbuf, argp, sizeof(excbuf))
			    >= sizeof(excbuf))
				return (dobeep_msg("strlcat error"));

			excline(excbuf);

			if (fin)
				break;

			*p = ' ';		/* unterminate arg string */
			spc = 1;
		}
	}
	return (TRUE);
}

/*
 * Is an item a value or a variable?
 */
static int
isvar(char **argp, char **varbuf, int sizof)
{
	struct varentry *v1 = NULL;

	if (SLIST_EMPTY(&varhead))
		return (FALSE);
#ifdef  MGLOG
	mglog_isvar(*varbuf, *argp, sizof);
#endif
	SLIST_FOREACH(v1, &varhead, entry) {
		if (strcmp(*argp, v1->name) == 0) {
			(void)(strlcpy(*varbuf, v1->vals, sizof) >= sizof);
			return (TRUE);
		}
	}
	return (FALSE);
}

/*
 * The define string _must_ adhere to the regex in parsexp().
 * This is not the correct way to do parsing but it does highlight
 * the issues.
 */
static int
foundvar(char *defstr)
{
	struct varentry *vt, *v1 = NULL;
	const char	 t[2] = "t";
	char		*p, *vnamep, *vendp = NULL, *valp;
	int		 spc;

	/* vars names can't start with these. */
	/* char *spchrs = "+-.#";	*/

	p = strstr(defstr, " ");        /* move to first ' ' char.    */
	vnamep = skipwhite(p);		/* find first char of var name. */
	vendp = vnamep;

	/* now find the end of the list name */
	while (1) {
		++vendp;
		if (*vendp == ' ')
			break;
	}
	*vendp = '\0';

	/*
	 * Check list name is not an existing function.
	 * Although could this be allowed? Shouldn't context dictate?
	 */
	if (name_function(vnamep) != NULL)
		return(dobeep_msgs("Variable/function name clash:", vnamep));

	p = ++vendp;
	p = skipwhite(p);

	if ((*p == 'l') && (*(p + 1) == 'i') && (*(p + 2) == 's')) {
		p = strstr(p, t);	/* find 't' in 'list'.	*/
		valp = skipwhite(++p);	/* find first value	*/
	} else
		valp = p;
	/*
	 * Now we have the name of the list starting at 'vnamep',
	 * and the first value is at 'valp', record the details
	 * in a linked list. But first remove variable, if existing already.
	 */
	if (!SLIST_EMPTY(&varhead)) {
		SLIST_FOREACH_SAFE(v1, &varhead, entry, vt) {
			if (strcmp(vnamep, v1->name) == 0)
				SLIST_REMOVE(&varhead, v1, varentry, entry);
		}
	}
	if ((v1 = malloc(sizeof(struct varentry))) == NULL)
		return (ABORT);
	SLIST_INSERT_HEAD(&varhead, v1, entry);
	if ((v1->name = strndup(vnamep, BUFSIZE)) == NULL)
		return(dobeep_msg("strndup error"));
	v1->count = 0;
	vendp = NULL;
	
	/* initially fake a space so we find first value */
	spc = 1;
	/* now loop through values in list value string while counting them */
	for (p = valp; *p != '\0'; p++) {
		if (*p != ' ' && *p != '\t') {
			if (spc == 1)
				v1->count++;
			spc = 0;
		}
	}
	if ((v1->vals = strndup(valp, BUFSIZE)) == NULL)
		return(dobeep_msg("strndup error"));

#ifdef  MGLOG
        mglog_misc("var:%s\t#items:%d\tvals:|%s|\n", vnamep, v1->count, v1->vals);
#endif

	return (TRUE);
}

/*
 * Finished with buffer evaluation, so clean up any vars.
 * Perhaps keeps them in mg even after use,...
 */
static int
clearvars(void)
{
	struct varentry	*v1 = NULL;

	while (!SLIST_EMPTY(&varhead)) {
		v1 = SLIST_FIRST(&varhead);
		SLIST_REMOVE_HEAD(&varhead, entry);
		free(v1->vals);
		free(v1->name);
		free(v1);
	}
	return (FALSE);
}

/*
 * Finished with block evaluation, so clean up any expressions.
 */
static void
clearexp(void)
{
	struct expentry	*e1 = NULL;

	while (!TAILQ_EMPTY(&ehead)) {
		e1 = TAILQ_FIRST(&ehead);
		TAILQ_REMOVE(&ehead, e1, eentry);
		free(e1->exp);
		free(e1);
	}
	return;
}

/*
 * Cleanup before leaving.
 */
void
cleanup(void)
{
	clearexp();
	clearvars();
}

/*
 * Test a string against a regular expression.
 */
static int
doregex(char *r, char *e)
{
	regex_t  regex_buff;

	if (regcomp(&regex_buff, r, REG_EXTENDED)) {
		regfree(&regex_buff);
		return(dobeep_msg("Regex compilation error"));
	}
	if (!regexec(&regex_buff, e, 0, NULL, 0)) {
		regfree(&regex_buff);
		return(TRUE);
	}
	regfree(&regex_buff);
	return(FALSE);
}
