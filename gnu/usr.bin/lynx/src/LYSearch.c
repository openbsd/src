/*
 * $LynxId: LYSearch.c,v 1.23 2009/01/01 23:28:39 tom Exp $
 */
#include <HTUtils.h>
#include <HTAlert.h>
#include <LYUtils.h>
#include <LYStrings.h>
#include <LYSearch.h>
#include <LYGlobalDefs.h>
#include <GridText.h>

#include <LYLeaks.h>

static BOOL link_has_target(int cur,
			    char *target)
{
    LinkInfo *a = &links[cur];
    OptionType *option;
    char *stars = NULL;
    const char *cp;
    int count;

    /*
     * Search the hightext strings, if present, taking the case_sensitive
     * setting into account.
     */
    for (count = 0;; ++count) {
	const char *text = LYGetHiliteStr(cur, count);

	if (text == NULL)
	    break;
	if (LYno_attr_strstr(text, target))
	    return TRUE;
    }

    /*
     * Search the relevant form fields, taking the case_sensitive setting into
     * account.  - FM
     */
    if ((a->l_form != NULL && a->l_form->value != NULL) &&
	a->l_form->type != F_HIDDEN_TYPE) {
	if (a->l_form->type == F_PASSWORD_TYPE) {
	    /*
	     * Check the actual, hidden password, and then the displayed
	     * string.  - FM
	     */
	    if (LYno_attr_strstr(a->l_form->value, target)) {
		return TRUE;
	    }
	    StrAllocCopy(stars, a->l_form->value);
	    memset(stars, '*', strlen(stars));
	    if (LYno_attr_strstr(stars, target)) {
		FREE(stars);
		return TRUE;
	    }
	    FREE(stars);
	} else if (a->l_form->type == F_OPTION_LIST_TYPE) {
	    /*
	     * Search the option strings that are displayed when the popup is
	     * invoked.  - FM
	     */
	    option = a->l_form->select_list;
	    while (option != NULL) {
		if (LYno_attr_strstr(option->name, target)) {
		    return TRUE;
		}
		option = option->next;
	    }
	} else if (a->l_form->type == F_RADIO_TYPE) {
	    /*
	     * Search for checked or unchecked parens.  - FM
	     */
	    if (a->l_form->num_value) {
		cp = checked_radio;
	    } else {
		cp = unchecked_radio;
	    }
	    if (LYno_attr_strstr(cp, target)) {
		return TRUE;
	    }
	} else if (a->l_form->type == F_CHECKBOX_TYPE) {
	    /*
	     * Search for checked or unchecked square brackets.  - FM
	     */
	    if (a->l_form->num_value) {
		cp = checked_box;
	    } else {
		cp = unchecked_box;
	    }
	    if (LYno_attr_strstr(cp, target)) {
		return TRUE;
	    }
	} else {
	    /*
	     * Check the values intended for display.  May have been found
	     * already via the hightext search, but make sure here that the
	     * entire value is searched.  - FM
	     */
	    if (LYno_attr_strstr(a->l_form->value, target)) {
		return TRUE;
	    }
	}
    }
    return FALSE;
}

/*
 * Search for the target string inside of the links that are currently
 * displayed on the screen beginning with the one after the currently selected
 * one.  If found set cur to the new value and return TRUE.  If not found do
 * not reset cur and return FALSE.
 */

static int check_next_target_in_links(int *cur,
				      char *target)
{
    int i;

    if (nlinks != 0) {
	for (i = *cur + 1; i < nlinks; ++i) {
	    if (link_has_target(i, target)) {
		*cur = i;
		return TRUE;
	    }
	}
    }
    return FALSE;
}

static int check_prev_target_in_links(int *cur,
				      char *target)
{
    int i;

    if (nlinks != 0) {
	for (i = *cur - 1; i >= 0; --i) {
	    if (link_has_target(i, target)) {
		*cur = i;
		return TRUE;
	    }
	}
    }
    return FALSE;
}

/*
 * Textsearch checks the prev_target variable to see if it is empty.  If it is
 * then it requests a new search string.  It then searches the current file for
 * the next instance of the search string and finds the line number that the
 * string is on
 *
 * This is the primary USER search engine and is case sensitive or case
 * insensitive depending on the 'case_sensitive' global variable
 */
BOOL textsearch(DocInfo *cur_doc, char *prev_target,
		int target_size,
		int direction)
{
    int offset;
    int oldcur = cur_doc->link;
    static char prev_target_buffer[512];	/* Search string buffer */
    static BOOL first = TRUE;
    char *cp;
    int ch = 0;
    RecallType recall;
    int QueryTotal;
    int QueryNum;
    BOOLEAN FirstRecall = TRUE;

    /*
     * Initialize the search string buffer.  - FM
     */
    if (first) {
	*prev_target_buffer = '\0';
	first = FALSE;
    }

    QueryTotal = (search_queries ? HTList_count(search_queries) : 0);
    recall = ((QueryTotal >= 1) ? RECALL_URL : NORECALL);
    QueryNum = QueryTotal;

    if (direction != 0)
	/*
	 * LYK_NEXT or LYK_PREV was pressed, so copy the buffer into
	 * prev_target.
	 */
	LYstrncpy(prev_target, prev_target_buffer, target_size);

    if (strlen(prev_target) == 0) {
	/*
	 * This is a new WHEREIS search ('/'), or LYK_NEXT was pressed but
	 * there was no previous search, so we need to get a search string from
	 * the user.  - FM
	 */
	_statusline(ENTER_WHEREIS_QUERY);

	ch = LYgetstr(prev_target, VISIBLE, (unsigned) target_size, recall);
	if (ch < 0) {
	    /*
	     * User cancelled the search via ^G.  Restore prev_target and
	     * return.  - FM
	     */
	    LYstrncpy(prev_target, prev_target_buffer, target_size);
	    HTInfoMsg(CANCELLED);
	    return (FALSE);
	}
    }

  check_recall:
    if (strlen(prev_target) == 0 &&
	!(recall && (ch == UPARROW || ch == DNARROW))) {
	/*
	 * No entry.  Simply return, retaining the current buffer.  Because
	 * prev_target is now reset, highlighting of the previous search string
	 * will no longer occur, but it can be used again via LYK_NEXT or
	 * LYK_PREV.
	 */
	HTInfoMsg(CANCELLED);
	return (FALSE);
    }

    if (recall && ch == UPARROW) {
	if (FirstRecall) {
	    /*
	     * Use the current string or last query in the list.  - FM
	     */
	    FirstRecall = FALSE;
	    if (*prev_target_buffer) {
		for (QueryNum = (QueryTotal - 1); QueryNum > 0; QueryNum--) {
		    if ((cp = (char *) HTList_objectAt(search_queries,
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
	     * Go back to the previous query in the list.  - FM
	     */
	    QueryNum++;
	}
	if (QueryNum >= QueryTotal)
	    /*
	     * Roll around to the last query in the list.  - FM
	     */
	    QueryNum = 0;
	if ((cp = (char *) HTList_objectAt(search_queries,
					   QueryNum)) != NULL) {
	    LYstrncpy(prev_target, cp, target_size);
	    if (*prev_target_buffer &&
		!strcmp(prev_target_buffer, prev_target)) {
		_statusline(EDIT_CURRENT_QUERY);
	    } else if ((*prev_target_buffer && QueryTotal == 2) ||
		       (!(*prev_target_buffer) && QueryTotal == 1)) {
		_statusline(EDIT_THE_PREV_QUERY);
	    } else {
		_statusline(EDIT_A_PREV_QUERY);
	    }
	    ch = LYgetstr(prev_target, VISIBLE, (unsigned) target_size, recall);
	    if (ch < 0) {
		/*
		 * User canceled the search via ^G.  Restore prev_target and
		 * return.  - FM
		 */
		LYstrncpy(prev_target, prev_target_buffer, target_size);
		HTInfoMsg(CANCELLED);
		return (FALSE);
	    }
	    goto check_recall;
	}
    } else if (recall && ch == DNARROW) {
	if (FirstRecall) {
	    /*
	     * Use the current string or first query in the list.  - FM
	     */
	    FirstRecall = FALSE;
	    if (*prev_target_buffer) {
		for (QueryNum = 0; QueryNum < (QueryTotal - 1); QueryNum++) {
		    if ((cp = (char *) HTList_objectAt(search_queries,
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
	     * Advance to the next query in the list.  - FM
	     */
	    QueryNum--;
	}
	if (QueryNum < 0)
	    /*
	     * Roll around to the first query in the list.  - FM
	     */
	    QueryNum = QueryTotal - 1;
	if ((cp = (char *) HTList_objectAt(search_queries,
					   QueryNum)) != NULL) {
	    LYstrncpy(prev_target, cp, target_size);
	    if (*prev_target_buffer &&
		!strcmp(prev_target_buffer, prev_target)) {
		_statusline(EDIT_CURRENT_QUERY);
	    } else if ((*prev_target_buffer && QueryTotal == 2) ||
		       (!(*prev_target_buffer) && QueryTotal == 1)) {
		_statusline(EDIT_THE_PREV_QUERY);
	    } else {
		_statusline(EDIT_A_PREV_QUERY);
	    }
	    ch = LYgetstr(prev_target, VISIBLE, (unsigned) target_size, recall);
	    if (ch < 0) {
		/*
		 * User cancelled the search via ^G.  Restore prev_target and
		 * return.  - FM
		 */
		LYstrncpy(prev_target, prev_target_buffer, target_size);
		HTInfoMsg(CANCELLED);
		return (FALSE);
	    }
	    goto check_recall;
	}
    }
    /*
     * Replace the search string buffer with the new target.  - FM
     */
    LYstrncpy(prev_target_buffer, prev_target, sizeof(prev_target_buffer) - 1);
    HTAddSearchQuery(prev_target_buffer);

    if (direction < 0) {
	offset = 0;
	if (check_prev_target_in_links(&cur_doc->link, prev_target)) {
	    /*
	     * Found in link, changed cur, we're done.
	     */
	    LYhighlight(OFF, oldcur, prev_target);
	    return (TRUE);
	}
    } else {

	/*
	 * Search the links on the currently displayed page for the string,
	 * starting after the current link.  - FM
	 */
	if (check_next_target_in_links(&cur_doc->link, prev_target)) {
	    /*
	     * Found in link, changed cur, we're done.
	     */
	    LYhighlight(OFF, oldcur, prev_target);
	    return (TRUE);
	}

	/*
	 * We'll search the text starting from the link we are on, or the next
	 * page.
	 */
	if (nlinks == 0)
	    offset = (display_lines - 1);
	else
	    offset = links[cur_doc->link].ly - 1;
    }

    /*
     * Resume search, this time for all text.  Set www_search_result if string
     * found, and position the hit near top of screen.
     */
    www_user_search((cur_doc->line + offset), cur_doc, prev_target, direction);
    if (cur_doc->link != oldcur) {
	LYhighlight(OFF, oldcur, prev_target);
	return (TRUE);
    }
    return (BOOL) (www_search_result > 0);
}
