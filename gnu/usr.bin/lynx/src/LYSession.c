/* $LynxId: LYSession.c,v 1.6 2008/07/02 21:24:27 Paul.B.Mahol Exp $ */

#include <LYSession.h>

#include <LYUtils.h>
#include <LYStrings.h>
#include <LYHistory.h>
#include <LYGlobalDefs.h>
#include <LYMainLoop.h>
#include <GridText.h>

#ifdef USE_SESSIONS

/* Example of how a session file may look:
 */

/* # lynx session
 * / files
 * / hereby
 * / reduce
 * g file://localhost/COPYRIGHT
 * g http://lynx.isc.org
 * h 1 -1 file://localhost/COPYRIGHT       Entry into main screen
 * h 1 0 LYNXCACHE:/       Cache Jar
 * h 1 16 file://localhost/usr/local/share/lynx_help/Lynx_users_guide.html#Cache   Lynx Users Guide v2.8.6
 * h 1 -1 file://localhost/COPYRIGHT       Entry into main screen
 * h 1 2 file://localhost/tmp/lynxmSefvcbXes/L12110-6407TMP.html#current   Visited Links Page
 * h 1 -1 file://localhost/COPYRIGHT       Entry into main screen
 * h 1 -1 LYNXMESSAGES:/   Your recent statusline messages
 * V 0 file://localhost/COPYRIGHT  Entry into main screen
 * V 3 file://localhost/usr/local/share/lynx_help/Lynx_users_guide.html#Bookmarks  Lynx Users Guide v2.8.6
 */

static char *get_filename(char *given_name)
{
    char *actual_filename = given_name;

    /*
     * If the specific "-sessionin" or "-sessionout" value is not given,
     * try the "-session" value (if the AUTO_SESSION configuration is set).
     * Finally try the SESSION_FILE configuration value.
     */
    if (isEmpty(actual_filename)) {
	actual_filename = session_file;
	if (isEmpty(actual_filename)) {
	    if (LYAutoSession) {
		actual_filename = LYSessionFile;
	    }
	}
    }

    return actual_filename;
}

/* Restore session from file, pretty slow, but it should be fine
 * for everyday, normal use.
 */
void RestoreSession(void)
{
    char *my_filename = get_filename(sessionin_file);
    FILE *fp;
    char *buffer = 0;
    DocInfo doc;
    VisitedLink *vl;
    int i = 0;
    short errors = 10;		/* how many syntax errors are allowed in */

    /* session file before aborting. */
    char *value1, *value2, *rsline, *linktext, *rslevel;

    /*
     * This should be done only once, here:  iff USE_SESSIONS is defined or: 
     * in mainloop(), otherwise history entries are lost
     */
    nhist = 0;

    if (my_filename == NULL) {
	/* nothing to do, so exit */
	return;
    }

    CTRACE((tfp, "RestoreSession %s\n", my_filename));
    SetDefaultMode(O_TEXT);
    if ((fp = fopen(my_filename, TXT_R)) != NULL) {

	/*
	 * This should be safe, entries are added to lynx until memory is
	 * exhausted
	 */
	while (LYSafeGets(&buffer, fp) != 0) {
	    LYTrimNewline(buffer);
	    if (*buffer == '/') {
#ifdef SEARCH_OUT_SESSION
		if ((value1 = strchr(buffer, ' ')) == 0) {
		    continue;
		} else {
		    value1++;
		    HTAddSearchQuery(value1);
		}
#endif /* SEARCH_OUT_SESSION */
	    } else if (*buffer == 'g') {
#ifdef GOTOURL_OUT_SESSION
		if ((value1 = strchr(buffer, ' ')) == 0)
		    continue;
		else {
		    value1++;
		    HTAddGotoURL(value1);
		}
#endif /* GOTOURL_OUT_SESSION */
	    } else if (*buffer == 'h') {
#ifdef HISTORY_OUT_SESSION
		if ((rsline = strchr(buffer, ' ')) == 0)
		    continue;
		else {
		    rsline++;
		    if ((linktext = strchr(rsline, ' ')) == 0)
			continue;
		    else
			*linktext++ = 0;
		    if ((value1 = strchr(linktext, ' ')) == 0)
			continue;
		    else
			*value1++ = 0;
		    if ((value2 = strchr(value1, '\t')) != 0) {
			*value2++ = 0;
			doc.line = atoi(rsline);
			doc.link = atoi(linktext);
			StrAllocCopy(doc.address, value1);
			StrAllocCopy(doc.title, value2);
			LYpush(&doc, TRUE);
		    }
		}
#endif /* HISTORY_OUT_SESSION */
	    } else if (*buffer == 'V') {
#ifdef VLINK_OUT_SESSION
		if ((rslevel = strchr(buffer, ' ')) == 0)
		    continue;
		else {
		    rslevel++;
		    if ((value1 = strchr(rslevel, ' ')) == 0)
			continue;
		    else
			*value1++ = 0;
		    if ((value2 = strchr(value1, '\t')) != 0) {
			*value2++ = 0;
			StrAllocCopy(doc.address, value1);
			StrAllocCopy(doc.title, value2);
			LYAddVisitedLink(&doc);
			vl = (VisitedLink *)
			    HTList_objectAt(Visited_Links, i);
			if (vl != NULL) {
			    vl->level = atoi(rslevel);
			    i++;
			}
		    }
		}
#endif /* VLINK_OUT_SESSION */
	    } else if (*buffer == '#') {
		/* This is comment; ignore it */
		continue;
	    } else if (errors-- < 0) {
		FREE(buffer);
		break;
	    } else
		continue;
	}

	LYCloseOutput(fp);
    }
    SetDefaultMode(O_BINARY);
}

/*
 * Save session to file, overwriting one.
 */
void SaveSession(void)
{
    char *my_filename = get_filename(sessionout_file);
    FILE *fp;
    VisitedLink *vl;
    int i, j, k;

    if (my_filename == NULL) {
	/* nothing to do, so exit */
	return;
    }

    CTRACE((tfp, "SaveSession %s\n", my_filename));

    SetDefaultMode(O_TEXT);
    if ((fp = fopen(my_filename, TXT_W)) != NULL) {

	fprintf(fp, "# lynx session\n");	/* @@@ simple for now */

	/* Note use of session_limit, the most recent entries in list,
	 * from the end of list, are saved.
	 */

#ifdef SEARCH_IN_SESSION
	k = HTList_count(search_queries);
	if (k > session_limit)
	    j = k - session_limit;
	else
	    j = 0;
	for (i = j; i < k; i++) {
	    fprintf(fp, "/ ");
	    fputs((char *) HTList_objectAt(search_queries, i), fp);
	    fprintf(fp, "\n");
	}
#endif /* SEARCH_IN_SESSION */

#ifdef GOTOURL_IN_SESSION
	k = HTList_count(Goto_URLs);
	if (k > session_limit)
	    j = k - session_limit;
	else
	    j = 0;
	for (i = j; i < k; i++) {
	    fprintf(fp, "g ");
	    fputs((char *) HTList_objectAt(Goto_URLs, i), fp);
	    fprintf(fp, "\n");
	}
#endif /* GOTOURL_IN_SESSION */

#ifdef HISTORY_IN_SESSION
	k = nhist + nhist_extra;
	if (k > session_limit)
	    j = k - session_limit;
	else
	    j = 0;

	for (i = j; i < k; i++) {
	    fprintf(fp, "h %d %d ", HDOC(i).line, HDOC(i).link);
	    fputs(HDOC(i).address, fp);
	    fprintf(fp, "\t");
	    fputs(HDOC(i).title, fp);
	    fprintf(fp, "\n");
	}
#endif /* HISTORY_IN_SESSION */

#ifdef VLINK_IN_SESSION
	k = HTList_count(Visited_Links);
	if (k > session_limit)
	    j = k - session_limit;
	else
	    j = 0;

	for (i = j; i < k; i++) {
	    vl = (VisitedLink *) HTList_objectAt(Visited_Links, i);
	    if (vl != NULL) {
		fprintf(fp, "V %d ", vl->level);
		fputs(vl->address, fp);
		fprintf(fp, "\t");
		fputs(vl->title, fp);
		fprintf(fp, "\n");
	    }
	}
#endif /* VLINK_IN_SESSION */

	LYCloseOutput(fp);
    }
    SetDefaultMode(O_BINARY);
}

#endif /* USE_SESSIONS */
