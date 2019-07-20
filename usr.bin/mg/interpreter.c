/*      $OpenBSD: interpreter.c,v 1.5 2019/07/20 11:06:33 lum Exp $	*/
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
 * (find-file a.txt b.txt. c.txt)
 *
 * 2. Define a list:
 * (define myfiles(list d.txt e.txt))
 *
 * 3. Use the previously defined list:
 * (find-file myfiles)
 *
 * To do:
 * 1. multiline parsing - currently only single lines supported.
 * 2. parsing for '(' and ')' throughout whole string and evaluate correctly.
 * 3. conditional execution.
 * 4. define single value variables (define i 0)
 * 5. deal with quotes around a string: "x x"
 * 6. oh so many things....
 * [...]
 * n. implement user definable functions.
 */
#include <sys/queue.h>
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
static int	 foundlist(char *);


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
 * Pass a list of arguments to a function.
 */
static int
multiarg(char *funstr)
{
	regex_t  regex_buff;
	PF	 funcp;
	char	 excbuf[BUFSIZE], argbuf[BUFSIZE], *contbuf, tmpbuf[BUFSIZE];
	char	*cmdp, *argp, *fendp, *endp, *p, *t, *s = " ";
	int	 singlecmd = 0, spc, numparams, numspc;
	int	 inlist, foundlst = 0, eolst, rpar, sizof, fin;
	
	contbuf = NULL;
	endp = strrchr(funstr, ')');
	if (endp == NULL) {
		ewprintf("No closing parenthesis found");
		return(FALSE);
	}
	p = endp + 1;
	if (*p != '\0')
		*p = '\0';
	/* we now know that string starts with '(' and ends with ')' */
	if (regcomp(&regex_buff, "^[(][\t ]*[)]$", REG_EXTENDED)) {
		regfree(&regex_buff);
		return (dobeep_msg("Could not compile regex"));
	}
	if (!regexec(&regex_buff, funstr, 0, NULL, 0)) {
		regfree(&regex_buff);
		return (dobeep_msg("No command found"));
	}
	/* currently there are no mg commands that don't have a letter */
	if (regcomp(&regex_buff, "^[(][\t ]*[A-Za-z-]+[\t ]*[)]$",
	    REG_EXTENDED)) {
		regfree(&regex_buff);
		return (dobeep_msg("Could not compile regex"));
	}
	if (!regexec(&regex_buff, funstr, 0, NULL, 0))
		singlecmd = 1;

	regfree(&regex_buff);
	p = funstr + 1;		/* move past first '(' char.	*/
	cmdp = skipwhite(p);	/* find first char of command.	*/

	if (singlecmd) {
		/* remove ')', then check for spaces at the end */
		cmdp[strlen(cmdp) - 1] = '\0'; 
		if ((fendp = strchr(cmdp, ' ')) != NULL)
			*fendp = '\0';
		else if ((fendp = strchr(cmdp, '\t')) != NULL)
			*fendp = '\0';
		return(excline(cmdp));
	}
	if ((fendp = strchr(cmdp, ' ')) == NULL) 
		fendp = strchr(cmdp, '\t');

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
	inlist = eolst = fin = rpar = 0;

	for (p = argp; fin == 0; p++) {
#ifdef  MGLOG
		mglog_execbuf("", excbuf, argbuf, argp, eolst, inlist, cmdp,
		    p, contbuf);
#endif
		if (foundlst) {
			foundlst = 0;
			p--;	/* otherwise 1st arg is missed from list. */
		}
		if (*p == ')') {
			rpar = 1;
			*p = '\0';
		}
		if (*p == ' ' || *p == '\t' || *p == '\0') {
			if (spc == 1)
				continue;
			if (spc == 0 && (numspc % numparams == 0)) {
				if (*p == '\0')
					eolst = 1;
				else
					eolst = 0;
				*p = '\0'; 	/* terminate arg string */
				endp = p + 1;			
				excbuf[0] = '\0';
				/* Is arg a var? */
				if (!inlist) {
					sizof = sizeof(tmpbuf);
					t = tmpbuf;
					if (isvar(&argp, &t, sizof)) {
						if ((contbuf = strndup(endp,
						    BUFSIZE)) == NULL)
							return(FALSE);
						*p = ' ';
						(void)(strlcpy(argbuf, tmpbuf,
						    sizof) >= sizof);
						p = argp = argbuf;
						spc = 1;
						foundlst = inlist = 1;
						continue;
					}
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
#ifdef  MGLOG
				mglog_execbuf("  ", excbuf, argbuf, argp,
				    eolst, inlist, cmdp, p, contbuf);
#endif
				*p = ' ';	/* so 'for' loop can continue */
				if (eolst) {
					if (contbuf != NULL) {
						(void)strlcpy(argbuf, contbuf,
						    sizeof(argbuf));
						free(contbuf);
						contbuf = NULL;
						p = argp = argbuf;
						foundlst = 1;
						inlist = 0;
						if (rpar)
							fin = 1;
						continue;
					}
					spc = 1;
					inlist = 0;
				}
				if (eolst && rpar)
					fin = 1;
			}
			numspc++;
			spc = 1;
		} else {
			if (spc == 1)
				if ((numparams == 1) ||
				    ((numspc + 1) % numparams) == 0)
					argp = p;
			spc = 0;
		}
	}
	return (TRUE);
}


/*
 * Is an item a value or a variable?
 */
static int
isvar(char **argp, char **tmpbuf, int sizof)
{
	struct varentry *v1 = NULL;

	if (SLIST_EMPTY(&varhead))
		return (FALSE);
#ifdef  MGLOG
	mglog_isvar(*tmpbuf, *argp, sizof);
#endif
	SLIST_FOREACH(v1, &varhead, entry) {
		if (strcmp(*argp, v1->name) == 0) {
			(void)(strlcpy(*tmpbuf, v1->vals, sizof) >= sizof);
			return (TRUE);
		}
	}
	return (FALSE);
}


/*
 * The (define string _must_ adhere to the regex in foundparen.
 * This is not the correct way to do parsing but it does highlight
 * the issues.
 */
static int
foundlist(char *defstr)
{
	struct varentry *vt, *v1 = NULL;
	const char	 e[1] = "e", t[1] = "t";
	char		*p, *vnamep, *vendp = NULL, *valp, *o;
	int		 spc;


	p = defstr + 1;         /* move past first '(' char.    */
	p = skipwhite(p);    	/* find first char of 'define'. */
	p = strstr(p, e);	/* find first 'e' in 'define'.	*/
	p = strstr(++p, e);	/* find second 'e' in 'define'.	*/
	p++;			/* move past second 'e'.	*/
	vnamep = skipwhite(p);  /* find first char of var name. */
	vendp = vnamep;

	/* now find the end of the list name */
	while (1) {
		++vendp;
		if (*vendp == '(' || *vendp == ' ' || *vendp == '\t')
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
	p = strstr(p, t);	/* find 't' in 'list'.	*/
	valp = skipwhite(++p);	/* find first value	*/
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
		if (*p == ' ' || *p == '\t') {
			if (spc == 0)
				vendp = p;
			spc = 1;
		} else if (*p == ')') {
			o = p - 1;
			if (*o != ' ' && *o != '\t')
				vendp = p;
			break;
		} else {
			if (spc == 1)
				v1->count++;
			spc = 0;
		}
	}
	*vendp = '\0';
	if ((v1->vals = strndup(valp, BUFSIZE)) == NULL)
		return(dobeep_msg("strndup error"));

	return (TRUE);
}


/*
 * to do
 */
static int
foundvar(char *funstr)
{
	ewprintf("to do");
	return (TRUE);
}

/*
 * Finished with evaluation, so clean up any vars.
 */
int
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
 * Line has a '(' as the first non-white char.
 * Do some very basic parsing of line with '(' as the first character.
 * Multi-line not supported at the moment, To do.
 */
int
foundparen(char *funstr)
{
	regex_t  regex_buff;
	char	*regs;

	/* Does the line have a list 'define' like: */
	/* (define alist(list 1 2 3 4)) */
	regs = "^[(][\t ]*define[\t ]+[^\t (]+[\t ]*[(][\t ]*list[\t ]+"\
		"[^\t ]+.*[)][\t ]*[)]";
	if (regcomp(&regex_buff, regs, REG_EXTENDED)) {
		regfree(&regex_buff);
		return(dobeep_msg("Could not compile regex"));
	}
	if (!regexec(&regex_buff, funstr, 0, NULL, 0)) {
		regfree(&regex_buff);
		return(foundlist(funstr));
	}
	/* Does the line have a single variable 'define' like: */
	/* (define i 0) */
	regs = "^[(][\t ]*define[\t ]+[^\t (]+[\t ]*[^\t (]+[\t ]*[)]";
	if (regcomp(&regex_buff, regs, REG_EXTENDED)) {
		regfree(&regex_buff);
		return(dobeep_msg("Could not compile regex"));
	}
	if (!regexec(&regex_buff, funstr, 0, NULL, 0)) {
		regfree(&regex_buff);
		return(foundvar(funstr));
	}
	/* Does the line have an unrecognised 'define' */
	regs = "^[(][\t ]*define[\t ]+";
	if (regcomp(&regex_buff, regs, REG_EXTENDED)) {
		regfree(&regex_buff);
		return(dobeep_msg("Could not compile regex"));
	}
	if (!regexec(&regex_buff, funstr, 0, NULL, 0)) {
		regfree(&regex_buff);
		return(dobeep_msg("Invalid use of define"));
	}
	regfree(&regex_buff);
	return(multiarg(funstr));
}
