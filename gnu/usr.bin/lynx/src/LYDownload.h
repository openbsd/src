#ifndef LYDOWNLOAD_H
#define LYDOWNLOAD_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

extern void LYDownload PARAMS((char *line));
extern int LYdownload_options PARAMS((char **newfile, char *data_file));

#ifdef VMS
extern BOOLEAN LYDidRename;
#endif

#endif /* LYDOWNLOAD_H */
