/*
 *  ap_config_auto.h -- Automatically determined configuration stuff
 *  THIS FILE WAS AUTOMATICALLY GENERATED - DO NOT EDIT!
 */

#ifndef AP_CONFIG_AUTO_H
#define AP_CONFIG_AUTO_H

/* check: #include <dlfcn.h> */
#ifndef HAVE_DLFCN_H
#define HAVE_DLFCN_H 1
#endif

/* check: #include <dl.h> */
#ifdef HAVE_DL_H
#undef HAVE_DL_H
#endif

/* check: #include <bstring.h> */
#ifdef HAVE_BSTRING_H
#undef HAVE_BSTRING_H
#endif

/* check: #include <crypt.h> */
#ifdef HAVE_CRYPT_H
#undef HAVE_CRYPT_H
#endif

/* check: #include <unistd.h> */
#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H 1
#endif

/* check: #include <sys/resource.h> */
#ifndef HAVE_SYS_RESOURCE_H
#define HAVE_SYS_RESOURCE_H 1
#endif

/* check: #include <sys/select.h> */
#ifndef HAVE_SYS_SELECT_H
#define HAVE_SYS_SELECT_H 1
#endif

/* check: #include <sys/processor.h> */
#ifdef HAVE_SYS_PROCESSOR_H
#undef HAVE_SYS_PROCESSOR_H
#endif

/* check: #include <sys/param.h> */
#ifndef HAVE_SYS_PARAM_H
#define HAVE_SYS_PARAM_H 1
#endif

/* determine: isinf() found in libc */ 
#ifndef HAVE_ISINF
#define HAVE_ISINF 1
#endif

/* determine: isnan() found in libc */ 
#ifndef HAVE_ISNAN
#define HAVE_ISNAN 1
#endif

/* sizeof(off_t) == sizeof(quad_t) on OpenBSD */
#ifndef AP_OFF_T_IS_QUAD
#define AP_OFF_T_IS_QUAD 1
#endif

/* build flag: -DINET6 */
#ifndef INET6
#define INET6 1
#endif

/* build flag: -Dss_family=__ss_family */
#ifndef ss_family
#define ss_family __ss_family
#endif

/* build flag: -Dss_len=__ss_len */
#ifndef ss_len
#define ss_len __ss_len
#endif

/* build flag: -DHAVE_SOCKADDR_LEN */
#ifndef HAVE_SOCKADDR_LEN
#define HAVE_SOCKADDR_LEN 1
#endif

/* build flag: -DMOD_SSL=208116 */
#ifndef MOD_SSL
#define MOD_SSL 208116
#endif

/* build flag: -DEAPI */
#ifndef EAPI
#define EAPI 1
#endif

#endif /* AP_CONFIG_AUTO_H */
