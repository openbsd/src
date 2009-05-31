#ifndef LYCLEAN_H
#define LYCLEAN_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#ifdef VMS
    extern BOOLEAN HadVMSInterrupt;
#endif

    extern void cleanup_sig(int sig);
    extern void cleanup(void);
    extern void cleanup_files(void);
    extern void set_alarm(int sig);
    extern void reset_alarm(void);

#ifdef __cplusplus
}
#endif
#endif				/* LYCLEAN_H */
