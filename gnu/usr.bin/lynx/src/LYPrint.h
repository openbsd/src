#ifndef LYPRINT_H
#define LYPRINT_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern int printfile PARAMS((document *newdoc));
extern int print_options PARAMS((char **newfile,
				 CONST char *printed_url, int lines_in_file));
extern char * GetFileName NOPARAMS;

#endif /* LYPRINT_H */

