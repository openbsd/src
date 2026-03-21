/*
 * cpuset.h -- CPU affinity.
 *
 * Copyright (c) 2020, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
#ifndef CPUSET_H
#define CPUSET_H

#ifdef HAVE_SCHED_H
# include <sched.h>
#endif

#ifdef HAVE_SYS_CPUSET_H
# include <sys/cpuset.h>
#endif

/*
 * CPU affinity is currently only supported on Linux and FreeBSD. Other
 * operating systems may be supported in the future, but not all operating
 * systems offer the same functionality. OpenBSD for example does not support
 * any kind of CPU affinity, while Solaris offers specifying a set of
 * processors, but a processor can only be part of a single set.
 *
 * NOTE: On macOS Mojave, processor_set_create returned KERN_FAILURE which
 *       indicates processor allocation is not supported by the operating
 *       system.
 */

#ifndef HAVE_CPUSET_T
#ifdef HAVE_CPU_SET_T
#define HAVE_CPUSET_T 1
typedef cpu_set_t cpuset_t;
#endif
#endif

#ifndef HAVE_CPUID_T
#ifdef __linux__
typedef int cpuid_t;
#elif defined(__FreeBSD__) || defined(__gnu_hurd__) || defined(__DragonFly__)
typedef size_t cpuid_t;
#else
typedef size_t cpuid_t;
#endif
#endif

#ifndef HAVE_CPUSET_CREATE
cpuset_t *cpuset_create(void);
#endif

#ifndef HAVE_CPUSET_DESTROY
void cpuset_destroy(cpuset_t *set);
#endif

#ifndef HAVE_CPUSET_ZERO
void cpuset_zero(cpuset_t *set);
#endif

#ifndef HAVE_CPUSET_SET
int cpuset_set(cpuid_t cpu, cpuset_t *set);
#endif

#ifndef HAVE_CPUSET_CLR
int cpuset_clr(cpuid_t cpu, cpuset_t *set);
#endif

#ifndef HAVE_CPUSET_ISSET
int cpuset_isset(cpuid_t cpu, const cpuset_t *set);
#endif

#ifndef HAVE_CPUSET_SIZE
size_t cpuset_size(const cpuset_t *set);
#endif

void cpuset_or(cpuset_t *destset, const cpuset_t *srcset);

#endif /* CPUSET_H */
