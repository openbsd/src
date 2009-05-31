#ifndef LYDOWNLOAD_H
#define LYDOWNLOAD_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern void LYDownload(char *line);
    extern int LYdownload_options(char **newfile, char *data_file);

#ifdef VMS
    extern BOOLEAN LYDidRename;
#endif

#ifdef __cplusplus
}
#endif
#endif				/* LYDOWNLOAD_H */
