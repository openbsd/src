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

/* build flag: -DMOD_SSL=202103 */
#ifndef MOD_SSL
#define MOD_SSL 202103
#endif

/* build flag: -DEAPI */
#ifndef EAPI
#define EAPI 1
#endif

#endif /* AP_CONFIG_AUTO_H */
