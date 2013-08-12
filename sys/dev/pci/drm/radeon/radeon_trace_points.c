/*	$OpenBSD: radeon_trace_points.c,v 1.1 2013/08/12 04:11:53 jsg Exp $	*/

/* Copyright Red Hat Inc 2010.
 * Author : Dave Airlie <airlied@redhat.com>
 */
#include <dev/pci/drm/drmP.h>
#include <dev/pci/drm/radeon_drm.h>
#include "radeon.h"

#define CREATE_TRACE_POINTS
#include "radeon_trace.h"
