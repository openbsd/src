#ifndef LYCLEAN_H
#define LYCLEAN_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef VMS
extern BOOLEAN HadVMSInterrupt;
#endif

extern void cleanup_sig PARAMS((int sig));
extern void cleanup NOPARAMS;
extern void cleanup_files NOPARAMS;
extern void set_alarm PARAMS((int sig));
extern void reset_alarm NOPARAMS;

#endif /* LYCLEAN_H */
