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
} rule;

#ifndef NO_RULES

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
**	pattern 	points to 0-terminated string containing a single "*"
**	equiv		points to the equivalent string with * for the
**			place where the text matched by * goes.
**  On exit,
**	returns 	0 if success, -1 if error.
*/

PUBLIC int HTAddRule ARGS3(
    HTRuleOp,		op,
    CONST char *,	pattern,
    CONST char *,	equiv)
{ /* BYTE_ADDRESSING removed and memory check - AS - 1 Sep 93 */
    rule *	temp;
    char *	pPattern;

    temp = (rule *)malloc(sizeof(*temp));
    if (temp==NULL)
	outofmem(__FILE__, "HTAddRule");
    pPattern = (char *)malloc(strlen(pattern)+1);
    if (pPattern==NULL)
	outofmem(__FILE__, "HTAddRule");
    if (equiv) {		/* Two operands */
	char *	pEquiv = (char *)malloc(strlen(equiv)+1);
	if (pEquiv==NULL)
	    outofmem(__FILE__, "HTAddRule");
	temp->equiv = pEquiv;
	strcpy(pEquiv, equiv);
    } else {
	temp->equiv = 0;
    }
    temp->pattern = pPattern;
    temp->op = op;

    strcpy(pPattern, pattern);
    if (equiv) {
	CTRACE(tfp, "Rule: For `%s' op %d `%s'\n", pattern, op, equiv);
    } else {
	CTRACE(tfp, "Rule: For `%s' op %d\n", pattern, op);
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


/*	Clear all rules 					HTClearRules()
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
	FREE(temp);
    }
#ifndef PUT_ON_HEAD
    rule_tail = 0;
#endif
}


/*	Translate by rules					HTTranslate()
**	------------------
**
**	The most recently defined rules are applied first.
**
** On entry,
**	required	points to a string whose equivalent value is neeed
** On exit,
**	returns 	the address of the equivalent string allocated from
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

	switch (r->op) {		/* Perform operation */

#ifdef ACCESS_AUTH
	case HT_DefProt:
	case HT_Protect:
	    {
		char *local_copy = NULL;
		char *p2;
		char *eff_ids = NULL;
		char *prot_file = NULL;

		CTRACE(tfp, "HTRule: `%s' matched %s %s: `%s'\n",
			    current,
			    (r->op==HT_Protect ? "Protect" : "DefProt"),
			    "rule, setup",
			    (r->equiv ? r->equiv :
			     (r->op==HT_Protect ?"DEFAULT" :"NULL!!")));

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

	case HT_Pass:				/* Authorised */
		if (!r->equiv) {
		    CTRACE(tfp, "HTRule: Pass `%s'\n", current);
		    return current;
		}
		/* Else fall through ...to map and pass */

	case HT_Map:
	    if (*p == *q) { /* End of both strings, no wildcard */
		  CTRACE(tfp, "For `%s' using `%s'\n", current, r->equiv);
		  StrAllocCopy(current, r->equiv); /* use entire translation */
	    } else {
		  char * ins = strchr(r->equiv, '*');	/* Insertion point */
		  if (ins) {	/* Consistent rule!!! */
			char * temp = (char *)malloc(
				strlen(r->equiv)-1 + m + 1);
			if (temp==NULL)
			    outofmem(__FILE__, "HTTranslate"); /* NT & AS */
			strncpy(temp,	r->equiv, ins-r->equiv);
			/* Note: temp may be unterminated now! */
			strncpy(temp+(ins-r->equiv), q, m);  /* Matched bit */
			strcpy (temp+(ins-r->equiv)+m, ins+1);	/* Last bit */
			CTRACE(tfp, "For `%s' using `%s'\n",
				    current, temp);
			FREE(current);
			current = temp; 		/* Use this */

		    } else {	/* No insertion point */
			char * temp = (char *)malloc(strlen(r->equiv)+1);
			if (temp==NULL)
			    outofmem(__FILE__, "HTTranslate"); /* NT & AS */
			strcpy(temp, r->equiv);
			CTRACE(tfp, "For `%s' using `%s'\n",
						current, temp);
			FREE(current);
			current = temp; 		/* Use this */
		    } /* If no insertion point exists */
		}
		if (r->op == HT_Pass) {
		    CTRACE(tfp, "HTRule: ...and pass `%s'\n",
				current);
		    return current;
		}
		break;

	case HT_Invalid:
	case HT_Fail:				/* Unauthorised */
		CTRACE(tfp, "HTRule: *** FAIL `%s'\n",
			    current);
		FREE(current);
		return (char *)0;
	} /* if tail matches ... switch operation */

    } /* loop over rules */


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
				status >= 1? quality : 1.0);

    } else if (0==strcasecomp(word1, "presentation")) {
	if (pointer) status = sscanf(pointer, "%f%f%f%d",
			    &quality, &secs, &secs_per_byte, &maxbytes);
	else status = 0;
	HTSetPresentation(word2, word3,
		    status >= 1? quality		: 1.0,
		    status >= 2 ? secs			: 0.0,
		    status >= 3 ? secs_per_byte 	: 0.0,
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
	    :	0==strcasecomp(word1, "defprot") ? HT_DefProt
	    :	0==strcasecomp(word1, "protect") ? HT_Protect
	    :						HT_Invalid;
	if (op==HT_Invalid) {
	    fprintf(stderr, "HTRule: %s '%s'\n", RULE_INCORRECT, config);
	} else {
	    HTAddRule(op, word2, word3);
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
**	Returns 	0 if no error, 0 if error!
**
** Bugs:
**	The strings may not contain spaces.
*/

int HTLoadRules ARGS1(
    CONST char *,	filename)
{
    FILE * fp = fopen(filename, "r");
    char line[LINE_LENGTH+1];

    if (!fp) {
	CTRACE(tfp, "HTRules: Can't open rules file %s\n", filename);
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
