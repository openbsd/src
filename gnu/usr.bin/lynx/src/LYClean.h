#ifndef LYCLEAN_H
#define LYCLEAN_H

#ifdef VMS
extern BOOLEAN HadVMSInterrupt;
#endif

extern void cleanup_sig PARAMS((int sig));
extern BOOLEAN setup PARAMS((char *terminal));
extern void cleanup NOPARAMS;
extern void cleanup_files NOPARAMS;
extern void set_alarm PARAMS((int sig));
extern void reset_alarm NOPARAMS;

#define NEW_FILE     0
#define REMOVE_FILES 1

#endif /* LYCLEAN_H */
