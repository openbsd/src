
#ifndef LYPRINT_H
#define LYPRINT_H

#ifndef LYSTRUCTS_H
#include "LYStructs.h"
#endif /* LYSTRUCTS_H */

extern int printfile PARAMS((document *newdoc));
extern int print_options PARAMS((char **newfile, int lines_in_file));

#define PRINT_OPTIONS_TITLE "Lynx Printing Options"

#endif /* LYPRINT_H */

