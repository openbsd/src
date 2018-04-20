/*	$OpenBSD: radeon_trace_points.c,v 1.3 2018/04/20 16:09:37 deraadt Exp $	*/

/* Copyright Red Hat Inc 2010.
 * Author : Dave Airlie <airlied@redhat.com>
 */
#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/radeon_drm.h>
#include "radeon.h"

#define CREATE_TRACE_POINTS
#include "radeon_trace.h"
