#ifndef LYRCFILE_H
#define LYRCFILE_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern Config_Enum tbl_DTD_recovery[];
extern Config_Enum tbl_keypad_mode[];
extern Config_Enum tbl_multi_bookmarks[];
extern Config_Enum tbl_transfer_rate[];
extern Config_Enum tbl_user_mode[];

extern BOOL will_save_rc PARAMS((char * name));
extern int enable_lynxrc PARAMS((char * value));
extern int save_rc PARAMS((FILE *));
extern void read_rc PARAMS((FILE *));

#endif /* LYRCFILE_H */
