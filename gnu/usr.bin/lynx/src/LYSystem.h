
#ifndef LYSYSTEM_H
#define LYSYSTEM_H

#ifdef VMS
extern int DCLsystem PARAMS((char *command));
#define system(a) DCLsystem(a) /* use LYCurses.c routines for spawns */
#endif /* VMS */

#endif /* LYSYSTEM_H */
