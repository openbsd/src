/*
 * For common usage of elf platforms
 *
 * $Id: lwp_elf.h,v 1.1 2000/09/11 14:41:08 art Exp $
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
