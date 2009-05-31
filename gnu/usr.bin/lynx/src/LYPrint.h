#ifndef LYPRINT_H
#define LYPRINT_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern int printfile(DocInfo *newdoc);
    extern int print_options(char **newfile,
			     const char *printed_url,
			     int lines_in_file);
    extern char *GetFileName(void);

#ifdef __cplusplus
}
#endif
#endif				/* LYPRINT_H */
