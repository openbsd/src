
#ifndef LYVMSDEF_H
#define LYVMSDEF_H

/*
 *  These are VMS system definitions which may not be in the headers
 *  of old VMS compilers and contain non-ANSI extended tokens that
 *  generate warnings by some Unix compilers while looking for the
 *  "#endif" which closes the outer "#ifdef VMS".
 */
#ifndef CLI$M_TRUSTED
#define CLI$M_TRUSTED 64	/* May not be in the compiler's clidef.h */
#endif /* !CLI$M_TRUSTED */
#ifndef LIB$_INVARG
#define LIB$_INVARG 1409588	/* May not be in the compiler's libdef.h */
#endif /* !LIB$_INVARG */

#endif /* LYVMSDEF_H */
