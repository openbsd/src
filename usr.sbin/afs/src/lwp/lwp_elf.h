/*
 * For common usage of elf platforms
 *
 * $arla: lwp_elf.h,v 1.2 2000/03/03 08:53:43 assar Exp $
 */

#ifndef _C_LABEL
#if !defined(SYSV) && !defined(__ELF__) && !defined(AFS_SUN5_ENV)
#ifdef __STDC__
#define _C_LABEL(name)     _##name
#else
#define _C_LABEL(name)  _/**/name
#endif
#else /* SYSV || __ELF__ || AFS_SUN5_ENV */
#define _C_LABEL(name)  name
#endif
#endif /* _C_LABEL */

#ifndef ENTRY
#if !defined(SYSV) && !defined(__ELF__) && !defined(AFS_SUN5_ENV)
#ifdef __STDC__
#define ENTRY(name)    _##name##:
#else
#define ENTRY(name)     _/**/name/**/:
#endif
#else /* SYSV || __ELF__ || AFS_SUN5_ENV */
#define ENTRY(name)     name:
#endif
#endif /* _C_LABEL */
