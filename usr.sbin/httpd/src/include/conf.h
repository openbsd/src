/* $OpenBSD: conf.h,v 1.2 2005/03/28 23:26:51 niallo Exp $ */

/*
 *  conf.h -- backward compatibility header for ap_config.h
 */

#ifdef __GNUC__
#warning "This header is obsolete, use ap_config.h instead"
#endif

#include "ap_config.h"
