#include <HTUtils.h>
#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <LYClean.h>
#include <LYCurses.h>
#include <LYStrings.h>
#include <LYTraversal.h>

#include <LYexit.h>
#include <LYLeaks.h>

/* routines to handle special traversal feature */

PRIVATE void final_perror ARGS2(CONST char *,msg, BOOLEAN, clean_flag)
{
    int saved_errno = errno;
    if (LYCursesON) {
	if (clean_flag)
	    cleanup();
	else
	    stop_curses();
    }
    set_errno(saved_errno);
    perror(msg);
}

PRIVATE void exit_with_perror ARGS1(CONST char *,msg)
{
    final_perror(msg, TRUE);
    exit_immediately(EXIT_FAILURE);
}

PUBLIC BOOLEAN lookup ARGS1(char *,target)
{
    FILE *ifp;
    char *buffer = NULL;
    char *line = NULL;
    int result = FALSE;

    if ((ifp = fopen(TRAVERSE_FILE, TXT_R)) == NULL) {
	if ((ifp = LYNewTxtFile(TRAVERSE_FILE)) == NULL) {
	    exit_with_perror(CANNOT_OPEN_TRAV_FILE);
	} else {
	    LYCloseOutput(ifp);
	    return(FALSE);
	}
    }

    HTSprintf0(&line, "%s\n", target);

    while (LYSafeGets(&buffer, ifp) != NULL) {
	if (STREQ(line,buffer)) {
	    result = TRUE;
	    break;
	}
    } /* end while */
    FREE(line);
    FREE(buffer);

    LYCloseInput(ifp);
    return (BOOL) (result);
}

PUBLIC void add_to_table ARGS1(char *,target)
{

    FILE *ifp;

    if ((ifp = LYAppendToTxtFile(TRAVERSE_FILE)) == NULL) {
	exit_with_perror(CANNOT_OPEN_TRAV_FILE);
    }

    fprintf(ifp,"%s\n",target);

    LYCloseOutput(ifp);
}

PUBLIC void add_to_traverse_list ARGS2(char *,fname, char *,prev_link_name)
{

    FILE *ifp;

    if ((ifp = LYAppendToTxtFile(TRAVERSE_FOUND_FILE)) == NULL) {
	exit_with_perror(CANNOT_OPEN_TRAF_FILE);
    }

    fprintf(ifp,"%s\t%s\n",fname, prev_link_name);

    LYCloseOutput(ifp);
}

PUBLIC void dump_traversal_history NOARGS
{
    int x;
    FILE *ifp;

    if (nhist <= 0)
	return;

    if ((ifp = LYAppendToTxtFile(TRAVERSE_FILE)) == NULL) {
	final_perror(CANNOT_OPEN_TRAV_FILE, FALSE);
	return;
    }

    fprintf(ifp, "\n\n%s\n\n\t    %s\n\n",
	    TRAV_WAS_INTERRUPTED,
	    gettext("here is a list of the history stack so that you may rebuild"));

    for (x = nhist-1; x >= 0; x--) {
	fprintf(ifp,"%s\t%s\n", history[x].title, history[x].address);
    }

    LYCloseOutput(ifp);
}

PUBLIC void add_to_reject_list ARGS1(char *,target)
{

    FILE *ifp;

    if ((ifp = LYAppendToTxtFile(TRAVERSE_REJECT_FILE)) == NULL) {
	exit_with_perror(CANNOT_OPEN_REJ_FILE);
    }

    fprintf(ifp,"%s\n",target);

    LYCloseOutput(ifp);
}

/* there need not be a reject file, so if it doesn't open, just return
   FALSE, meaning "target not in reject file" If the last character in
   a line in a reject file is "*", then also reject if target matches up to
   that point in the string
   Blank lines are ignored
   Lines that contain just a * are allowed, but since they mean "reject
   everything" it shouldn't come up much!
 */

PUBLIC BOOLEAN lookup_reject ARGS1(char *,target)
{
    FILE *ifp;
    char *buffer = NULL;
    char *line = NULL;
    int len;
    int result = FALSE;

    if ((ifp = fopen(TRAVERSE_REJECT_FILE, TXT_R)) == NULL){
	return(FALSE);
    }

    HTSprintf0(&line, "%s\n", target);

    while (LYSafeGets(&buffer, ifp) != NULL && !result) {
	len = strlen(buffer);
	if (len > 0) { 	   /* if not an empty line */
	    if (buffer[len-1] == '*') {
		/* if last char is * and the rest of the chars match */
		if ((len == 1) || (strncmp(line,buffer,len - 1) == 0)) {
		    result = TRUE;
		}
	    } else {
		if (STREQ(line,buffer)) {
		    result = TRUE;
		}
	    }
	}
    } /* end while loop over the file */
    FREE(buffer);
    FREE(line);

    LYCloseInput(ifp);
    return (BOOL) (result);
}
