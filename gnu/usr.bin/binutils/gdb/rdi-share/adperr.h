/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:53 $
 *
 *
 *   Project: ANGEL
 *
 *     Title: Definitions of ADP error codes
 */

#ifndef angsd_adperrs_h
#define angsd_adperrs_h
/*
 * ADP failure codes start at 256 to distinguish them for debug purposes
 */
enum AdpErrs
{
    adp_ok = 0,
    adp_failed = 256,
    adp_malloc_failure,
    adp_illegal_args,
    adp_device_not_found,
    adp_device_open_failed,
    adp_device_already_open,
    adp_device_not_open,
    adp_bad_channel_id,
    adp_callback_already_registered,
    adp_write_busy,
    adp_bad_packet,
    adp_seq_high,
    adp_seq_low,
    adp_timeout_on_open,
    adp_abandon_boot_wait,
    adp_late_startup,
    adp_new_agent_starting
};

#ifndef __cplusplus
typedef enum AdpErrs AdpErrs;
#endif

#define AdpMess_Failed             "ADP Error - unspecific failure"
#define AdpMess_MallocFailed       "ADP Error - malloc failed"
#define AdpMess_IllegalArgs        "ADP Error - illegal arguments"
#define AdpMess_DeviceNotFound     "ADP Error - invalid device specified"
#define AdpMess_DeviceOpenFailed   "ADP Error - specified device failed to open"
#define AdpMess_DeviceAlreadyOpen  "ADP Error - device already open"
#define AdpMess_DeviceNotOpen      "ADP Error - device not open"
#define AdpMess_BadChannelId       "ADP Error - bad channel Id"
#define AdpMess_CBAlreadyRegd      "ADP Error - callback already registered"
#define AdpMess_WriteBusy          "ADP Error - write busy"
#define AdpMess_BadPacket          "ADP Error - bad packet"
#define AdpMess_SeqHigh            "ADP Error - sequence number too high"
#define AdpMess_SeqLow             "ADP Error - sequence number too low"
#define AdpMess_TimeoutOnOpen      "ADP Error - target did not respond"
#define AdpMess_AbandonBootWait    "abandoned wait for late startup"
#define AdpMess_LateStartup        "Target compiled with LATE_STARTUP set.\n" \
                                   "Waiting for target...\n"                  \
                                   "Press <Ctrl-C> to abort.\n"
#define AdpMessLen_LateStartup    (3*80)
#define AdpMess_NewAgentStarting   "New Debug Agent about to start.\n"
 
#endif /* ndef angsd_adperr_h */

/* EOF adperr.h */
