#include <sys/endian.h>
/*
 * M48T37Y TOD Registers 
 */

#ifndef _LOCORE
typedef struct {
    unsigned char flags;	
    unsigned char century;
    unsigned char alarm_secs;	
    unsigned char alarm_mins;
    unsigned char alarm_hours;	
    unsigned char interrupts;	
    unsigned char watchdog;	
    unsigned char control;
    unsigned char seconds;	
    unsigned char minutes;
    unsigned char hour;
    unsigned char day;	
    unsigned char date;
    unsigned char month;
    unsigned char year;
} plddev;

#endif

#define TOD_FLAG	0	
#define TOD_CENTURY	1
#define TOD_ALRM_SECS	2	
#define TOD_ALRM_MINS	3
#define TOD_ALRM_HRS	4	
#define TOD_ALRM_DATE	5	
#define TOD_INTS	6	
#define TOD_WDOG	7	
#define TOD_CTRL	8
#define TOD_SECOND	9	
#define TOD_MINUTE	10
#define TOD_HOUR	11
#define TOD_DAY		12	
#define TOD_DATE	13
#define TOD_MONTH	14
#define TOD_YEAR	15

#define TOD_FLAG_WDF	(1 << 7)	/* Watchdog Flag */
#define TOD_FLAG_AF	(1 << 6)	/* Alarm Flag */
#define TOD_FLAG_BL	(1 << 4)	/* Battery Low Flag */

#define TOD_ALRM_SECS_RPT1	(1 << 7)	/* Alarm Repeat Mode Bit 1 */
#define TOD_ALRM_MINS_RPT2	(1 << 7)	/* Alarm Repeat Mode Bit 2 */
#define TOD_ALRM_HRS_RPT3	(1 << 7)	/* Alarm Repeat Mode Bit 3 */
#define TOD_ALRM_DATE_RPT4	(1 << 7)	/* Alarm Repeat Mode Bit 4 */

#define TOD_INTS_AFE	(1 << 7)	/* Alarm Flag Enable */
#define TOD_INTS_ABE	(1 << 5)	/* Alarm Battery Backup Mode Enable */

#define TOD_WDOG_WDS	(1 << 7)	/* Watchdog Steering */
#define TOD_WDOG_BMB	(31 << 2)	/* Watchdog Multiplier */
#define TOD_WDOG_RB	(3 << 2)	/* Watchdog Resolution */

#define TOD_CTRL_W	(1 << 7)	/* Write Bit */
#define TOD_CTRL_R	(1 << 6)	/* Read Bit */
#define TOD_CTRL_S	(1 << 5)	/* Sign Bit */
#define TOD_CTRL_CAL	(31 << 0)	/* Calibration */

#define TOD_SECOND_ST	(1 << 7)	/* Stop Bit */

#define TOD_DAY_FT	(1 << 6)	/* Frequency Test */

