#include "HTUtils.h"
#include "tcp.h"
#include "LYUtils.h"
#include "LYStrings.h"
#include "LYSearch.h"
#include "LYGlobalDefs.h"
#include "GridText.h"
#include "LYSignal.h"

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

/*
 *  Search for the target string inside of the links
 *  that are currently displayed on the screen beginning
 *  with the one after the currently selected one.
 *  If found set cur to the new value and return TRUE.
 *  If not found do not reset cur and return FALSE.
 */

PRIVATE int check_for_target_in_links ARGS2(
	int *,		cur,
	char *,		new_target)
{
    int i = *cur + 1;
    OptionType *option;
    char *stars = NULL, *cp;

    if (nlinks == 0)
	return(FALSE);

    for (; i < nlinks; i++) {
        /*
	 *  Search the hightext string, and hightext2 if present,
	 *  taking the case_sensitive setting into account. - FM
	 */
	if (((links[i].hightext != NULL && case_sensitive == TRUE) &&
	     LYno_attr_char_strstr(links[i].hightext, new_target)) ||
	    ((links[i].hightext != NULL && case_sensitive == FALSE) &&
	     LYno_attr_char_case_strstr(links[i].hightext, new_target))) {
	    break;
	}
	if (((links[i].hightext2 != NULL && case_sensitive == TRUE) &&
	     LYno_attr_char_strstr(links[i].hightext2, new_target)) ||
	    ((links[i].hightext2 != NULL && case_sensitive == FALSE) &&
	     LYno_attr_char_case_strstr(links[i].hightext2, new_target))) {
	    break;
	}

	/*
	 *  Search the relevant form fields, taking the
	 *  case_sensitive setting into account. - FM
	 */
	if ((links[i].form != NULL && links[i].form->value != NULL) &&
	    links[i].form->type != F_HIDDEN_TYPE) {
	    if (links[i].form->type == F_PASSWORD_TYPE) {
	        /*
		 *  Check the actual, hidden password, and then
		 *  the displayed string. - FM
		 */
		if (((case_sensitive == TRUE) &&
		     LYno_attr_char_strstr(links[i].form->value,
					   new_target)) ||
		    ((case_sensitive == FALSE) &&
		     LYno_attr_char_case_strstr(links[i].form->value,
						new_target))) {
		    break;
		}
		StrAllocCopy(stars, links[i].form->value);
		for (cp = stars; *cp != '\0'; cp++)
		    *cp = '*';
		if (((case_sensitive == TRUE) &&
		     LYno_attr_char_strstr(stars, new_target)) ||
		    ((case_sensitive == FALSE) &&
		     LYno_attr_char_case_strstr(stars, new_target))) {
		    FREE(stars);
		    break;
		}
		FREE(stars);
	    } else if (links[i].form->type == F_OPTION_LIST_TYPE) {
		/*
		 *  Search the option strings that are displayed
		 *  when the popup is invoked. - FM
		 */
		option = links[i].form->select_list;
		while (option != NULL) {
		    if (((option->name != NULL &&
			  case_sensitive == TRUE) &&
			 LYno_attr_char_strstr(option->name, new_target)) ||
			((option->name != NULL &&
			  case_sensitive == FALSE) &&
			 LYno_attr_char_case_strstr(option->name,
						    new_target))) {
			break;
		    }
		    option = option->next;
		}
		if (option != NULL) {
		    break;
		}
	    } else if (links[i].form->type == F_RADIO_TYPE) {
		/*
		 *  Search for checked or unchecked parens. - FM
		 */
	        if (links[i].form->num_value) {
		    cp = checked_radio;
		} else {
		    cp = unchecked_radio;
		}
		if (((case_sensitive == TRUE) &&
		     LYno_attr_char_strstr(cp, new_target)) ||
		    ((case_sensitive == FALSE) &&
		     LYno_attr_char_case_strstr(cp, new_target))) {
		    break;
		}
	    } else if (links[i].form->type == F_CHECKBOX_TYPE) {
		/*
		 *  Search for checked or unchecked square brackets. - FM
		 */
	        if (links[i].form->num_value) {
		    cp = checked_box;
		} else {
		    cp = unchecked_box;
		}
		if (((case_sensitive == TRUE) &&
		     LYno_attr_char_strstr(cp, new_target)) ||
		    ((case_sensitive == FALSE) &&
		     LYno_attr_char_case_strstr(cp, new_target))) {
		    break;
		}
	    } else {
	        /*
		 *  Check the values intended for display.
		 *  May have been found already via the
		 *  hightext search, but make sure here
		 *  that the entire value is searched. - FM
		 */
		if (((case_sensitive == TRUE) &&
		     LYno_attr_char_strstr(links[i].form->value,
					   new_target)) ||
		    ((case_sensitive == FALSE) &&
		     LYno_attr_char_case_strstr(links[i].form->value,
						new_target))) {
		    break;
		}
	    }
	}
    }

    if (i == nlinks)
	return(FALSE);
 
    *cur = i;
    return(TRUE);
}

/*
 *  Textsearch checks the prev_target variable to see if it is empty.
 *  If it is then it requests a new search string.  It then searches 
 *  the current file for the next instance of the search string and
 *  finds the line number that the string is on
 * 
 *  This is the primary USER search engine and is case sensitive
 *  or case insensitive depending on the 'case_sensitive' global
 *  variable
 *
 */
		
PUBLIC BOOL textsearch ARGS3(
	document *,	cur_doc,
	char *,		prev_target,
	BOOL,		next)
{
    int offset;
    int oldcur = cur_doc->link;
    static char prev_target_buffer[512]; /* Search string buffer */
    static BOOL first = TRUE;
    char *cp;
    int ch = 0, recall;
    int QueryTotal;
    int QueryNum;
    BOOLEAN FirstRecall = TRUE;

    /*
     *  Initialize the search string buffer. - FM
     */
    if (first) {
	*prev_target_buffer = '\0';
	first = FALSE;
    }

    QueryTotal = (search_queries ? HTList_count(search_queries) : 0);
    recall = ((QueryTotal >= 1) ? RECALL : NORECALL);
    QueryNum = QueryTotal;

    if (next)
        /*
	 *  LYK_NEXT was pressed, so copy the
	 *  buffer into prev_target. - FM
	 */
	strcpy(prev_target, prev_target_buffer);

    if (strlen(prev_target) == 0 ) {
        /*
	 *  This is a new WHEREIS search ('/'), or
	 *  LYK_NEXT was pressed but there was no
	 *  previous search, so we need to get a
	 *  search string from the user. - FM
	 */
	_statusline(ENTER_WHEREIS_QUERY);

	if ((ch = LYgetstr(prev_target, VISIBLE,
	    		   sizeof(prev_target_buffer), recall)) < 0) {
	    /*
	     *  User cancelled the search via ^G.
	     *  Restore prev_target and return. - FM
	     */
	    strcpy(prev_target, prev_target_buffer);
	    _statusline(CANCELLED);
	    sleep(InfoSecs);
	    return(FALSE);
	}
    }

check_recall:
    if (strlen(prev_target) == 0 &&
        !(recall && (ch == UPARROW || ch == DNARROW))) {
        /*
	 *  No entry.  Simply return, retaining the current buffer.
	 *  Because prev_target is now reset, highlighting of the
	 *  previous search string will no longer occur, but it can
	 *  be used again via LYK_NEXT.   - FM
	 */
        _statusline(CANCELLED);
        sleep(InfoSecs);
	return(FALSE);
    }

    if (recall && ch == UPARROW) {
	if (FirstRecall) {
	    /*
	     *  Use the current string or last query in the list. - FM
	     */
	    FirstRecall = FALSE;
	    if (*prev_target_buffer) {
	        for (QueryNum = (QueryTotal - 1); QueryNum > 0; QueryNum--) {
		    if ((cp = (char *)HTList_objectAt(search_queries,
	    					      QueryNum)) != NULL &&
		        !strcmp(prev_target_buffer, cp)) {
		        break;
		    }
		 }
	     } else {
		QueryNum = 0;
	     }
	} else {
	    /*
	     *  Go back to the previous query in the list. - FM
	     */
	    QueryNum++;
	}
	if (QueryNum >= QueryTotal)
	    /*
	     *  Roll around to the last query in the list. - FM
	     */
	    QueryNum = 0;
	if ((cp = (char *)HTList_objectAt(search_queries,
	    				  QueryNum)) != NULL) {
	    strcpy(prev_target, cp);
	    if (*prev_target_buffer &&
	        !strcmp(prev_target_buffer, prev_target)) {
		_statusline(EDIT_CURRENT_QUERY);
	    } else if ((*prev_target_buffer && QueryTotal == 2) ||
		       (!(*prev_target_buffer) && QueryTotal == 1)) {
		_statusline(EDIT_THE_PREV_QUERY);
	    } else {
		_statusline(EDIT_A_PREV_QUERY);
	    }
	    if ((ch = LYgetstr(prev_target, VISIBLE,
	    		       sizeof(prev_target_buffer), recall)) < 0) {
	        /*
		 *  User cancelled the search via ^G.
		 *  Restore prev_target and return. - FM
		 */
		strcpy(prev_target, prev_target_buffer);
		_statusline(CANCELLED);
		sleep(InfoSecs);
		return(FALSE);
	    }
	    goto check_recall;
	}
    } else if (recall && ch == DNARROW) {
	if (FirstRecall) {
	    /*
	     *  Use the current string or first query in the list. - FM
	     */
	    FirstRecall = FALSE;
	    if (*prev_target_buffer) {
	        for (QueryNum = 0; QueryNum < (QueryTotal - 1); QueryNum++) {
		    if ((cp = (char *)HTList_objectAt(search_queries,
	    					      QueryNum)) != NULL &&
		        !strcmp(prev_target_buffer, cp)) {
		        break;
		    }
		}
	    } else {
		QueryNum = QueryTotal - 1;
	    }
	} else {
	    /*
	     *  Advance to the next query in the list. - FM
	     */
	    QueryNum--;
	}
	if (QueryNum < 0)
	    /*
	     *  Roll around to the first query in the list. - FM
	     */
	    QueryNum = QueryTotal - 1;
	if ((cp = (char *)HTList_objectAt(search_queries,
	    				  QueryNum)) != NULL) {
	    strcpy(prev_target, cp);
	    if (*prev_target_buffer &&
	        !strcmp(prev_target_buffer, prev_target)) {
		_statusline(EDIT_CURRENT_QUERY);
	    } else if ((*prev_target_buffer && QueryTotal == 2) ||
		       (!(*prev_target_buffer) && QueryTotal == 1)) {
		_statusline(EDIT_THE_PREV_QUERY);
	    } else {
		_statusline(EDIT_A_PREV_QUERY);
	    }
	    if ((ch = LYgetstr(prev_target, VISIBLE,
			       sizeof(prev_target_buffer), recall)) < 0) {
	        /*
		 *  User cancelled the search via ^G.
		 *  Restore prev_target and return. - FM
		 */
		strcpy(prev_target, prev_target_buffer);
		_statusline(CANCELLED);
		sleep(InfoSecs);
		return(FALSE);
	    }
	    goto check_recall;
	}
    }
    /*
     *  Replace the search string buffer with the new target. - FM
     */
    strcpy(prev_target_buffer, prev_target);
    HTAddSearchQuery(prev_target_buffer);

    /*
     *  Search the links on the currently displayed page for
     *  the string, starting after the current link. - FM
     */
    if (check_for_target_in_links(&cur_doc->link, prev_target)) {
	/*
	 *  Found in link, changed cur, we're done.
	 */
	highlight(OFF, oldcur, prev_target);
	return(TRUE); 
    }
	
    /*
     *  We'll search the text starting from the
     *  link we are on, or the next page.
     */
    if (nlinks == 0)
	offset = (display_lines - 1);
    else
	offset = links[cur_doc->link].ly - 1;

    /*
     *  Resume search, this time for all text.
     *  Set www_search_result if string found,
     *  and position the hit near top of screen.
     */
    www_user_search((cur_doc->line + offset), cur_doc, prev_target);
    if (cur_doc->link != oldcur) {
	highlight(OFF, oldcur, prev_target);
	return(TRUE);
    }
    return(www_search_result > 0);
}
