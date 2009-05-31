#ifndef LYUPLOAD_H
#define LYUPLOAD_H

#ifndef LYSTRUCTS_H
#include <LYStructs.h>
#endif /* LYSTRUCTS_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern int LYUpload(char *line);
    extern int LYUpload_options(char **newfile, char *directory);

#ifdef __cplusplus
}
#endif
#endif				/* LYUPLOAD_H */
