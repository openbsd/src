/* Public domain. */

#ifndef _LINUX_KCONFIG_H
#define _LINUX_KCONFIG_H

#include "agp.h"

#include <sys/endian.h>

#define IS_ENABLED(x) x - 0
#define IS_BUILTIN(x) 1

#define CONFIG_DRM_FBDEV_OVERALLOC		0
#define CONFIG_DRM_I915_DEBUG			0
#define CONFIG_DRM_I915_DEBUG_GEM		0
#define CONFIG_DRM_I915_FBDEV			1
#define CONFIG_DRM_I915_ALPHA_SUPPORT		0
#define CONFIG_DRM_I915_CAPTURE_ERROR		1
#define CONFIG_DRM_I915_GVT			0
#define CONFIG_DRM_I915_SW_FENCE_CHECK_DAG	0
#define CONFIG_PM				0
#define CONFIG_DRM_AMD_DC			1
#define CONFIG_DRM_AMD_DC_DCN1_0		1
#if 0
#define CONFIG_DRM_AMDGPU_CIK			1
#define CONFIG_DRM_AMDGPU_SI			1
#endif

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN
#else
#define __LITTLE_ENDIAN
#endif

#if NAGP > 0
#define CONFIG_AGP				1
#endif

#endif
