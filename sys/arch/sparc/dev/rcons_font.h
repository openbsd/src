/* $NetBSD: rcons_font.h,v 1.3 1995/11/29 22:03:53 pk Exp $ */

/*
 * Raster Console font definition; this file exports `console_font',
 * which is either a small one (`fixed'), or a PROM compatible
 * largr one (`gallant19').
 */

#ifdef RASTERCONS_SMALLFONT

static u_int32_t font_fixed_0_pixels[] = {
	0,0xf0000000,0xf0000000,0xf0000000,0xf0000000,0xf0000000,0xf0000000,0xf0000000,0xf0000000,0xf0000000,0,0,0
};
static struct raster font_fixed_0 = { 6, 13, 1, 1, font_fixed_0_pixels, 0 };

static u_int32_t font_fixed_1_pixels[] = {
	0,0,0,0,0,0x20000000,0x70000000,0xf8000000,0x70000000,0x20000000,0,0,0
};
static struct raster font_fixed_1 = { 6, 13, 1, 1, font_fixed_1_pixels, 0 };

static u_int32_t font_fixed_2_pixels[] = {
	0,0,0,0,0xa8000000,0x50000000,0xa8000000,0x50000000,0xa8000000,0x50000000,0xa8000000,0,0
};
static struct raster font_fixed_2 = { 6, 13, 1, 1, font_fixed_2_pixels, 0 };

static u_int32_t font_fixed_3_pixels[] = {
	0,0,0,0,0xa0000000,0xa0000000,0xe0000000,0xa0000000,0xa0000000,0x70000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_3 = { 6, 13, 1, 1, font_fixed_3_pixels, 0 };

static u_int32_t font_fixed_4_pixels[] = {
	0,0,0,0,0xe0000000,0x80000000,0xc0000000,0x80000000,0xf0000000,0x40000000,0x60000000,0x40000000,0x40000000
};
static struct raster font_fixed_4 = { 6, 13, 1, 1, font_fixed_4_pixels, 0 };

static u_int32_t font_fixed_5_pixels[] = {
	0,0,0,0,0x70000000,0x80000000,0x80000000,0x70000000,0x70000000,0x48000000,0x70000000,0x50000000,0x48000000
};
static struct raster font_fixed_5 = { 6, 13, 1, 1, font_fixed_5_pixels, 0 };

static u_int32_t font_fixed_6_pixels[] = {
	0,0,0,0,0x80000000,0x80000000,0x80000000,0xe0000000,0x70000000,0x40000000,0x60000000,0x40000000,0x40000000
};
static struct raster font_fixed_6 = { 6, 13, 1, 1, font_fixed_6_pixels, 0 };

static u_int32_t font_fixed_7_pixels[] = {
	0,0,0,0,0x60000000,0x90000000,0x90000000,0x60000000,0,0,0,0,0
};
static struct raster font_fixed_7 = { 6, 13, 1, 1, font_fixed_7_pixels, 0 };

static u_int32_t font_fixed_8_pixels[] = {
	0,0,0,0,0x20000000,0x20000000,0xf8000000,0x20000000,0x20000000,0,0xf8000000,0,0
};
static struct raster font_fixed_8 = { 6, 13, 1, 1, font_fixed_8_pixels, 0 };

static u_int32_t font_fixed_9_pixels[] = {
	0,0,0,0,0x88000000,0xc8000000,0xa8000000,0x98000000,0x88000000,0x40000000,0x40000000,0x40000000,0x78000000
};
static struct raster font_fixed_9 = { 6, 13, 1, 1, font_fixed_9_pixels, 0 };

static u_int32_t font_fixed_10_pixels[] = {
	0,0,0,0,0x88000000,0x88000000,0x50000000,0x20000000,0,0xf8000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_10 = { 6, 13, 1, 1, font_fixed_10_pixels, 0 };

static u_int32_t font_fixed_11_pixels[] = {
	0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0xe0000000,0,0,0,0,0
};
static struct raster font_fixed_11 = { 6, 13, 1, 1, font_fixed_11_pixels, 0 };

static u_int32_t font_fixed_12_pixels[] = {
	0,0,0,0,0,0,0,0xe0000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_12 = { 6, 13, 1, 1, font_fixed_12_pixels, 0 };

static u_int32_t font_fixed_13_pixels[] = {
	0,0,0,0,0,0,0,0x3c000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_13 = { 6, 13, 1, 1, font_fixed_13_pixels, 0 };

static u_int32_t font_fixed_14_pixels[] = {
	0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x3c000000,0,0,0,0,0
};
static struct raster font_fixed_14 = { 6, 13, 1, 1, font_fixed_14_pixels, 0 };

static u_int32_t font_fixed_15_pixels[] = {
	0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0xfc000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_15 = { 6, 13, 1, 1, font_fixed_15_pixels, 0 };

static u_int32_t font_fixed_16_pixels[] = {
	0,0,0,0xfc000000,0,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_16 = { 6, 13, 1, 1, font_fixed_16_pixels, 0 };

static u_int32_t font_fixed_17_pixels[] = {
	0,0,0,0,0,0xfc000000,0,0,0,0,0,0,0
};
static struct raster font_fixed_17 = { 6, 13, 1, 1, font_fixed_17_pixels, 0 };

static u_int32_t font_fixed_18_pixels[] = {
	0,0,0,0,0,0,0,0xfc000000,0,0,0,0,0
};
static struct raster font_fixed_18 = { 6, 13, 1, 1, font_fixed_18_pixels, 0 };

static u_int32_t font_fixed_19_pixels[] = {
	0,0,0,0,0,0,0,0,0,0xfc000000,0,0,0
};
static struct raster font_fixed_19 = { 6, 13, 1, 1, font_fixed_19_pixels, 0 };

static u_int32_t font_fixed_20_pixels[] = {
	0,0,0,0,0,0,0,0,0,0,0,0xfc000000,0
};
static struct raster font_fixed_20 = { 6, 13, 1, 1, font_fixed_20_pixels, 0 };

static u_int32_t font_fixed_21_pixels[] = {
	0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x3c000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_21 = { 6, 13, 1, 1, font_fixed_21_pixels, 0 };

static u_int32_t font_fixed_22_pixels[] = {
	0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0xe0000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_22 = { 6, 13, 1, 1, font_fixed_22_pixels, 0 };

static u_int32_t font_fixed_23_pixels[] = {
	0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0xfc000000,0,0,0,0,0
};
static struct raster font_fixed_23 = { 6, 13, 1, 1, font_fixed_23_pixels, 0 };

static u_int32_t font_fixed_24_pixels[] = {
	0,0,0,0,0,0,0,0xfc000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_24 = { 6, 13, 1, 1, font_fixed_24_pixels, 0 };

static u_int32_t font_fixed_25_pixels[] = {
	0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_25 = { 6, 13, 1, 1, font_fixed_25_pixels, 0 };

static u_int32_t font_fixed_26_pixels[] = {
	0,0,0,0x8000000,0x10000000,0x20000000,0x40000000,0x20000000,0x10000000,0x8000000,0xf8000000,0,0
};
static struct raster font_fixed_26 = { 6, 13, 1, 1, font_fixed_26_pixels, 0 };

static u_int32_t font_fixed_27_pixels[] = {
	0,0,0,0x80000000,0x40000000,0x20000000,0x10000000,0x20000000,0x40000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_27 = { 6, 13, 1, 1, font_fixed_27_pixels, 0 };

static u_int32_t font_fixed_28_pixels[] = {
	0,0,0,0,0,0,0xf8000000,0x50000000,0x50000000,0x50000000,0x90000000,0,0
};
static struct raster font_fixed_28 = { 6, 13, 1, 1, font_fixed_28_pixels, 0 };

static u_int32_t font_fixed_29_pixels[] = {
	0,0,0,0,0,0x8000000,0xf8000000,0x20000000,0xf8000000,0x80000000,0,0,0
};
static struct raster font_fixed_29 = { 6, 13, 1, 1, font_fixed_29_pixels, 0 };

static u_int32_t font_fixed_30_pixels[] = {
	0,0,0,0,0x30000000,0x48000000,0x40000000,0xf0000000,0x20000000,0xf0000000,0xa8000000,0xe0000000,0
};
static struct raster font_fixed_30 = { 6, 13, 1, 1, font_fixed_30_pixels, 0 };

static u_int32_t font_fixed_31_pixels[] = {
	0,0,0,0,0,0,0,0,0x20000000,0,0,0,0
};
static struct raster font_fixed_31 = { 6, 13, 1, 1, font_fixed_31_pixels, 0 };

static u_int32_t font_fixed_32_pixels[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_32 = { 6, 13, 1, 1, font_fixed_32_pixels, 0 };

static u_int32_t font_fixed_33_pixels[] = {
	0,0,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0,0x20000000,0,0
};
static struct raster font_fixed_33 = { 6, 13, 1, 1, font_fixed_33_pixels, 0 };

static u_int32_t font_fixed_34_pixels[] = {
	0,0,0x50000000,0x50000000,0x50000000,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_34 = { 6, 13, 1, 1, font_fixed_34_pixels, 0 };

static u_int32_t font_fixed_35_pixels[] = {
	0,0,0,0x50000000,0x50000000,0xf8000000,0x50000000,0xf8000000,0x50000000,0x50000000,0,0,0
};
static struct raster font_fixed_35 = { 6, 13, 1, 1, font_fixed_35_pixels, 0 };

static u_int32_t font_fixed_36_pixels[] = {
	0,0x20000000,0x70000000,0xa0000000,0xa0000000,0x70000000,0x28000000,0x28000000,0x70000000,0x20000000,0,0,0
};
static struct raster font_fixed_36 = { 6, 13, 1, 1, font_fixed_36_pixels, 0 };

static u_int32_t font_fixed_37_pixels[] = {
	0,0,0x48000000,0xa8000000,0x50000000,0x10000000,0x20000000,0x40000000,0x50000000,0xa8000000,0x90000000,0,0
};
static struct raster font_fixed_37 = { 6, 13, 1, 1, font_fixed_37_pixels, 0 };

static u_int32_t font_fixed_38_pixels[] = {
	0,0,0x40000000,0xa0000000,0xa0000000,0x40000000,0xa0000000,0x98000000,0x90000000,0x68000000,0,0,0
};
static struct raster font_fixed_38 = { 6, 13, 1, 1, font_fixed_38_pixels, 0 };

static u_int32_t font_fixed_39_pixels[] = {
	0,0,0x30000000,0x20000000,0x40000000,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_39 = { 6, 13, 1, 1, font_fixed_39_pixels, 0 };

static u_int32_t font_fixed_40_pixels[] = {
	0,0,0x10000000,0x20000000,0x20000000,0x40000000,0x40000000,0x40000000,0x20000000,0x20000000,0x10000000,0,0
};
static struct raster font_fixed_40 = { 6, 13, 1, 1, font_fixed_40_pixels, 0 };

static u_int32_t font_fixed_41_pixels[] = {
	0,0,0x40000000,0x20000000,0x20000000,0x10000000,0x10000000,0x10000000,0x20000000,0x20000000,0x40000000,0,0
};
static struct raster font_fixed_41 = { 6, 13, 1, 1, font_fixed_41_pixels, 0 };

static u_int32_t font_fixed_42_pixels[] = {
	0,0,0,0x20000000,0xa8000000,0xf8000000,0x70000000,0xf8000000,0xa8000000,0x20000000,0,0,0
};
static struct raster font_fixed_42 = { 6, 13, 1, 1, font_fixed_42_pixels, 0 };

static u_int32_t font_fixed_43_pixels[] = {
	0,0,0,0,0x20000000,0x20000000,0xf8000000,0x20000000,0x20000000,0,0,0,0
};
static struct raster font_fixed_43 = { 6, 13, 1, 1, font_fixed_43_pixels, 0 };

static u_int32_t font_fixed_44_pixels[] = {
	0,0,0,0,0,0,0,0,0,0x30000000,0x20000000,0x40000000,0
};
static struct raster font_fixed_44 = { 6, 13, 1, 1, font_fixed_44_pixels, 0 };

static u_int32_t font_fixed_45_pixels[] = {
	0,0,0,0,0,0,0xf8000000,0,0,0,0,0,0
};
static struct raster font_fixed_45 = { 6, 13, 1, 1, font_fixed_45_pixels, 0 };

static u_int32_t font_fixed_46_pixels[] = {
	0,0,0,0,0,0,0,0,0,0x20000000,0x70000000,0x20000000,0
};
static struct raster font_fixed_46 = { 6, 13, 1, 1, font_fixed_46_pixels, 0 };

static u_int32_t font_fixed_47_pixels[] = {
	0,0,0x8000000,0x8000000,0x10000000,0x10000000,0x20000000,0x40000000,0x40000000,0x80000000,0x80000000,0,0
};
static struct raster font_fixed_47 = { 6, 13, 1, 1, font_fixed_47_pixels, 0 };

static u_int32_t font_fixed_48_pixels[] = {
	0,0,0x20000000,0x50000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x50000000,0x20000000,0,0
};
static struct raster font_fixed_48 = { 6, 13, 1, 1, font_fixed_48_pixels, 0 };

static u_int32_t font_fixed_49_pixels[] = {
	0,0,0x20000000,0x60000000,0xa0000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0xf8000000,0,0
};
static struct raster font_fixed_49 = { 6, 13, 1, 1, font_fixed_49_pixels, 0 };

static u_int32_t font_fixed_50_pixels[] = {
	0,0,0x70000000,0x88000000,0x88000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_50 = { 6, 13, 1, 1, font_fixed_50_pixels, 0 };

static u_int32_t font_fixed_51_pixels[] = {
	0,0,0xf8000000,0x8000000,0x10000000,0x20000000,0x70000000,0x8000000,0x8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_51 = { 6, 13, 1, 1, font_fixed_51_pixels, 0 };

static u_int32_t font_fixed_52_pixels[] = {
	0,0,0x10000000,0x10000000,0x30000000,0x50000000,0x50000000,0x90000000,0xf8000000,0x10000000,0x10000000,0,0
};
static struct raster font_fixed_52 = { 6, 13, 1, 1, font_fixed_52_pixels, 0 };

static u_int32_t font_fixed_53_pixels[] = {
	0,0,0xf8000000,0x80000000,0x80000000,0xb0000000,0xc8000000,0x8000000,0x8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_53 = { 6, 13, 1, 1, font_fixed_53_pixels, 0 };

static u_int32_t font_fixed_54_pixels[] = {
	0,0,0x70000000,0x88000000,0x80000000,0x80000000,0xf0000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_54 = { 6, 13, 1, 1, font_fixed_54_pixels, 0 };

static u_int32_t font_fixed_55_pixels[] = {
	0,0,0xf8000000,0x8000000,0x10000000,0x10000000,0x20000000,0x20000000,0x40000000,0x40000000,0x40000000,0,0
};
static struct raster font_fixed_55 = { 6, 13, 1, 1, font_fixed_55_pixels, 0 };

static u_int32_t font_fixed_56_pixels[] = {
	0,0,0x70000000,0x88000000,0x88000000,0x88000000,0x70000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_56 = { 6, 13, 1, 1, font_fixed_56_pixels, 0 };

static u_int32_t font_fixed_57_pixels[] = {
	0,0,0x70000000,0x88000000,0x88000000,0x88000000,0x78000000,0x8000000,0x8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_57 = { 6, 13, 1, 1, font_fixed_57_pixels, 0 };

static u_int32_t font_fixed_58_pixels[] = {
	0,0,0,0,0x20000000,0x70000000,0x20000000,0,0,0x20000000,0x70000000,0x20000000,0
};
static struct raster font_fixed_58 = { 6, 13, 1, 1, font_fixed_58_pixels, 0 };

static u_int32_t font_fixed_59_pixels[] = {
	0,0,0,0,0x20000000,0x70000000,0x20000000,0,0,0x30000000,0x20000000,0x40000000,0
};
static struct raster font_fixed_59 = { 6, 13, 1, 1, font_fixed_59_pixels, 0 };

static u_int32_t font_fixed_60_pixels[] = {
	0,0,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0x40000000,0x20000000,0x10000000,0x8000000,0,0
};
static struct raster font_fixed_60 = { 6, 13, 1, 1, font_fixed_60_pixels, 0 };

static u_int32_t font_fixed_61_pixels[] = {
	0,0,0,0,0,0xf8000000,0,0,0xf8000000,0,0,0,0
};
static struct raster font_fixed_61 = { 6, 13, 1, 1, font_fixed_61_pixels, 0 };

static u_int32_t font_fixed_62_pixels[] = {
	0,0,0x80000000,0x40000000,0x20000000,0x10000000,0x8000000,0x10000000,0x20000000,0x40000000,0x80000000,0,0
};
static struct raster font_fixed_62 = { 6, 13, 1, 1, font_fixed_62_pixels, 0 };

static u_int32_t font_fixed_63_pixels[] = {
	0,0,0x70000000,0x88000000,0x88000000,0x8000000,0x10000000,0x20000000,0x20000000,0,0x20000000,0,0
};
static struct raster font_fixed_63 = { 6, 13, 1, 1, font_fixed_63_pixels, 0 };

static u_int32_t font_fixed_64_pixels[] = {
	0,0,0x70000000,0x88000000,0x88000000,0x98000000,0xa8000000,0xa8000000,0xb0000000,0x80000000,0x78000000,0,0
};
static struct raster font_fixed_64 = { 6, 13, 1, 1, font_fixed_64_pixels, 0 };

static u_int32_t font_fixed_65_pixels[] = {
	0,0,0x20000000,0x50000000,0x88000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_65 = { 6, 13, 1, 1, font_fixed_65_pixels, 0 };

static u_int32_t font_fixed_66_pixels[] = {
	0,0,0xf0000000,0x48000000,0x48000000,0x48000000,0x70000000,0x48000000,0x48000000,0x48000000,0xf0000000,0,0
};
static struct raster font_fixed_66 = { 6, 13, 1, 1, font_fixed_66_pixels, 0 };

static u_int32_t font_fixed_67_pixels[] = {
	0,0,0x70000000,0x88000000,0x80000000,0x80000000,0x80000000,0x80000000,0x80000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_67 = { 6, 13, 1, 1, font_fixed_67_pixels, 0 };

static u_int32_t font_fixed_68_pixels[] = {
	0,0,0xf0000000,0x48000000,0x48000000,0x48000000,0x48000000,0x48000000,0x48000000,0x48000000,0xf0000000,0,0
};
static struct raster font_fixed_68 = { 6, 13, 1, 1, font_fixed_68_pixels, 0 };

static u_int32_t font_fixed_69_pixels[] = {
	0,0,0xf8000000,0x80000000,0x80000000,0x80000000,0xf0000000,0x80000000,0x80000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_69 = { 6, 13, 1, 1, font_fixed_69_pixels, 0 };

static u_int32_t font_fixed_70_pixels[] = {
	0,0,0xf8000000,0x80000000,0x80000000,0x80000000,0xf0000000,0x80000000,0x80000000,0x80000000,0x80000000,0,0
};
static struct raster font_fixed_70 = { 6, 13, 1, 1, font_fixed_70_pixels, 0 };

static u_int32_t font_fixed_71_pixels[] = {
	0,0,0x70000000,0x88000000,0x80000000,0x80000000,0x80000000,0x98000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_71 = { 6, 13, 1, 1, font_fixed_71_pixels, 0 };

static u_int32_t font_fixed_72_pixels[] = {
	0,0,0x88000000,0x88000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_72 = { 6, 13, 1, 1, font_fixed_72_pixels, 0 };

static u_int32_t font_fixed_73_pixels[] = {
	0,0,0x70000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_73 = { 6, 13, 1, 1, font_fixed_73_pixels, 0 };

static u_int32_t font_fixed_74_pixels[] = {
	0,0,0x38000000,0x10000000,0x10000000,0x10000000,0x10000000,0x10000000,0x10000000,0x90000000,0x60000000,0,0
};
static struct raster font_fixed_74 = { 6, 13, 1, 1, font_fixed_74_pixels, 0 };

static u_int32_t font_fixed_75_pixels[] = {
	0,0,0x88000000,0x88000000,0x90000000,0xa0000000,0xc0000000,0xa0000000,0x90000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_75 = { 6, 13, 1, 1, font_fixed_75_pixels, 0 };

static u_int32_t font_fixed_76_pixels[] = {
	0,0,0x80000000,0x80000000,0x80000000,0x80000000,0x80000000,0x80000000,0x80000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_76 = { 6, 13, 1, 1, font_fixed_76_pixels, 0 };

static u_int32_t font_fixed_77_pixels[] = {
	0,0,0x88000000,0x88000000,0xd8000000,0xa8000000,0xa8000000,0x88000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_77 = { 6, 13, 1, 1, font_fixed_77_pixels, 0 };

static u_int32_t font_fixed_78_pixels[] = {
	0,0,0x88000000,0xc8000000,0xc8000000,0xa8000000,0xa8000000,0x98000000,0x98000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_78 = { 6, 13, 1, 1, font_fixed_78_pixels, 0 };

static u_int32_t font_fixed_79_pixels[] = {
	0,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_79 = { 6, 13, 1, 1, font_fixed_79_pixels, 0 };

static u_int32_t font_fixed_80_pixels[] = {
	0,0,0xf0000000,0x88000000,0x88000000,0x88000000,0xf0000000,0x80000000,0x80000000,0x80000000,0x80000000,0,0
};
static struct raster font_fixed_80 = { 6, 13, 1, 1, font_fixed_80_pixels, 0 };

static u_int32_t font_fixed_81_pixels[] = {
	0,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0xa8000000,0x70000000,0x8000000,0
};
static struct raster font_fixed_81 = { 6, 13, 1, 1, font_fixed_81_pixels, 0 };

static u_int32_t font_fixed_82_pixels[] = {
	0,0,0xf0000000,0x88000000,0x88000000,0x88000000,0xf0000000,0xa0000000,0x90000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_82 = { 6, 13, 1, 1, font_fixed_82_pixels, 0 };

static u_int32_t font_fixed_83_pixels[] = {
	0,0,0x70000000,0x88000000,0x80000000,0x80000000,0x70000000,0x8000000,0x8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_83 = { 6, 13, 1, 1, font_fixed_83_pixels, 0 };

static u_int32_t font_fixed_84_pixels[] = {
	0,0,0xf8000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0,0
};
static struct raster font_fixed_84 = { 6, 13, 1, 1, font_fixed_84_pixels, 0 };

static u_int32_t font_fixed_85_pixels[] = {
	0,0,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_85 = { 6, 13, 1, 1, font_fixed_85_pixels, 0 };

static u_int32_t font_fixed_86_pixels[] = {
	0,0,0x88000000,0x88000000,0x88000000,0x88000000,0x50000000,0x50000000,0x50000000,0x20000000,0x20000000,0,0
};
static struct raster font_fixed_86 = { 6, 13, 1, 1, font_fixed_86_pixels, 0 };

static u_int32_t font_fixed_87_pixels[] = {
	0,0,0x88000000,0x88000000,0x88000000,0x88000000,0xa8000000,0xa8000000,0xa8000000,0xd8000000,0x88000000,0,0
};
static struct raster font_fixed_87 = { 6, 13, 1, 1, font_fixed_87_pixels, 0 };

static u_int32_t font_fixed_88_pixels[] = {
	0,0,0x88000000,0x88000000,0x50000000,0x50000000,0x20000000,0x50000000,0x50000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_88 = { 6, 13, 1, 1, font_fixed_88_pixels, 0 };

static u_int32_t font_fixed_89_pixels[] = {
	0,0,0x88000000,0x88000000,0x50000000,0x50000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0,0
};
static struct raster font_fixed_89 = { 6, 13, 1, 1, font_fixed_89_pixels, 0 };

static u_int32_t font_fixed_90_pixels[] = {
	0,0,0xf8000000,0x8000000,0x10000000,0x10000000,0x20000000,0x40000000,0x40000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_90 = { 6, 13, 1, 1, font_fixed_90_pixels, 0 };

static u_int32_t font_fixed_91_pixels[] = {
	0,0,0x70000000,0x40000000,0x40000000,0x40000000,0x40000000,0x40000000,0x40000000,0x40000000,0x70000000,0,0
};
static struct raster font_fixed_91 = { 6, 13, 1, 1, font_fixed_91_pixels, 0 };

static u_int32_t font_fixed_92_pixels[] = {
	0,0,0x80000000,0x80000000,0x40000000,0x40000000,0x20000000,0x10000000,0x10000000,0x8000000,0x8000000,0,0
};
static struct raster font_fixed_92 = { 6, 13, 1, 1, font_fixed_92_pixels, 0 };

static u_int32_t font_fixed_93_pixels[] = {
	0,0,0x70000000,0x10000000,0x10000000,0x10000000,0x10000000,0x10000000,0x10000000,0x10000000,0x70000000,0,0
};
static struct raster font_fixed_93 = { 6, 13, 1, 1, font_fixed_93_pixels, 0 };

static u_int32_t font_fixed_94_pixels[] = {
	0,0,0x20000000,0x50000000,0x88000000,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_94 = { 6, 13, 1, 1, font_fixed_94_pixels, 0 };

static u_int32_t font_fixed_95_pixels[] = {
	0,0,0,0,0,0,0,0,0,0,0,0xf8000000,0
};
static struct raster font_fixed_95 = { 6, 13, 1, 1, font_fixed_95_pixels, 0 };

static u_int32_t font_fixed_96_pixels[] = {
	0,0,0x30000000,0x10000000,0x8000000,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_96 = { 6, 13, 1, 1, font_fixed_96_pixels, 0 };

static u_int32_t font_fixed_97_pixels[] = {
	0,0,0,0,0,0x70000000,0x8000000,0x78000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_97 = { 6, 13, 1, 1, font_fixed_97_pixels, 0 };

static u_int32_t font_fixed_98_pixels[] = {
	0,0,0x80000000,0x80000000,0x80000000,0xf0000000,0x88000000,0x88000000,0x88000000,0x88000000,0xf0000000,0,0
};
static struct raster font_fixed_98 = { 6, 13, 1, 1, font_fixed_98_pixels, 0 };

static u_int32_t font_fixed_99_pixels[] = {
	0,0,0,0,0,0x70000000,0x88000000,0x80000000,0x80000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_99 = { 6, 13, 1, 1, font_fixed_99_pixels, 0 };

static u_int32_t font_fixed_100_pixels[] = {
	0,0,0x8000000,0x8000000,0x8000000,0x78000000,0x88000000,0x88000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_100 = { 6, 13, 1, 1, font_fixed_100_pixels, 0 };

static u_int32_t font_fixed_101_pixels[] = {
	0,0,0,0,0,0x70000000,0x88000000,0xf8000000,0x80000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_101 = { 6, 13, 1, 1, font_fixed_101_pixels, 0 };

static u_int32_t font_fixed_102_pixels[] = {
	0,0,0x30000000,0x48000000,0x40000000,0x40000000,0xf0000000,0x40000000,0x40000000,0x40000000,0x40000000,0,0
};
static struct raster font_fixed_102 = { 6, 13, 1, 1, font_fixed_102_pixels, 0 };

static u_int32_t font_fixed_103_pixels[] = {
	0,0,0,0,0,0x70000000,0x88000000,0x88000000,0x88000000,0x78000000,0x8000000,0x88000000,0x70000000
};
static struct raster font_fixed_103 = { 6, 13, 1, 1, font_fixed_103_pixels, 0 };

static u_int32_t font_fixed_104_pixels[] = {
	0,0,0x80000000,0x80000000,0x80000000,0xb0000000,0xc8000000,0x88000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_104 = { 6, 13, 1, 1, font_fixed_104_pixels, 0 };

static u_int32_t font_fixed_105_pixels[] = {
	0,0,0,0x20000000,0,0x60000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_105 = { 6, 13, 1, 1, font_fixed_105_pixels, 0 };

static u_int32_t font_fixed_106_pixels[] = {
	0,0,0,0x10000000,0,0x30000000,0x10000000,0x10000000,0x10000000,0x10000000,0x90000000,0x90000000,0x60000000
};
static struct raster font_fixed_106 = { 6, 13, 1, 1, font_fixed_106_pixels, 0 };

static u_int32_t font_fixed_107_pixels[] = {
	0,0,0x80000000,0x80000000,0x80000000,0x90000000,0xa0000000,0xc0000000,0xa0000000,0x90000000,0x88000000,0,0
};
static struct raster font_fixed_107 = { 6, 13, 1, 1, font_fixed_107_pixels, 0 };

static u_int32_t font_fixed_108_pixels[] = {
	0,0,0x60000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_108 = { 6, 13, 1, 1, font_fixed_108_pixels, 0 };

static u_int32_t font_fixed_109_pixels[] = {
	0,0,0,0,0,0xd0000000,0xa8000000,0xa8000000,0xa8000000,0xa8000000,0x88000000,0,0
};
static struct raster font_fixed_109 = { 6, 13, 1, 1, font_fixed_109_pixels, 0 };

static u_int32_t font_fixed_110_pixels[] = {
	0,0,0,0,0,0xb0000000,0xc8000000,0x88000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_110 = { 6, 13, 1, 1, font_fixed_110_pixels, 0 };

static u_int32_t font_fixed_111_pixels[] = {
	0,0,0,0,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_111 = { 6, 13, 1, 1, font_fixed_111_pixels, 0 };

static u_int32_t font_fixed_112_pixels[] = {
	0,0,0,0,0,0xf0000000,0x88000000,0x88000000,0x88000000,0xf0000000,0x80000000,0x80000000,0x80000000
};
static struct raster font_fixed_112 = { 6, 13, 1, 1, font_fixed_112_pixels, 0 };

static u_int32_t font_fixed_113_pixels[] = {
	0,0,0,0,0,0x78000000,0x88000000,0x88000000,0x88000000,0x78000000,0x8000000,0x8000000,0x8000000
};
static struct raster font_fixed_113 = { 6, 13, 1, 1, font_fixed_113_pixels, 0 };

static u_int32_t font_fixed_114_pixels[] = {
	0,0,0,0,0,0xb0000000,0xc8000000,0x80000000,0x80000000,0x80000000,0x80000000,0,0
};
static struct raster font_fixed_114 = { 6, 13, 1, 1, font_fixed_114_pixels, 0 };

static u_int32_t font_fixed_115_pixels[] = {
	0,0,0,0,0,0x70000000,0x88000000,0x60000000,0x10000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_115 = { 6, 13, 1, 1, font_fixed_115_pixels, 0 };

static u_int32_t font_fixed_116_pixels[] = {
	0,0,0,0x40000000,0x40000000,0xf0000000,0x40000000,0x40000000,0x40000000,0x48000000,0x30000000,0,0
};
static struct raster font_fixed_116 = { 6, 13, 1, 1, font_fixed_116_pixels, 0 };

static u_int32_t font_fixed_117_pixels[] = {
	0,0,0,0,0,0x88000000,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0,0
};
static struct raster font_fixed_117 = { 6, 13, 1, 1, font_fixed_117_pixels, 0 };

static u_int32_t font_fixed_118_pixels[] = {
	0,0,0,0,0,0x88000000,0x88000000,0x88000000,0x50000000,0x50000000,0x20000000,0,0
};
static struct raster font_fixed_118 = { 6, 13, 1, 1, font_fixed_118_pixels, 0 };

static u_int32_t font_fixed_119_pixels[] = {
	0,0,0,0,0,0x88000000,0x88000000,0xa8000000,0xa8000000,0xa8000000,0x50000000,0,0
};
static struct raster font_fixed_119 = { 6, 13, 1, 1, font_fixed_119_pixels, 0 };

static u_int32_t font_fixed_120_pixels[] = {
	0,0,0,0,0,0x88000000,0x50000000,0x20000000,0x20000000,0x50000000,0x88000000,0,0
};
static struct raster font_fixed_120 = { 6, 13, 1, 1, font_fixed_120_pixels, 0 };

static u_int32_t font_fixed_121_pixels[] = {
	0,0,0,0,0,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0x8000000,0x88000000,0x70000000
};
static struct raster font_fixed_121 = { 6, 13, 1, 1, font_fixed_121_pixels, 0 };

static u_int32_t font_fixed_122_pixels[] = {
	0,0,0,0,0,0xf8000000,0x10000000,0x20000000,0x40000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_122 = { 6, 13, 1, 1, font_fixed_122_pixels, 0 };

static u_int32_t font_fixed_123_pixels[] = {
	0,0,0x18000000,0x20000000,0x20000000,0x20000000,0xc0000000,0x20000000,0x20000000,0x20000000,0x18000000,0,0
};
static struct raster font_fixed_123 = { 6, 13, 1, 1, font_fixed_123_pixels, 0 };

static u_int32_t font_fixed_124_pixels[] = {
	0,0,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0,0
};
static struct raster font_fixed_124 = { 6, 13, 1, 1, font_fixed_124_pixels, 0 };

static u_int32_t font_fixed_125_pixels[] = {
	0,0,0xc0000000,0x20000000,0x20000000,0x20000000,0x18000000,0x20000000,0x20000000,0x20000000,0xc0000000,0,0
};
static struct raster font_fixed_125 = { 6, 13, 1, 1, font_fixed_125_pixels, 0 };

static u_int32_t font_fixed_126_pixels[] = {
	0,0,0x48000000,0xa8000000,0x90000000,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_126 = { 6, 13, 1, 1, font_fixed_126_pixels, 0 };

static u_int32_t font_fixed_127_pixels[] = {
	0xa8000000,0x50000000,0xa8000000,0x50000000,0xa8000000,0x50000000,0xa8000000,0x50000000,0xa8000000,0x50000000,0xa8000000,0x50000000,0xa8000000
};
static struct raster font_fixed_127 = { 6, 13, 1, 1, font_fixed_127_pixels, 0 };

static u_int32_t font_fixed_160_pixels[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_160 = { 6, 13, 1, 1, font_fixed_160_pixels, 0 };

static u_int32_t font_fixed_161_pixels[] = {
	0,0,0,0,0x20000000,0,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000
};
static struct raster font_fixed_161 = { 6, 13, 1, 1, font_fixed_161_pixels, 0 };

static u_int32_t font_fixed_162_pixels[] = {
	0,0,0x20000000,0x20000000,0x70000000,0x88000000,0x80000000,0x88000000,0x70000000,0x20000000,0x20000000,0,0
};
static struct raster font_fixed_162 = { 6, 13, 1, 1, font_fixed_162_pixels, 0 };

static u_int32_t font_fixed_163_pixels[] = {
	0,0,0x30000000,0x48000000,0x40000000,0x40000000,0xf0000000,0x40000000,0x40000000,0x88000000,0xf0000000,0,0
};
static struct raster font_fixed_163 = { 6, 13, 1, 1, font_fixed_163_pixels, 0 };

static u_int32_t font_fixed_164_pixels[] = {
	0,0,0,0xa8000000,0x50000000,0x88000000,0x88000000,0x50000000,0xa8000000,0,0,0,0
};
static struct raster font_fixed_164 = { 6, 13, 1, 1, font_fixed_164_pixels, 0 };

static u_int32_t font_fixed_165_pixels[] = {
	0,0,0x88000000,0x88000000,0x50000000,0x50000000,0x20000000,0xf8000000,0x20000000,0xf8000000,0x20000000,0,0
};
static struct raster font_fixed_165 = { 6, 13, 1, 1, font_fixed_165_pixels, 0 };

static u_int32_t font_fixed_166_pixels[] = {
	0,0,0x20000000,0x20000000,0x20000000,0x20000000,0,0x20000000,0x20000000,0x20000000,0x20000000,0,0
};
static struct raster font_fixed_166 = { 6, 13, 1, 1, font_fixed_166_pixels, 0 };

static u_int32_t font_fixed_167_pixels[] = {
	0,0,0x70000000,0x88000000,0x80000000,0x70000000,0x88000000,0x88000000,0x70000000,0x8000000,0x88000000,0x70000000,0
};
static struct raster font_fixed_167 = { 6, 13, 1, 1, font_fixed_167_pixels, 0 };

static u_int32_t font_fixed_168_pixels[] = {
	0,0x50000000,0,0,0,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_168 = { 6, 13, 1, 1, font_fixed_168_pixels, 0 };

static u_int32_t font_fixed_169_pixels[] = {
	0,0,0x70000000,0x88000000,0xa8000000,0xd8000000,0xc8000000,0xd8000000,0xa8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_169 = { 6, 13, 1, 1, font_fixed_169_pixels, 0 };

static u_int32_t font_fixed_170_pixels[] = {
	0,0,0,0x60000000,0x90000000,0x70000000,0x90000000,0xe8000000,0,0xf8000000,0,0,0
};
static struct raster font_fixed_170 = { 6, 13, 1, 1, font_fixed_170_pixels, 0 };

static u_int32_t font_fixed_171_pixels[] = {
	0,0,0,0,0,0,0x28000000,0x50000000,0xa0000000,0x50000000,0x28000000,0,0
};
static struct raster font_fixed_171 = { 6, 13, 1, 1, font_fixed_171_pixels, 0 };

static u_int32_t font_fixed_172_pixels[] = {
	0,0,0,0,0,0,0xf8000000,0x8000000,0x8000000,0,0,0,0
};
static struct raster font_fixed_172 = { 6, 13, 1, 1, font_fixed_172_pixels, 0 };

static u_int32_t font_fixed_173_pixels[] = {
	0,0,0,0,0,0,0x78000000,0,0,0,0,0,0
};
static struct raster font_fixed_173 = { 6, 13, 1, 1, font_fixed_173_pixels, 0 };

static u_int32_t font_fixed_174_pixels[] = {
	0,0,0x70000000,0x88000000,0xe8000000,0xd8000000,0xe8000000,0xd8000000,0xd8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_174 = { 6, 13, 1, 1, font_fixed_174_pixels, 0 };

static u_int32_t font_fixed_175_pixels[] = {
	0,0,0x70000000,0,0,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_175 = { 6, 13, 1, 1, font_fixed_175_pixels, 0 };

static u_int32_t font_fixed_176_pixels[] = {
	0,0,0x60000000,0x90000000,0x90000000,0x60000000,0,0,0,0,0,0,0
};
static struct raster font_fixed_176 = { 6, 13, 1, 1, font_fixed_176_pixels, 0 };

static u_int32_t font_fixed_177_pixels[] = {
	0,0,0,0,0x20000000,0x20000000,0xf8000000,0x20000000,0x20000000,0,0xf8000000,0,0
};
static struct raster font_fixed_177 = { 6, 13, 1, 1, font_fixed_177_pixels, 0 };

static u_int32_t font_fixed_178_pixels[] = {
	0,0x60000000,0x90000000,0x90000000,0x20000000,0x40000000,0xf0000000,0,0,0,0,0,0
};
static struct raster font_fixed_178 = { 6, 13, 1, 1, font_fixed_178_pixels, 0 };

static u_int32_t font_fixed_179_pixels[] = {
	0,0x60000000,0x90000000,0x20000000,0x10000000,0x90000000,0x60000000,0,0,0,0,0,0
};
static struct raster font_fixed_179 = { 6, 13, 1, 1, font_fixed_179_pixels, 0 };

static u_int32_t font_fixed_180_pixels[] = {
	0,0x18000000,0x60000000,0,0,0,0,0,0,0,0,0,0
};
static struct raster font_fixed_180 = { 6, 13, 1, 1, font_fixed_180_pixels, 0 };

static u_int32_t font_fixed_181_pixels[] = {
	0,0,0,0,0,0x88000000,0x88000000,0x88000000,0x88000000,0x98000000,0xe8000000,0x80000000,0x80000000
};
static struct raster font_fixed_181 = { 6, 13, 1, 1, font_fixed_181_pixels, 0 };

static u_int32_t font_fixed_182_pixels[] = {
	0,0x78000000,0xa8000000,0xa8000000,0xa8000000,0x68000000,0x28000000,0x28000000,0x28000000,0x28000000,0x28000000,0,0
};
static struct raster font_fixed_182 = { 6, 13, 1, 1, font_fixed_182_pixels, 0 };

static u_int32_t font_fixed_183_pixels[] = {
	0,0,0,0,0,0x20000000,0x70000000,0x20000000,0,0,0,0,0
};
static struct raster font_fixed_183 = { 6, 13, 1, 1, font_fixed_183_pixels, 0 };

static u_int32_t font_fixed_184_pixels[] = {
	0,0,0,0,0,0,0,0,0,0,0,0x10000000,0x30000000
};
static struct raster font_fixed_184 = { 6, 13, 1, 1, font_fixed_184_pixels, 0 };

static u_int32_t font_fixed_185_pixels[] = {
	0,0x20000000,0x60000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0,0,0,0,0
};
static struct raster font_fixed_185 = { 6, 13, 1, 1, font_fixed_185_pixels, 0 };

static u_int32_t font_fixed_186_pixels[] = {
	0,0,0,0x70000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0xf8000000,0,0,0
};
static struct raster font_fixed_186 = { 6, 13, 1, 1, font_fixed_186_pixels, 0 };

static u_int32_t font_fixed_187_pixels[] = {
	0,0,0,0,0,0,0xa0000000,0x50000000,0x28000000,0x50000000,0xa0000000,0,0
};
static struct raster font_fixed_187 = { 6, 13, 1, 1, font_fixed_187_pixels, 0 };

static u_int32_t font_fixed_188_pixels[] = {
	0,0x40000000,0xc0000000,0x40000000,0x48000000,0xf0000000,0x38000000,0x38000000,0x68000000,0xbc000000,0x8000000,0,0
};
static struct raster font_fixed_188 = { 6, 13, 1, 1, font_fixed_188_pixels, 0 };

static u_int32_t font_fixed_189_pixels[] = {
	0,0x40000000,0xc0000000,0x40000000,0x48000000,0xf0000000,0x38000000,0x24000000,0x48000000,0x90000000,0x1c000000,0,0
};
static struct raster font_fixed_189 = { 6, 13, 1, 1, font_fixed_189_pixels, 0 };

static u_int32_t font_fixed_190_pixels[] = {
	0,0x60000000,0x90000000,0x20000000,0x98000000,0x70000000,0x38000000,0x38000000,0x68000000,0xbc000000,0x8000000,0,0
};
static struct raster font_fixed_190 = { 6, 13, 1, 1, font_fixed_190_pixels, 0 };

static u_int32_t font_fixed_191_pixels[] = {
	0,0,0,0x20000000,0,0x20000000,0x20000000,0x40000000,0x80000000,0x88000000,0x88000000,0x70000000,0
};
static struct raster font_fixed_191 = { 6, 13, 1, 1, font_fixed_191_pixels, 0 };

static u_int32_t font_fixed_192_pixels[] = {
	0xc0000000,0x30000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_192 = { 6, 13, 1, 1, font_fixed_192_pixels, 0 };

static u_int32_t font_fixed_193_pixels[] = {
	0x18000000,0x60000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_193 = { 6, 13, 1, 1, font_fixed_193_pixels, 0 };

static u_int32_t font_fixed_194_pixels[] = {
	0x20000000,0x50000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_194 = { 6, 13, 1, 1, font_fixed_194_pixels, 0 };

static u_int32_t font_fixed_195_pixels[] = {
	0x68000000,0xb0000000,0x20000000,0x50000000,0x88000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_195 = { 6, 13, 1, 1, font_fixed_195_pixels, 0 };

static u_int32_t font_fixed_196_pixels[] = {
	0x50000000,0,0x20000000,0x50000000,0x88000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_196 = { 6, 13, 1, 1, font_fixed_196_pixels, 0 };

static u_int32_t font_fixed_197_pixels[] = {
	0x20000000,0x50000000,0x20000000,0,0x70000000,0x88000000,0x88000000,0xf8000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_197 = { 6, 13, 1, 1, font_fixed_197_pixels, 0 };

static u_int32_t font_fixed_198_pixels[] = {
	0,0,0x3c000000,0x50000000,0x90000000,0x90000000,0x9c000000,0xf0000000,0x90000000,0x90000000,0x9c000000,0,0
};
static struct raster font_fixed_198 = { 6, 13, 1, 1, font_fixed_198_pixels, 0 };

static u_int32_t font_fixed_199_pixels[] = {
	0,0,0x70000000,0x88000000,0x80000000,0x80000000,0x80000000,0x80000000,0x80000000,0x88000000,0x70000000,0x10000000,0x30000000
};
static struct raster font_fixed_199 = { 6, 13, 1, 1, font_fixed_199_pixels, 0 };

static u_int32_t font_fixed_200_pixels[] = {
	0xc0000000,0x30000000,0xf8000000,0x80000000,0x80000000,0x80000000,0xf0000000,0x80000000,0x80000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_200 = { 6, 13, 1, 1, font_fixed_200_pixels, 0 };

static u_int32_t font_fixed_201_pixels[] = {
	0x18000000,0x60000000,0xf8000000,0x80000000,0x80000000,0x80000000,0xf0000000,0x80000000,0x80000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_201 = { 6, 13, 1, 1, font_fixed_201_pixels, 0 };

static u_int32_t font_fixed_202_pixels[] = {
	0x20000000,0x50000000,0xf8000000,0x80000000,0x80000000,0x80000000,0xf0000000,0x80000000,0x80000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_202 = { 6, 13, 1, 1, font_fixed_202_pixels, 0 };

static u_int32_t font_fixed_203_pixels[] = {
	0x50000000,0,0xf8000000,0x80000000,0x80000000,0x80000000,0xf0000000,0x80000000,0x80000000,0x80000000,0xf8000000,0,0
};
static struct raster font_fixed_203 = { 6, 13, 1, 1, font_fixed_203_pixels, 0 };

static u_int32_t font_fixed_204_pixels[] = {
	0xc0000000,0x30000000,0x70000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_204 = { 6, 13, 1, 1, font_fixed_204_pixels, 0 };

static u_int32_t font_fixed_205_pixels[] = {
	0x18000000,0x60000000,0x70000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_205 = { 6, 13, 1, 1, font_fixed_205_pixels, 0 };

static u_int32_t font_fixed_206_pixels[] = {
	0x20000000,0x50000000,0x70000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_206 = { 6, 13, 1, 1, font_fixed_206_pixels, 0 };

static u_int32_t font_fixed_207_pixels[] = {
	0x50000000,0,0x70000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_207 = { 6, 13, 1, 1, font_fixed_207_pixels, 0 };

static u_int32_t font_fixed_208_pixels[] = {
	0,0,0xf0000000,0x48000000,0x48000000,0x48000000,0xe8000000,0x48000000,0x48000000,0x48000000,0xf0000000,0,0
};
static struct raster font_fixed_208 = { 6, 13, 1, 1, font_fixed_208_pixels, 0 };

static u_int32_t font_fixed_209_pixels[] = {
	0x68000000,0xb0000000,0x88000000,0xc8000000,0xc8000000,0xa8000000,0xa8000000,0x98000000,0x98000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_209 = { 6, 13, 1, 1, font_fixed_209_pixels, 0 };

static u_int32_t font_fixed_210_pixels[] = {
	0xc0000000,0x30000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_210 = { 6, 13, 1, 1, font_fixed_210_pixels, 0 };

static u_int32_t font_fixed_211_pixels[] = {
	0x18000000,0x60000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_211 = { 6, 13, 1, 1, font_fixed_211_pixels, 0 };

static u_int32_t font_fixed_212_pixels[] = {
	0x20000000,0x50000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_212 = { 6, 13, 1, 1, font_fixed_212_pixels, 0 };

static u_int32_t font_fixed_213_pixels[] = {
	0x68000000,0xb0000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_213 = { 6, 13, 1, 1, font_fixed_213_pixels, 0 };

static u_int32_t font_fixed_214_pixels[] = {
	0x50000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_214 = { 6, 13, 1, 1, font_fixed_214_pixels, 0 };

static u_int32_t font_fixed_215_pixels[] = {
	0,0,0,0,0x88000000,0x50000000,0x20000000,0x50000000,0x88000000,0,0,0,0
};
static struct raster font_fixed_215 = { 6, 13, 1, 1, font_fixed_215_pixels, 0 };

static u_int32_t font_fixed_216_pixels[] = {
	0,0,0x78000000,0x98000000,0x98000000,0xa8000000,0xa8000000,0xc8000000,0xc8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_216 = { 6, 13, 1, 1, font_fixed_216_pixels, 0 };

static u_int32_t font_fixed_217_pixels[] = {
	0xc0000000,0x30000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_217 = { 6, 13, 1, 1, font_fixed_217_pixels, 0 };

static u_int32_t font_fixed_218_pixels[] = {
	0x18000000,0x60000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_218 = { 6, 13, 1, 1, font_fixed_218_pixels, 0 };

static u_int32_t font_fixed_219_pixels[] = {
	0x20000000,0x50000000,0,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_219 = { 6, 13, 1, 1, font_fixed_219_pixels, 0 };

static u_int32_t font_fixed_220_pixels[] = {
	0x50000000,0,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_220 = { 6, 13, 1, 1, font_fixed_220_pixels, 0 };

static u_int32_t font_fixed_221_pixels[] = {
	0x18000000,0x60000000,0x88000000,0x88000000,0x50000000,0x50000000,0x20000000,0x20000000,0x20000000,0x20000000,0x20000000,0,0
};
static struct raster font_fixed_221 = { 6, 13, 1, 1, font_fixed_221_pixels, 0 };

static u_int32_t font_fixed_222_pixels[] = {
	0,0,0x80000000,0x80000000,0xf0000000,0x88000000,0x88000000,0x88000000,0xf0000000,0x80000000,0x80000000,0,0
};
static struct raster font_fixed_222 = { 6, 13, 1, 1, font_fixed_222_pixels, 0 };

static u_int32_t font_fixed_223_pixels[] = {
	0,0,0x60000000,0x90000000,0x90000000,0xa0000000,0xb0000000,0x88000000,0x88000000,0x88000000,0xb0000000,0,0
};
static struct raster font_fixed_223 = { 6, 13, 1, 1, font_fixed_223_pixels, 0 };

static u_int32_t font_fixed_224_pixels[] = {
	0,0,0xc0000000,0x30000000,0,0x70000000,0x8000000,0x78000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_224 = { 6, 13, 1, 1, font_fixed_224_pixels, 0 };

static u_int32_t font_fixed_225_pixels[] = {
	0,0,0x18000000,0x60000000,0,0x70000000,0x8000000,0x78000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_225 = { 6, 13, 1, 1, font_fixed_225_pixels, 0 };

static u_int32_t font_fixed_226_pixels[] = {
	0,0,0x20000000,0x50000000,0,0x70000000,0x8000000,0x78000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_226 = { 6, 13, 1, 1, font_fixed_226_pixels, 0 };

static u_int32_t font_fixed_227_pixels[] = {
	0,0,0x68000000,0xb0000000,0,0x70000000,0x8000000,0x78000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_227 = { 6, 13, 1, 1, font_fixed_227_pixels, 0 };

static u_int32_t font_fixed_228_pixels[] = {
	0,0,0,0x50000000,0,0x70000000,0x8000000,0x78000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_228 = { 6, 13, 1, 1, font_fixed_228_pixels, 0 };

static u_int32_t font_fixed_229_pixels[] = {
	0,0x20000000,0x50000000,0x20000000,0,0x70000000,0x8000000,0x78000000,0x88000000,0x88000000,0x78000000,0,0
};
static struct raster font_fixed_229 = { 6, 13, 1, 1, font_fixed_229_pixels, 0 };

static u_int32_t font_fixed_230_pixels[] = {
	0,0,0,0,0,0xd0000000,0x28000000,0x78000000,0xa0000000,0xa0000000,0x78000000,0,0
};
static struct raster font_fixed_230 = { 6, 13, 1, 1, font_fixed_230_pixels, 0 };

static u_int32_t font_fixed_231_pixels[] = {
	0,0,0,0,0,0x70000000,0x88000000,0x80000000,0x80000000,0x88000000,0x70000000,0x10000000,0x30000000
};
static struct raster font_fixed_231 = { 6, 13, 1, 1, font_fixed_231_pixels, 0 };

static u_int32_t font_fixed_232_pixels[] = {
	0,0,0xc0000000,0x30000000,0,0x70000000,0x88000000,0xf8000000,0x80000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_232 = { 6, 13, 1, 1, font_fixed_232_pixels, 0 };

static u_int32_t font_fixed_233_pixels[] = {
	0,0,0x18000000,0x60000000,0,0x70000000,0x88000000,0xf8000000,0x80000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_233 = { 6, 13, 1, 1, font_fixed_233_pixels, 0 };

static u_int32_t font_fixed_234_pixels[] = {
	0,0,0x20000000,0x50000000,0,0x70000000,0x88000000,0xf8000000,0x80000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_234 = { 6, 13, 1, 1, font_fixed_234_pixels, 0 };

static u_int32_t font_fixed_235_pixels[] = {
	0,0,0,0x50000000,0,0x70000000,0x88000000,0xf8000000,0x80000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_235 = { 6, 13, 1, 1, font_fixed_235_pixels, 0 };

static u_int32_t font_fixed_236_pixels[] = {
	0,0,0xc0000000,0x30000000,0,0x60000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_236 = { 6, 13, 1, 1, font_fixed_236_pixels, 0 };

static u_int32_t font_fixed_237_pixels[] = {
	0,0,0x30000000,0xc0000000,0,0x60000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_237 = { 6, 13, 1, 1, font_fixed_237_pixels, 0 };

static u_int32_t font_fixed_238_pixels[] = {
	0,0,0x20000000,0x50000000,0,0x60000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_238 = { 6, 13, 1, 1, font_fixed_238_pixels, 0 };

static u_int32_t font_fixed_239_pixels[] = {
	0,0,0,0x50000000,0,0x60000000,0x20000000,0x20000000,0x20000000,0x20000000,0x70000000,0,0
};
static struct raster font_fixed_239 = { 6, 13, 1, 1, font_fixed_239_pixels, 0 };

static u_int32_t font_fixed_240_pixels[] = {
	0,0,0xd8000000,0x20000000,0xd0000000,0x8000000,0x78000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_240 = { 6, 13, 1, 1, font_fixed_240_pixels, 0 };

static u_int32_t font_fixed_241_pixels[] = {
	0,0,0x68000000,0xb0000000,0,0xb0000000,0xc8000000,0x88000000,0x88000000,0x88000000,0x88000000,0,0
};
static struct raster font_fixed_241 = { 6, 13, 1, 1, font_fixed_241_pixels, 0 };

static u_int32_t font_fixed_242_pixels[] = {
	0,0,0xc0000000,0x30000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_242 = { 6, 13, 1, 1, font_fixed_242_pixels, 0 };

static u_int32_t font_fixed_243_pixels[] = {
	0,0,0x18000000,0x60000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_243 = { 6, 13, 1, 1, font_fixed_243_pixels, 0 };

static u_int32_t font_fixed_244_pixels[] = {
	0,0,0x20000000,0x50000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_244 = { 6, 13, 1, 1, font_fixed_244_pixels, 0 };

static u_int32_t font_fixed_245_pixels[] = {
	0,0,0x68000000,0xb0000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_245 = { 6, 13, 1, 1, font_fixed_245_pixels, 0 };

static u_int32_t font_fixed_246_pixels[] = {
	0,0,0,0x50000000,0,0x70000000,0x88000000,0x88000000,0x88000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_246 = { 6, 13, 1, 1, font_fixed_246_pixels, 0 };

static u_int32_t font_fixed_247_pixels[] = {
	0,0,0,0x20000000,0x20000000,0,0xf8000000,0,0x20000000,0x20000000,0,0,0
};
static struct raster font_fixed_247 = { 6, 13, 1, 1, font_fixed_247_pixels, 0 };

static u_int32_t font_fixed_248_pixels[] = {
	0,0,0,0,0,0x78000000,0x98000000,0xa8000000,0xc8000000,0x88000000,0x70000000,0,0
};
static struct raster font_fixed_248 = { 6, 13, 1, 1, font_fixed_248_pixels, 0 };

static u_int32_t font_fixed_249_pixels[] = {
	0,0,0xc0000000,0x30000000,0,0x88000000,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0,0
};
static struct raster font_fixed_249 = { 6, 13, 1, 1, font_fixed_249_pixels, 0 };

static u_int32_t font_fixed_250_pixels[] = {
	0,0,0x18000000,0x60000000,0,0x88000000,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0,0
};
static struct raster font_fixed_250 = { 6, 13, 1, 1, font_fixed_250_pixels, 0 };

static u_int32_t font_fixed_251_pixels[] = {
	0,0,0x20000000,0x50000000,0,0x88000000,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0,0
};
static struct raster font_fixed_251 = { 6, 13, 1, 1, font_fixed_251_pixels, 0 };

static u_int32_t font_fixed_252_pixels[] = {
	0,0,0,0x50000000,0,0x88000000,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0,0
};
static struct raster font_fixed_252 = { 6, 13, 1, 1, font_fixed_252_pixels, 0 };

static u_int32_t font_fixed_253_pixels[] = {
	0,0,0x18000000,0x60000000,0,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0x8000000,0x88000000,0x70000000
};
static struct raster font_fixed_253 = { 6, 13, 1, 1, font_fixed_253_pixels, 0 };

static u_int32_t font_fixed_254_pixels[] = {
	0,0,0x80000000,0x80000000,0xf0000000,0x88000000,0x88000000,0x88000000,0x88000000,0xf0000000,0x80000000,0x80000000,0
};
static struct raster font_fixed_254 = { 6, 13, 1, 1, font_fixed_254_pixels, 0 };

static u_int32_t font_fixed_255_pixels[] = {
	0,0,0x50000000,0,0,0x88000000,0x88000000,0x88000000,0x98000000,0x68000000,0x8000000,0x88000000,0x70000000
};
static struct raster font_fixed_255 = { 6, 13, 1, 1, font_fixed_255_pixels, 0 };

#define null1 {0}
#define null2 null1, null1
#define null4 null2, null2
#define null8 null4, null4
#define null16 null8, null8
#define null32 null16, null16
#define null64 null32, null32
#define null128 null64, null64

struct raster_font console_font = {
    6, 13, 10, RASFONT_FIXEDWIDTH|RASFONT_NOVERTICALMOVEMENT,
    {
	{ &font_fixed_0, 0, -10, 6, 0 },
	{ &font_fixed_1, 0, -10, 6, 0 },
	{ &font_fixed_2, 0, -10, 6, 0 },
	{ &font_fixed_3, 0, -10, 6, 0 },
	{ &font_fixed_4, 0, -10, 6, 0 },
	{ &font_fixed_5, 0, -10, 6, 0 },
	{ &font_fixed_6, 0, -10, 6, 0 },
	{ &font_fixed_7, 0, -10, 6, 0 },
	{ &font_fixed_8, 0, -10, 6, 0 },
	{ &font_fixed_9, 0, -10, 6, 0 },
	{ &font_fixed_10, 0, -10, 6, 0 },
	{ &font_fixed_11, 0, -10, 6, 0 },
	{ &font_fixed_12, 0, -10, 6, 0 },
	{ &font_fixed_13, 0, -10, 6, 0 },
	{ &font_fixed_14, 0, -10, 6, 0 },
	{ &font_fixed_15, 0, -10, 6, 0 },
	{ &font_fixed_16, 0, -10, 6, 0 },
	{ &font_fixed_17, 0, -10, 6, 0 },
	{ &font_fixed_18, 0, -10, 6, 0 },
	{ &font_fixed_19, 0, -10, 6, 0 },
	{ &font_fixed_20, 0, -10, 6, 0 },
	{ &font_fixed_21, 0, -10, 6, 0 },
	{ &font_fixed_22, 0, -10, 6, 0 },
	{ &font_fixed_23, 0, -10, 6, 0 },
	{ &font_fixed_24, 0, -10, 6, 0 },
	{ &font_fixed_25, 0, -10, 6, 0 },
	{ &font_fixed_26, 0, -10, 6, 0 },
	{ &font_fixed_27, 0, -10, 6, 0 },
	{ &font_fixed_28, 0, -10, 6, 0 },
	{ &font_fixed_29, 0, -10, 6, 0 },
	{ &font_fixed_30, 0, -10, 6, 0 },
	{ &font_fixed_31, 0, -10, 6, 0 },
	{ &font_fixed_32, 0, -10, 6, 0 },
	{ &font_fixed_33, 0, -10, 6, 0 },
	{ &font_fixed_34, 0, -10, 6, 0 },
	{ &font_fixed_35, 0, -10, 6, 0 },
	{ &font_fixed_36, 0, -10, 6, 0 },
	{ &font_fixed_37, 0, -10, 6, 0 },
	{ &font_fixed_38, 0, -10, 6, 0 },
	{ &font_fixed_39, 0, -10, 6, 0 },
	{ &font_fixed_40, 0, -10, 6, 0 },
	{ &font_fixed_41, 0, -10, 6, 0 },
	{ &font_fixed_42, 0, -10, 6, 0 },
	{ &font_fixed_43, 0, -10, 6, 0 },
	{ &font_fixed_44, 0, -10, 6, 0 },
	{ &font_fixed_45, 0, -10, 6, 0 },
	{ &font_fixed_46, 0, -10, 6, 0 },
	{ &font_fixed_47, 0, -10, 6, 0 },
	{ &font_fixed_48, 0, -10, 6, 0 },
	{ &font_fixed_49, 0, -10, 6, 0 },
	{ &font_fixed_50, 0, -10, 6, 0 },
	{ &font_fixed_51, 0, -10, 6, 0 },
	{ &font_fixed_52, 0, -10, 6, 0 },
	{ &font_fixed_53, 0, -10, 6, 0 },
	{ &font_fixed_54, 0, -10, 6, 0 },
	{ &font_fixed_55, 0, -10, 6, 0 },
	{ &font_fixed_56, 0, -10, 6, 0 },
	{ &font_fixed_57, 0, -10, 6, 0 },
	{ &font_fixed_58, 0, -10, 6, 0 },
	{ &font_fixed_59, 0, -10, 6, 0 },
	{ &font_fixed_60, 0, -10, 6, 0 },
	{ &font_fixed_61, 0, -10, 6, 0 },
	{ &font_fixed_62, 0, -10, 6, 0 },
	{ &font_fixed_63, 0, -10, 6, 0 },
	{ &font_fixed_64, 0, -10, 6, 0 },
	{ &font_fixed_65, 0, -10, 6, 0 },
	{ &font_fixed_66, 0, -10, 6, 0 },
	{ &font_fixed_67, 0, -10, 6, 0 },
	{ &font_fixed_68, 0, -10, 6, 0 },
	{ &font_fixed_69, 0, -10, 6, 0 },
	{ &font_fixed_70, 0, -10, 6, 0 },
	{ &font_fixed_71, 0, -10, 6, 0 },
	{ &font_fixed_72, 0, -10, 6, 0 },
	{ &font_fixed_73, 0, -10, 6, 0 },
	{ &font_fixed_74, 0, -10, 6, 0 },
	{ &font_fixed_75, 0, -10, 6, 0 },
	{ &font_fixed_76, 0, -10, 6, 0 },
	{ &font_fixed_77, 0, -10, 6, 0 },
	{ &font_fixed_78, 0, -10, 6, 0 },
	{ &font_fixed_79, 0, -10, 6, 0 },
	{ &font_fixed_80, 0, -10, 6, 0 },
	{ &font_fixed_81, 0, -10, 6, 0 },
	{ &font_fixed_82, 0, -10, 6, 0 },
	{ &font_fixed_83, 0, -10, 6, 0 },
	{ &font_fixed_84, 0, -10, 6, 0 },
	{ &font_fixed_85, 0, -10, 6, 0 },
	{ &font_fixed_86, 0, -10, 6, 0 },
	{ &font_fixed_87, 0, -10, 6, 0 },
	{ &font_fixed_88, 0, -10, 6, 0 },
	{ &font_fixed_89, 0, -10, 6, 0 },
	{ &font_fixed_90, 0, -10, 6, 0 },
	{ &font_fixed_91, 0, -10, 6, 0 },
	{ &font_fixed_92, 0, -10, 6, 0 },
	{ &font_fixed_93, 0, -10, 6, 0 },
	{ &font_fixed_94, 0, -10, 6, 0 },
	{ &font_fixed_95, 0, -10, 6, 0 },
	{ &font_fixed_96, 0, -10, 6, 0 },
	{ &font_fixed_97, 0, -10, 6, 0 },
	{ &font_fixed_98, 0, -10, 6, 0 },
	{ &font_fixed_99, 0, -10, 6, 0 },
	{ &font_fixed_100, 0, -10, 6, 0 },
	{ &font_fixed_101, 0, -10, 6, 0 },
	{ &font_fixed_102, 0, -10, 6, 0 },
	{ &font_fixed_103, 0, -10, 6, 0 },
	{ &font_fixed_104, 0, -10, 6, 0 },
	{ &font_fixed_105, 0, -10, 6, 0 },
	{ &font_fixed_106, 0, -10, 6, 0 },
	{ &font_fixed_107, 0, -10, 6, 0 },
	{ &font_fixed_108, 0, -10, 6, 0 },
	{ &font_fixed_109, 0, -10, 6, 0 },
	{ &font_fixed_110, 0, -10, 6, 0 },
	{ &font_fixed_111, 0, -10, 6, 0 },
	{ &font_fixed_112, 0, -10, 6, 0 },
	{ &font_fixed_113, 0, -10, 6, 0 },
	{ &font_fixed_114, 0, -10, 6, 0 },
	{ &font_fixed_115, 0, -10, 6, 0 },
	{ &font_fixed_116, 0, -10, 6, 0 },
	{ &font_fixed_117, 0, -10, 6, 0 },
	{ &font_fixed_118, 0, -10, 6, 0 },
	{ &font_fixed_119, 0, -10, 6, 0 },
	{ &font_fixed_120, 0, -10, 6, 0 },
	{ &font_fixed_121, 0, -10, 6, 0 },
	{ &font_fixed_122, 0, -10, 6, 0 },
	{ &font_fixed_123, 0, -10, 6, 0 },
	{ &font_fixed_124, 0, -10, 6, 0 },
	{ &font_fixed_125, 0, -10, 6, 0 },
	{ &font_fixed_126, 0, -10, 6, 0 },
	{ &font_fixed_127, 0, -10, 6, 0 },
	null32,
	{ &font_fixed_160, 0, -10, 6, 0 },
	{ &font_fixed_161, 0, -10, 6, 0 },
	{ &font_fixed_162, 0, -10, 6, 0 },
	{ &font_fixed_163, 0, -10, 6, 0 },
	{ &font_fixed_164, 0, -10, 6, 0 },
	{ &font_fixed_165, 0, -10, 6, 0 },
	{ &font_fixed_166, 0, -10, 6, 0 },
	{ &font_fixed_167, 0, -10, 6, 0 },
	{ &font_fixed_168, 0, -10, 6, 0 },
	{ &font_fixed_169, 0, -10, 6, 0 },
	{ &font_fixed_170, 0, -10, 6, 0 },
	{ &font_fixed_171, 0, -10, 6, 0 },
	{ &font_fixed_172, 0, -10, 6, 0 },
	{ &font_fixed_173, 0, -10, 6, 0 },
	{ &font_fixed_174, 0, -10, 6, 0 },
	{ &font_fixed_175, 0, -10, 6, 0 },
	{ &font_fixed_176, 0, -10, 6, 0 },
	{ &font_fixed_177, 0, -10, 6, 0 },
	{ &font_fixed_178, 0, -10, 6, 0 },
	{ &font_fixed_179, 0, -10, 6, 0 },
	{ &font_fixed_180, 0, -10, 6, 0 },
	{ &font_fixed_181, 0, -10, 6, 0 },
	{ &font_fixed_182, 0, -10, 6, 0 },
	{ &font_fixed_183, 0, -10, 6, 0 },
	{ &font_fixed_184, 0, -10, 6, 0 },
	{ &font_fixed_185, 0, -10, 6, 0 },
	{ &font_fixed_186, 0, -10, 6, 0 },
	{ &font_fixed_187, 0, -10, 6, 0 },
	{ &font_fixed_188, 0, -10, 6, 0 },
	{ &font_fixed_189, 0, -10, 6, 0 },
	{ &font_fixed_190, 0, -10, 6, 0 },
	{ &font_fixed_191, 0, -10, 6, 0 },
	{ &font_fixed_192, 0, -10, 6, 0 },
	{ &font_fixed_193, 0, -10, 6, 0 },
	{ &font_fixed_194, 0, -10, 6, 0 },
	{ &font_fixed_195, 0, -10, 6, 0 },
	{ &font_fixed_196, 0, -10, 6, 0 },
	{ &font_fixed_197, 0, -10, 6, 0 },
	{ &font_fixed_198, 0, -10, 6, 0 },
	{ &font_fixed_199, 0, -10, 6, 0 },
	{ &font_fixed_200, 0, -10, 6, 0 },
	{ &font_fixed_201, 0, -10, 6, 0 },
	{ &font_fixed_202, 0, -10, 6, 0 },
	{ &font_fixed_203, 0, -10, 6, 0 },
	{ &font_fixed_204, 0, -10, 6, 0 },
	{ &font_fixed_205, 0, -10, 6, 0 },
	{ &font_fixed_206, 0, -10, 6, 0 },
	{ &font_fixed_207, 0, -10, 6, 0 },
	{ &font_fixed_208, 0, -10, 6, 0 },
	{ &font_fixed_209, 0, -10, 6, 0 },
	{ &font_fixed_210, 0, -10, 6, 0 },
	{ &font_fixed_211, 0, -10, 6, 0 },
	{ &font_fixed_212, 0, -10, 6, 0 },
	{ &font_fixed_213, 0, -10, 6, 0 },
	{ &font_fixed_214, 0, -10, 6, 0 },
	{ &font_fixed_215, 0, -10, 6, 0 },
	{ &font_fixed_216, 0, -10, 6, 0 },
	{ &font_fixed_217, 0, -10, 6, 0 },
	{ &font_fixed_218, 0, -10, 6, 0 },
	{ &font_fixed_219, 0, -10, 6, 0 },
	{ &font_fixed_220, 0, -10, 6, 0 },
	{ &font_fixed_221, 0, -10, 6, 0 },
	{ &font_fixed_222, 0, -10, 6, 0 },
	{ &font_fixed_223, 0, -10, 6, 0 },
	{ &font_fixed_224, 0, -10, 6, 0 },
	{ &font_fixed_225, 0, -10, 6, 0 },
	{ &font_fixed_226, 0, -10, 6, 0 },
	{ &font_fixed_227, 0, -10, 6, 0 },
	{ &font_fixed_228, 0, -10, 6, 0 },
	{ &font_fixed_229, 0, -10, 6, 0 },
	{ &font_fixed_230, 0, -10, 6, 0 },
	{ &font_fixed_231, 0, -10, 6, 0 },
	{ &font_fixed_232, 0, -10, 6, 0 },
	{ &font_fixed_233, 0, -10, 6, 0 },
	{ &font_fixed_234, 0, -10, 6, 0 },
	{ &font_fixed_235, 0, -10, 6, 0 },
	{ &font_fixed_236, 0, -10, 6, 0 },
	{ &font_fixed_237, 0, -10, 6, 0 },
	{ &font_fixed_238, 0, -10, 6, 0 },
	{ &font_fixed_239, 0, -10, 6, 0 },
	{ &font_fixed_240, 0, -10, 6, 0 },
	{ &font_fixed_241, 0, -10, 6, 0 },
	{ &font_fixed_242, 0, -10, 6, 0 },
	{ &font_fixed_243, 0, -10, 6, 0 },
	{ &font_fixed_244, 0, -10, 6, 0 },
	{ &font_fixed_245, 0, -10, 6, 0 },
	{ &font_fixed_246, 0, -10, 6, 0 },
	{ &font_fixed_247, 0, -10, 6, 0 },
	{ &font_fixed_248, 0, -10, 6, 0 },
	{ &font_fixed_249, 0, -10, 6, 0 },
	{ &font_fixed_250, 0, -10, 6, 0 },
	{ &font_fixed_251, 0, -10, 6, 0 },
	{ &font_fixed_252, 0, -10, 6, 0 },
	{ &font_fixed_253, 0, -10, 6, 0 },
	{ &font_fixed_254, 0, -10, 6, 0 },
	{ &font_fixed_255, 0, -10, 6, 0 },
    },
#ifdef COLORFONT_CACHE
    (struct raster_fontcache*) -1
#endif /*COLORFONT_CACHE*/
};

#undef null1
#undef null2
#undef null4
#undef null8
#undef null16
#undef null32
#undef null64
#undef null128

#else /* RASTERCONS_SMALLFONT */

/*
 * PROM compatible fount
 */
static u_int32_t gallant19_0_pixels[] = { 0, 0, 0x7fe00000, 0x7fe00000,
0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000,
0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000, 0x7fe00000,
0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_0 = { 12, 22, 1, 1, gallant19_0_pixels, 0 };
static u_int32_t gallant19_1_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x06000000,
0x0f000000, 0x1f800000, 0x3fc00000, 0x7fe00000, 0x7fe00000, 0x3fc00000,
0x1f800000, 0x0f000000, 0x06000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_1 = { 12, 22, 1, 1, gallant19_1_pixels, 0 };
static u_int32_t gallant19_2_pixels[] = { 0, 0, 0, 0, 0, 0, 0x55400000,
0x2aa00000, 0x55400000, 0x2aa00000, 0x55400000, 0x2aa00000, 0x55400000,
0x2aa00000, 0x55400000, 0x2aa00000, 0x55400000, 0, 0, 0, 0, 0 };
static struct raster gallant19_2 = { 12, 22, 1, 1, gallant19_2_pixels, 0 };
static u_int32_t gallant19_3_pixels[] = { 0, 0, 0, 0, 0, 0x44000000,
0x44000000, 0x44000000, 0x7c000000, 0x44000000, 0x44000000, 0x44000000,
0x03e00000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000,
0x00800000, 0, 0, 0 };
static struct raster gallant19_3 = { 12, 22, 1, 1, gallant19_3_pixels, 0 };
static u_int32_t gallant19_4_pixels[] = { 0, 0, 0, 0, 0, 0x7c000000,
0x40000000, 0x40000000, 0x78000000, 0x40000000, 0x40000000, 0x40000000,
0x03e00000, 0x02000000, 0x02000000, 0x03c00000, 0x02000000, 0x02000000,
0x02000000, 0, 0, 0 };
static struct raster gallant19_4 = { 12, 22, 1, 1, gallant19_4_pixels, 0 };
static u_int32_t gallant19_5_pixels[] = { 0, 0, 0, 0, 0, 0x38000000,
0x44000000, 0x40000000, 0x40000000, 0x40000000, 0x44000000, 0x38000000,
0x03c00000, 0x02200000, 0x02200000, 0x03c00000, 0x02800000, 0x02400000,
0x02200000, 0, 0, 0 };
static struct raster gallant19_5 = { 12, 22, 1, 1, gallant19_5_pixels, 0 };
static u_int32_t gallant19_6_pixels[] = { 0, 0, 0, 0, 0, 0x40000000,
0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x40000000, 0x7c000000,
0x03e00000, 0x02000000, 0x02000000, 0x03c00000, 0x02000000, 0x02000000,
0x02000000, 0, 0, 0 };
static struct raster gallant19_6 = { 12, 22, 1, 1, gallant19_6_pixels, 0 };
static u_int32_t gallant19_7_pixels[] = { 0, 0, 0x0e000000, 0x17000000,
0x23800000, 0x61800000, 0x61800000, 0x71000000, 0x3a000000, 0x1c000000,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_7 = { 12, 22, 1, 1, gallant19_7_pixels, 0 };
static u_int32_t gallant19_8_pixels[] = { 0, 0, 0, 0, 0, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x7fe00000, 0x7fe00000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0, 0x7ff00000, 0x7ff00000, 0, 0, 0,
0 };
static struct raster gallant19_8 = { 12, 22, 1, 1, gallant19_8_pixels, 0 };
static u_int32_t gallant19_9_pixels[] = { 0, 0, 0, 0, 0, 0x44000000,
0x64000000, 0x64000000, 0x54000000, 0x4c000000, 0x4c000000, 0x44000000,
0x02000000, 0x02000000, 0x02000000, 0x02000000, 0x02000000, 0x02000000,
0x03e00000, 0, 0, 0 };
static struct raster gallant19_9 = { 12, 22, 1, 1, gallant19_9_pixels, 0 };
static u_int32_t gallant19_10_pixels[] = { 0, 0, 0, 0, 0, 0x44000000,
0x44000000, 0x44000000, 0x28000000, 0x28000000, 0x10000000, 0x10000000,
0x03e00000, 0x00800000, 0x00800000, 0x00800000, 0x00800000, 0x00800000,
0x00800000, 0, 0, 0 };
static struct raster gallant19_10 = { 12, 22, 1, 1, gallant19_10_pixels, 0 };
static u_int32_t gallant19_11_pixels[] = { 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0xfe000000, 0xfe000000, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0 };
static struct raster gallant19_11 = { 12, 22, 1, 1, gallant19_11_pixels, 0 };
static u_int32_t gallant19_12_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0xfe000000, 0xfe000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000 };
static struct raster gallant19_12 = { 12, 22, 1, 1, gallant19_12_pixels, 0 };
static u_int32_t gallant19_13_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x07f00000, 0x07f00000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000 };
static struct raster gallant19_13 = { 12, 22, 1, 1, gallant19_13_pixels, 0 };
static u_int32_t gallant19_14_pixels[] = { 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x07f00000, 0x07f00000, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0 };
static struct raster gallant19_14 = { 12, 22, 1, 1, gallant19_14_pixels, 0 };
static u_int32_t gallant19_15_pixels[] = { 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0xfff00000, 0xfff00000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000 };
static struct raster gallant19_15 = { 12, 22, 1, 1, gallant19_15_pixels, 0 };
static u_int32_t gallant19_16_pixels[] = { 0, 0xfff00000, 0xfff00000, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_16 = { 12, 22, 1, 1, gallant19_16_pixels, 0 };
static u_int32_t gallant19_17_pixels[] = { 0, 0, 0, 0, 0, 0, 0xfff00000,
0xfff00000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_17 = { 12, 22, 1, 1, gallant19_17_pixels, 0 };
static u_int32_t gallant19_18_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0xfff00000, 0xfff00000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_18 = { 12, 22, 1, 1, gallant19_18_pixels, 0 };
static u_int32_t gallant19_19_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0xfff00000, 0xfff00000, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_19 = { 12, 22, 1, 1, gallant19_19_pixels, 0 };
static u_int32_t gallant19_20_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0xfff00000, 0xfff00000, 0 };
static struct raster gallant19_20 = { 12, 22, 1, 1, gallant19_20_pixels, 0 };
static u_int32_t gallant19_21_pixels[] = { 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x07f00000, 0x07f00000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000 };
static struct raster gallant19_21 = { 12, 22, 1, 1, gallant19_21_pixels, 0 };
static u_int32_t gallant19_22_pixels[] = { 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0xfe000000, 0xfe000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000 };
static struct raster gallant19_22 = { 12, 22, 1, 1, gallant19_22_pixels, 0 };
static u_int32_t gallant19_23_pixels[] = { 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0xfff00000, 0xfff00000, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0 };
static struct raster gallant19_23 = { 12, 22, 1, 1, gallant19_23_pixels, 0 };
static u_int32_t gallant19_24_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0xfff00000, 0xfff00000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000 };
static struct raster gallant19_24 = { 12, 22, 1, 1, gallant19_24_pixels, 0 };
static u_int32_t gallant19_25_pixels[] = { 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000 };
static struct raster gallant19_25 = { 12, 22, 1, 1, gallant19_25_pixels, 0 };
static u_int32_t gallant19_26_pixels[] = { 0, 0, 0, 0, 0, 0, 0x00e00000,
0x07800000, 0x1e000000, 0x78000000, 0x78000000, 0x1e000000, 0x07800000,
0x00e00000, 0, 0x7fe00000, 0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_26 = { 12, 22, 1, 1, gallant19_26_pixels, 0 };
static u_int32_t gallant19_27_pixels[] = { 0, 0, 0, 0, 0, 0, 0x70000000,
0x1e000000, 0x07800000, 0x01e00000, 0x01e00000, 0x07800000, 0x1e000000,
0x70000000, 0, 0x7fe00000, 0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_27 = { 12, 22, 1, 1, gallant19_27_pixels, 0 };
static u_int32_t gallant19_28_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x3fe00000,
0x7fc00000, 0x19800000, 0x19800000, 0x19800000, 0x19800000, 0x31800000,
0x31800000, 0x31c00000, 0x60c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_28 = { 12, 22, 1, 1, gallant19_28_pixels, 0 };
static u_int32_t gallant19_29_pixels[] = { 0, 0, 0, 0, 0, 0, 0x00400000,
0x00c00000, 0x01800000, 0x7fc00000, 0x7fc00000, 0x06000000, 0x0c000000,
0x7fc00000, 0x7fc00000, 0x30000000, 0x60000000, 0x40000000, 0, 0, 0, 0 };
static struct raster gallant19_29 = { 12, 22, 1, 1, gallant19_29_pixels, 0 };
static u_int32_t gallant19_30_pixels[] = { 0, 0x06000000, 0x0c000000,
0x10000000, 0x10000000, 0x30000000, 0x30000000, 0x30000000, 0x3e000000,
0x7c000000, 0x18000000, 0x18000000, 0x18000000, 0x18000000, 0x3f200000,
0x3fe00000, 0x31c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_30 = { 12, 22, 1, 1, gallant19_30_pixels, 0 };
static u_int32_t gallant19_31_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x06000000, 0x06000000, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_31 = { 12, 22, 1, 1, gallant19_31_pixels, 0 };
static u_int32_t gallant19_32_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_32 = { 12, 22, 1, 1, gallant19_32_pixels, 0 };
static u_int32_t gallant19_33_pixels[] = { 0, 0, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0, 0, 0x06000000, 0x06000000, 0, 0,
0, 0, 0 };
static struct raster gallant19_33 = { 12, 22, 1, 1, gallant19_33_pixels, 0 };
static u_int32_t gallant19_34_pixels[] = { 0, 0, 0x19800000, 0x19800000,
0x19800000, 0x19800000, 0x19800000, 0x19800000, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0 };
static struct raster gallant19_34 = { 12, 22, 1, 1, gallant19_34_pixels, 0 };
static u_int32_t gallant19_35_pixels[] = { 0, 0, 0x03300000, 0x03300000,
0x03300000, 0x06600000, 0x1ff00000, 0x1ff00000, 0x0cc00000, 0x0cc00000,
0x19800000, 0x19800000, 0x7fc00000, 0x7fc00000, 0x33000000, 0x66000000,
0x66000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_35 = { 12, 22, 1, 1, gallant19_35_pixels, 0 };
static u_int32_t gallant19_36_pixels[] = { 0, 0, 0x06000000, 0x1f800000,
0x3fc00000, 0x66e00000, 0x66600000, 0x66000000, 0x3e000000, 0x1f800000,
0x07c00000, 0x06600000, 0x06600000, 0x66600000, 0x7fc00000, 0x3f800000,
0x06000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_36 = { 12, 22, 1, 1, gallant19_36_pixels, 0 };
static u_int32_t gallant19_37_pixels[] = { 0, 0, 0x38600000, 0x44c00000,
0x44c00000, 0x45800000, 0x39800000, 0x03000000, 0x03000000, 0x06000000,
0x0c000000, 0x0c000000, 0x19c00000, 0x1a200000, 0x32200000, 0x32200000,
0x61c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_37 = { 12, 22, 1, 1, gallant19_37_pixels, 0 };
static u_int32_t gallant19_38_pixels[] = { 0, 0, 0x07000000, 0x0f800000,
0x18c00000, 0x18c00000, 0x18c00000, 0x0f800000, 0x1e000000, 0x3e000000,
0x77000000, 0x63600000, 0x61e00000, 0x61c00000, 0x61800000, 0x3fe00000,
0x1e600000, 0, 0, 0, 0, 0 };
static struct raster gallant19_38 = { 12, 22, 1, 1, gallant19_38_pixels, 0 };
static u_int32_t gallant19_39_pixels[] = { 0, 0, 0x0c000000, 0x1e000000,
0x1e000000, 0x06000000, 0x06000000, 0x0c000000, 0x18000000, 0x10000000,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_39 = { 12, 22, 1, 1, gallant19_39_pixels, 0 };
static u_int32_t gallant19_40_pixels[] = { 0, 0, 0x00c00000, 0x01800000,
0x03800000, 0x03000000, 0x07000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x07000000, 0x03000000, 0x03800000, 0x01800000,
0x00c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_40 = { 12, 22, 1, 1, gallant19_40_pixels, 0 };
static u_int32_t gallant19_41_pixels[] = { 0, 0, 0x30000000, 0x18000000,
0x1c000000, 0x0c000000, 0x0e000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x0e000000, 0x0c000000, 0x1c000000, 0x18000000,
0x30000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_41 = { 12, 22, 1, 1, gallant19_41_pixels, 0 };
static u_int32_t gallant19_42_pixels[] = { 0, 0, 0, 0, 0, 0, 0x0f000000,
0x06000000, 0x66600000, 0x76e00000, 0x19800000, 0, 0x19800000,
0x76e00000, 0x66600000, 0x06000000, 0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_42 = { 12, 22, 1, 1, gallant19_42_pixels, 0 };
static u_int32_t gallant19_43_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x7fe00000, 0x7fe00000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_43 = { 12, 22, 1, 1, gallant19_43_pixels, 0 };
static u_int32_t gallant19_44_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0x0c000000, 0x1e000000, 0x1e000000, 0x06000000, 0x06000000,
0x0c000000, 0x18000000, 0x10000000 };
static struct raster gallant19_44 = { 12, 22, 1, 1, gallant19_44_pixels, 0 };
static u_int32_t gallant19_45_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x7fe00000, 0x7fe00000, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_45 = { 12, 22, 1, 1, gallant19_45_pixels, 0 };
static u_int32_t gallant19_46_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0x0c000000, 0x1e000000, 0x1e000000, 0x0c000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_46 = { 12, 22, 1, 1, gallant19_46_pixels, 0 };
static u_int32_t gallant19_47_pixels[] = { 0, 0, 0x00600000, 0x00c00000,
0x00c00000, 0x01800000, 0x01800000, 0x03000000, 0x03000000, 0x06000000,
0x0c000000, 0x0c000000, 0x18000000, 0x18000000, 0x30000000, 0x30000000,
0x60000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_47 = { 12, 22, 1, 1, gallant19_47_pixels, 0 };
static u_int32_t gallant19_48_pixels[] = { 0, 0, 0x07000000, 0x0f800000,
0x11800000, 0x10c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30800000, 0x18800000, 0x1f000000,
0x0e000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_48 = { 12, 22, 1, 1, gallant19_48_pixels, 0 };
static u_int32_t gallant19_49_pixels[] = { 0, 0, 0x02000000, 0x06000000,
0x0e000000, 0x1e000000, 0x36000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x3fc00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_49 = { 12, 22, 1, 1, gallant19_49_pixels, 0 };
static u_int32_t gallant19_50_pixels[] = { 0, 0, 0x1f000000, 0x3f800000,
0x61c00000, 0x40c00000, 0x00c00000, 0x00c00000, 0x00c00000, 0x01800000,
0x03000000, 0x06000000, 0x0c000000, 0x18000000, 0x30200000, 0x7fe00000,
0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_50 = { 12, 22, 1, 1, gallant19_50_pixels, 0 };
static u_int32_t gallant19_51_pixels[] = { 0, 0, 0x0f800000, 0x1fc00000,
0x20e00000, 0x40600000, 0x00600000, 0x00e00000, 0x07c00000, 0x0fc00000,
0x00e00000, 0x00600000, 0x00600000, 0x40600000, 0x60400000, 0x3f800000,
0x1f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_51 = { 12, 22, 1, 1, gallant19_51_pixels, 0 };
static u_int32_t gallant19_52_pixels[] = { 0, 0, 0x01800000, 0x03800000,
0x03800000, 0x05800000, 0x05800000, 0x09800000, 0x09800000, 0x11800000,
0x11800000, 0x21800000, 0x3fe00000, 0x7fe00000, 0x01800000, 0x01800000,
0x01800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_52 = { 12, 22, 1, 1, gallant19_52_pixels, 0 };
static u_int32_t gallant19_53_pixels[] = { 0, 0, 0x0fc00000, 0x0fc00000,
0x10000000, 0x10000000, 0x20000000, 0x3f800000, 0x31c00000, 0x00e00000,
0x00600000, 0x00600000, 0x00600000, 0x40600000, 0x60600000, 0x30c00000,
0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_53 = { 12, 22, 1, 1, gallant19_53_pixels, 0 };
static u_int32_t gallant19_54_pixels[] = { 0, 0, 0x07000000, 0x0c000000,
0x18000000, 0x30000000, 0x30000000, 0x60000000, 0x67800000, 0x6fc00000,
0x70e00000, 0x60600000, 0x60600000, 0x60600000, 0x70400000, 0x3f800000,
0x1f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_54 = { 12, 22, 1, 1, gallant19_54_pixels, 0 };
static u_int32_t gallant19_55_pixels[] = { 0, 0, 0x1fe00000, 0x3fe00000,
0x60400000, 0x00400000, 0x00c00000, 0x00800000, 0x00800000, 0x01800000,
0x01000000, 0x01000000, 0x03000000, 0x02000000, 0x02000000, 0x06000000,
0x04000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_55 = { 12, 22, 1, 1, gallant19_55_pixels, 0 };
static u_int32_t gallant19_56_pixels[] = { 0, 0, 0x0f000000, 0x11800000,
0x30c00000, 0x30c00000, 0x30c00000, 0x18800000, 0x0d000000, 0x06000000,
0x0b000000, 0x11800000, 0x30c00000, 0x30c00000, 0x30c00000, 0x18800000,
0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_56 = { 12, 22, 1, 1, gallant19_56_pixels, 0 };
static u_int32_t gallant19_57_pixels[] = { 0, 0, 0x0f800000, 0x11c00000,
0x20e00000, 0x60600000, 0x60600000, 0x60600000, 0x70e00000, 0x3f600000,
0x1e600000, 0x00600000, 0x00c00000, 0x00c00000, 0x01800000, 0x07000000,
0x3c000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_57 = { 12, 22, 1, 1, gallant19_57_pixels, 0 };
static u_int32_t gallant19_58_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x0c000000,
0x1e000000, 0x1e000000, 0x0c000000, 0, 0, 0x0c000000, 0x1e000000,
0x1e000000, 0x0c000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_58 = { 12, 22, 1, 1, gallant19_58_pixels, 0 };
static u_int32_t gallant19_59_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0,
0x0c000000, 0x1e000000, 0x1e000000, 0x0c000000, 0, 0, 0x0c000000,
0x1e000000, 0x1e000000, 0x06000000, 0x06000000, 0x0c000000, 0x18000000,
0x10000000 };
static struct raster gallant19_59 = { 12, 22, 1, 1, gallant19_59_pixels, 0 };
static u_int32_t gallant19_60_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x00600000,
0x01c00000, 0x07000000, 0x1e000000, 0x78000000, 0x78000000, 0x1e000000,
0x07000000, 0x01c00000, 0x00600000, 0, 0, 0, 0, 0 };
static struct raster gallant19_60 = { 12, 22, 1, 1, gallant19_60_pixels, 0 };
static u_int32_t gallant19_61_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x7fc00000, 0x7fc00000, 0, 0, 0x7fc00000, 0x7fc00000, 0, 0, 0, 0, 0, 0,
0 };
static struct raster gallant19_61 = { 12, 22, 1, 1, gallant19_61_pixels, 0 };
static u_int32_t gallant19_62_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x60000000,
0x38000000, 0x1e000000, 0x07800000, 0x01e00000, 0x01e00000, 0x07800000,
0x1e000000, 0x38000000, 0x60000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_62 = { 12, 22, 1, 1, gallant19_62_pixels, 0 };
static u_int32_t gallant19_63_pixels[] = { 0, 0, 0x0f000000, 0x1f800000,
0x39c00000, 0x20c00000, 0x00c00000, 0x00c00000, 0x01800000, 0x03000000,
0x06000000, 0x0c000000, 0x0c000000, 0, 0, 0x0c000000, 0x0c000000, 0, 0,
0, 0, 0 };
static struct raster gallant19_63 = { 12, 22, 1, 1, gallant19_63_pixels, 0 };
static u_int32_t gallant19_64_pixels[] = { 0, 0, 0, 0, 0, 0x0f800000,
0x3fc00000, 0x30600000, 0x60600000, 0x67200000, 0x6fa00000, 0x6ca00000,
0x6ca00000, 0x67e00000, 0x60000000, 0x30000000, 0x3fe00000, 0x0fe00000,
0, 0, 0, 0 };
static struct raster gallant19_64 = { 12, 22, 1, 1, gallant19_64_pixels, 0 };
static u_int32_t gallant19_65_pixels[] = { 0, 0, 0, 0x06000000, 0x06000000,
0x0b000000, 0x0b000000, 0x09000000, 0x11800000, 0x11800000, 0x10800000,
0x3fc00000, 0x20c00000, 0x20400000, 0x40600000, 0x40600000, 0xe0f00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_65 = { 12, 22, 1, 1, gallant19_65_pixels, 0 };
static u_int32_t gallant19_66_pixels[] = { 0, 0, 0, 0xff000000, 0x60800000,
0x60c00000, 0x60c00000, 0x60c00000, 0x61800000, 0x7f800000, 0x60c00000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60c00000, 0xff800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_66 = { 12, 22, 1, 1, gallant19_66_pixels, 0 };
static u_int32_t gallant19_67_pixels[] = { 0, 0, 0, 0x0fc00000, 0x10600000,
0x20200000, 0x20000000, 0x60000000, 0x60000000, 0x60000000, 0x60000000,
0x60000000, 0x60000000, 0x20000000, 0x30200000, 0x18400000, 0x0f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_67 = { 12, 22, 1, 1, gallant19_67_pixels, 0 };
static u_int32_t gallant19_68_pixels[] = { 0, 0, 0, 0xff000000, 0x61c00000,
0x60c00000, 0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60400000, 0x61800000, 0xfe000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_68 = { 12, 22, 1, 1, gallant19_68_pixels, 0 };
static u_int32_t gallant19_69_pixels[] = { 0, 0, 0, 0x7fc00000, 0x30400000,
0x30400000, 0x30000000, 0x30000000, 0x30800000, 0x3f800000, 0x30800000,
0x30000000, 0x30000000, 0x30000000, 0x30200000, 0x30200000, 0x7fe00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_69 = { 12, 22, 1, 1, gallant19_69_pixels, 0 };
static u_int32_t gallant19_70_pixels[] = { 0, 0, 0, 0x7fc00000, 0x30400000,
0x30400000, 0x30000000, 0x30000000, 0x30800000, 0x3f800000, 0x30800000,
0x30000000, 0x30000000, 0x30000000, 0x30000000, 0x30000000, 0x78000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_70 = { 12, 22, 1, 1, gallant19_70_pixels, 0 };
static u_int32_t gallant19_71_pixels[] = { 0, 0, 0, 0x0fc00000, 0x10600000,
0x20200000, 0x20000000, 0x60000000, 0x60000000, 0x60000000, 0x60000000,
0x61f00000, 0x60600000, 0x20600000, 0x30600000, 0x18600000, 0x0f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_71 = { 12, 22, 1, 1, gallant19_71_pixels, 0 };
static u_int32_t gallant19_72_pixels[] = { 0, 0, 0, 0xf0f00000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x7fe00000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000, 0xf0f00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_72 = { 12, 22, 1, 1, gallant19_72_pixels, 0 };
static u_int32_t gallant19_73_pixels[] = { 0, 0, 0, 0x1f800000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x1f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_73 = { 12, 22, 1, 1, gallant19_73_pixels, 0 };
static u_int32_t gallant19_74_pixels[] = { 0, 0, 0, 0x1f800000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x04000000, 0x38000000, 0x30000000 };
static struct raster gallant19_74 = { 12, 22, 1, 1, gallant19_74_pixels, 0 };
static u_int32_t gallant19_75_pixels[] = { 0, 0, 0, 0xf0e00000, 0x61800000,
0x63000000, 0x66000000, 0x6c000000, 0x78000000, 0x78000000, 0x7c000000,
0x6e000000, 0x67000000, 0x63800000, 0x61c00000, 0x60e00000, 0xf0700000,
0, 0, 0, 0, 0 };
static struct raster gallant19_75 = { 12, 22, 1, 1, gallant19_75_pixels, 0 };
static u_int32_t gallant19_76_pixels[] = { 0, 0, 0, 0x78000000, 0x30000000,
0x30000000, 0x30000000, 0x30000000, 0x30000000, 0x30000000, 0x30000000,
0x30000000, 0x30000000, 0x30000000, 0x30200000, 0x30200000, 0x7fe00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_76 = { 12, 22, 1, 1, gallant19_76_pixels, 0 };
static u_int32_t gallant19_77_pixels[] = { 0, 0, 0, 0xe0700000, 0x60e00000,
0x70e00000, 0x70e00000, 0x70e00000, 0x59600000, 0x59600000, 0x59600000,
0x4d600000, 0x4e600000, 0x4e600000, 0x44600000, 0x44600000, 0xe4f00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_77 = { 12, 22, 1, 1, gallant19_77_pixels, 0 };
static u_int32_t gallant19_78_pixels[] = { 0, 0, 0, 0xc0700000, 0x60200000,
0x70200000, 0x78200000, 0x58200000, 0x4c200000, 0x46200000, 0x47200000,
0x43200000, 0x41a00000, 0x40e00000, 0x40e00000, 0x40600000, 0xe0300000,
0, 0, 0, 0, 0 };
static struct raster gallant19_78 = { 12, 22, 1, 1, gallant19_78_pixels, 0 };
static u_int32_t gallant19_79_pixels[] = { 0, 0, 0, 0x0f000000, 0x11c00000,
0x20c00000, 0x20600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000,
0x60600000, 0x60600000, 0x20400000, 0x30400000, 0x18800000, 0x0f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_79 = { 12, 22, 1, 1, gallant19_79_pixels, 0 };
static u_int32_t gallant19_80_pixels[] = { 0, 0, 0, 0x7f800000, 0x30c00000,
0x30600000, 0x30600000, 0x30600000, 0x30c00000, 0x37800000, 0x30000000,
0x30000000, 0x30000000, 0x30000000, 0x30000000, 0x30000000, 0x78000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_80 = { 12, 22, 1, 1, gallant19_80_pixels, 0 };
static u_int32_t gallant19_81_pixels[] = { 0, 0, 0, 0x0f000000, 0x11c00000,
0x20c00000, 0x20600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000,
0x60600000, 0x60600000, 0x30400000, 0x38400000, 0x1f800000, 0x0e000000,
0x1f000000, 0x23900000, 0x01e00000, 0, 0 };
static struct raster gallant19_81 = { 12, 22, 1, 1, gallant19_81_pixels, 0 };
static u_int32_t gallant19_82_pixels[] = { 0, 0, 0, 0xff000000, 0x61800000,
0x60c00000, 0x60c00000, 0x60c00000, 0x60800000, 0x7f000000, 0x7c000000,
0x6e000000, 0x67000000, 0x63800000, 0x61c00000, 0x60e00000, 0xf0700000,
0, 0, 0, 0, 0 };
static struct raster gallant19_82 = { 12, 22, 1, 1, gallant19_82_pixels, 0 };
static u_int32_t gallant19_83_pixels[] = { 0, 0, 0, 0x1fe00000, 0x30600000,
0x60200000, 0x60200000, 0x70000000, 0x3c000000, 0x1e000000, 0x07800000,
0x01c00000, 0x00e00000, 0x40600000, 0x40600000, 0x60c00000, 0x7f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_83 = { 12, 22, 1, 1, gallant19_83_pixels, 0 };
static u_int32_t gallant19_84_pixels[] = { 0, 0, 0, 0x7fe00000, 0x46200000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x1f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_84 = { 12, 22, 1, 1, gallant19_84_pixels, 0 };
static u_int32_t gallant19_85_pixels[] = { 0, 0, 0, 0xf0700000, 0x60200000,
0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x60200000, 0x60200000, 0x60200000, 0x70400000, 0x3fc00000, 0x1f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_85 = { 12, 22, 1, 1, gallant19_85_pixels, 0 };
static u_int32_t gallant19_86_pixels[] = { 0, 0, 0, 0xe0e00000, 0x60400000,
0x30800000, 0x30800000, 0x30800000, 0x19000000, 0x19000000, 0x19000000,
0x0c000000, 0x0e000000, 0x0e000000, 0x04000000, 0x04000000, 0x04000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_86 = { 12, 22, 1, 1, gallant19_86_pixels, 0 };
static u_int32_t gallant19_87_pixels[] = { 0, 0, 0, 0xfef00000, 0x66200000,
0x66200000, 0x66200000, 0x76200000, 0x77400000, 0x33400000, 0x37400000,
0x3bc00000, 0x3b800000, 0x19800000, 0x19800000, 0x19800000, 0x19800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_87 = { 12, 22, 1, 1, gallant19_87_pixels, 0 };
static u_int32_t gallant19_88_pixels[] = { 0, 0, 0, 0xf0700000, 0x60200000,
0x30400000, 0x38800000, 0x18800000, 0x0d000000, 0x06000000, 0x06000000,
0x0b000000, 0x11800000, 0x11c00000, 0x20c00000, 0x40600000, 0xe0f00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_88 = { 12, 22, 1, 1, gallant19_88_pixels, 0 };
static u_int32_t gallant19_89_pixels[] = { 0, 0, 0, 0xf0700000, 0x60200000,
0x30400000, 0x18800000, 0x18800000, 0x0d000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x0f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_89 = { 12, 22, 1, 1, gallant19_89_pixels, 0 };
static u_int32_t gallant19_90_pixels[] = { 0, 0, 0, 0x3fe00000, 0x20c00000,
0x00c00000, 0x01800000, 0x01800000, 0x03000000, 0x03000000, 0x06000000,
0x06000000, 0x0c000000, 0x0c000000, 0x18000000, 0x18200000, 0x3fe00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_90 = { 12, 22, 1, 1, gallant19_90_pixels, 0 };
static u_int32_t gallant19_91_pixels[] = { 0, 0, 0x07c00000, 0x07c00000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x07c00000,
0x07c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_91 = { 12, 22, 1, 1, gallant19_91_pixels, 0 };
static u_int32_t gallant19_92_pixels[] = { 0, 0, 0x60000000, 0x60000000,
0x30000000, 0x30000000, 0x18000000, 0x18000000, 0x0c000000, 0x0c000000,
0x06000000, 0x03000000, 0x03000000, 0x01800000, 0x01800000, 0x00c00000,
0x00c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_92 = { 12, 22, 1, 1, gallant19_92_pixels, 0 };
static u_int32_t gallant19_93_pixels[] = { 0, 0, 0x7c000000, 0x7c000000,
0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000,
0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x7c000000,
0x7c000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_93 = { 12, 22, 1, 1, gallant19_93_pixels, 0 };
static u_int32_t gallant19_94_pixels[] = { 0, 0, 0x04000000, 0x0e000000,
0x1b000000, 0x31800000, 0x60c00000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0 };
static struct raster gallant19_94 = { 12, 22, 1, 1, gallant19_94_pixels, 0 };
static u_int32_t gallant19_95_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0xfff00000, 0xfff00000, 0, 0 };
static struct raster gallant19_95 = { 12, 22, 1, 1, gallant19_95_pixels, 0 };
static u_int32_t gallant19_96_pixels[] = { 0, 0, 0x01000000, 0x03000000,
0x06000000, 0x06000000, 0x07800000, 0x07800000, 0x03000000, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_96 = { 12, 22, 1, 1, gallant19_96_pixels, 0 };
static u_int32_t gallant19_97_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x0f800000,
0x18c00000, 0x10c00000, 0x03c00000, 0x1cc00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x39c00000, 0x1ee00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_97 = { 12, 22, 1, 1, gallant19_97_pixels, 0 };
static u_int32_t gallant19_98_pixels[] = { 0, 0, 0x20000000, 0x60000000,
0xe0000000, 0x60000000, 0x60000000, 0x67800000, 0x6fc00000, 0x70e00000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x70600000, 0x78c00000,
0x4f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_98 = { 12, 22, 1, 1, gallant19_98_pixels, 0 };
static u_int32_t gallant19_99_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x1f800000,
0x31c00000, 0x20c00000, 0x60000000, 0x60000000, 0x60000000, 0x60000000,
0x70400000, 0x30c00000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_99 = { 12, 22, 1, 1, gallant19_99_pixels, 0 };
static u_int32_t gallant19_100_pixels[] = { 0, 0, 0x00600000, 0x00e00000,
0x00600000, 0x00600000, 0x00600000, 0x0f600000, 0x31e00000, 0x20e00000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x70e00000, 0x39600000,
0x1e700000, 0, 0, 0, 0, 0 };
static struct raster gallant19_100 = { 12, 22, 1, 1, gallant19_100_pixels, 0 };
static u_int32_t gallant19_101_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x0f000000,
0x30c00000, 0x60600000, 0x60600000, 0x7fe00000, 0x60000000, 0x60000000,
0x30000000, 0x18600000, 0x0f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_101 = { 12, 22, 1, 1, gallant19_101_pixels, 0 };
static u_int32_t gallant19_102_pixels[] = { 0, 0, 0x03800000, 0x04c00000,
0x04c00000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x1f800000,
0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000, 0x0c000000,
0x1e000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_102 = { 12, 22, 1, 1, gallant19_102_pixels, 0 };
static u_int32_t gallant19_103_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x1f200000,
0x31e00000, 0x60c00000, 0x60c00000, 0x60c00000, 0x31800000, 0x3f000000,
0x60000000, 0x7fc00000, 0x3fe00000, 0x20600000, 0x40200000, 0x40200000,
0x7fc00000, 0x3f800000 };
static struct raster gallant19_103 = { 12, 22, 1, 1, gallant19_103_pixels, 0 };
static u_int32_t gallant19_104_pixels[] = { 0, 0, 0x10000000, 0x30000000,
0x70000000, 0x30000000, 0x30000000, 0x37800000, 0x39c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x79e00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_104 = { 12, 22, 1, 1, gallant19_104_pixels, 0 };
static u_int32_t gallant19_105_pixels[] = { 0, 0, 0, 0x06000000, 0x06000000,
0, 0, 0x1e000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x1f800000, 0, 0, 0, 0,
0 };
static struct raster gallant19_105 = { 12, 22, 1, 1, gallant19_105_pixels, 0 };
static u_int32_t gallant19_106_pixels[] = { 0, 0, 0, 0x00c00000, 0x00c00000,
0, 0, 0x03c00000, 0x00c00000, 0x00c00000, 0x00c00000, 0x00c00000,
0x00c00000, 0x00c00000, 0x00c00000, 0x00c00000, 0x00c00000, 0x20c00000,
0x30c00000, 0x38800000, 0x1f000000, 0x0e000000 };
static struct raster gallant19_106 = { 12, 22, 1, 1, gallant19_106_pixels, 0 };
static u_int32_t gallant19_107_pixels[] = { 0, 0, 0x60000000, 0xe0000000,
0x60000000, 0x60000000, 0x60000000, 0x61c00000, 0x63000000, 0x66000000,
0x7c000000, 0x78000000, 0x7c000000, 0x6e000000, 0x67000000, 0x63800000,
0xf1e00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_107 = { 12, 22, 1, 1, gallant19_107_pixels, 0 };
static u_int32_t gallant19_108_pixels[] = { 0, 0, 0x1e000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_108 = { 12, 22, 1, 1, gallant19_108_pixels, 0 };
static u_int32_t gallant19_109_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0xddc00000,
0x6ee00000, 0x66600000, 0x66600000, 0x66600000, 0x66600000, 0x66600000,
0x66600000, 0x66600000, 0xef700000, 0, 0, 0, 0, 0 };
static struct raster gallant19_109 = { 12, 22, 1, 1, gallant19_109_pixels, 0 };
static u_int32_t gallant19_110_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x27800000,
0x79c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x79e00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_110 = { 12, 22, 1, 1, gallant19_110_pixels, 0 };
static u_int32_t gallant19_111_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x0f800000,
0x11c00000, 0x20e00000, 0x60600000, 0x60600000, 0x60600000, 0x60600000,
0x70400000, 0x38800000, 0x1f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_111 = { 12, 22, 1, 1, gallant19_111_pixels, 0 };
static u_int32_t gallant19_112_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0xef800000,
0x71c00000, 0x60e00000, 0x60600000, 0x60600000, 0x60600000, 0x60600000,
0x60400000, 0x70800000, 0x7f000000, 0x60000000, 0x60000000, 0x60000000,
0x60000000, 0xf0000000 };
static struct raster gallant19_112 = { 12, 22, 1, 1, gallant19_112_pixels, 0 };
static u_int32_t gallant19_113_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x0f200000,
0x11e00000, 0x20e00000, 0x60600000, 0x60600000, 0x60600000, 0x60600000,
0x70600000, 0x38e00000, 0x1fe00000, 0x00600000, 0x00600000, 0x00600000,
0x00600000, 0x00f00000 };
static struct raster gallant19_113 = { 12, 22, 1, 1, gallant19_113_pixels, 0 };
static u_int32_t gallant19_114_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x73800000,
0x34c00000, 0x38c00000, 0x30000000, 0x30000000, 0x30000000, 0x30000000,
0x30000000, 0x30000000, 0x78000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_114 = { 12, 22, 1, 1, gallant19_114_pixels, 0 };
static u_int32_t gallant19_115_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x1fc00000,
0x30c00000, 0x30400000, 0x38000000, 0x1e000000, 0x07800000, 0x01c00000,
0x20c00000, 0x30c00000, 0x3f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_115 = { 12, 22, 1, 1, gallant19_115_pixels, 0 };
static u_int32_t gallant19_116_pixels[] = { 0, 0, 0, 0, 0x04000000,
0x04000000, 0x0c000000, 0x7fc00000, 0x0c000000, 0x0c000000, 0x0c000000,
0x0c000000, 0x0c000000, 0x0c000000, 0x0c200000, 0x0e400000, 0x07800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_116 = { 12, 22, 1, 1, gallant19_116_pixels, 0 };
static u_int32_t gallant19_117_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x79e00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x39c00000, 0x1e600000, 0, 0, 0, 0, 0 };
static struct raster gallant19_117 = { 12, 22, 1, 1, gallant19_117_pixels, 0 };
static u_int32_t gallant19_118_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0xf0700000,
0x60200000, 0x30400000, 0x30400000, 0x18800000, 0x18800000, 0x0d000000,
0x0d000000, 0x06000000, 0x06000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_118 = { 12, 22, 1, 1, gallant19_118_pixels, 0 };
static u_int32_t gallant19_119_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0xff700000,
0x66200000, 0x66200000, 0x66200000, 0x37400000, 0x3b400000, 0x3b400000,
0x19800000, 0x19800000, 0x19800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_119 = { 12, 22, 1, 1, gallant19_119_pixels, 0 };
static u_int32_t gallant19_120_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0xf8f00000,
0x70400000, 0x38800000, 0x1d000000, 0x0e000000, 0x07000000, 0x0b800000,
0x11c00000, 0x20e00000, 0xf1f00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_120 = { 12, 22, 1, 1, gallant19_120_pixels, 0 };
static u_int32_t gallant19_121_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0xf0f00000,
0x60200000, 0x30400000, 0x30400000, 0x18800000, 0x18800000, 0x0d000000,
0x0d000000, 0x06000000, 0x06000000, 0x04000000, 0x0c000000, 0x08000000,
0x78000000, 0x70000000 };
static struct raster gallant19_121 = { 12, 22, 1, 1, gallant19_121_pixels, 0 };
static u_int32_t gallant19_122_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x7fe00000,
0x60e00000, 0x41c00000, 0x03800000, 0x07000000, 0x0e000000, 0x1c000000,
0x38200000, 0x70600000, 0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_122 = { 12, 22, 1, 1, gallant19_122_pixels, 0 };
static u_int32_t gallant19_123_pixels[] = { 0, 0, 0x01c00000, 0x03000000,
0x03000000, 0x01800000, 0x01800000, 0x01800000, 0x03000000, 0x07000000,
0x03000000, 0x01800000, 0x01800000, 0x01800000, 0x03000000, 0x03000000,
0x01c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_123 = { 12, 22, 1, 1, gallant19_123_pixels, 0 };
static u_int32_t gallant19_124_pixels[] = { 0, 0, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000 };
static struct raster gallant19_124 = { 12, 22, 1, 1, gallant19_124_pixels, 0 };
static u_int32_t gallant19_125_pixels[] = { 0, 0, 0x38000000, 0x0c000000,
0x0c000000, 0x18000000, 0x18000000, 0x18000000, 0x0c000000, 0x0e000000,
0x0c000000, 0x18000000, 0x18000000, 0x18000000, 0x0c000000, 0x0c000000,
0x38000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_125 = { 12, 22, 1, 1, gallant19_125_pixels, 0 };
static u_int32_t gallant19_126_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x1c200000, 0x3e600000, 0x36c00000, 0x67c00000, 0x43800000, 0, 0, 0, 0,
0, 0, 0 };
static struct raster gallant19_126 = { 12, 22, 1, 1, gallant19_126_pixels, 0 };
static u_int32_t gallant19_127_pixels[] = { 0xaaa00000, 0x55500000,
0xaaa00000, 0x55500000, 0xaaa00000, 0x55500000, 0xaaa00000, 0x55500000,
0xaaa00000, 0x55500000, 0xaaa00000, 0x55500000, 0xaaa00000, 0x55500000,
0xaaa00000, 0x55500000, 0xaaa00000, 0x55500000, 0xaaa00000, 0x55500000,
0xaaa00000, 0x55500000 };
static struct raster gallant19_127 = { 12, 22, 1, 1, gallant19_127_pixels, 0 };
static u_int32_t gallant19_128_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_128 = { 12, 22, 1, 1, gallant19_128_pixels, 0 };
static u_int32_t gallant19_129_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_129 = { 12, 22, 1, 1, gallant19_129_pixels, 0 };
static u_int32_t gallant19_130_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_130 = { 12, 22, 1, 1, gallant19_130_pixels, 0 };
static u_int32_t gallant19_131_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_131 = { 12, 22, 1, 1, gallant19_131_pixels, 0 };
static u_int32_t gallant19_132_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_132 = { 12, 22, 1, 1, gallant19_132_pixels, 0 };
static u_int32_t gallant19_133_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_133 = { 12, 22, 1, 1, gallant19_133_pixels, 0 };
static u_int32_t gallant19_134_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_134 = { 12, 22, 1, 1, gallant19_134_pixels, 0 };
static u_int32_t gallant19_135_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_135 = { 12, 22, 1, 1, gallant19_135_pixels, 0 };
static u_int32_t gallant19_136_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_136 = { 12, 22, 1, 1, gallant19_136_pixels, 0 };
static u_int32_t gallant19_137_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_137 = { 12, 22, 1, 1, gallant19_137_pixels, 0 };
static u_int32_t gallant19_138_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_138 = { 12, 22, 1, 1, gallant19_138_pixels, 0 };
static u_int32_t gallant19_139_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_139 = { 12, 22, 1, 1, gallant19_139_pixels, 0 };
static u_int32_t gallant19_140_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_140 = { 12, 22, 1, 1, gallant19_140_pixels, 0 };
static u_int32_t gallant19_141_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_141 = { 12, 22, 1, 1, gallant19_141_pixels, 0 };
static u_int32_t gallant19_142_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_142 = { 12, 22, 1, 1, gallant19_142_pixels, 0 };
static u_int32_t gallant19_143_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_143 = { 12, 22, 1, 1, gallant19_143_pixels, 0 };
static u_int32_t gallant19_144_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_144 = { 12, 22, 1, 1, gallant19_144_pixels, 0 };
static u_int32_t gallant19_145_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_145 = { 12, 22, 1, 1, gallant19_145_pixels, 0 };
static u_int32_t gallant19_146_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_146 = { 12, 22, 1, 1, gallant19_146_pixels, 0 };
static u_int32_t gallant19_147_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_147 = { 12, 22, 1, 1, gallant19_147_pixels, 0 };
static u_int32_t gallant19_148_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_148 = { 12, 22, 1, 1, gallant19_148_pixels, 0 };
static u_int32_t gallant19_149_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_149 = { 12, 22, 1, 1, gallant19_149_pixels, 0 };
static u_int32_t gallant19_150_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_150 = { 12, 22, 1, 1, gallant19_150_pixels, 0 };
static u_int32_t gallant19_151_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_151 = { 12, 22, 1, 1, gallant19_151_pixels, 0 };
static u_int32_t gallant19_152_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_152 = { 12, 22, 1, 1, gallant19_152_pixels, 0 };
static u_int32_t gallant19_153_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_153 = { 12, 22, 1, 1, gallant19_153_pixels, 0 };
static u_int32_t gallant19_154_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_154 = { 12, 22, 1, 1, gallant19_154_pixels, 0 };
static u_int32_t gallant19_155_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_155 = { 12, 22, 1, 1, gallant19_155_pixels, 0 };
static u_int32_t gallant19_156_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_156 = { 12, 22, 1, 1, gallant19_156_pixels, 0 };
static u_int32_t gallant19_157_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_157 = { 12, 22, 1, 1, gallant19_157_pixels, 0 };
static u_int32_t gallant19_158_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_158 = { 12, 22, 1, 1, gallant19_158_pixels, 0 };
static u_int32_t gallant19_159_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_159 = { 12, 22, 1, 1, gallant19_159_pixels, 0 };
static u_int32_t gallant19_160_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_160 = { 12, 22, 1, 1, gallant19_160_pixels, 0 };
static u_int32_t gallant19_161_pixels[] = { 0, 0, 0, 0x06000000, 0x06000000,
0, 0, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0, 0, 0, 0 };
static struct raster gallant19_161 = { 12, 22, 1, 1, gallant19_161_pixels, 0 };
static u_int32_t gallant19_162_pixels[] = { 0, 0, 0, 0x01000000, 0x01000000,
0x03000000, 0x02000000, 0x1f000000, 0x37800000, 0x25800000, 0x64000000,
0x6c000000, 0x68000000, 0x78800000, 0x39800000, 0x1f000000, 0x10000000,
0x30000000, 0x20000000, 0x20000000, 0, 0 };
static struct raster gallant19_162 = { 12, 22, 1, 1, gallant19_162_pixels, 0 };
static u_int32_t gallant19_163_pixels[] = { 0, 0x06000000, 0x0c000000,
0x10000000, 0x10000000, 0x30000000, 0x30000000, 0x30000000, 0x3e000000,
0x7c000000, 0x18000000, 0x18000000, 0x18000000, 0x18000000, 0x3f200000,
0x3fe00000, 0x31c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_163 = { 12, 22, 1, 1, gallant19_163_pixels, 0 };
static u_int32_t gallant19_164_pixels[] = { 0, 0, 0, 0, 0, 0x60200000,
0x77400000, 0x3b800000, 0x11c00000, 0x30c00000, 0x30c00000, 0x38800000,
0x1dc00000, 0x2ee00000, 0x40600000, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_164 = { 12, 22, 1, 1, gallant19_164_pixels, 0 };
static u_int32_t gallant19_165_pixels[] = { 0, 0, 0, 0xf0700000, 0x60200000,
0x30400000, 0x18800000, 0x18800000, 0x0d000000, 0x06000000, 0x3fc00000,
0x06000000, 0x3fc00000, 0x06000000, 0x06000000, 0x06000000, 0x0f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_165 = { 12, 22, 1, 1, gallant19_165_pixels, 0 };
static u_int32_t gallant19_166_pixels[] = { 0, 0, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0, 0, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000 };
static struct raster gallant19_166 = { 12, 22, 1, 1, gallant19_166_pixels, 0 };
static u_int32_t gallant19_167_pixels[] = { 0, 0, 0x0fe00000, 0x18600000,
0x30200000, 0x38200000, 0x1e000000, 0x1f800000, 0x31c00000, 0x60e00000,
0x70600000, 0x38c00000, 0x1f800000, 0x07800000, 0x41c00000, 0x40c00000,
0x61800000, 0x7f000000, 0, 0, 0, 0 };
static struct raster gallant19_167 = { 12, 22, 1, 1, gallant19_167_pixels, 0 };
static u_int32_t gallant19_168_pixels[] = { 0x19800000, 0x19800000, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_168 = { 12, 22, 1, 1, gallant19_168_pixels, 0 };
static u_int32_t gallant19_169_pixels[] = { 0, 0, 0, 0x0f000000, 0x10800000,
0x20400000, 0x2f400000, 0x59a00000, 0x59a00000, 0x58200000, 0x58200000,
0x59a00000, 0x59a00000, 0x2f400000, 0x20400000, 0x10800000, 0x0f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_169 = { 12, 22, 1, 1, gallant19_169_pixels, 0 };
static u_int32_t gallant19_170_pixels[] = { 0, 0, 0, 0x0e000000, 0x1f000000,
0x13800000, 0x0f800000, 0x19800000, 0x31800000, 0x3fc00000, 0x1ec00000,
0, 0, 0x3fe00000, 0x7fc00000, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_170 = { 12, 22, 1, 1, gallant19_170_pixels, 0 };
static u_int32_t gallant19_171_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0,
0x02200000, 0x04400000, 0x08800000, 0x11000000, 0x33000000, 0x19800000,
0x0cc00000, 0x06600000, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_171 = { 12, 22, 1, 1, gallant19_171_pixels, 0 };
static u_int32_t gallant19_172_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x7fe00000, 0x7fe00000, 0x00600000, 0x00600000, 0x00600000, 0x00600000,
0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_172 = { 12, 22, 1, 1, gallant19_172_pixels, 0 };
static u_int32_t gallant19_173_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x1f800000, 0x1f800000, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_173 = { 12, 22, 1, 1, gallant19_173_pixels, 0 };
static u_int32_t gallant19_174_pixels[] = { 0, 0, 0, 0x0f000000, 0x10800000,
0x20400000, 0x3f400000, 0x59a00000, 0x59a00000, 0x5fa00000, 0x5b200000,
0x5b200000, 0x59a00000, 0x39c00000, 0x20400000, 0x10800000, 0x0f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_174 = { 12, 22, 1, 1, gallant19_174_pixels, 0 };
static u_int32_t gallant19_175_pixels[] = { 0x3fc00000, 0x3fc00000, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_175 = { 12, 22, 1, 1, gallant19_175_pixels, 0 };
static u_int32_t gallant19_176_pixels[] = { 0, 0, 0x0e000000, 0x17000000,
0x23800000, 0x61800000, 0x61800000, 0x71000000, 0x3a000000, 0x1c000000,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_176 = { 12, 22, 1, 1, gallant19_176_pixels, 0 };
static u_int32_t gallant19_177_pixels[] = { 0, 0, 0, 0, 0, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x7fe00000, 0x7fe00000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0, 0x7ff00000, 0x7ff00000, 0, 0, 0,
0 };
static struct raster gallant19_177 = { 12, 22, 1, 1, gallant19_177_pixels, 0 };
static u_int32_t gallant19_178_pixels[] = { 0, 0, 0, 0x0e000000, 0x1f000000,
0x13000000, 0x03000000, 0x06000000, 0x0c000000, 0x18000000, 0x1f000000,
0x1f000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_178 = { 12, 22, 1, 1, gallant19_178_pixels, 0 };
static u_int32_t gallant19_179_pixels[] = { 0, 0, 0, 0x0e000000, 0x1f000000,
0x13000000, 0x03000000, 0x06000000, 0x03000000, 0x13000000, 0x1f000000,
0x0e000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_179 = { 12, 22, 1, 1, gallant19_179_pixels, 0 };
static u_int32_t gallant19_180_pixels[] = { 0x03800000, 0x0f000000,
0x1c000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_180 = { 12, 22, 1, 1, gallant19_180_pixels, 0 };
static u_int32_t gallant19_181_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x79e00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x39c00000, 0x3e600000, 0x30000000, 0x30000000, 0x30000000,
0x30000000, 0x60000000 };
static struct raster gallant19_181 = { 12, 22, 1, 1, gallant19_181_pixels, 0 };
static u_int32_t gallant19_182_pixels[] = { 0, 0, 0, 0x1fc00000, 0x3ec00000,
0x7ec00000, 0x7ec00000, 0x7ec00000, 0x3ec00000, 0x1ec00000, 0x06c00000,
0x06c00000, 0x06c00000, 0x06c00000, 0x06c00000, 0x06c00000, 0x06c00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_182 = { 12, 22, 1, 1, gallant19_182_pixels, 0 };
static u_int32_t gallant19_183_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0x06000000, 0x0f000000, 0x0f000000, 0x06000000, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_183 = { 12, 22, 1, 1, gallant19_183_pixels, 0 };
static u_int32_t gallant19_184_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0x02000000, 0x03000000, 0x01800000, 0x09800000,
0x07000000 };
static struct raster gallant19_184 = { 12, 22, 1, 1, gallant19_184_pixels, 0 };
static u_int32_t gallant19_185_pixels[] = { 0, 0, 0, 0x06000000, 0x0e000000,
0x0e000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x0f000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_185 = { 12, 22, 1, 1, gallant19_185_pixels, 0 };
static u_int32_t gallant19_186_pixels[] = { 0, 0, 0, 0x07000000, 0x0b800000,
0x11c00000, 0x30c00000, 0x30c00000, 0x38800000, 0x1d000000, 0x0e000000,
0, 0, 0x3fe00000, 0x7fc00000, 0, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_186 = { 12, 22, 1, 1, gallant19_186_pixels, 0 };
static u_int32_t gallant19_187_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0,
0x66000000, 0x33000000, 0x19800000, 0x0cc00000, 0x08800000, 0x11000000,
0x22000000, 0x44000000, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_187 = { 12, 22, 1, 1, gallant19_187_pixels, 0 };
static u_int32_t gallant19_188_pixels[] = { 0, 0x18000000, 0x38000000,
0x38000000, 0x18000000, 0x18200000, 0x18600000, 0x18c00000, 0x19800000,
0x3f400000, 0x06c00000, 0x0dc00000, 0x19c00000, 0x32c00000, 0x64c00000,
0x47e00000, 0x00c00000, 0x00c00000, 0, 0, 0, 0 };
static struct raster gallant19_188 = { 12, 22, 1, 1, gallant19_188_pixels, 0 };
static u_int32_t gallant19_189_pixels[] = { 0, 0x30000000, 0x70000000,
0x70000000, 0x30200000, 0x30600000, 0x30c00000, 0x31800000, 0x33000000,
0x7fc00000, 0x0fe00000, 0x1b600000, 0x32600000, 0x60c00000, 0x41800000,
0x03000000, 0x03e00000, 0x03e00000, 0, 0, 0, 0 };
static struct raster gallant19_189 = { 12, 22, 1, 1, gallant19_189_pixels, 0 };
static u_int32_t gallant19_190_pixels[] = { 0, 0x38000000, 0x7c000000,
0x4c000000, 0x0c000000, 0x18200000, 0x0c600000, 0x4cc00000, 0x7d800000,
0x3b400000, 0x06c00000, 0x0dc00000, 0x19c00000, 0x32c00000, 0x64c00000,
0x47e00000, 0x00c00000, 0x00c00000, 0, 0, 0, 0 };
static struct raster gallant19_190 = { 12, 22, 1, 1, gallant19_190_pixels, 0 };
static u_int32_t gallant19_191_pixels[] = { 0, 0, 0, 0x03000000, 0x03000000,
0, 0, 0x03000000, 0x03000000, 0x06000000, 0x0c000000, 0x18000000,
0x30000000, 0x30000000, 0x30400000, 0x39c00000, 0x1f800000, 0x0f000000,
0, 0, 0, 0 };
static struct raster gallant19_191 = { 12, 22, 1, 1, gallant19_191_pixels, 0 };
static u_int32_t gallant19_192_pixels[] = { 0x1c000000, 0x0f000000,
0x03800000, 0x06000000, 0x06000000, 0x0b000000, 0x0b000000, 0x09000000,
0x11800000, 0x11800000, 0x10800000, 0x3fc00000, 0x20c00000, 0x20400000,
0x40600000, 0x40600000, 0xe0f00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_192 = { 12, 22, 1, 1, gallant19_192_pixels, 0 };
static u_int32_t gallant19_193_pixels[] = { 0x03800000, 0x0f000000,
0x1c000000, 0x06000000, 0x06000000, 0x0b000000, 0x0b000000, 0x09000000,
0x11800000, 0x11800000, 0x10800000, 0x3fc00000, 0x20c00000, 0x20400000,
0x40600000, 0x40600000, 0xe0f00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_193 = { 12, 22, 1, 1, gallant19_193_pixels, 0 };
static u_int32_t gallant19_194_pixels[] = { 0x06000000, 0x0f000000,
0x19800000, 0x06000000, 0x06000000, 0x0b000000, 0x0b000000, 0x09000000,
0x11800000, 0x11800000, 0x10800000, 0x3fc00000, 0x20c00000, 0x20400000,
0x40600000, 0x40600000, 0xe0f00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_194 = { 12, 22, 1, 1, gallant19_194_pixels, 0 };
static u_int32_t gallant19_195_pixels[] = { 0x0cc00000, 0x1f800000,
0x33000000, 0x06000000, 0x06000000, 0x0b000000, 0x0b000000, 0x09000000,
0x11800000, 0x11800000, 0x10800000, 0x3fc00000, 0x20c00000, 0x20400000,
0x40600000, 0x40600000, 0xe0f00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_195 = { 12, 22, 1, 1, gallant19_195_pixels, 0 };
static u_int32_t gallant19_196_pixels[] = { 0x19800000, 0x19800000, 0,
0x06000000, 0x06000000, 0x0b000000, 0x0b000000, 0x09000000, 0x11800000,
0x11800000, 0x10800000, 0x3fc00000, 0x20c00000, 0x20400000, 0x40600000,
0x40600000, 0xe0f00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_196 = { 12, 22, 1, 1, gallant19_196_pixels, 0 };
static u_int32_t gallant19_197_pixels[] = { 0x06000000, 0x0f000000,
0x19800000, 0x0f000000, 0x06000000, 0x0b000000, 0x0b000000, 0x09000000,
0x11800000, 0x11800000, 0x10800000, 0x3fc00000, 0x20c00000, 0x20400000,
0x40600000, 0x40600000, 0xe0f00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_197 = { 12, 22, 1, 1, gallant19_197_pixels, 0 };
static u_int32_t gallant19_198_pixels[] = { 0, 0, 0, 0x0fe00000, 0x0e200000,
0x16200000, 0x16000000, 0x16000000, 0x16400000, 0x27c00000, 0x26400000,
0x3e000000, 0x26000000, 0x46000000, 0x46100000, 0x46100000, 0xe7f00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_198 = { 12, 22, 1, 1, gallant19_198_pixels, 0 };
static u_int32_t gallant19_199_pixels[] = { 0, 0, 0, 0x0fc00000, 0x10600000,
0x20200000, 0x20000000, 0x60000000, 0x60000000, 0x60000000, 0x60000000,
0x60000000, 0x60000000, 0x20000000, 0x30200000, 0x18400000, 0x0f800000,
0x02000000, 0x03000000, 0x01800000, 0x09800000, 0x07000000 };
static struct raster gallant19_199 = { 12, 22, 1, 1, gallant19_199_pixels, 0 };
static u_int32_t gallant19_200_pixels[] = { 0x1c000000, 0x0f000000,
0x03800000, 0x7fc00000, 0x30400000, 0x30400000, 0x30000000, 0x30000000,
0x30800000, 0x3f800000, 0x30800000, 0x30000000, 0x30000000, 0x30000000,
0x30200000, 0x30200000, 0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_200 = { 12, 22, 1, 1, gallant19_200_pixels, 0 };
static u_int32_t gallant19_201_pixels[] = { 0x03800000, 0x0f000000,
0x1c000000, 0x7fc00000, 0x30400000, 0x30400000, 0x30000000, 0x30000000,
0x30800000, 0x3f800000, 0x30800000, 0x30000000, 0x30000000, 0x30000000,
0x30200000, 0x30200000, 0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_201 = { 12, 22, 1, 1, gallant19_201_pixels, 0 };
static u_int32_t gallant19_202_pixels[] = { 0x06000000, 0x0f000000,
0x19800000, 0x7fc00000, 0x30400000, 0x30400000, 0x30000000, 0x30000000,
0x30800000, 0x3f800000, 0x30800000, 0x30000000, 0x30000000, 0x30000000,
0x30200000, 0x30200000, 0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_202 = { 12, 22, 1, 1, gallant19_202_pixels, 0 };
static u_int32_t gallant19_203_pixels[] = { 0x19800000, 0x19800000, 0,
0x7fc00000, 0x30400000, 0x30400000, 0x30000000, 0x30000000, 0x30800000,
0x3f800000, 0x30800000, 0x30000000, 0x30000000, 0x30000000, 0x30200000,
0x30200000, 0x7fe00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_203 = { 12, 22, 1, 1, gallant19_203_pixels, 0 };
static u_int32_t gallant19_204_pixels[] = { 0x1c000000, 0x0f000000,
0x03800000, 0x1f800000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_204 = { 12, 22, 1, 1, gallant19_204_pixels, 0 };
static u_int32_t gallant19_205_pixels[] = { 0x03800000, 0x0f000000,
0x1c000000, 0x1f800000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_205 = { 12, 22, 1, 1, gallant19_205_pixels, 0 };
static u_int32_t gallant19_206_pixels[] = { 0x06000000, 0x0f000000,
0x19800000, 0x1f800000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_206 = { 12, 22, 1, 1, gallant19_206_pixels, 0 };
static u_int32_t gallant19_207_pixels[] = { 0x19800000, 0x19800000, 0,
0x1f800000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_207 = { 12, 22, 1, 1, gallant19_207_pixels, 0 };
static u_int32_t gallant19_208_pixels[] = { 0, 0, 0, 0xff000000, 0x61c00000,
0x60c00000, 0x60600000, 0x60600000, 0x60600000, 0xf8600000, 0xf8600000,
0x60600000, 0x60600000, 0x60600000, 0x60400000, 0x61800000, 0xfe000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_208 = { 12, 22, 1, 1, gallant19_208_pixels, 0 };
static u_int32_t gallant19_209_pixels[] = { 0x0cc00000, 0x1f800000,
0x33000000, 0xc0700000, 0x60200000, 0x70200000, 0x78200000, 0x58200000,
0x4c200000, 0x46200000, 0x47200000, 0x43200000, 0x41a00000, 0x40e00000,
0x40e00000, 0x40600000, 0xe0300000, 0, 0, 0, 0, 0 };
static struct raster gallant19_209 = { 12, 22, 1, 1, gallant19_209_pixels, 0 };
static u_int32_t gallant19_210_pixels[] = { 0x1c000000, 0x0f000000,
0x03800000, 0x0f000000, 0x11c00000, 0x20c00000, 0x20600000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x20400000,
0x30400000, 0x18800000, 0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_210 = { 12, 22, 1, 1, gallant19_210_pixels, 0 };
static u_int32_t gallant19_211_pixels[] = { 0x03800000, 0x0f000000,
0x1c000000, 0x0f000000, 0x11c00000, 0x20c00000, 0x20600000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x20400000,
0x30400000, 0x18800000, 0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_211 = { 12, 22, 1, 1, gallant19_211_pixels, 0 };
static u_int32_t gallant19_212_pixels[] = { 0x06000000, 0x0f000000,
0x19800000, 0x0f000000, 0x11c00000, 0x20c00000, 0x20600000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x20400000,
0x30400000, 0x18800000, 0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_212 = { 12, 22, 1, 1, gallant19_212_pixels, 0 };
static u_int32_t gallant19_213_pixels[] = { 0x0cc00000, 0x1f800000,
0x33000000, 0x0f000000, 0x11c00000, 0x20c00000, 0x20600000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x20400000,
0x30400000, 0x18800000, 0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_213 = { 12, 22, 1, 1, gallant19_213_pixels, 0 };
static u_int32_t gallant19_214_pixels[] = { 0x19800000, 0x19800000, 0,
0x0f000000, 0x11c00000, 0x20c00000, 0x20600000, 0x60600000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x20400000, 0x30400000,
0x18800000, 0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_214 = { 12, 22, 1, 1, gallant19_214_pixels, 0 };
static u_int32_t gallant19_215_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x60600000,
0x30c00000, 0x19800000, 0x0f000000, 0x06000000, 0x0f000000, 0x19800000,
0x30c00000, 0x60600000, 0, 0, 0, 0, 0, 0 };
static struct raster gallant19_215 = { 12, 22, 1, 1, gallant19_215_pixels, 0 };
static u_int32_t gallant19_216_pixels[] = { 0, 0, 0x00600000, 0x0fc00000,
0x11c00000, 0x21c00000, 0x21e00000, 0x63600000, 0x63600000, 0x66600000,
0x6c600000, 0x6c600000, 0x78600000, 0x38400000, 0x30400000, 0x38800000,
0x6f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_216 = { 12, 22, 1, 1, gallant19_216_pixels, 0 };
static u_int32_t gallant19_217_pixels[] = { 0x1c000000, 0x0f000000,
0x03800000, 0xf0700000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x70400000, 0x3fc00000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_217 = { 12, 22, 1, 1, gallant19_217_pixels, 0 };
static u_int32_t gallant19_218_pixels[] = { 0x03800000, 0x0f000000,
0x1c000000, 0xf0700000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x70400000, 0x3fc00000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_218 = { 12, 22, 1, 1, gallant19_218_pixels, 0 };
static u_int32_t gallant19_219_pixels[] = { 0x06000000, 0x0f000000,
0x19800000, 0xf0700000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x70400000, 0x3fc00000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_219 = { 12, 22, 1, 1, gallant19_219_pixels, 0 };
static u_int32_t gallant19_220_pixels[] = { 0x19800000, 0x19800000, 0,
0xf0700000, 0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000,
0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x60200000, 0x70400000,
0x3fc00000, 0x1f800000, 0, 0, 0, 0, 0 };
static struct raster gallant19_220 = { 12, 22, 1, 1, gallant19_220_pixels, 0 };
static u_int32_t gallant19_221_pixels[] = { 0x03800000, 0x0f000000,
0x1c000000, 0xf0700000, 0x60200000, 0x30400000, 0x18800000, 0x18800000,
0x0d000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x0f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_221 = { 12, 22, 1, 1, gallant19_221_pixels, 0 };
static u_int32_t gallant19_222_pixels[] = { 0, 0, 0, 0x78000000, 0x30000000,
0x30000000, 0x3f800000, 0x30c00000, 0x30600000, 0x30600000, 0x30600000,
0x30600000, 0x30c00000, 0x3f800000, 0x30000000, 0x30000000, 0x78000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_222 = { 12, 22, 1, 1, gallant19_222_pixels, 0 };
static u_int32_t gallant19_223_pixels[] = { 0, 0, 0x0f000000, 0x19800000,
0x19800000, 0x31800000, 0x31800000, 0x33800000, 0x36000000, 0x36000000,
0x36000000, 0x33800000, 0x31c00000, 0x30e00000, 0x34600000, 0x36600000,
0x77c00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_223 = { 12, 22, 1, 1, gallant19_223_pixels, 0 };
static u_int32_t gallant19_224_pixels[] = { 0, 0, 0, 0x1c000000, 0x0f000000,
0x03800000, 0, 0x0f800000, 0x18c00000, 0x10c00000, 0x03c00000,
0x1cc00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1ee00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_224 = { 12, 22, 1, 1, gallant19_224_pixels, 0 };
static u_int32_t gallant19_225_pixels[] = { 0, 0, 0, 0x03800000, 0x0f000000,
0x1c000000, 0, 0x0f800000, 0x18c00000, 0x10c00000, 0x03c00000,
0x1cc00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1ee00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_225 = { 12, 22, 1, 1, gallant19_225_pixels, 0 };
static u_int32_t gallant19_226_pixels[] = { 0, 0, 0, 0x06000000, 0x0f000000,
0x19800000, 0, 0x0f800000, 0x18c00000, 0x10c00000, 0x03c00000,
0x1cc00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1ee00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_226 = { 12, 22, 1, 1, gallant19_226_pixels, 0 };
static u_int32_t gallant19_227_pixels[] = { 0, 0, 0, 0x0cc00000, 0x1f800000,
0x33000000, 0, 0x0f800000, 0x18c00000, 0x10c00000, 0x03c00000,
0x1cc00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1ee00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_227 = { 12, 22, 1, 1, gallant19_227_pixels, 0 };
static u_int32_t gallant19_228_pixels[] = { 0, 0, 0, 0x19800000, 0x19800000,
0, 0, 0x0f800000, 0x18c00000, 0x10c00000, 0x03c00000, 0x1cc00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1ee00000, 0, 0, 0, 0,
0 };
static struct raster gallant19_228 = { 12, 22, 1, 1, gallant19_228_pixels, 0 };
static u_int32_t gallant19_229_pixels[] = { 0, 0x06000000, 0x0f000000,
0x19800000, 0x0f000000, 0x06000000, 0, 0x0f800000, 0x18c00000,
0x10c00000, 0x03c00000, 0x1cc00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x39c00000, 0x1ee00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_229 = { 12, 22, 1, 1, gallant19_229_pixels, 0 };
static u_int32_t gallant19_230_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x1f800000,
0x36400000, 0x26600000, 0x0e600000, 0x3fe00000, 0x66000000, 0x66000000,
0x66000000, 0x67600000, 0x3fc00000, 0, 0, 0, 0, 0 };
static struct raster gallant19_230 = { 12, 22, 1, 1, gallant19_230_pixels, 0 };
static u_int32_t gallant19_231_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x1f800000,
0x31c00000, 0x20c00000, 0x60000000, 0x60000000, 0x60000000, 0x60000000,
0x70400000, 0x30c00000, 0x1f800000, 0x02000000, 0x03000000, 0x01800000,
0x09800000, 0x07000000 };
static struct raster gallant19_231 = { 12, 22, 1, 1, gallant19_231_pixels, 0 };
static u_int32_t gallant19_232_pixels[] = { 0, 0, 0, 0x1c000000, 0x0f000000,
0x03800000, 0, 0x0f000000, 0x30c00000, 0x60600000, 0x60600000,
0x7fe00000, 0x60000000, 0x60000000, 0x30000000, 0x18600000, 0x0f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_232 = { 12, 22, 1, 1, gallant19_232_pixels, 0 };
static u_int32_t gallant19_233_pixels[] = { 0, 0, 0, 0x03800000, 0x0f000000,
0x1c000000, 0, 0x0f000000, 0x30c00000, 0x60600000, 0x60600000,
0x7fe00000, 0x60000000, 0x60000000, 0x30000000, 0x18600000, 0x0f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_233 = { 12, 22, 1, 1, gallant19_233_pixels, 0 };
static u_int32_t gallant19_234_pixels[] = { 0, 0, 0, 0x06000000, 0x0f000000,
0x19800000, 0, 0x0f000000, 0x30c00000, 0x60600000, 0x60600000,
0x7fe00000, 0x60000000, 0x60000000, 0x30000000, 0x18600000, 0x0f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_234 = { 12, 22, 1, 1, gallant19_234_pixels, 0 };
static u_int32_t gallant19_235_pixels[] = { 0, 0, 0, 0x19800000, 0x19800000,
0, 0, 0x0f000000, 0x30c00000, 0x60600000, 0x60600000, 0x7fe00000,
0x60000000, 0x60000000, 0x30000000, 0x18600000, 0x0f800000, 0, 0, 0, 0,
0 };
static struct raster gallant19_235 = { 12, 22, 1, 1, gallant19_235_pixels, 0 };
static u_int32_t gallant19_236_pixels[] = { 0, 0, 0, 0x1c000000, 0x0f000000,
0x03800000, 0, 0x1e000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x1f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_236 = { 12, 22, 1, 1, gallant19_236_pixels, 0 };
static u_int32_t gallant19_237_pixels[] = { 0, 0, 0, 0x03800000, 0x0f000000,
0x1c000000, 0, 0x1e000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x1f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_237 = { 12, 22, 1, 1, gallant19_237_pixels, 0 };
static u_int32_t gallant19_238_pixels[] = { 0, 0, 0, 0x06000000, 0x0f000000,
0x19800000, 0, 0x1e000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x1f800000,
0, 0, 0, 0, 0 };
static struct raster gallant19_238 = { 12, 22, 1, 1, gallant19_238_pixels, 0 };
static u_int32_t gallant19_239_pixels[] = { 0, 0, 0, 0x19800000, 0x19800000,
0, 0, 0x1e000000, 0x06000000, 0x06000000, 0x06000000, 0x06000000,
0x06000000, 0x06000000, 0x06000000, 0x06000000, 0x1f800000, 0, 0, 0, 0,
0 };
static struct raster gallant19_239 = { 12, 22, 1, 1, gallant19_239_pixels, 0 };
static u_int32_t gallant19_240_pixels[] = { 0, 0, 0x30c00000, 0x1f800000,
0x06000000, 0x1f000000, 0x31800000, 0x01c00000, 0x0fc00000, 0x10e00000,
0x20e00000, 0x60600000, 0x60600000, 0x60600000, 0x70400000, 0x38800000,
0x1f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_240 = { 12, 22, 1, 1, gallant19_240_pixels, 0 };
static u_int32_t gallant19_241_pixels[] = { 0, 0, 0, 0x0cc00000, 0x1f800000,
0x33000000, 0, 0x27800000, 0x79c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x79e00000,
0, 0, 0, 0, 0 };
static struct raster gallant19_241 = { 12, 22, 1, 1, gallant19_241_pixels, 0 };
static u_int32_t gallant19_242_pixels[] = { 0, 0, 0, 0x1c000000, 0x0f000000,
0x03800000, 0, 0x0f800000, 0x11c00000, 0x20e00000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x70400000, 0x38800000, 0x1f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_242 = { 12, 22, 1, 1, gallant19_242_pixels, 0 };
static u_int32_t gallant19_243_pixels[] = { 0, 0, 0, 0x03800000, 0x0f000000,
0x1c000000, 0, 0x0f800000, 0x11c00000, 0x20e00000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x70400000, 0x38800000, 0x1f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_243 = { 12, 22, 1, 1, gallant19_243_pixels, 0 };
static u_int32_t gallant19_244_pixels[] = { 0, 0, 0, 0x06000000, 0x0f000000,
0x19800000, 0, 0x0f800000, 0x11c00000, 0x20e00000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x70400000, 0x38800000, 0x1f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_244 = { 12, 22, 1, 1, gallant19_244_pixels, 0 };
static u_int32_t gallant19_245_pixels[] = { 0, 0, 0, 0x0cc00000, 0x1f800000,
0x33000000, 0, 0x0f800000, 0x11c00000, 0x20e00000, 0x60600000,
0x60600000, 0x60600000, 0x60600000, 0x70400000, 0x38800000, 0x1f000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_245 = { 12, 22, 1, 1, gallant19_245_pixels, 0 };
static u_int32_t gallant19_246_pixels[] = { 0, 0, 0, 0x19800000, 0x19800000,
0, 0, 0x0f800000, 0x11c00000, 0x20e00000, 0x60600000, 0x60600000,
0x60600000, 0x60600000, 0x70400000, 0x38800000, 0x1f000000, 0, 0, 0, 0,
0 };
static struct raster gallant19_246 = { 12, 22, 1, 1, gallant19_246_pixels, 0 };
static u_int32_t gallant19_247_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x06000000,
0x06000000, 0, 0, 0x7fe00000, 0x7fe00000, 0, 0, 0x06000000, 0x06000000,
0, 0, 0, 0, 0 };
static struct raster gallant19_247 = { 12, 22, 1, 1, gallant19_247_pixels, 0 };
static u_int32_t gallant19_248_pixels[] = { 0, 0, 0, 0, 0, 0, 0, 0x0fe00000,
0x11c00000, 0x21e00000, 0x63600000, 0x66600000, 0x66600000, 0x6c600000,
0x78400000, 0x38800000, 0x7f000000, 0, 0, 0, 0, 0 };
static struct raster gallant19_248 = { 12, 22, 1, 1, gallant19_248_pixels, 0 };
static u_int32_t gallant19_249_pixels[] = { 0, 0, 0, 0x1c000000, 0x0f000000,
0x03800000, 0, 0x79e00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1e600000,
0, 0, 0, 0, 0 };
static struct raster gallant19_249 = { 12, 22, 1, 1, gallant19_249_pixels, 0 };
static u_int32_t gallant19_250_pixels[] = { 0, 0, 0, 0x03800000, 0x0f000000,
0x1c000000, 0, 0x79e00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1e600000,
0, 0, 0, 0, 0 };
static struct raster gallant19_250 = { 12, 22, 1, 1, gallant19_250_pixels, 0 };
static u_int32_t gallant19_251_pixels[] = { 0, 0, 0, 0x06000000, 0x0f000000,
0x19800000, 0, 0x79e00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1e600000,
0, 0, 0, 0, 0 };
static struct raster gallant19_251 = { 12, 22, 1, 1, gallant19_251_pixels, 0 };
static u_int32_t gallant19_252_pixels[] = { 0, 0, 0, 0x19800000, 0x19800000,
0, 0, 0x79e00000, 0x30c00000, 0x30c00000, 0x30c00000, 0x30c00000,
0x30c00000, 0x30c00000, 0x30c00000, 0x39c00000, 0x1e600000, 0, 0, 0, 0,
0 };
static struct raster gallant19_252 = { 12, 22, 1, 1, gallant19_252_pixels, 0 };
static u_int32_t gallant19_253_pixels[] = { 0, 0, 0, 0x03800000, 0x0f000000,
0x1c000000, 0, 0xf0f00000, 0x60200000, 0x30400000, 0x30400000,
0x18800000, 0x18800000, 0x0d000000, 0x0d000000, 0x06000000, 0x06000000,
0x04000000, 0x0c000000, 0x08000000, 0x78000000, 0x70000000 };
static struct raster gallant19_253 = { 12, 22, 1, 1, gallant19_253_pixels, 0 };
static u_int32_t gallant19_254_pixels[] = { 0, 0, 0xe0000000, 0x60000000,
0x60000000, 0x60000000, 0x60000000, 0x6f800000, 0x71c00000, 0x60e00000,
0x60600000, 0x60600000, 0x60600000, 0x60600000, 0x60400000, 0x70800000,
0x7f000000, 0x60000000, 0x60000000, 0x60000000, 0x60000000, 0xf0000000 };
static struct raster gallant19_254 = { 12, 22, 1, 1, gallant19_254_pixels, 0 };
static u_int32_t gallant19_255_pixels[] = { 0, 0, 0, 0x19800000, 0x19800000,
0, 0, 0xf0f00000, 0x60200000, 0x30400000, 0x30400000, 0x18800000,
0x18800000, 0x0d000000, 0x0d000000, 0x06000000, 0x06000000, 0x04000000,
0x0c000000, 0x08000000, 0x78000000, 0x70000000 };
static struct raster gallant19_255 = { 12, 22, 1, 1, gallant19_255_pixels, 0 };

struct raster_font console_font = {
12, 22, 15, RASFONT_FIXEDWIDTH|RASFONT_NOVERTICALMOVEMENT,
{
{ &gallant19_0, 0, -15, 12, 0 },
{ &gallant19_1, 0, -15, 12, 0 },
{ &gallant19_2, 0, -15, 12, 0 },
{ &gallant19_3, 0, -15, 12, 0 },
{ &gallant19_4, 0, -15, 12, 0 },
{ &gallant19_5, 0, -15, 12, 0 },
{ &gallant19_6, 0, -15, 12, 0 },
{ &gallant19_7, 0, -15, 12, 0 },
{ &gallant19_8, 0, -15, 12, 0 },
{ &gallant19_9, 0, -15, 12, 0 },
{ &gallant19_10, 0, -15, 12, 0 },
{ &gallant19_11, 0, -15, 12, 0 },
{ &gallant19_12, 0, -15, 12, 0 },
{ &gallant19_13, 0, -15, 12, 0 },
{ &gallant19_14, 0, -15, 12, 0 },
{ &gallant19_15, 0, -15, 12, 0 },
{ &gallant19_16, 0, -15, 12, 0 },
{ &gallant19_17, 0, -15, 12, 0 },
{ &gallant19_18, 0, -15, 12, 0 },
{ &gallant19_19, 0, -15, 12, 0 },
{ &gallant19_20, 0, -15, 12, 0 },
{ &gallant19_21, 0, -15, 12, 0 },
{ &gallant19_22, 0, -15, 12, 0 },
{ &gallant19_23, 0, -15, 12, 0 },
{ &gallant19_24, 0, -15, 12, 0 },
{ &gallant19_25, 0, -15, 12, 0 },
{ &gallant19_26, 0, -15, 12, 0 },
{ &gallant19_27, 0, -15, 12, 0 },
{ &gallant19_28, 0, -15, 12, 0 },
{ &gallant19_29, 0, -15, 12, 0 },
{ &gallant19_30, 0, -15, 12, 0 },
{ &gallant19_31, 0, -15, 12, 0 },
{ &gallant19_32, 0, -15, 12, 0 },
{ &gallant19_33, 0, -15, 12, 0 },
{ &gallant19_34, 0, -15, 12, 0 },
{ &gallant19_35, 0, -15, 12, 0 },
{ &gallant19_36, 0, -15, 12, 0 },
{ &gallant19_37, 0, -15, 12, 0 },
{ &gallant19_38, 0, -15, 12, 0 },
{ &gallant19_39, 0, -15, 12, 0 },
{ &gallant19_40, 0, -15, 12, 0 },
{ &gallant19_41, 0, -15, 12, 0 },
{ &gallant19_42, 0, -15, 12, 0 },
{ &gallant19_43, 0, -15, 12, 0 },
{ &gallant19_44, 0, -15, 12, 0 },
{ &gallant19_45, 0, -15, 12, 0 },
{ &gallant19_46, 0, -15, 12, 0 },
{ &gallant19_47, 0, -15, 12, 0 },
{ &gallant19_48, 0, -15, 12, 0 },
{ &gallant19_49, 0, -15, 12, 0 },
{ &gallant19_50, 0, -15, 12, 0 },
{ &gallant19_51, 0, -15, 12, 0 },
{ &gallant19_52, 0, -15, 12, 0 },
{ &gallant19_53, 0, -15, 12, 0 },
{ &gallant19_54, 0, -15, 12, 0 },
{ &gallant19_55, 0, -15, 12, 0 },
{ &gallant19_56, 0, -15, 12, 0 },
{ &gallant19_57, 0, -15, 12, 0 },
{ &gallant19_58, 0, -15, 12, 0 },
{ &gallant19_59, 0, -15, 12, 0 },
{ &gallant19_60, 0, -15, 12, 0 },
{ &gallant19_61, 0, -15, 12, 0 },
{ &gallant19_62, 0, -15, 12, 0 },
{ &gallant19_63, 0, -15, 12, 0 },
{ &gallant19_64, 0, -15, 12, 0 },
{ &gallant19_65, 0, -15, 12, 0 },
{ &gallant19_66, 0, -15, 12, 0 },
{ &gallant19_67, 0, -15, 12, 0 },
{ &gallant19_68, 0, -15, 12, 0 },
{ &gallant19_69, 0, -15, 12, 0 },
{ &gallant19_70, 0, -15, 12, 0 },
{ &gallant19_71, 0, -15, 12, 0 },
{ &gallant19_72, 0, -15, 12, 0 },
{ &gallant19_73, 0, -15, 12, 0 },
{ &gallant19_74, 0, -15, 12, 0 },
{ &gallant19_75, 0, -15, 12, 0 },
{ &gallant19_76, 0, -15, 12, 0 },
{ &gallant19_77, 0, -15, 12, 0 },
{ &gallant19_78, 0, -15, 12, 0 },
{ &gallant19_79, 0, -15, 12, 0 },
{ &gallant19_80, 0, -15, 12, 0 },
{ &gallant19_81, 0, -15, 12, 0 },
{ &gallant19_82, 0, -15, 12, 0 },
{ &gallant19_83, 0, -15, 12, 0 },
{ &gallant19_84, 0, -15, 12, 0 },
{ &gallant19_85, 0, -15, 12, 0 },
{ &gallant19_86, 0, -15, 12, 0 },
{ &gallant19_87, 0, -15, 12, 0 },
{ &gallant19_88, 0, -15, 12, 0 },
{ &gallant19_89, 0, -15, 12, 0 },
{ &gallant19_90, 0, -15, 12, 0 },
{ &gallant19_91, 0, -15, 12, 0 },
{ &gallant19_92, 0, -15, 12, 0 },
{ &gallant19_93, 0, -15, 12, 0 },
{ &gallant19_94, 0, -15, 12, 0 },
{ &gallant19_95, 0, -15, 12, 0 },
{ &gallant19_96, 0, -15, 12, 0 },
{ &gallant19_97, 0, -15, 12, 0 },
{ &gallant19_98, 0, -15, 12, 0 },
{ &gallant19_99, 0, -15, 12, 0 },
{ &gallant19_100, 0, -15, 12, 0 },
{ &gallant19_101, 0, -15, 12, 0 },
{ &gallant19_102, 0, -15, 12, 0 },
{ &gallant19_103, 0, -15, 12, 0 },
{ &gallant19_104, 0, -15, 12, 0 },
{ &gallant19_105, 0, -15, 12, 0 },
{ &gallant19_106, 0, -15, 12, 0 },
{ &gallant19_107, 0, -15, 12, 0 },
{ &gallant19_108, 0, -15, 12, 0 },
{ &gallant19_109, 0, -15, 12, 0 },
{ &gallant19_110, 0, -15, 12, 0 },
{ &gallant19_111, 0, -15, 12, 0 },
{ &gallant19_112, 0, -15, 12, 0 },
{ &gallant19_113, 0, -15, 12, 0 },
{ &gallant19_114, 0, -15, 12, 0 },
{ &gallant19_115, 0, -15, 12, 0 },
{ &gallant19_116, 0, -15, 12, 0 },
{ &gallant19_117, 0, -15, 12, 0 },
{ &gallant19_118, 0, -15, 12, 0 },
{ &gallant19_119, 0, -15, 12, 0 },
{ &gallant19_120, 0, -15, 12, 0 },
{ &gallant19_121, 0, -15, 12, 0 },
{ &gallant19_122, 0, -15, 12, 0 },
{ &gallant19_123, 0, -15, 12, 0 },
{ &gallant19_124, 0, -15, 12, 0 },
{ &gallant19_125, 0, -15, 12, 0 },
{ &gallant19_126, 0, -15, 12, 0 },
{ &gallant19_127, 0, -15, 12, 0 },
{ &gallant19_128, 0, -15, 12, 0 },
{ &gallant19_129, 0, -15, 12, 0 },
{ &gallant19_130, 0, -15, 12, 0 },
{ &gallant19_131, 0, -15, 12, 0 },
{ &gallant19_132, 0, -15, 12, 0 },
{ &gallant19_133, 0, -15, 12, 0 },
{ &gallant19_134, 0, -15, 12, 0 },
{ &gallant19_135, 0, -15, 12, 0 },
{ &gallant19_136, 0, -15, 12, 0 },
{ &gallant19_137, 0, -15, 12, 0 },
{ &gallant19_138, 0, -15, 12, 0 },
{ &gallant19_139, 0, -15, 12, 0 },
{ &gallant19_140, 0, -15, 12, 0 },
{ &gallant19_141, 0, -15, 12, 0 },
{ &gallant19_142, 0, -15, 12, 0 },
{ &gallant19_143, 0, -15, 12, 0 },
{ &gallant19_144, 0, -15, 12, 0 },
{ &gallant19_145, 0, -15, 12, 0 },
{ &gallant19_146, 0, -15, 12, 0 },
{ &gallant19_147, 0, -15, 12, 0 },
{ &gallant19_148, 0, -15, 12, 0 },
{ &gallant19_149, 0, -15, 12, 0 },
{ &gallant19_150, 0, -15, 12, 0 },
{ &gallant19_151, 0, -15, 12, 0 },
{ &gallant19_152, 0, -15, 12, 0 },
{ &gallant19_153, 0, -15, 12, 0 },
{ &gallant19_154, 0, -15, 12, 0 },
{ &gallant19_155, 0, -15, 12, 0 },
{ &gallant19_156, 0, -15, 12, 0 },
{ &gallant19_157, 0, -15, 12, 0 },
{ &gallant19_158, 0, -15, 12, 0 },
{ &gallant19_159, 0, -15, 12, 0 },
{ &gallant19_160, 0, -15, 12, 0 },
{ &gallant19_161, 0, -15, 12, 0 },
{ &gallant19_162, 0, -15, 12, 0 },
{ &gallant19_163, 0, -15, 12, 0 },
{ &gallant19_164, 0, -15, 12, 0 },
{ &gallant19_165, 0, -15, 12, 0 },
{ &gallant19_166, 0, -15, 12, 0 },
{ &gallant19_167, 0, -15, 12, 0 },
{ &gallant19_168, 0, -15, 12, 0 },
{ &gallant19_169, 0, -15, 12, 0 },
{ &gallant19_170, 0, -15, 12, 0 },
{ &gallant19_171, 0, -15, 12, 0 },
{ &gallant19_172, 0, -15, 12, 0 },
{ &gallant19_173, 0, -15, 12, 0 },
{ &gallant19_174, 0, -15, 12, 0 },
{ &gallant19_175, 0, -15, 12, 0 },
{ &gallant19_176, 0, -15, 12, 0 },
{ &gallant19_177, 0, -15, 12, 0 },
{ &gallant19_178, 0, -15, 12, 0 },
{ &gallant19_179, 0, -15, 12, 0 },
{ &gallant19_180, 0, -15, 12, 0 },
{ &gallant19_181, 0, -15, 12, 0 },
{ &gallant19_182, 0, -15, 12, 0 },
{ &gallant19_183, 0, -15, 12, 0 },
{ &gallant19_184, 0, -15, 12, 0 },
{ &gallant19_185, 0, -15, 12, 0 },
{ &gallant19_186, 0, -15, 12, 0 },
{ &gallant19_187, 0, -15, 12, 0 },
{ &gallant19_188, 0, -15, 12, 0 },
{ &gallant19_189, 0, -15, 12, 0 },
{ &gallant19_190, 0, -15, 12, 0 },
{ &gallant19_191, 0, -15, 12, 0 },
{ &gallant19_192, 0, -15, 12, 0 },
{ &gallant19_193, 0, -15, 12, 0 },
{ &gallant19_194, 0, -15, 12, 0 },
{ &gallant19_195, 0, -15, 12, 0 },
{ &gallant19_196, 0, -15, 12, 0 },
{ &gallant19_197, 0, -15, 12, 0 },
{ &gallant19_198, 0, -15, 12, 0 },
{ &gallant19_199, 0, -15, 12, 0 },
{ &gallant19_200, 0, -15, 12, 0 },
{ &gallant19_201, 0, -15, 12, 0 },
{ &gallant19_202, 0, -15, 12, 0 },
{ &gallant19_203, 0, -15, 12, 0 },
{ &gallant19_204, 0, -15, 12, 0 },
{ &gallant19_205, 0, -15, 12, 0 },
{ &gallant19_206, 0, -15, 12, 0 },
{ &gallant19_207, 0, -15, 12, 0 },
{ &gallant19_208, 0, -15, 12, 0 },
{ &gallant19_209, 0, -15, 12, 0 },
{ &gallant19_210, 0, -15, 12, 0 },
{ &gallant19_211, 0, -15, 12, 0 },
{ &gallant19_212, 0, -15, 12, 0 },
{ &gallant19_213, 0, -15, 12, 0 },
{ &gallant19_214, 0, -15, 12, 0 },
{ &gallant19_215, 0, -15, 12, 0 },
{ &gallant19_216, 0, -15, 12, 0 },
{ &gallant19_217, 0, -15, 12, 0 },
{ &gallant19_218, 0, -15, 12, 0 },
{ &gallant19_219, 0, -15, 12, 0 },
{ &gallant19_220, 0, -15, 12, 0 },
{ &gallant19_221, 0, -15, 12, 0 },
{ &gallant19_222, 0, -15, 12, 0 },
{ &gallant19_223, 0, -15, 12, 0 },
{ &gallant19_224, 0, -15, 12, 0 },
{ &gallant19_225, 0, -15, 12, 0 },
{ &gallant19_226, 0, -15, 12, 0 },
{ &gallant19_227, 0, -15, 12, 0 },
{ &gallant19_228, 0, -15, 12, 0 },
{ &gallant19_229, 0, -15, 12, 0 },
{ &gallant19_230, 0, -15, 12, 0 },
{ &gallant19_231, 0, -15, 12, 0 },
{ &gallant19_232, 0, -15, 12, 0 },
{ &gallant19_233, 0, -15, 12, 0 },
{ &gallant19_234, 0, -15, 12, 0 },
{ &gallant19_235, 0, -15, 12, 0 },
{ &gallant19_236, 0, -15, 12, 0 },
{ &gallant19_237, 0, -15, 12, 0 },
{ &gallant19_238, 0, -15, 12, 0 },
{ &gallant19_239, 0, -15, 12, 0 },
{ &gallant19_240, 0, -15, 12, 0 },
{ &gallant19_241, 0, -15, 12, 0 },
{ &gallant19_242, 0, -15, 12, 0 },
{ &gallant19_243, 0, -15, 12, 0 },
{ &gallant19_244, 0, -15, 12, 0 },
{ &gallant19_245, 0, -15, 12, 0 },
{ &gallant19_246, 0, -15, 12, 0 },
{ &gallant19_247, 0, -15, 12, 0 },
{ &gallant19_248, 0, -15, 12, 0 },
{ &gallant19_249, 0, -15, 12, 0 },
{ &gallant19_250, 0, -15, 12, 0 },
{ &gallant19_251, 0, -15, 12, 0 },
{ &gallant19_252, 0, -15, 12, 0 },
{ &gallant19_253, 0, -15, 12, 0 },
{ &gallant19_254, 0, -15, 12, 0 },
{ &gallant19_255, 0, -15, 12, 0 },
},
#ifdef COLORFONT_CACHE
(struct raster_fontcache*) -1
#endif /*COLORFONT_CACHE*/
};

#endif /* RASTERCONS_SMALLFONT */
