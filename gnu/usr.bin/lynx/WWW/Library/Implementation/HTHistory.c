#include "HTUtils.h"
#include "tcp.h"		/* for standard io */

#include "HTHistory.h"

#include "LYLeaks.h"

static HTList * history;	/* List of visited anchors */


/*				Navigation
**				==========
*/

/*		Record the jump to an anchor
**		----------------------------
*/

void HTHistory_record
  ARGS1 (HTAnchor *,destination)
{
  if (destination) {
    if (! history)
      history = HTList_new();
    HTList_addObject (history, destination);
  }
}

/*		Go back in history (find the last visited node)
**		------------------
*/

HTAnchor * HTHistory_backtrack
  NOARGS  /* FIXME: Should we add a `sticky' option ? */
{
  if (HTHistory_canBacktrack())
    HTList_removeLastObject(history);
  return(HTAnchor *)HTList_lastObject(history); /* is Home if can't backtrack */
}

BOOL HTHistory_canBacktrack
  NOARGS
{
  return (HTList_objectAt (history, 1) != NULL);
}

/*		Browse through references in the same parent node
**		-------------------------------------------------
**
**	Take the n-th child's link after or before the one we took to get here.
**	Positive offset means go towards most recently added children.
*/

HTAnchor * HTHistory_moveBy
 ARGS1 (int,offset)
{
  HTAnchor * last = (HTAnchor *)HTList_objectAt (history, 1);
  if (! last)
    return NULL;  /* No last visited node */
  if (last != (HTAnchor *) last->parent) {  /* Was a child */
    HTList * kids = last->parent->children;
    int i = HTList_indexOf (kids, last); 
    HTAnchor * nextOne = (HTAnchor *)HTList_objectAt (kids, i - offset);
    if (nextOne) {
      HTAnchor * destination = HTAnchor_followMainLink (nextOne);
      if (destination) {
	HTList_removeLastObject (history);
	HTList_removeLastObject (history);
	HTList_addObject (history, nextOne);
	HTList_addObject (history, destination);
      }
      return destination;
    } else {
      if (TRACE) fprintf(stderr, 
      		"HTHistory_moveBy: offset by %+d goes out of list %p.\n",
		offset, (void*)kids);
      return NULL;
    }
  } else {  /* Was a parent */
    return NULL;  /* FIXME we could possibly follow the next link... */
  }
}

BOOL HTHistory_canMoveBy
 ARGS1 (int,offset)
{
  HTAnchor * last = (HTAnchor *)HTList_objectAt (history, 1);
  if (! last)
    return NO;  /* No last visited node */
  if (last != (HTAnchor *) last->parent) {  /* Was a child */
    HTList * kids = last->parent->children;
    int i = HTList_indexOf (kids, last); 
    return (HTList_objectAt (kids, i - offset) != NULL);
  } else {  /* Was a parent */
    return NO;  /* FIXME we could possibly follow the next link... */
  }
}


/*				Retrieval
**				=========
*/

/*		Read numbered visited anchor (1 is the oldest)
**		----------------------------
*/

HTAnchor * HTHistory_read
  ARGS1 (int,number)
{
  return (HTAnchor *)HTList_objectAt(history, HTList_count (history) - number);
}


/*		Recall numbered visited anchor (1 is the oldest)
**		------------------------------
**	This reads the anchor and stores it again in the list, except if last.
*/

HTAnchor * HTHistory_recall
  ARGS1 (int,number)
{
  HTAnchor * destination =
    (HTAnchor *)HTList_objectAt (history, HTList_count (history) - number);
  if (destination && destination != (HTAnchor *)HTList_lastObject (history))
    HTList_addObject (history, destination);
  return destination;
}

/*		Number of Anchors stored
**		------------------------
**
**	This is needed in order to check the validity of certain commands
**	for menus, etc.
(not needed for now. Use canBacktrack, etc.)
int HTHistory_count
  NOARGS
{
  return HTList_count (history);
}
*/

/*		Change last history entry
**		-------------------------
**
**	Sometimes we load a node by one anchor but leave by a different
**	one, and it is the one we left from which we want to remember.
*/

void HTHistory_leavingFrom
  ARGS1 (HTAnchor *,anchor)
{
  if (HTList_removeLastObject (history))
    HTList_addObject (history, anchor);
  else
    if (TRACE) fprintf(stderr, "HTHistory_leavingFrom: empty history !\n");
}
