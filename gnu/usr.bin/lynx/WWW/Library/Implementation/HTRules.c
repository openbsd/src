/*	Configuration manager for Hypertext Daemon		HTRules.c
**	==========================================
**
**
** History:
**	 3 Jun 91	Written TBL
**	10 Aug 91	Authorisation added after Daniel Martin (pass, fail)
**			Rule order in file changed
**			Comments allowed with # on 1st char of rule line
**	17 Jun 92	Bug fix: pass and fail failed if didn't contain '*' TBL
**	 1 Sep 93	Bug fix: no memory check - Nathan Torkington
**			BYTE_ADDRESSING removed - Arthur Secret
**	11 Sep 93  MD	Changed %i into %d in debug printf.
**			VMS does not recognize %i.
**			Bug Fix: in case of PASS, only one parameter to printf.
**	19 Sep 93  AL	Added Access Authorization stuff.
**	 1 Nov 93  AL	Added htbin.
**	25 May 99  KW	Added redirect for lynx.
**
*/

#include <HTUtils.h>

/* (c) CERN WorldWideWeb project 1990,91. See Copyright.html for details */
#include <HTRules.h>

#include <HTFile.h>
#include <LYLeaks.h>
#include <HTAAProt.h>

#define LINE_LENGTH 256


typedef struct _rule {
	struct _rule *	next;
	HTRuleOp	op;
	char *		pattern;
	char *		equiv;
	char *		condition_op; /* as strings - may be inefficient, */
	char *		condition;    /* but this is not for a server - kw */
} rule;

#ifndef NO_RULES

#include <HTTP.h> /* for redirecting_url, indirectly HTPermitRedir - kw */
#include <LYGlobalDefs.h> /* for LYUserSpecifiedURL - kw */
#include <LYUtils.h>		/* for LYFixCursesOn - kw */
#include <HTAlert.h>

/*	Global variables
**	----------------
*/
PUBLIC char *HTBinDir = NULL;	/* Physical /htbin directory path.	*/
				/* In future this should not be global. */
PUBLIC char *HTSearchScript = NULL;	/* Search script name.		*/


/*	Module-wide variables
**	---------------------
*/

PRIVATE rule * rules = 0;	/* Pointer to first on list */
#ifndef PUT_ON_HEAD
PRIVATE rule * rule_tail = 0;	/* Pointer to last on list */
#endif


/*	Add rule to the list					HTAddRule()
**	--------------------
**
**  On entry,
**	pattern		points to 0-terminated string containing a single "*"
**	equiv		points to the equivalent string with * for the
**			place where the text matched by * goes.
**  On exit,
**	returns		0 if success, -1 if error.
*/

PUBLIC int HTAddRule ARGS5(
    HTRuleOp,		op,
    CONST char *,	pattern,
    CONST char *,	equiv,
    CONST char *,	cond_op,
    CONST char *,	cond)
{ /* BYTE_ADDRESSING removed and memory check - AS - 1 Sep 93 */
    rule *	temp;
    char *	pPattern = NULL;

    temp = typecalloc(rule);
    if (temp==NULL)
	outofmem(__FILE__, "HTAddRule");
    if (equiv) {		/* Two operands */
	char *	pEquiv = NULL;
	StrAllocCopy(pEquiv, equiv);
	temp->equiv = pEquiv;
    } else {
	temp->equiv = 0;
    }
    if (cond_op) {
	StrAllocCopy(temp->condition_op, cond_op);
	StrAllocCopy(temp->condition, cond);
    }
    StrAllocCopy(pPattern, pattern);
    temp->pattern = pPattern;
    temp->op = op;

    if (equiv) {
	CTRACE((tfp, "Rule: For `%s' op %d `%s'", pattern, op, equiv));
    } else {
	CTRACE((tfp, "Rule: For `%s' op %d", pattern, op));
    }
    if (cond_op) {
	CTRACE((tfp, "\t%s %s\n", cond_op, NONNULL(cond)));
    } else {
	CTRACE((tfp, "\n"));
    }

    if (!rules) {
#ifdef LY_FIND_LEAKS
	atexit(HTClearRules);
#endif
    }
#ifdef PUT_ON_HEAD
    temp->next = rules;
    rules = temp;
#else
    temp->next = 0;
    if (rule_tail) rule_tail->next = temp;
    else rules = temp;
    rule_tail = temp;
#endif


    return 0;
}


/*	Clear all rules						HTClearRules()
**	---------------
**
** On exit,
**	There are no rules
**
** See also
**	HTAddRule()
*/
void HTClearRules NOARGS
{
    while (rules) {
	rule * temp = rules;
	rules = temp->next;
	FREE(temp->pattern);
	FREE(temp->equiv);
	FREE(temp->condition_op);
	FREE(temp->condition);
	FREE(temp);
    }
#ifndef PUT_ON_HEAD
    rule_tail = 0;
#endif
}

PRIVATE BOOL rule_cond_ok ARGS1(
    rule *,	 r)
{
    BOOL result;
    if (!r->condition_op)
	return YES;
    if (strcmp(r->condition_op, "if") && strcmp(r->condition_op, "unless")) {
	CTRACE((tfp, "....... rule ignored, unrecognized `%s'!\n",
	       r->condition_op));
	return NO;
    }
    if (!strcmp(r->condition, "redirected"))
	result = (BOOL) (redirection_attempts > 0);
    else if (!strcmp(r->condition, "userspec"))
	result = LYUserSpecifiedURL;
    else {
	CTRACE((tfp, "....... rule ignored, unrecognized `%s %s'!\n",
	       r->condition_op, NONNULL(r->condition)));
	return NO;
    }
    if (!strcmp(r->condition_op, "if"))
	return result;
    else
	return (BOOL) (!result);

}
/*	Translate by rules					HTTranslate()
**	------------------
**
**	The most recently defined rules are applied first.
**
** On entry,
**	required	points to a string whose equivalent value is neeed
** On exit,
**	returns		the address of the equivalent string allocated from
**			the heap which the CALLER MUST FREE. If no translation
**			occured, then it is a copy of te original.
** NEW FEATURES:
**			When a "protect" or "defprot" rule is mathed,
**			a call to HTAA_setCurrentProtection() or
**			HTAA_setDefaultProtection() is made to notify
**			the Access Authorization module that the file is
**			protected, and so it knows how to handle it.
**								-- AL
*/
char * HTTranslate ARGS1(
    CONST char *,	required)
{
    rule * r;
    char *current = NULL;
    char *msgtmp = NULL, *pMsg;
    int proxy_none_flag = 0;
    int permitredir_flag = 0;
    StrAllocCopy(current, required);

    HTAA_clearProtections();	/* Reset from previous call -- AL */

    for(r = rules; r; r = r->next) {
	char * p = r->pattern;
	int m=0;   /* Number of characters matched against wildcard */
	CONST char * q = current;
	for(;*p && *q; p++, q++) {   /* Find first mismatch */
	    if (*p!=*q) break;
	}

	if (*p == '*') {		/* Match up to wildcard */
	    m = strlen(q) - strlen(p+1); /* Amount to match to wildcard */
	    if(m<0) continue;		/* tail is too short to match */
	    if (0!=strcmp(q+m, p+1)) continue;	/* Tail mismatch */
	} else				/* Not wildcard */
	    if (*p != *q) continue;	/* plain mismatch: go to next rule */

	if (!rule_cond_ok(r))	/* check condition, next rule if false - kw */
	    continue;

	switch (r->op) {		/* Perform operation */

#ifdef ACCESS_AUTH
	case HT_DefProt:
	case HT_Protect:
	    {
		char *local_copy = NULL;
		char *p2;
		char *eff_ids = NULL;
		char *prot_file = NULL;

		CTRACE((tfp, "HTRule: `%s' matched %s %s: `%s'\n",
			    current,
			    (r->op==HT_Protect ? "Protect" : "DefProt"),
			    "rule, setup",
			    (r->equiv ? r->equiv :
			     (r->op==HT_Protect ?"DEFAULT" :"NULL!!"))));

		if (r->equiv) {
		    StrAllocCopy(local_copy, r->equiv);
		    p2 = local_copy;
		    prot_file = HTNextField(&p2);
		    eff_ids = HTNextField(&p2);
		}

		if (r->op == HT_Protect)
		    HTAA_setCurrentProtection(current, prot_file, eff_ids);
		else
		    HTAA_setDefaultProtection(current, prot_file, eff_ids);

		FREE(local_copy);

		/* continue translating rules */
	    }
	    break;
#endif /* ACCESS_AUTH */

	case HT_UserMsg:		/* Produce message immediately */
	    LYFixCursesOn("show rule message:");
	    HTUserMsg2((r->equiv ? r->equiv : "Rule: %s"), current);
	    break;
	case HT_InfoMsg:		/* Produce messages immediately */
	case HT_Progress:
	case HT_Alert:
	    LYFixCursesOn("show rule message:"); /* and fall through */
	case HT_AlwaysAlert:
	    pMsg = r->equiv ? r->equiv :
		(r->op==HT_AlwaysAlert) ? "%s" : "Rule: %s";
	    if (strchr(pMsg, '%')) {
		HTSprintf0(&msgtmp, pMsg, current);
		pMsg = msgtmp;
	    }
	    switch (r->op) {		/* Actually produce message */
	    case HT_InfoMsg:	HTInfoMsg(pMsg);	break;
	    case HT_Progress:	HTProgress(pMsg);	break;
	    case HT_Alert:	HTAlert(pMsg);		break;
	    case HT_AlwaysAlert: HTAlwaysAlert("Rule alert:", pMsg);	break;
	    default: break;
	    }
	    FREE(msgtmp);
	    break;

	case HT_PermitRedir:			/* Set special flag */
	    permitredir_flag = 1;
	    CTRACE((tfp, "HTRule: Mark for redirection permitted\n"));
	    break;

	case HT_Pass:				/* Authorised */
	    if (!r->equiv) {
		if (proxy_none_flag) {
		    char * temp = NULL;
		    StrAllocCopy(temp, "NoProxy=");
		    StrAllocCat(temp, current);
		    FREE(current);
		    current = temp;
		}
		CTRACE((tfp, "HTRule: Pass `%s'\n", current));
		return current;
	    }
	    /* Else fall through ...to map and pass */

	case HT_Map:
	case HT_Redirect:
	case HT_RedirectPerm:
	    if (*p == *q) { /* End of both strings, no wildcard */
		  CTRACE((tfp, "For `%s' using `%s'\n", current, r->equiv));
		  StrAllocCopy(current, r->equiv); /* use entire translation */
	    } else {
		  char * ins = strchr(r->equiv, '*');	/* Insertion point */
		  if (ins) {	/* Consistent rule!!! */
			char * temp = NULL;

			HTSprintf0(&temp, "%.*s%.*s%s",
				   ins - r->equiv,
				   r->equiv,
				   m,
				   q,
				   ins + 1);
			CTRACE((tfp, "For `%s' using `%s'\n",
				    current, temp));
			FREE(current);
			current = temp;			/* Use this */

		    } else {	/* No insertion point */
			char * temp = NULL;

			StrAllocCopy(temp, r->equiv);
			CTRACE((tfp, "For `%s' using `%s'\n",
						current, temp));
			FREE(current);
			current = temp;			/* Use this */
		    } /* If no insertion point exists */
		}
		if (r->op == HT_Pass) {
		    if (proxy_none_flag) {
			char * temp = NULL;
			StrAllocCopy(temp, "NoProxy=");
			StrAllocCat(temp, current);
			FREE(current);
			current = temp;
		    }
		    CTRACE((tfp, "HTRule: ...and pass `%s'\n",
				current));
		    return current;
		} else if (r->op == HT_Redirect) {
		    CTRACE((tfp, "HTRule: ...and redirect to `%s'\n",
				current));
		    redirecting_url = current;
		    HTPermitRedir = (BOOL) (permitredir_flag == 1);
		    return (char *)0;
		} else if (r->op == HT_RedirectPerm) {
		    CTRACE((tfp, "HTRule: ...and redirect like 301 to `%s'\n",
				current));
		    redirecting_url = current;
		    permanent_redirection = TRUE;
		    HTPermitRedir = (BOOL) (permitredir_flag == 1);
		    return (char *)0;
		}
		break;

	case HT_UseProxy:
		if (r->equiv && 0==strcasecomp(r->equiv, "none")) {
		    CTRACE((tfp, "For `%s' will not use proxy\n", current));
		    proxy_none_flag = 1;
		} else if (proxy_none_flag) {
		    CTRACE((tfp, "For `%s' proxy server ignored: %s\n",
			   current,
			   NONNULL(r->equiv)));
		} else {
		    char * temp = NULL;
		    StrAllocCopy(temp, "Proxied=");
		    StrAllocCat(temp, r->equiv);
		    StrAllocCat(temp, current);
		    CTRACE((tfp, "HTRule: proxy server found: %s\n",
			   NONNULL(r->equiv)));
		    FREE(current);
		    return temp;
		}
		break;

	case HT_Invalid:
	case HT_Fail:				/* Unauthorised */
		CTRACE((tfp, "HTRule: *** FAIL `%s'\n",
			    current));
		FREE(current);
		return (char *)0;
	} /* if tail matches ... switch operation */

    } /* loop over rules */

    if (proxy_none_flag) {
	char * temp = NULL;
	StrAllocCopy(temp, "NoProxy=");
	StrAllocCat(temp, current);
	return temp;
    }

    return current;
}

/*	Load one line of configuration
**	------------------------------
**
**	Call this, for example, to load a X resource with config info.
**
** returns	0 OK, < 0 syntax error.
*/
PUBLIC int  HTSetConfiguration ARGS1(
    char *,		config)
{
    HTRuleOp op;
    char * line = NULL;
    char * pointer = line;
    char *word1, *word2, *word3;
    char *cond_op=NULL, *cond=NULL;
    float quality, secs, secs_per_byte;
    int maxbytes;
    int status;

    StrAllocCopy(line, config);
    {
	char * p = strchr(line, '#');	/* Chop off comments */
	if (p) *p = 0;
    }
    pointer = line;
    word1 = HTNextField(&pointer);
    if (!word1) {
	FREE(line);
	return 0;
    } ; /* Comment only or blank */

    word2 = HTNextField(&pointer);

    if (0==strcasecomp(word1, "defprot") ||
	0==strcasecomp(word1, "protect"))
	word3 = pointer;  /* The rest of the line to be parsed by AA module */
    else
	word3 = HTNextField(&pointer);	/* Just the next word */

    if (!word2) {
	fprintf(stderr, "HTRule: %s %s\n", RULE_NEEDS_DATA, line);
	FREE(line);
	return -2;	/*syntax error */
    }

    if (0==strcasecomp(word1, "suffix")) {
	char * encoding = HTNextField(&pointer);
	if (pointer) status = sscanf(pointer, "%f", &quality);
	else status = 0;
	HTSetSuffix(word2,	word3,
				encoding ? encoding : "binary",
				status >= 1? quality : (float) 1.0);

    } else if (0==strcasecomp(word1, "presentation")) {
	if (pointer) status = sscanf(pointer, "%f%f%f%d",
			    &quality, &secs, &secs_per_byte, &maxbytes);
	else status = 0;
	HTSetPresentation(word2, word3,
		    status >= 1 ? quality		: 1.0,
		    status >= 2 ? secs			: 0.0,
		    status >= 3 ? secs_per_byte		: 0.0,
		    status >= 4 ? maxbytes		: 0 );

    } else if (0==strncasecomp(word1, "htbin", 5) ||
	       0==strncasecomp(word1, "bindir", 6)) {
	StrAllocCopy(HTBinDir, word2);	/* Physical /htbin location */

    } else if (0==strncasecomp(word1, "search", 6)) {
	StrAllocCopy(HTSearchScript, word2);	/* Search script name */

    } else {
	op =	0==strcasecomp(word1, "map")  ? HT_Map
	    :	0==strcasecomp(word1, "pass") ? HT_Pass
	    :	0==strcasecomp(word1, "fail") ? HT_Fail
	    :	0==strcasecomp(word1, "redirect") ? HT_Redirect
	    :	0==strncasecomp(word1, "redirectperm", 12) ? HT_RedirectPerm
	    :	0==strcasecomp(word1, "redirecttemp") ? HT_Redirect
	    :	0==strcasecomp(word1, "permitredirection") ? HT_PermitRedir
	    :	0==strcasecomp(word1, "useproxy") ? HT_UseProxy
	    :	0==strcasecomp(word1, "alert") ? HT_Alert
	    :	0==strcasecomp(word1, "alwaysalert") ? HT_AlwaysAlert
	    :	0==strcasecomp(word1, "progress") ? HT_Progress
	    :	0==strcasecomp(word1, "usermsg") ? HT_UserMsg
	    :	0==strcasecomp(word1, "infomsg") ? HT_InfoMsg
	    :	0==strcasecomp(word1, "defprot") ? HT_DefProt
	    :	0==strcasecomp(word1, "protect") ? HT_Protect
	    :						HT_Invalid;
	if (op==HT_Invalid) {
	    fprintf(stderr, "HTRule: %s '%s'\n", RULE_INCORRECT, config);
	} else {
	    switch (op) {
	    case HT_Fail:	/* never a or other 2nd parameter */
	    case HT_PermitRedir:
		cond_op = word3;
		if (cond_op && *cond_op) {
		    word3 = NULL;
		    cond = HTNextField(&pointer);
		}
		break;

	    case HT_Pass:	/* possibly a URL2 */
		if (word3 && (!strcasecomp(word3, "if") ||
			      !strcasecomp(word3, "unless"))) {
		    cond_op = word3;
		    word3 = NULL;
		    cond = HTNextField(&pointer);
		    break;
		} /* else fall through */

	    case HT_Map:	/* always a URL2 (or other 2nd parameter) */
	    case HT_Redirect:
	    case HT_RedirectPerm:
	    case HT_UseProxy:
		cond_op = HTNextField(&pointer);
		/* check for extra status word in "Redirect" */
		if (op==HT_Redirect && 0==strcasecomp(word1, "redirect") &&
		    cond_op &&
		    strcasecomp(cond_op, "if") &&
		    strcasecomp(cond_op, "unless")) {
		    if (0==strcmp(word2, "301") ||
			0==strcasecomp(word2, "permanent")) {
			op = HT_RedirectPerm;
		    } else if (!(0==strcmp(word2, "302") ||
				 0==strcmp(word2, "303") ||
				 0==strcasecomp(word2, "temp") ||
				 0==strcasecomp(word2, "seeother"))) {
			CTRACE((tfp, "Rule: Ignoring `%s' in Redirect\n", word2));
		    }
		    word2 = word3;
		    word3 = cond_op; /* cond_op isn't condition op after all */
		    cond_op = HTNextField(&pointer);
		}
		if (cond_op && *cond_op)
		    cond = HTNextField(&pointer);
		break;

	    case HT_Progress:
	    case HT_InfoMsg:
	    case HT_UserMsg:
	    case HT_Alert:
	    case HT_AlwaysAlert:
		cond_op = HTNextField(&pointer);
		if (cond_op && *cond_op)
		    cond = HTNextField(&pointer);
		if (word3) {	/* Fix string with too may %s - kw */
		    char *cp = word3, *cp1, *cp2;
		    while ((cp1=strchr(cp, '%'))) {
			if (cp1[1] == '\0') {
			    *cp1 = '\0';
			    break;
			} else if (cp1[1] == '%') {
			    cp = cp1 + 2;
			    continue;
			} else while ((cp2=strchr(cp1+2, '%'))) {
			    if (cp2[1] == '\0') {
				*cp2 = '\0';
				break;
			    } else if (cp2[1] == '%') {
				cp1 = cp2;
			    } else {
				*cp2 = '?'; /* replace bad % */
				cp1 = cp2;
			    }
			}
			break;
		    }
		}
		break;

		default:
		break;
	    }
	    if (cond_op && cond && *cond && !strcasecomp(cond_op, "unless")) {
		cond_op = "unless";
	    } else if (cond_op && cond && *cond &&
		       !strcasecomp(cond_op, "if")) {
		cond_op = "if";
	    } else if (cond_op || cond) {
		fprintf(stderr, "HTRule: %s '%s'\n", RULE_INCORRECT, config);
		FREE(line);	/* syntax error, condition is a mess - kw */
		return -2;	/* NB unrecognized cond passes here - kw */
	    }
	    if (cond && !strncasecomp(cond, "redirected", strlen(cond))) {
		cond = "redirected"; /* recognized, canonical case - kw */
	    } else if (cond && strlen(cond) >= 8 &&
		!strncasecomp(cond, "userspecified", strlen(cond))) {
		cond = "userspec"; /* also allow abbreviation - kw */
	    }
	    HTAddRule(op, word2, word3, cond_op, cond);
	}
    }
    FREE(line);
    return 0;
}


/*	Load the rules from a file				HTLoadRules()
**	--------------------------
**
** On entry,
**	Rules can be in any state
** On exit,
**	Any existing rules will have been kept.
**	Any new rules will have been loaded.
**	Returns		0 if no error, 0 if error!
**
** Bugs:
**	The strings may not contain spaces.
*/

int HTLoadRules ARGS1(
    CONST char *,	filename)
{
    FILE * fp = fopen(filename, TXT_R);
    char line[LINE_LENGTH+1];

    if (!fp) {
	CTRACE((tfp, "HTRules: Can't open rules file %s\n", filename));
	return -1; /* File open error */
    }
    for(;;) {
	if (!fgets(line, LINE_LENGTH+1, fp)) break;	/* EOF or error */
	(void) HTSetConfiguration(line);
    }
    fclose(fp);
    return 0;		/* No error or syntax errors ignored */
}

#endif /* NO_RULES */
