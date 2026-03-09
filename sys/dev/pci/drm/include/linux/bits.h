/* from i915_reg_defs.h */
/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Intel Corporation
 */

#ifndef _LINUX_BITS_H
#define _LINUX_BITS_H

/**
 * BIT_U32() - Prepare a u32 bit value
 * @__n: 0-based bit number
 *
 * Local wrapper for BIT() to force u32, with compile time checks.
 *
 * @return: Value with bit @__n set.
 */
#define BIT_U32(__n)							\
	((u32)(BIT(__n) +						\
	       BUILD_BUG_ON_ZERO(__is_constexpr(__n) &&		\
				 ((__n) < 0 || (__n) > 31))))

/**
 * BIT_U8() - Prepare a u8 bit value
 * @__n: 0-based bit number
 *
 * Local wrapper for BIT() to force u8, with compile time checks.
 *
 * @return: Value with bit @__n set.
 */
#define BIT_U8(__n)                                                   \
	((u8)(BIT(__n) +                                                \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__n) &&         \
				 ((__n) < 0 || (__n) > 7))))

/**
 * GENMASK_U32() - Prepare a continuous u32 bitmask
 * @__high: 0-based high bit
 * @__low: 0-based low bit
 *
 * Local wrapper for GENMASK() to force u32, with compile time checks.
 *
 * @return: Continuous bitmask from @__high to @__low, inclusive.
 */
#define GENMASK_U32(__high, __low)					\
	((u32)(GENMASK(__high, __low) +					\
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&	\
				 __is_constexpr(__low) &&		\
				 ((__low) < 0 || (__high) > 31 || (__low) > (__high)))))

/**
 * GENMASK_U64() - Prepare a continuous u64 bitmask
 * @__high: 0-based high bit
 * @__low: 0-based low bit
 *
 * Local wrapper for GENMASK_ULL() to force u64, with compile time checks.
 *
 * @return: Continuous bitmask from @__high to @__low, inclusive.
 */
#define GENMASK_U64(__high, __low)					\
	((u64)(GENMASK_ULL(__high, __low) +				\
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&		\
				 __is_constexpr(__low) &&		\
				 ((__low) < 0 || (__high) > 63 || (__low) > (__high)))))

/**
 * GENMASK_U8() - Prepare a continuous u8 bitmask
 * @__high: 0-based high bit
 * @__low: 0-based low bit
 *
 * Local wrapper for GENMASK() to force u8, with compile time checks.
 *
 * @return: Continuous bitmask from @__high to @__low, inclusive.
 */
#define GENMASK_U8(__high, __low)                                     \
	((u8)(GENMASK(__high, __low) +                                  \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&      \
				 __is_constexpr(__low) &&               \
				 ((__low) < 0 || (__high) > 7 || (__low) > (__high)))))

/**
 * BIT_U16() - Prepare a u16 bit value
 * @__n: 0-based bit number
 *
 * Local wrapper for BIT() to force u16, with compile time
 * checks.
 *
 * @return: Value with bit @__n set.
 */
#define BIT_U16(__n)                                                   \
	((u16)(BIT(__n) +                                                \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__n) &&         \
				 ((__n) < 0 || (__n) > 15))))

/**
 * GENMASK_U16() - Prepare a continuous u8 bitmask
 * @__high: 0-based high bit
 * @__low: 0-based low bit
 *
 * Local wrapper for GENMASK() to force u16, with compile time
 * checks.
 *
 * @return: Continuous bitmask from @__high to @__low, inclusive.
 */
#define GENMASK_U16(__high, __low)                                     \
	((u16)(GENMASK(__high, __low) +                                  \
	       BUILD_BUG_ON_ZERO(__is_constexpr(__high) &&      \
				 __is_constexpr(__low) &&               \
				 ((__low) < 0 || (__high) > 15 || (__low) > (__high)))))

#endif
