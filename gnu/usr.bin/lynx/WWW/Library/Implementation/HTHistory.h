/*  */

#ifndef HTHISTORY_H
#define HTHISTORY_H

#include "HTAnchor.h"

#ifdef SHORT_NAMES
#define HTHistory_record                HTHiReco
#define HTHistory_backtrack             HTHiBack
#define HTHistory_canBacktrack          HTHiCaBa
#define HTHistory_moveBy                HTHiMoBy
#define HTHistory_canMoveBy             HTHiCaMo
#define HTHistory_read                  HTHiRead
#define HTHistory_recall                HTHiReca
#define HTHistory_count                 HTHiCoun
#define HTHistory_leavingFrom           HTHiLeFr
#endif

/*                              Navigation
**                              ==========
*/

/*              Record the jump to an anchor
**              ----------------------------
*/

extern void HTHistory_record
  PARAMS(
    (HTAnchor * destination)
  );

/*              Go back in history (find the last visited node)
**              ------------------
*/

extern HTAnchor * HTHistory_backtrack
  NOPARAMS;  /* FIXME: Should we add a `sticky' option ? */

extern BOOL HTHistory_canBacktrack
  NOPARAMS;

/*              Browse through references in the same parent node
**              -------------------------------------------------
**
**      Take the n-th child's link after or before the one we took to get here.
**      Positive offset means go towards most recently added children.
*/

extern HTAnchor * HTHistory_moveBy
  PARAMS(
     (int offset)
     );

extern BOOL HTHistory_canMoveBy
  PARAMS(
     (int offset)
     );

#define HTHistory_next (HTHistory_moveBy (+1))
#define HTHistory_canNext (HTHistory_canMoveBy (+1))
#define HTHistory_previous (HTHistory_moveBy (-1))
#define HTHistory_canPrevious (HTHistory_canMoveBy (-1))


/*                              Retrieval
**                              =========
*/

/*              Read numbered visited anchor (1 is the oldest)
**              ----------------------------
*/

extern HTAnchor * HTHistory_read
  PARAMS(
    (int number)
  );

/*              Recall numbered visited anchor (1 is the oldest)
**              ------------------------------
**      This reads the anchor and stores it again in the list, except if last.
*/

extern HTAnchor * HTHistory_recall
  PARAMS(
    (int number)
  );

/*              Number of Anchors stored
**              ------------------------
**
**      This is needed in order to check the validity of certain commands
**      for menus, etc.
(not needed for now. Use canBacktrack, etc.)
extern int HTHistory_count NOPARAMS;
*/

/*              Change last history entry
**              -------------------------
**
**      Sometimes we load a node by one anchor but leave by a different
**      one, and it is the one we left from which we want to remember.
*/
extern void HTHistory_leavingFrom
  PARAMS(
    (HTAnchor * anchor)
  );

#endif /* HTHISTORY_H */
/*

    */
