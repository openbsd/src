/*	$OpenBSD: est.c,v 1.15 2006/05/19 19:43:41 dim Exp $ */
/*
 * Copyright (c) 2003 Michael Eriksson.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * This is a driver for Intel's Enhanced SpeedStep, as implemented in
 * Pentium M processors.
 *
 * Reference documentation:
 *
 * - IA-32 Intel Architecture Software Developer's Manual, Volume 3:
 *   System Programming Guide.
 *   Section 13.14, Enhanced Intel SpeedStep technology.
 *   Table B-2, MSRs in Pentium M Processors.
 *   http://www.intel.com/design/pentium4/manuals/245472.htm
 *
 * - Intel Pentium M Processor Datasheet.
 *   Table 5, Voltage and Current Specifications.
 *   http://www.intel.com/design/mobile/datashts/252612.htm
 *
 * - Intel Pentium M Processor on 90 nm Process with 2-MB L2 Cache Datasheet
 *   Table 3-4, Voltage and Current Specifications.
 *   http://www.intel.com/design/mobile/datashts/302189.htm
 *
 * - Linux cpufreq patches, speedstep-centrino.c.
 *   Encoding of MSR_PERF_CTL and MSR_PERF_STATUS.
 *   http://www.codemonkey.org.uk/projects/cpufreq/cpufreq-2.4.22-pre6-1.gz
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>


struct fq_info {
	u_short mhz;
	u_short mv;
};

/* Ultra Low Voltage Intel Pentium M processor 900 MHz */
static const struct fq_info pm130_900_ulv[] = {
	{  900, 1004 },
	{  800,  988 },
	{  600,  844 },
};

/* Ultra Low Voltage Intel Pentium M processor 1.00 GHz */
static const struct fq_info pm130_1000_ulv[] = {
	{ 1000, 1004 },
	{  900,  988 },
	{  800,  972 },
	{  600,  844 },
};

/* Ultra Low Voltage Intel Pentium M processor 1.10 GHz */
static const struct fq_info pm130_1100_ulv[] = {
	{ 1100, 1004 },
	{ 1000,  988 },
	{  900,  972 },
	{  800,  956 },
	{  600,  844 },
};

/* Low Voltage Intel Pentium M processor 1.10 GHz */
static const struct fq_info pm130_1100_lv[] = {
	{ 1100, 1180 },
	{ 1000, 1164 },
	{  900, 1100 },
	{  800, 1020 },
	{  600,  956 },
};

/* Low Voltage Intel Pentium M processor 1.20 GHz */
static const struct fq_info pm130_1200_lv[] = {
	{ 1200, 1180 },
	{ 1100, 1164 },
	{ 1000, 1100 },
	{  900, 1020 },
	{  800, 1004 },
	{  600,  956 },
};

/* Low Voltage Intel Pentium M processor 1.30 GHz */
static const struct fq_info pm130_1300_lv[] = {
	{ 1300, 1180 },
	{ 1200, 1164 },
	{ 1100, 1100 },
	{ 1000, 1020 },
	{  900, 1004 },
	{  800,  988 },
	{  600,  956 },
};

/* Intel Pentium M processor 1.30 GHz */
static const struct fq_info pm130_1300[] = {
	{ 1300, 1388 },
	{ 1200, 1356 },
	{ 1000, 1292 },
	{  800, 1260 },
	{  600,  956 },
};

/* Intel Pentium M processor 1.40 GHz */
static const struct fq_info pm130_1400[] = {
	{ 1400, 1484 },
	{ 1200, 1436 },
	{ 1000, 1308 },
	{  800, 1180 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.50 GHz */
static const struct fq_info pm130_1500[] = {
	{ 1500, 1484 },
	{ 1400, 1452 },
	{ 1200, 1356 },
	{ 1000, 1228 },
	{  800, 1116 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.60 GHz */
static const struct fq_info pm130_1600[] = {
	{ 1600, 1484 },
	{ 1400, 1420 },
	{ 1200, 1276 },
	{ 1000, 1164 },
	{  800, 1036 },
	{  600,  956 }
};

/* Intel Pentium M processor 1.70 GHz */
static const struct fq_info pm130_1700[] = {
	{ 1700, 1484 },
	{ 1400, 1308 },
	{ 1200, 1228 },
	{ 1000, 1116 },
	{  800, 1004 },
	{  600,  956 }
};

/* Intel Pentium M processor 723 1.0 GHz */
static const struct fq_info pm90_n723[] = {
	{ 1000,  940 },
	{  900,  908 },
	{  800,  876 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz */
static const struct fq_info pm90_n733[] = {
	{ 1100,  940 },
	{ 1000,  924 },
	{  900,  892 },
	{  800,  876 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz, VID #G */
static const struct fq_info pm90_n733g[] = {
	{ 1100,  956 },
	{ 1000,  940 },
	{  900,  908 },
	{  800,  876 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz, VID #H */
static const struct fq_info pm90_n733h[] = {
	{ 1100,  940 },
	{ 1000,  924 },
	{  900,  892 },
	{  800,  876 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz, VID #I */
static const struct fq_info pm90_n733i[] = {
	{ 1100,  924 },
	{ 1000,  908 },
	{  900,  892 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz, VID #J */
static const struct fq_info pm90_n733j[] = {
	{ 1100,  908 },
	{ 1000,  892 },
	{  900,  876 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz, VID #K */
static const struct fq_info pm90_n733k[] = {
	{ 1100,  892 },
	{ 1000,  876 },
	{  900,  860 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 733 1.1 GHz, VID #L */
static const struct fq_info pm90_n733l[] = {
	{ 1100,  876 },
	{ 1000,  876 },
	{  900,  860 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 753 1.2 GHz, VID #G */
static const struct fq_info pm90_n753g[] = {
	{ 1200,  956 },
	{ 1100,  940 },
	{ 1000,  908 },
	{  900,  892 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 753 1.2 GHz, VID #H */
static const struct fq_info pm90_n753h[] = {
	{ 1200,  940 },
	{ 1100,  924 },
	{ 1000,  908 },
	{  900,  876 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 753 1.2 GHz, VID #I */
static const struct fq_info pm90_n753i[] = {
	{ 1200,  924 },
	{ 1100,  908 },
	{ 1000,  892 },
	{  900,  876 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 753 1.2 GHz, VID #J */
static const struct fq_info pm90_n753j[] = {
	{ 1200,  908 },
	{ 1100,  892 },
	{ 1000,  876 },
	{  900,  860 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 753 1.2 GHz, VID #K */
static const struct fq_info pm90_n753k[] = {
	{ 1200,  892 },
	{ 1100,  892 },
	{ 1000,  876 },
	{  900,  860 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 753 1.2 GHz, VID #L */
static const struct fq_info pm90_n753l[] = {
	{ 1200,  876 },
	{ 1100,  876 },
	{ 1000,  860 },
	{  900,  844 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 773 1.3 GHz, VID #G */
static const struct fq_info pm90_n773g[] = {
	{ 1300,  956 },
	{ 1200,  940 },
	{ 1100,  924 },
	{ 1000,  908 },
	{  900,  876 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 773 1.3 GHz, VID #H */
static const struct fq_info pm90_n773h[] = {
	{ 1300,  940 },
	{ 1200,  924 },
	{ 1100,  908 },
	{ 1000,  892 },
	{  900,  876 },
	{  800,  860 },
	{  600,  812 }
};

/* Intel Pentium M processor 773 1.3 GHz, VID #I */
static const struct fq_info pm90_n773i[] = {
	{ 1300,  924 },
	{ 1200,  908 },
	{ 1100,  892 },
	{ 1000,  876 },
	{  900,  860 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 773 1.3 GHz, VID #J */
static const struct fq_info pm90_n773j[] = {
	{ 1300,  908 },
	{ 1200,  908 },
	{ 1100,  892 },
	{ 1000,  876 },
	{  900,  860 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 773 1.3 GHz, VID #K */
static const struct fq_info pm90_n773k[] = {
	{ 1300,  892 },
	{ 1200,  892 },
	{ 1100,  876 },
	{ 1000,  860 },
	{  900,  860 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 773 1.3 GHz, VID #L */
static const struct fq_info pm90_n773l[] = {
	{ 1300,  876 },
	{ 1200,  876 },
	{ 1100,  860 },
	{ 1000,  860 },
	{  900,  844 },
	{  800,  844 },
	{  600,  812 }
};

/* Intel Pentium M processor 738 1.4 GHz */
static const struct fq_info pm90_n738[] = {
	{ 1400, 1116 },
	{ 1300, 1116 },
	{ 1200, 1100 },
	{ 1100, 1068 },
	{ 1000, 1052 },
	{  900, 1036 },
	{  800, 1020 },
	{  600,  988 }
};

/* Intel Pentium M processor 758 1.5 GHz */
static const struct fq_info pm90_n758[] = {
	{ 1500, 1116 },
	{ 1400, 1116 },
	{ 1300, 1100 },
	{ 1200, 1084 },
	{ 1100, 1068 },
	{ 1000, 1052 },
	{  900, 1036 },
	{  800, 1020 },
	{  600,  988 }
};

/* Intel Pentium M processor 778 1.6 GHz */
static const struct fq_info pm90_n778[] = {
	{ 1600, 1116 },
	{ 1500, 1116 },
	{ 1400, 1100 },
	{ 1300, 1184 },
	{ 1200, 1068 },
	{ 1100, 1052 },
	{ 1000, 1052 },
	{  900, 1036 },
	{  800, 1020 },
	{  600,  988 }
};

/* Intel Pentium M processor 715 1.5 GHz, VID #A */
static const struct fq_info pm90_n715a[] = {
	{ 1500, 1340 },
	{ 1200, 1228 },
	{ 1000, 1148 },
	{  800, 1068 },
	{  600,  988 }
};

/* Intel Pentium M processor 715 1.5 GHz, VID #B */
static const struct fq_info pm90_n715b[] = {
	{ 1500, 1324 },
	{ 1200, 1212 },
	{ 1000, 1148 },
	{  800, 1068 },
	{  600,  988 }
};

/* Intel Pentium M processor 715 1.5 GHz, VID #C */
static const struct fq_info pm90_n715c[] = {
	{ 1500, 1308 },
	{ 1200, 1212 },
	{ 1000, 1132 },
	{  800, 1068 },
	{  600,  988 }
};

/* Intel Pentium M processor 715 1.5 GHz, VID #D */
static const struct fq_info pm90_n715d[] = {
	{ 1500, 1276 },
	{ 1200, 1180 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 725 1.6 GHz, VID #A */
static const struct fq_info pm90_n725a[] = {
	{ 1600, 1340 },
	{ 1400, 1276 },
	{ 1200, 1212 },
	{ 1000, 1132 },
	{  800, 1068 },
	{  600,  988 }
};

/* Intel Pentium M processor 725 1.6 GHz, VID #B */
static const struct fq_info pm90_n725b[] = {
	{ 1600, 1324 },
	{ 1400, 1260 },
	{ 1200, 1196 },
	{ 1000, 1132 },
	{  800, 1068 },
	{  600,  988 }
};

/* Intel Pentium M processor 725 1.6 GHz, VID #C */
static const struct fq_info pm90_n725c[] = {
	{ 1600, 1308 },
	{ 1400, 1244 },
	{ 1200, 1180 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 725 1.6 GHz, VID #D */
static const struct fq_info pm90_n725d[] = {
	{ 1600, 1276 },
	{ 1400, 1228 },
	{ 1200, 1164 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 735 1.7 GHz, VID #A */
static const struct fq_info pm90_n735a[] = {
	{ 1700, 1340 },
	{ 1400, 1244 },
	{ 1200, 1180 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 735 1.7 GHz, VID #B */
static const struct fq_info pm90_n735b[] = {
	{ 1700, 1324 },
	{ 1400, 1244 },
	{ 1200, 1180 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 735 1.7 GHz, VID #C */
static const struct fq_info pm90_n735c[] = {
	{ 1700, 1308 },
	{ 1400, 1228 },
	{ 1200, 1164 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 735 1.7 GHz, VID #D */
static const struct fq_info pm90_n735d[] = {
	{ 1700, 1276 },
	{ 1400, 1212 },
	{ 1200, 1148 },
	{ 1000, 1100 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 745 1.8 GHz, VID #A */
static const struct fq_info pm90_n745a[] = {
	{ 1800, 1340 },
	{ 1600, 1292 },
	{ 1400, 1228 },
	{ 1200, 1164 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 745 1.8 GHz, VID #B */
static const struct fq_info pm90_n745b[] = {
	{ 1800, 1324 },
	{ 1600, 1276 },
	{ 1400, 1212 },
	{ 1200, 1164 },
	{ 1000, 1116 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 745 1.8 GHz, VID #C */
static const struct fq_info pm90_n745c[] = {
	{ 1800, 1308 },
	{ 1600, 1260 },
	{ 1400, 1212 },
	{ 1200, 1148 },
	{ 1000, 1100 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 745 1.8 GHz, VID #D */
static const struct fq_info pm90_n745d[] = {
	{ 1800, 1276 },
	{ 1600, 1228 },
	{ 1400, 1180 },
	{ 1200, 1132 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/* Intel Pentium M processor 755 2.0 GHz, VID #A */
static const struct fq_info pm90_n755a[] = {
	{ 2000, 1340 },
	{ 1800, 1292 },
	{ 1600, 1244 },
	{ 1400, 1196 },
	{ 1200, 1148 },
	{ 1000, 1100 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 755 2.0 GHz, VID #B */
static const struct fq_info pm90_n755b[] = {
	{ 2000, 1324 },
	{ 1800, 1276 },
	{ 1600, 1228 },
	{ 1400, 1180 },
	{ 1200, 1132 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/* Intel Pentium M processor 755 2.0 GHz, VID #C */
static const struct fq_info pm90_n755c[] = {
	{ 2000, 1308 },
	{ 1800, 1276 },
	{ 1600, 1228 },
	{ 1400, 1180 },
	{ 1200, 1132 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/* Intel Pentium M processor 755 2.0 GHz, VID #D */
static const struct fq_info pm90_n755d[] = {
	{ 2000, 1276 },
	{ 1800, 1244 },
	{ 1600, 1196 },
	{ 1400, 1164 },
	{ 1200, 1116 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/* Intel Pentium M processor 765 2.1 GHz, VID #A */
static const struct fq_info pm90_n765a[] = {
	{ 2100, 1340 },
	{ 1800, 1276 },
	{ 1600, 1228 },
	{ 1400, 1180 },
	{ 1200, 1132 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/* Intel Pentium M processor 765 2.1 GHz, VID #B */
static const struct fq_info pm90_n765b[] = {
	{ 2100, 1324 },
	{ 1800, 1260 },
	{ 1600, 1212 },
	{ 1400, 1180 },
	{ 1200, 1132 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/* Intel Pentium M processor 765 2.1 GHz, VID #C */
static const struct fq_info pm90_n765c[] = {
	{ 2100, 1308 },
	{ 1800, 1244 },
	{ 1600, 1212 },
	{ 1400, 1164 },
	{ 1200, 1116 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/* Intel Pentium M processor 765 2.1 GHz, VID #E */
static const struct fq_info pm90_n765e[] = {
	{ 2100, 1356 },
	{ 1800, 1292 },
	{ 1600, 1244 },
	{ 1400, 1196 },
	{ 1200, 1148 },
	{ 1000, 1100 },
	{  800, 1052 },
	{  600,  988 }
};

/* Intel Pentium M processor 770 2.13 GHz */
static const struct fq_info pm90_n770[] = {
	{ 2130, 1551 },
	{ 1800, 1429 },
	{ 1600, 1356 },
	{ 1400, 1180 },
	{ 1200, 1132 },
	{ 1000, 1084 },
	{  800, 1036 },
	{  600,  988 }
};

/*
 * VIA C7-M 500 MHz FSB, 400 MHz FSB, and ULV variants.
 * Data from the "VIA C7-M Processor BIOS Writer's Guide (v2.17)" datasheet.
 */

/* 1.00GHz Centaur C7-M ULV */
static const struct fq_info C7M_770_ULV[] = {
	{ 1000,  844 },
	{  800,  796 },
	{  600,  796 },
	{  400,  796 },
};

/* 1.00GHz Centaur C7-M ULV */
static const struct fq_info C7M_779_ULV[] = {
	{ 1000,  796 },
	{  800,  796 },
	{  600,  796 },
	{  400,  796 },
};

/* 1.20GHz Centaur C7-M ULV */
static const struct fq_info C7M_772_ULV[] = {
	{ 1200,  844 },
	{ 1000,  844 },
	{  800,  828 },
	{  600,  796 },
	{  400,  796 },
};

/* 1.50GHz Centaur C7-M ULV */
static const struct fq_info C7M_775_ULV[] = {
	{ 1500,  956 },
	{ 1400,  940 },
	{ 1000,  860 },
	{  800,  828 },
	{  600,  796 },
	{  400,  796 },
};

/* 1.20GHz Centaur C7-M 400 Mhz FSB */
static const struct fq_info C7M_771[] = {
	{ 1200,  860 },
	{ 1000,  860 },
	{  800,  844 },
	{  600,  844 },
	{  400,  844 },
};

/* 1.50GHz Centaur C7-M 400 Mhz FSB */
static const struct fq_info C7M_754[] = {
	{ 1500, 1004 },
	{ 1400,  988 },
	{ 1000,  940 },
	{  800,  844 },
	{  600,  844 },
	{  400,  844 },
};

/* 1.60GHz Centaur C7-M 400 Mhz FSB */
static const struct fq_info C7M_764[] = {
	{ 1600, 1084 },
	{ 1400, 1052 },
	{ 1000, 1004 },
	{  800,  844 },
	{  600,  844 },
	{  400,  844 },
};

/* 1.80GHz Centaur C7-M 400 Mhz FSB */
static const struct fq_info C7M_784[] = {
	{ 1800, 1148 },
	{ 1600, 1100 },
	{ 1400, 1052 },
	{ 1000, 1004 },
	{  800,  844 },
	{  600,  844 },
	{  400,  844 },
};

/* 2.00GHz Centaur C7-M 400 Mhz FSB */
static const struct fq_info C7M_794[] = {
	{ 2000, 1148 },
	{ 1800, 1132 },
	{ 1600, 1100 },
	{ 1400, 1052 },
	{ 1000, 1004 },
	{  800,  844 },
	{  600,  844 },
	{  400,  844 },
};

/* 1.60GHz Centaur C7-M 533 Mhz FSB */
static const struct fq_info C7M_765[] = {
	{ 1600, 1084 },
	{ 1467, 1052 },
	{ 1200, 1004 },
	{  800,  844 },
	{  667,  844 },
	{  533,  844 },
};

/* 2.00GHz Centaur C7-M 533 Mhz FSB */
static const struct fq_info C7M_785[] = {
	{ 1867, 1148 },
	{ 1600, 1100 },
	{ 1467, 1052 },
	{ 1200, 1004 },
	{  800,  844 },
	{  667,  844 },
	{  533,  844 },
};

/* 2.00GHz Centaur C7-M 533 Mhz FSB */
static const struct fq_info C7M_795[] = {
	{ 2000, 1148 },
	{ 1867, 1132 },
	{ 1600, 1100 },
	{ 1467, 1052 },
	{ 1200, 1004 },
	{  800,  844 },
	{  667,  844 },
	{  533,  844 },
};

/* Convert MHz and mV into IDs for passing to the MSR. */
#define ID16(MHz, mV, bus_clk) \
	(((MHz / bus_clk) << 8) | ((mV ? mV - 700 : 0) >> 4))
#define ID32(MHz_hi, mV_hi, MHz_lo, mV_lo, bus_clk) \
	((ID16(MHz_lo, mV_lo, bus_clk) << 16) | (ID16(MHz_hi, mV_hi, bus_clk)))

struct fqlist {
	int vendor;
	u_int32_t id32;
	u_int32_t bus_clk;
	const struct fq_info *table;
	unsigned n;
};

#define ENTRY(ven, tab, zhi, vhi, zlo, vlo, bus_clk) \
	{ CPUVENDOR_##ven, ID32(zhi, vhi, zlo, vlo, bus_clk), bus_clk, tab, \
	sizeof(tab) / sizeof((tab)[0]) }

static const struct fqlist est_cpus[] = {
	ENTRY(INTEL, pm130_900_ulv,	 900, 1004, 600, 844, 100),
	ENTRY(INTEL, pm130_1000_ulv,	1000, 1004, 600, 844, 100),
	ENTRY(INTEL, pm130_1100_ulv,	1100, 1004, 600, 844, 100),
	ENTRY(INTEL, pm130_1100_lv,	1100, 1180, 600, 956, 100),
	ENTRY(INTEL, pm130_1200_lv,	1200, 1180, 600, 956, 100),
	ENTRY(INTEL, pm130_1300_lv,	1300, 1180, 600, 956, 100),
	ENTRY(INTEL, pm130_1300,	1300, 1388, 600, 956, 100),
	ENTRY(INTEL, pm130_1400,	1400, 1484, 600, 956, 100),
	ENTRY(INTEL, pm130_1500,	1500, 1484, 600, 956, 100),
	ENTRY(INTEL, pm130_1600,	1600, 1484, 600, 956, 100),
	ENTRY(INTEL, pm130_1700,	1700, 1484, 600, 956, 100),

	ENTRY(INTEL, pm90_n723,		1000,  940, 600, 812, 100),
	ENTRY(INTEL, pm90_n733,		1100,  940, 600, 812, 100),
	ENTRY(INTEL, pm90_n733g,	1100,  956, 600, 812, 100),
	ENTRY(INTEL, pm90_n733h,	1100,  940, 600, 812, 100),
	ENTRY(INTEL, pm90_n733i,	1100,  924, 600, 812, 100),
	ENTRY(INTEL, pm90_n733j,	1100,  908, 600, 812, 100),
	ENTRY(INTEL, pm90_n733k,	1100,  892, 600, 812, 100),
	ENTRY(INTEL, pm90_n733l,	1100,  876, 600, 812, 100),
	ENTRY(INTEL, pm90_n753g,	1200,  956, 600, 812, 100),
	ENTRY(INTEL, pm90_n753h,	1200,  940, 600, 812, 100),
	ENTRY(INTEL, pm90_n753i,	1200,  924, 600, 812, 100),
	ENTRY(INTEL, pm90_n753j,	1200,  908, 600, 812, 100),
	ENTRY(INTEL, pm90_n753k,	1200,  892, 600, 812, 100),
	ENTRY(INTEL, pm90_n753l,	1200,  876, 600, 812, 100),
	ENTRY(INTEL, pm90_n773g,	1300,  956, 600, 812, 100),
	ENTRY(INTEL, pm90_n773h,	1300,  940, 600, 812, 100),
	ENTRY(INTEL, pm90_n773i,	1300,  924, 600, 812, 100),
	ENTRY(INTEL, pm90_n773j,	1300,  908, 600, 812, 100),
	ENTRY(INTEL, pm90_n773k,	1300,  892, 600, 812, 100),
	ENTRY(INTEL, pm90_n773l,	1300,  876, 600, 812, 100),
	ENTRY(INTEL, pm90_n738,		1400, 1116, 600, 988, 100),
	ENTRY(INTEL, pm90_n758,		1500, 1116, 600, 988, 100),
	ENTRY(INTEL, pm90_n778,		1600, 1116, 600, 988, 100),
	ENTRY(INTEL, pm90_n715a,	1500, 1340, 600, 988, 100),
	ENTRY(INTEL, pm90_n715b,	1500, 1324, 600, 988, 100),
	ENTRY(INTEL, pm90_n715c,	1500, 1308, 600, 988, 100),
	ENTRY(INTEL, pm90_n715d,	1500, 1276, 600, 988, 100),
	ENTRY(INTEL, pm90_n725a,	1600, 1340, 600, 988, 100),
	ENTRY(INTEL, pm90_n725b,	1600, 1324, 600, 988, 100),
	ENTRY(INTEL, pm90_n725c,	1600, 1308, 600, 988, 100),
	ENTRY(INTEL, pm90_n725d,	1600, 1276, 600, 988, 100),
	ENTRY(INTEL, pm90_n735a,	1700, 1340, 600, 988, 100),
	ENTRY(INTEL, pm90_n735b,	1700, 1324, 600, 988, 100),
	ENTRY(INTEL, pm90_n735c,	1700, 1308, 600, 988, 100),
	ENTRY(INTEL, pm90_n735d,	1700, 1276, 600, 988, 100),
	ENTRY(INTEL, pm90_n745a,	1800, 1340, 600, 988, 100),
	ENTRY(INTEL, pm90_n745b,	1800, 1324, 600, 988, 100),
	ENTRY(INTEL, pm90_n745c,	1800, 1308, 600, 988, 100),
	ENTRY(INTEL, pm90_n745d,	1800, 1276, 600, 988, 100),
	ENTRY(INTEL, pm90_n755a,	2000, 1340, 600, 988, 100),
	ENTRY(INTEL, pm90_n755b,	2000, 1324, 600, 988, 100),
	ENTRY(INTEL, pm90_n755c,	2000, 1308, 600, 988, 100),
	ENTRY(INTEL, pm90_n755d,	2000, 1276, 600, 988, 100),
	ENTRY(INTEL, pm90_n765a,	2100, 1340, 600, 988, 100),
	ENTRY(INTEL, pm90_n765b,	2100, 1324, 600, 988, 100),
	ENTRY(INTEL, pm90_n765c,	2100, 1308, 600, 988, 100),
	ENTRY(INTEL, pm90_n765e,	2100, 1356, 600, 988, 100),
	ENTRY(INTEL, pm90_n770,		2130, 1551, 600, 988, 100),

	ENTRY(VIA,   C7M_770_ULV,	1000,  844, 400, 796, 100),
	ENTRY(VIA,   C7M_779_ULV,	1000,  796, 400, 796, 100),
	ENTRY(VIA,   C7M_772_ULV,	1200,  844, 400, 796, 100),
	ENTRY(VIA,   C7M_771,		1200,  860, 400, 844, 100),
	ENTRY(VIA,   C7M_775_ULV,	1500,  956, 400, 796, 100),
	ENTRY(VIA,   C7M_754,		1500, 1004, 400, 844, 100),
	ENTRY(VIA,   C7M_764,		1600, 1084, 400, 844, 100),
	ENTRY(VIA,   C7M_765,		1600, 1084, 533, 844, 133),
	ENTRY(VIA,   C7M_784,		1800, 1148, 400, 844, 100),
	ENTRY(VIA,   C7M_785,		1867, 1148, 533, 844, 133),
	ENTRY(VIA,   C7M_794,		2000, 1148, 400, 844, 100),
	ENTRY(VIA,   C7M_795,		2000, 1148, 533, 844, 133),
};

#define NESTCPUS	  (sizeof(est_cpus) / sizeof(est_cpus[0]))


#define MSRVALUE(mhz, mv, bus)	((((mhz) / (bus)) << 8) | (((mv) - 700) / 16))
#define MSR2MHZ(msr)		((((int) (msr) >> 8) & 0xff) * 100)
#define MSR2MV(msr)		(((int) (msr) & 0xff) * 16 + 700)

static const struct fqlist *est_fqlist;

extern int setperf_prio;
extern int perflevel;

void
est_init(const char *cpu_device, int vendor)
{
	int i, mhz, mv, low, high;
	u_int32_t id;
	u_int64_t msr;

	if (setperf_prio > 3)
		return;

	if ((cpu_ecxfeature & CPUIDECX_EST) == 0)
		return;

	msr = rdmsr(MSR_PERF_STATUS);
	mhz = MSR2MHZ(msr);
	mv = MSR2MV(msr);
	printf("%s: Enhanced SpeedStep %d MHz (%d mV)",
	    cpu_device, mhz, mv);

	/*
	 * Find an entry which matches (vendor, id32)
	 */
	id = msr >> 32;
	for (i = 0; i < NESTCPUS; i++) {
		if (est_cpus[i].vendor == vendor && est_cpus[i].id32 == id) {
			est_fqlist = &est_cpus[i];
			break;
		}
	}
	if (est_fqlist == NULL) {
		printf(": unknown EST cpu, msr %016llx\n", msr);
		return;
	}

	/*
	 * Check that the current operating point is in our list.
	 */
	for (i = est_fqlist->n - 1; i >= 0; i--)
		if (est_fqlist->table[i].mhz == mhz &&
		    est_fqlist->table[i].mv == mv)
			break;
	if (i < 0) {
		printf(" (not in table)\n");
		return;
	}
	low = est_fqlist->table[est_fqlist->n - 1].mhz;
	high = est_fqlist->table[0].mhz;
	perflevel = (mhz - low) * 100 / (high - low);

	/*
	 * OK, tell the user the available frequencies.
	 */
	printf(": speeds: ");
	for (i = 0; i < est_fqlist->n; i++)
		printf("%d%s", est_fqlist->table[i].mhz,
		    i < est_fqlist->n - 1 ? ", " : " MHz\n");

	cpu_setperf = est_setperf;
	setperf_prio = 3;
}

int
est_setperf(int level)
{
	int low, high, i, fq;
	uint64_t msr;

	if (est_fqlist == NULL)
		return (EOPNOTSUPP);

	low = est_fqlist->table[est_fqlist->n - 1].mhz;
	high = est_fqlist->table[0].mhz;
	fq = low + (high - low) * level / 100;

	for (i = est_fqlist->n - 1; i > 0; i--)
		if (est_fqlist->table[i].mhz >= fq)
			break;
	msr = (rdmsr(MSR_PERF_CTL) & ~0xffffULL) |
	    MSRVALUE(est_fqlist->table[i].mhz, est_fqlist->table[i].mv,
	    est_fqlist->bus_clk);
	wrmsr(MSR_PERF_CTL, msr);
	pentium_mhz = est_fqlist->table[i].mhz;

	return (0);
}
