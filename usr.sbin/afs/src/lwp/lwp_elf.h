/*
 * For common usage of elf platforms
 *
 * $Id: lwp_elf.h,v 1.2 2002/02/19 19:39:39 millert Exp $
 */

#ifndef _C_LABEL
#if !defined(SYSV) && !defined(__ELF__) && !defined(AFS_SUN5_ENV)
#define _C_LABEL(name)     _##name
#else /* SYSV || __ELF__ || AFS_SUN5_ENV */
#define _C_LABEL(name)  name
#endif
#endif /* _C_LABEL */

#ifndef ENTRY
#if !defined(SYSV) && !defined(__ELF__) && !defined(AFS_SUN5_ENV)
#define ENTRY(name)    _##name##:
#else /* SYSV || __ELF__ || AFS_SUN5_ENV */
#define ENTRY(name)     name:
#endif
#endif /* _C_LABEL */
