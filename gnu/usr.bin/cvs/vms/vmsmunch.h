/*---------------------------------------------------------------------------

  VMSmunch.h

  A few handy #defines, plus the contents of three header files from Joe
  Meadows' FILE program.  Used by VMSmunch and by various routines which
  call VMSmunch (e.g., in Zip and UnZip).

	02-Apr-1994	Jamie Hanrahan	jeh@cmkrnl.com
			Moved definition of VMStimbuf struct from vmsmunch.c
			to here.

	06-Apr-1994	Jamie Hanrahan	jeh@cmkrnl.com
			Moved "contents of three header files" (not needed by 
			callers of vmsmunch) to vmsmunch_private.h .

	07-Apr-1994	Richard Levitte levitte@e.kth.se
			Inserted a forward declaration of VMSmunch.
  ---------------------------------------------------------------------------*/

#define GET_TIMES       4
#define SET_TIMES       0
#define GET_RTYPE       1
#define CHANGE_RTYPE    2
#define RESTORE_RTYPE   3

struct VMStimbuf {      /* VMSmunch */
    char *actime;       /* VMS revision date, ASCII format */
    char *modtime;      /* VMS creation date, ASCII format */
};

extern int VMSmunch();
