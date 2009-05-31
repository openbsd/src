#ifndef TRSTABLE_H
#define TRSTABLE_H

#include <HTUtils.h>

#ifdef __cplusplus
extern "C" {
#endif
/* TRST_MAXCOLSPAN and TRST_MAXCOLSPAN are defined in userdefs.h */ typedef struct _STable_info STable_info;
    extern STable_info *Stbl_startTABLE(short);
    extern int Stbl_finishTABLE(STable_info *);
    extern void Stbl_free(STable_info *);
    extern int Stbl_addRowToTable(STable_info *, int, int);
    extern int Stbl_addCellToTable(STable_info *, int, int, int, int, int,
				   int, int);
    extern int Stbl_finishCellInTable(STable_info *, int, int, int, int);
    extern int Stbl_addColInfo(STable_info *, int, short, BOOL);
    extern int Stbl_finishColGroup(STable_info *);
    extern int Stbl_addRowGroup(STable_info *, short);

#define TRST_ENDCELL_ENDTD	1
#define TRST_ENDCELL_LINEBREAK	0
#define TRST_ENDCELL_MASK	1
#define TRST_FAKING_CELLS	2
#define Stbl_lineBreak(stbl,l,off,pos) Stbl_finishCellInTable(stbl, TRST_ENDCELL_LINEBREAK, l, off, pos)

    extern int Stbl_getStartLine(STable_info *);
    extern int Stbl_getFixupPositions(STable_info *me, int lineno,
				      int *oldpos,
				      int *newpos);
    extern short Stbl_getAlignment(STable_info *);

#ifdef EXP_NESTED_TABLES
    extern void Stbl_update_enclosing(STable_info *me, int max_width,
				      int last_lineno);
    struct _TextAnchor;
    extern void Stbl_set_enclosing(STable_info *me, STable_info *encl, struct _TextAnchor *last_anchor);
    extern STable_info *Stbl_get_enclosing(STable_info *me);
    extern struct _TextAnchor *Stbl_get_last_anchor_before(STable_info *me);
    extern int Stbl_getStartLineDeep(STable_info *);

#else
#define Stbl_getStartLineDeep(t) Stbl_getStartLine(t)
#endif

#ifdef __cplusplus
}
#endif
#endif				/* TRSTABLE_H */
