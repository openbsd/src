/* $OpenBSD: compat.h,v 1.2 2005/03/28 23:26:51 niallo Exp $ */

/*
 *  compat.h -- backward compatibility header for ap_compat.h
 */

#ifdef __GNUC__
#warning "This header is obsolete, use ap_compat.h instead"
#endif

#include "ap_compat.h"
