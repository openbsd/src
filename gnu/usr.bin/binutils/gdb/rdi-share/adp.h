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
 *
 * INTRODUCTION
 * ------------
 * The early RDP message definitions were held in an ARM Ltd "armdbg"
 * source file. Since the relevant header files were not exported
 * publicly as part of an ARM Ltd core tools release, it was a problem
 * for developers manipulating the target side of the protocol.
 *
 * For Angel, this new (ANSI 'C' clean) header file defines the ADP
 * protocol. The header should be useable by both host and target
 * systems, thus avoiding problems that can arise from duplicate
 * definitions. Care has been taken in the construction of this header
 * file to avoid any host/target differences.
 *
 * MESSAGE FORMAT
 * --------------
 * Format of the "data" section of debug and boot agent messages. This is
 * the standard ADP (Angel Debug Protocol) message format:
 *
 *  unsigned32 reason     - Main debug reason code.
 *  unsigned32 debugID    - Information describing host debug world;
 *                        - private to host and used in any target initiated
 *                          messages.
 *  unsigned32 OSinfo1    \ Target OS information to identify process/thread
 *  unsigned32 OSinfo2    / memory/world, etc. These two fields are target
 *                          defined.
 *  byte       args[n]    - Data for message "reason" code.
 *
 * NOTE: The message format is the same for single threaded debugging,
 * except that the "OSinfo" fields should be -1 (0xFFFFFFFF). Even
 * single-threaded debugging *MAY* have different host specified
 * debugID values, so the Angel debug system will preserve the "debugID"
 * information for replies, and the relevant asynchronous target-to-host
 * messages. The "debugID" is defined by the host-end of the
 * protocol, and is used by the host to ensure that messages are
 * routed to the correct handler program/veneer.
 *
 * The reason there are two target specified "OSinfo" words is because
 * thread identifiers may not be unique when processes/tasks have
 * private virtual address spaces. It allows more flexibility when
 * supporting multi-threaded or O/S aware debugging.
 *
 * NOTE: The reason that there is no "size" information, is that the
 * message IDs themselves encode the format of any arguments. Also it
  * would be a duplication of information used by the physical
 * transport layer (which is distinct from this logical message
 * layer). Any routing of messages through programs, hosts,
 * etc. should be performed at the physical layer, or the boundaries
 * between physical layers. i.e. packet received on socket in host,
 * and transferred to serial packet for passing on down the line.
 *
 * NOTE: Pointers aren't passed in messages because they are dangerous in
 * a multi-threaded environment.
 *
 * ADP REASON CODE
 * ---------------
 * The message reason codes contain some information that ties them to
 * the channel and direction that the message will be used with. This
 * will ensure that even if the message "#define name" is not
 * completely descriptive, the message reason code is.
 *
 *      b31    = direction. 0=Host-to-Target; 1=Target-to-Host;
 *      b30-28 = debug agent multi-threaded control (see below)
 *      b27-24 = reserved. should be zero.
 *      b23-16 = channelid. The fixed Angel channel number
 *               (see "channels.h").
 *      b15-0  = message reason code.
 *
 * It is unfortunate that to aid the error-checking capabilities of
 * the Angel communications we have changed the message numbers from
 * the original ARM Ltd RDP. However this also has benefits, in that
 * the Angel work is meant to be a clean break.
 *
 * However, it isn't so bad since even though the numbers are
 * different, the majority of the reason codes have exactly the same
 * functionality as the original RDP messages.
 *
 * NOTES
 * -----
 * It would be ideal to use "rpcgen" (or some equivalent) to
 * automatically maintain compatibility between the target and host
 * ends of the protocol. However, ARM Ltd expressed that the message
 * handling should be hand-coded, to avoid dependance on external
 * tools.
 *
 * All other channels have undefined data formats and are purely
 * application defined. The C library "_sys_" support will provide a
 * veneer to perform message block operations as required.
 *
 * It is IMPLIED that all of the ADP messages will fit within the
 * buffer DATASIZE. This has a minimum value, calculated from
 * BUFFERMINSIZE.
 *
 * All messages are passed and received to the channel system in little
 * endian order (ie. use little endian order when writing a word as
 * a sequence of bytes within a message).
 *
 * A reply / acknowledgement to an ADP message is always sent and has the
 * same reason code as the original except that the TtoH / HtoT bit is
 * reversed.  This makes it simple to check that the reply really
 * is a reply to the message which was just sent!  [Boot Channel messages
 * also require that this protocol is used].
 */

#ifndef angel_adp_h
#define angel_adp_h

#include "chandefs.h"


/*
 * Buffer minimum sizes
 */

/* the minimum target internal size */
#define ADP_BUFFER_MIN_SIZE (256)

/* a word is always reserved for internal use in the target */
#define ADP_BUFFER_MAX_INTERNAL (sizeof(word))

/* the minimum available data portion */
#define ADP_BUFFER_MIN_DATASIZE \
    (ADP_BUFFER_MIN_SIZE - ADP_BUFFER_MAX_INTERNAL - CHAN_HEADER_SIZE)

/*
 * the space taken up by the standard ADP header
 * (reason, debugID, OSinfo1, OSinfo2)
 */
#define ADP_DEFAULT_HEADER_SIZE (4*sizeof(word))


/* 8bit ADP version identification */
#define ADPVSN  (0x03)
/* This value can be used to identify the protocol version supported
 * by target or host systems. This version number should only be
 * changed if the protocol undergoes a non-backward compatible
 * change. It should *NOT* be used to reflect extensions to the
 * protocol. Such extensions can be added to the existing protocol
 * version by allocating new reason codes, and by extending the
 * ADP_Info message to identify new features.
 */

/* The following value is used in the OSinfo fields for
 * single-threaded messages, or where the host wants to alter the
 * global CPU state. NOTE: The "debugID" field should always be
 * defined by the host, and returned in target initiated messages. The
 * only exception to this rule is the ADP_Booted message at the
 * start-of-day.
 */
#define ADP_HandleUnknown (-1)

/******************************************************************
 *
 * ADP reason code subfields
 *
 */

/* The following bits are used to describe the basic direction of
 * messages. This allows some extra checking of message validity to be
 * performed, as well as providing a description of the message that
 * may not be available in the "cpp" macro:
 */
#define HtoT    ((unsigned)0 << 31)     /* Host-to-Target message */
#define TtoH    ((unsigned)1 << 31)     /* Target-to-Host message */

/* The following bits are used to control how the target system
 * executes whilst processing messages. This allows for O/S specific
 * host-based debug programs to interrogate system structures whilst
 * ensuring that the access is atomic within the constraints imposed
 * by the target O/S.
 *
 * NOTE: That only the channel is inserted into the reason code
 * automatically.  Thus both direction and multi thread control bits
 * must be added by the host / target.
 */
/* Disable FIQ whilst processing message */
#define DisableFIQ              (1 << 30)
/* Disable IRQ whilst processing message */
#define DisableIRQ              (1 << 29)
/* Disable O/S pre-emption whilst processing message */
#define DisablePreemption       (1 << 28)

/* The channel identification number is held in the reason code as a
 * check:
 */
#define ADPCHANNEL(b)   (((b) & 0xFF) << 16)

/* The following macro constructs the reason code number, from the
 * various fields - note that the direction is NOT inlcuded since
 * this depends on whether the Host or Target system is including
 * this file!
 */
#define ADPREASON(c,r)        (ADPCHANNEL(c) | ((r) & 0xFFFF))

/* This macros is used when constructing manifests for sub-reason
 * codes. At the moment it is identical to the main reason macro. If
 * desired we could add a new bit that explicitly identifies the value
 * as a sub-reason code, where the corresponding bit in the main
 * message ID would be zero.
 */
#define ADPSUBREASON(c,r)     (ADPCHANNEL(c) | ((r) & 0xFFFF))

/* All other undefined bits are reserved, and should be zero. */



/*****************************************************************
 *
 * channel_BOOT messages
 *
 */

/* The BOOT agent only supports a few messages. They are used purely
 * to control the "start-of-day" connection to a host program. All
 * Angel systems with host communications *MUST* provide the BOOT
 * agent, even if they don't have support for either the single- or
 * multi-threaded debug agents.
 *
 * The way the BOOT channel will be used on startup will be as follows:
 *
 * a) Target board is powered up before host debugger is invoked
 *
 * After switching on the target and initialisation is completed the
 * target will send an ADP_Booted or ADP_Reset message.  The debugger
 * has not been started yet so this message will not be received.  In
 * a serial world this makes it important that any buffers on the host
 * side are flushed during initialisation of the debugger, and in an
 * Ethernet world it makes it important that the target can cope with the
 * message not being received.
 *
 * Eventually the Debugger will be started up and will send an
 * ADP_Reboot or ADP_Reset request.  The target will respond to this with
 * an ADP_Reboot or ADP_Reset acknowldege and will then reboot, finally
 * sending an ADP_Rebooted when it has done all it needs to do (very little
 * in the case of ADP_Reset, but completely rebooting in the case of
 * ADP_Reboot).  Note that it is important that an ADP_Rebooted message is
 * sent so that the Debugger does not attempt to send any data after it has
 * made a request to ADP_Reboot and before it receives an ADP_Rebooted, as
 * data can be lost be the target during this time.
 *
 * The target and host are now ready to start a debug session.
 *
 * b) Target board is powered up after host debugger is invoked
 *
 * The debugger will send an ADP_Reboot or ADP_Reset request, but will
 * receive no reply until the target is powered up.
/ *
 * When the target is powered up then it will send an ADP_Rebooted
 * message to the debugger.  The debugger should accept this message
 * even though it has received no ADP_Reboot or ADP_Reset acknowldege message
 * from the target.
 *
 * The target and host are now ready to start a debug session.
 *
 *
 * If at any point during the bootup sequence and ADP messages are
 * sent down the S_DBG channel then they should be responded to with a
 * RDI_NotInitialised error. [This should never happen however].
 *
 * An ADP_Boot or ADP Rebooted message should be accepted at
 * any point, since it is possible for a catastrophe to occur (such as
 * disconnecteing the host and target during a debug message) which
 * requires that one or other end be reset.
 *
 */

/*
 * A list of parameter types - for now just baud rate
 */
typedef enum ADP_Parameter {
    AP_PARAMS_START = 0xC000,
    AP_BAUD_RATE = AP_PARAMS_START,
    /* extra parameters go in here */
#ifdef TEST_PARAMS
    AP_CAFE_MENU,               /* extra just for testing */
#endif
    AP_PARAMS_END
} ADP_Parameter;

#define AP_NUM_PARAMS (AP_PARAMS_END - AP_PARAMS_START)

/*
 * Parameter types should have associated semantics which can be represented
 * within one word per parameter, or an associated enum for choices.
 *
 * AP_BAUD_RATE: the word contains the exact baud rate, eg. 9600, 38400.
 */

/* this is not strictly necessary, but it's an example */
typedef enum ADP_BaudRate {
    AB_9600  =  9600,
    AB_19200 = 19200,
    AB_38400 = 38400,
    AB_57600 = 57600,
    AB_115200 = 115200
} ADP_BaudRate;

#define AB_NUM_BAUD_RATES 5     /* this is more useful, for sizing arrays */

/* This must be set to the max number of options per parameter type */
#define AP_MAX_OPTIONS (AB_NUM_BAUD_RATES)


#define ADP_Booted      ADPREASON(CI_TBOOT,0)
/* This message is sent by the target after the Angel system has been
 * initialised.  This message also contains information describing the
 * Angel world. The information can then be used to check that the
 * target debug agent and source debugger are compatible.
 *
 * Message arguments:
 *      word    Angel message default buffer size.
 *      word    Angel message large buffer size (may be same as default)
 *      word    Angel version ; inc. type (e.g. boot ROM) See (1)
 *      word    ADP version.  See (2)
 *      word    ARM Architecture info See (3)
 *      word    ARM CPU information ; including target endianness. See (4)
 *      word    Target hardware status. See (5)
 *      word    Number of bytes in banner message
 *      bytes   Startup banner message (single-threaded readable
 *              descriptive text - NOT NULL terminated).
 *
 * Reply:
 *      word    status
 *
 *      'status' returns RDIError_NoError for success, and otherwise
 *      indicates an error.
 */

/* Angel version word [Reference(1)] : */
/* Angel version number is a 16bit BCD value */
#define ADP_ANGELVSN_MASK           (0x0000FFFF)
#define ADP_ANGELVSN_SHIFT          (0)

/* Type of Angel system */
#define ADP_ANGELVSN_TYPE_MASK      (0x00FF0000)
#define ADP_ANGELVSN_TYPE_SHIFT     (16)

typedef enum {
 ADP_AngelType_bootROM, /* Simple ROM system providing download capability */
 ADP_AngelType_appROM,  /* ROM based application */
 ADP_AngelType_appDLOAD,/* Downloaded Angel based application */
 ADP_AngelType_Last     /* Unknown type. This typedef can be extended */
                        /* but if the host and target vsns differ */
                        /* Then one will spot that it dies not understand */
} ADP_Angel_Types ;     /* this field and can whinge appropriately */

/* First unknown ADP_AngelType */
#define ADP_ANGELVSN_UNKTYPE_MASK   (0xFF000000)
#define ADP_ANGELVSN_UNKYPE_SHIFT   (24)

/* Currently only 8 bits are used in the word: */
/* ADP protocol supported by target [Reference (2)] */
#define ADP_ANGELVSN_ADP_MASK       (0x000000FF)
#define ADP_ANGELVSN_ADP_SHIFT      (0)

/* ARM Architecture info: [Reference (3)] */
/* ARM Architecture Verson of target CPU */
#define ADP_ARM_ARCH_VSN_MASK       (0x000000FF)
#define ADP_ARM_ARCH_VSN_SHIFT      (0)
/* Does the processor support the Thumb Instruction Set */
#define ADP_ARM_ARCH_THUMB          (0x80000000)
/* Does the processor support Long Multiplies */
#define ADP_ARM_ARCH_LONGMUL        (0x40000000)
/* All other flags are current undefined, and should be zero. */

/* The following flags describe the feature set of the processor: */
/* Set if cpu supports little-endian model [Reference (4)] */
#define ADP_CPU_LE              (1 << 0)
/* Set if cpu supports big-endian model */
#define ADP_CPU_BE              (1 << 1)
/* Set if processor has a cache */
#define ADP_CPU_CACHE           (1 << 2)
/* Set if processor has a MMU */
#define ADP_CPU_MMU             (1 << 3)
/* All other flags are current undefined, and should be zero. */

/* The following flags reflect current Target hardware status: */
/* [Reference (5)] */
/* 0 = no MMU or MMU off; 1 = MMU on */
#define ADP_CPU_MMUOn           (1 << 29)
/* 0 = no cache or cache off; 1 = cache on */
#define ADP_CPU_CacheOn         (1 << 30)
/* 0 = little-endian; 1 = big-endian */
#define ADP_CPU_BigEndian       (1U << 31)
/* All other flags are current undefined, and should be zero. */


#ifdef LINK_RECOVERY

#define ADP_TargetResetIndication       ADPREASON(CI_TBOOT, 1)
/*
 * If parameter negotiation is enabled at the target, it configures itself
 * to various likely parameter settings and sends this message at each
 * configuration.  The message describes the default settings, and after
 * sending at each configuration the target sets itself to the defaults
 * it has just broadcast, to await either an ack on TBOOT or a request
 * or reset indication on HBOOT.
 *
 * If the host receives this message successfully, it should reset to the
 * indicated parameters and send a reply.
 *
 * Message arguments:
 *      word    status                   (always 0, makes body same as
 *                                        ADP_ParamNegotiate response)
 *      word    n-parameters
 *      n-parameters * {
 *              word    ADP_Parameter
 *              word    parameter-value
 *      }
 *
 * Reply:
 *      -       empty acknowledgement
 */

#endif /* def LINK_RECOVERY */

typedef enum ADP_Boot_Ack {
    AB_NORMAL_ACK,              /* will comply, immediate booted message */
    AB_LATE_ACK,                /* will comply, late startup */
    AB_ERROR                    /* cannot comply */
} ADP_Boot_Ack;

/* If the host sets neither of these in the word sent on a Reset / Reboot
 * then it doesn;t care about the endianess of the target
 */
#define ADP_BootHostFeature_LittleEnd 0x80000000
#define ADP_BootHostFeature_BigEnd    0x40000000

#define ADP_Reboot      ADPREASON(CI_HBOOT,2)
/* This message is sent when the host wants the target system to be
 * completely reset, back to the boot monitor Angel. This is the
 * method of the host forcing a cold-reboot.
 * Note that an acknowledgement message will be sent immediately and
 * that this must be sent before the target can reset.
 *
 * The parameter to this function is a bitset of host supported
 * features. (in fact the same as ADP_Reset below.  This can be used by
 * the target system to avoid using debug channel bandwidth raising
 * messages that will be ignored by the host.
 *
 * Parameters:
 *      word    host supported features (see above)
 *
 * Reply:
 *      word    status, one of enum ADP_Boot_Ack above.
 *
 * Currently there are no such features defined, so the word indicating
 * host supported features should be set to 0.
 */



#define ADP_Reset       ADPREASON(CI_HBOOT,3)
/* This message is a request from the host, which should eventually
 * result in the "ADP_Booted" message being sent by the target.
 * Note that an acknowledgement message will be sent immediately and
 * that this must be sent before the target can reset.
 * This reset message is *ALWAYS* treated as a warm boot, with the target
 * preserving as much state as possible.
 *
 * The parameter to this function is a bitset of host supported
 * features. This can be used by the target system to avoid using
 * debug channel bandwitdth raising messages that will be ignored by
 * the host.
 *
 * Parameters:
 *      word    host supported features (see above)
 *
 * Reply:
 *      word    status, one of enum ADP_Boot_Ack above.
 *
 * Currently there are no such features defined, so the word indicating
 * host supported features should be set to 0.
 */


#ifdef LINK_RECOVERY

#define ADP_HostResetIndication         ADPREASON(CI_HBOOT, 4)
/*
 * This is as for ADP_TargetResetIndication, but is sent by the host when
 * it first starts up in case the target is listening at a non-default
 * setting.  Having sent at various configurations, the host then listens
 * at the defaults it has just broadcast, to await either an ack on HBOOT
 * or a reset indication on TBOOT.
 *
 * For arguments and reply, see ADP_TargetResetIndication.
 */

#endif /* def LINK_RECOVERY */


#define ADP_ParamNegotiate              ADPREASON(CI_HBOOT, 5)
/*
 * The host sends this messages to negotiate new parameters with the target.
 * For each parameter the host specifies a range of possibilities, starting
 * with the most favoured.  All possible combinations of parameters
 * must be valid.
 *
 * If the target can operate at a combination of the offered parameters,
 * it will reply with the parameters it is willing to use.  AFTER sending
 * the reply, the target switches to this combination.  On receiving the
 * reply, the host will switch to the new combination and send a LinkCheck
 * message (see below).
 *
 * If the target cannot operate at any combination of the offered parameters,
 * it will reply with an error status.
 *
 * Message arguments:
 *      word    n-parameter-blocks
 *      n-parameter-blocks * {
 *              word    ADP_Parameter
 *              word    n-options
 *              n-options * { word      parameter-value }
 *      }
 *
 * Reply:
 *      word    status
 *      if (status == RDIError_NoError) {
 *              word    n-parameters
 *              n-parameters * {
 *                      word    ADP_Parameter
 *                      word    chosen-value
 *              }
 *      }
 */

#define ADP_LinkCheck                   ADPREASON(CI_HBOOT, 6)
/*
 * This should be the first message that the host sends after a successful
 * parameter negotiation.  It is really just a 'ping'.
 *
 * Message arguments:
 *      -       empty message
 *
 * Reply:
 *      -       empty acknowledgement
 */


/********************************************************************
 *
 * CI_HADP messages
 *
 */

#define ADP_HADPUnrecognised        ADPREASON(CI_HADP,0)
/* This message is unusual in that it is normally sent in reply to
 * another message which is not understood.  This is an exception
 * to the normal protocol which says that a reply must have the
 * same base reason code as the original.  There is a single reply
 * parameter which is the reason code which was not understood.
 *
 * As well as being a reply this message can also be sent and will
 * return as if this message were unrecognised!
 *
 * Parameters:
 *      none
 *
 * Reply:
 *      word    reason code which was not recognised
 */


#define ADP_Info                ADPREASON(CI_HADP,1)
/* This is the new ADP information message. It is used to interrogate
 * the target debug agent.  It provides information on the processor,
 * as well as the state of the debug world. This allows the host to
 * configure itself to the capabilities of the target.
 *
 * We try not to use feature bitsets, since we could quickly run out
 * of known bits.  Thus when the feature set is extended, this can be
 * done in a couple of supported ways:
 *
 *  If an undivided reason code is to be added (no reason subcodes)
 *  then add a new ADP_Info code which responds with a flag indicating
 *  whether that feature is supported by the target.  If this has not
 *  even been implemented then the reply will be ADP_HADPUnrecognised
 *
 *  If a reason code which is subdivided into reason subcodes is
 *  added then reason subcode 0 should be set aside to indicate
 *  whether the functionality of that reason code is supported
 *  by the target.  If it is not even implemented then the reply will
 *  be ADP_Unrecognised.
 *
 * The first parameter to ADP_Info is a reason subcode, and subsequent
 * parameters are defined by that subcode
 *
 * Parameters:
 *      word         reason subcode
 *      other arguments as reason subcode determines.
 *
 * Reply:
 *      word         reason subcode
 *      other argument as reason subcode determines
 */

/* ADP_Info reason subcodes: */



#define ADP_Info_NOP                    ADPSUBREASON(CI_HADP,0)
/* ADP_Info_NOP
 * ------------
 * Summary: This message is used to check for ADP_Info being supported.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' returns RDIError_NoError for success, non-zero indicates an error.
 * If an error is returned then there is no handler for the ADP_Info
 * message. The normal action will be to return an OK status.
 */


#define ADP_Info_Target                 ADPSUBREASON(CI_HADP,1)
/* ADP_Info_Target
 * ---------------
 * Summary:
 * This reason code is used to interrogate target system details.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word bitset, word model)
 *
 * 'status' is RDIError_NoError to indicate OK, or non-zero to indicate
 * some sort of error.
 * 'bitset' is described in more detail below, and is mostly compatible
 * with the old RDI/RDP system to avoid gratuitous changes to the debugger
 * toolbox.
 * 'model' is the target hardware ID word, as returned by the ADP_Booted
 * message.
 *
 * NOTE: The minimum and maximum protocol levels are no longer supported.
 * It is the Angel view that debugging complexity should be shifted to the
 * host if at all possible.  This means that the host debugger should
 * always try to configure itself to the features available in the target
 * debug agent.  This can be done by checking individual messages, rather
 * than by a blanket version number dictating the feature set.
 */

/* 'bitset':- */
/* Target speed in instructions per second = 10**(bits0..3). */
#define ADP_Info_Target_LogSpeedMask         (0xF)

/* Target is running on [0 = emulator / 1 = hardware] */
#define ADP_Info_Target_HW                   (1 << 4)

/* Bits 5..10 are currently undefined and should be zero. */
/* Other bis are kept the same as the RDP in order to */
/* eliminate the need to change the position of some bits */

/* If set then the debug agent can be reloaded. */
#define ADP_Info_Target_CanReloadAgent       (1 << 11)

/* Can request AngelBufferSize information. */
#define ADP_Info_Target_CanInquireBufferSize (1 << 12)

/* Bit 13 is no longer required as it inquired whether
 * a special RDP Interrupt code was supported
 */

/* Debug agent can perform profiling. */
#define ADP_Info_Target_Profiling            (1 << 14)

/* Debug agent can support Thumb code. */
#define ADP_Info_Target_Thumb                (1 << 15)

/* Bit 16 was the communications channel check.
 * This is always available on Angel systems.
 */

#define ADP_Info_Points                 ADPSUBREASON(CI_HADP,2)
/* ADP_Info_Points
 * ---------------
 * Summary: Returns a 32bit wide bitset of break- and watch-point
 * features supported by the target debug agent.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word breakinfo)
 *
 * 'status' returns RDIError_NoError on success or non-zero to indicate
 * some sort of error.
 * 'breakinfo' is a 32bit wide bitset described in detail below.  Note
 * that only bits 1..12 are used.
 */

/* 'breakinfo':- */
/* Can trap on address equality. */
#define ADP_Info_Points_Comparison      (1 << 0)

/* Can trap on address range. */
#define ADP_Info_Points_Range           (1 << 1)

/* Can trap on 8bit memory reads. */
#define ADP_Info_Points_ReadByteWatch   (1 << 2)

/* Can trap on 16bit memory reads. */
#define ADP_Info_Points_ReadHalfWatch   (1 << 3)

/* Can trap on 32bit memory reads. */
#define ADP_Info_Points_ReadWordWatch   (1 << 4)

/* Can trap on 8bit write accesses. */
#define ADP_Info_Points_WriteByteWatch  (1 << 5)

/* Can trap on 16bit write accesses. */
#define ADP_Info_Points_WriteHalfWatch  (1 << 6)

/* Can trap on 32bit write accesses. */
#define ADP_Info_Points_WriteWordWatch  (1 << 7)

/* Like range, but based on address bitmask<. */
#define ADP_Info_Points_Mask            (1 << 8)

/* Multi-threaded support only - thread specific breakpoints. */
#define ADP_Info_Points_ThreadBreak     (1 << 9)

/* Multi-threaded support only - thread specific watchpoints. */
#define ADP_Info_Points_ThreadWatch     (1 << 10)

/* Allows conditional breakpoints. */
#define ADP_Info_Points_Conditionals    (1 << 11)

/* Break- and watch-points can be interrogated */
#define ADP_Info_Points_Status          (1 << 12)


#define ADP_Info_Step                   ADPSUBREASON(CI_HADP,3)
/* ADP_Info_Step
 * -------------
 * Summary: Returns a 32bit wide bitmask of the single-stepping
 * capabilities of the target debug agent.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word stepinfo)
 *
 * 'status' returns RDIError_NoError on success, or non-zero to indicate
 * some kind of error.
 * 'stepinfo' is a 32bit wide bitmask described in detail below.  Note that
 * only 3 bits are used.
 */

/* 'stepinfo':- */
/* Single-stepping of more than one instruction is possible. */
#define ADP_Info_Step_Multiple  (1 << 0)

/* Single-stepping until next direct PC change is possible. */
#define ADP_Info_Step_PCChange  (1 << 1)

/* Single-stepping of a single instruction is possible. */
#define ADP_Info_Step_Single    (1 << 2)


#define ADP_Info_MMU                    ADPSUBREASON(CI_HADP,4)
/* ADP_Info_MMU
 * ------------
 * Summary: Returns information about the memory management system (if
 * any).
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word meminfo)
 *
 * 'status' returns RDIError_NoError to indicate success or non-zero to
 * indicate some kind of error.
 * 'meminfo' should be a 32bit unique ID, or zero if there is no MMU
 * support on the target.
 */


#define ADP_Info_SemiHosting            ADPSUBREASON(CI_HADP,5)
/* ADP_Info_SemiHosting
 * --------------------
 * Summary: This message is used to check whether semi-hosting info calls
 * are available on the target.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' returns RDIError_NoError if semi-hosting info calls are available,
 * non-zero otherwise.
 */


#define ADP_Info_CoPro                  ADPSUBREASON(CI_HADP,6)
/* ADP_Info_CoPro
 * --------------
 * Summary: This message checks whether CoProcessor info calls are
 * supported.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' returns RDIError_NoError to indicate these facilities
 * are supported, non-zero otherwise.
 */


#define ADP_Info_Cycles                 ADPSUBREASON(CI_HADP,7)
/* ADP_Info_Cycles
 * ---------------
 * Summary: Returns the number of instructions and cycles executed since
 * the target was initialised.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word ninstr, word Scycles, word Ncycles,
 *          word Icycles, word Ccycles, word Fcycles)
 *
 * 'status' is RDIError_NoError to indicate success, or non-zero if there
 * is no target support for gathering cycle count information.
 * 'ninstr' is the number of instructions executed.
 * 'Scycles' is the number of S-cycles executed.
 * 'Ncycles' is the number of N-cycles executed.
 * 'Icycles' is the number of I-cycles executed.
 * 'Ccycles' is the number of C-cycles executed.
 * 'Fcycles' is the number of F-cycles executed.
 */


#define ADP_Info_DescribeCoPro          ADPSUBREASON(CI_HADP,8)
/* ADP_Info_DescribeCoPro
 * ----------------------
 * Summary: Describe the registers of a coprocessor.  Use only if
 * ADP_Info_CoPro return RDIError_NoError.
 *
 * Arguments:
 * Send:   Arguments of the form:
 *         (byte cpno, byte rmin, byte rmax, byte nbytes, byte access,
 *          byte cprt_r_b0, byte cprt_r_b1, byte cprt_w_b0, byte cprt_w_b1)
 *         And a terminating byte = 0xff.  Must be within maximum buffer size.
 * Return: (word status)
 *
 * 'cpno' is the number of the coprocessor to be described.
 * 'rmin' is the bottom of a range of registers with the same description.
 * 'rmax' is the top of a range of registers with the same description.
 * 'nbytes' is the size of the register.
 * 'access' describes access to the register and is described in more detail
 * below.
 *
 * If bit 2 of access is set:-
 * 'cprt_r0' provides bits 0 to 7, and
 * 'cprt_r1' provides bits 16 to 23 of a CPRT instruction to read the
 * register.
 * 'cprt_w0' provides bits 0 to 7, and
 * 'cprt_w1' provides bits 16 to 23 of a CPRT instruction to write the
 * register.
 *
 * Otherwise, 'cprt_r0' provides bits 12 to 15, and 'cprt_r1' bit 22 of CPDT
 * instructions to read and write the register ('cprt_w0' and 'cprt_w1' are
 * junk).
 */

/* 'access':- */
/* Readable. */
#define ADP_Info_DescribeCoPro_Readable   (1 << 0)

/* Writeable. */
#define ADP_Info_DescribeCoPro_Writeable  (1 << 1)

/* Registers read or written via CPDT instructions (else CPRT) with this
   bit set. */
#define ADP_Info_DescribeCoPro_CPDT       (1 << 2)

#define ADP_Info_RequestCoProDesc       ADPSUBREASON(CI_HADP,9)
/* ADP_Info_RequestCoProDesc
 * -------------------------
 * Summary: Requests a description of the registers of a coprocessor.  Use
 * only if ADP_Info_CoPro return RDIError_NoError.
 *
 * Arguments:
 * Send:   (byte cpno)
 * Return: Arguments of the form:-
 *         (word status, byte rmin, byte rmax, byte nbytes, byte access)
 *         Followed by a terminating byte = 0xFF.  Must be within maximum
 *         buffer size.
 * 'cpno' is the number of the coprocessor to describe.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'rmin' is the bottom of a range of registers with the same description.
 * 'rmax' is the top of a range of registers with the same description.
 * 'nbytes' is the size in bytes of the register(s).
 * 'access' is as above in ADP_Info_DescribeCoPro.
 */


#define ADP_Info_AngelBufferSize        ADPSUBREASON(CI_HADP,10)
/* ADP_Info_AngelBufferSize
 * ------------------------
 * Summary: Returns the Angel buffer sizes.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word defaultsize, word maxsize)
 *
 * 'status' returns RDIError_NoError to indicate success or non-zero to
 * indicate some kind of error.
 * 'defaultsize' is the default Angel ADP buffer size in bytes. This is
 * at least 256 bytes.
 * 'maxsize' is the largest Angel ADP buffer size in bytes. This will be
 * greater than or equal to defaultsize.  The target will accept ADP messages
 * of up to this length for download, etc.
 *
 * Was DownLoadSize in RDP/RDI world.  This is the amount that the target
 * should transmit in a single operation.  This should now be the Angel
 * buffer size.  This information is also given in the ADP_Booted message.
 *
 * NOTE: The value returned should be the DATASIZE and *NOT* BUFFERDEFSIZE.
 * This is needed to ensure that the transport protocol information
 * can be wrapped around the data.
 */

#define ADP_Info_ChangeableSHSWI        ADPSUBREASON(CI_HADP,11)
/* ADP_Info_ChangeableSHSWI
 * ------------------------
 * Summary: This message is used to check whether it is possible to change
 * which SWI's are used for semihosting.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' returns RDIError_NoError if semi-hosting info calls are available,
 * non-zero otherwise.
 */

#define ADP_Info_CanTargetExecute      ADPSUBREASON(CI_HADP,12)
/* ADP_Info_CanTargetExecute
 * -------------------------
 * Summary: This message is used to see if the target is currently in
 * an executable state.  Typically this is called after the debugger
 * initialises.  If a non-error statis is returned then the user is
 * allowed to 'go' immediately.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' returns RDIError_NoError if target is ready to execute.
 * other values indicate why it cannot execute.
 */

#define ADP_Info_AgentEndianess     ADPSUBREASON(CI_HADP,13)
/* ADP_Info_AgentEndianess
 * -------------------------
 * Summary: This message is used to determine the endianess of the
 * debug agent
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * status should be RDIError_LittleEndian or RDIError_BigEndian
 * any other value indicates the target does not support this
 * request, so the debugger will have to make a best guess, which
 * probably means only allow little endian loadagenting.
 */


#define ADP_Control             ADPREASON(CI_HADP,2)
/* This message allows for the state of the debug agent to be
 * manipulated by the host.
 */

/* The following are sub reason codes to ADP control, the first parameter
 * is the sub reason code which defines the format of subsequent parameters.
 *
 * word         sub reason code
 */

#define ADP_Ctrl_NOP                    ADPSUBREASON(CI_HADP,0)
/* ADP_Ctrl_NOP
 * ------------
 * Summary: This message is used to check that ADP_Ctrl messages are
 * supported.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate ADP_Ctrl messages are
 * supported, non-zero otherwise.
 */

#define ADP_Ctrl_VectorCatch            ADPSUBREASON(CI_HADP,1)
/* ADP_Ctrl_VectorCatch
 * --------------------
 * Summary: Specifies which hardware exceptions should be reported to the
 * debugger.
 *
 * Arguments:
 * Send:   (word bitmap)
 * Return: (word status)
 *
 * 'bitmap' is a bit-mask of exceptions to be reported, described in more
 * detail below.  A set bit indicates that the exception should be
 * reported to the debugger, a clear bit indicates that the corresponding
 * exception vector should be taken.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */

/* 'bitmap':- */
/* Reset(branch through zero). */
#define ADP_Ctrl_VectorCatch_BranchThroughZero      (1 << 0)

/* Undefined Instruction. */
#define ADP_Ctrl_VectorCatch_UndefinedInstr         (1 << 1)

/* Software Interrupt. */
#define ADP_Ctrl_VectorCatch_SWI                    (1 << 2)

/* Prefetch Abort. */
#define ADP_Ctrl_VectorCatch_PrefetchAbort          (1 << 3)

/* Data Abort. */
#define ADP_Ctrl_VectorCatch_DataAbort              (1 << 4)

/* Address Exception. */
#define ADP_Ctrl_VectorCatch_AddressException       (1 << 5)

/* Interrupt Request. */
#define ADP_Ctrl_VectorCatch_IRQ                    (1 << 6)

/* Fast Interrupt Request. */
#define ADP_Ctrl_VectorCatch_FIQ                    (1 << 7)

/* Error. */
#define ADP_Ctrl_VectorCatch_Error                  (1 << 8)


#define ADP_Ctrl_PointStatus_Watch      ADPSUBREASON(CI_HADP,2)
/* ADP_Ctrl_PointStatus_Watch
 * --------------------------
 * Summary: Returns the hardware resource number and the type of that
 * resource when given a watchpoint handle.  Should only be called if
 * the value returned by ADP_Info_Points had ADP_Info_Points_Status set.
 *
 * Arguments:
 * Send:   (word handle)
 * Return: (word status, word hwresource, word type)
 *
 * 'handle' is a handle to a watchpoint.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'hwresource' is the hardware resource number. !!!!!
 * 'type' is the type of the resource.
 */


#define ADP_Ctrl_PointStatus_Break      ADPSUBREASON(CI_HADP,3)
/* ADP_Ctrl_PointStatus_Break
 * --------------------------
 * Summary: Returns the hardware resource number and the type of that
 * resource when given a breakpoint handle.  Should only be called if
 * the value returned by ADP_Info_Points had ADP_Info_Points_Status set.
 *
 * Arguments:
 * Send:   (word handle)
 * Return: (word status, word hwresource, word type)
 *
 * 'handle' is a handle to a breakpoint.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'hwresource' is the hardware resource number.
 * 'type' is the type of the resource.
 */

#define ADP_Ctrl_SemiHosting_SetState   ADPSUBREASON(CI_HADP,4)
/* ADP_Ctrl_SemiHosting_SetState
 * -----------------------------
 * Summary: Sets whether or not semi-hosting is enabled.
 *
 * Arguments:
 * Send:   (word semihostingstate)
 * Return: (word status)
 *
 * 'semihostingstate' sets semi-hosting to enabled if zero, otherwise
 * it disables semi-hosting.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: This should only be called if ADP_Info_SemiHosting didn't return
 * an error.
 */


#define ADP_Ctrl_SemiHosting_GetState   ADPSUBREASON(CI_HADP,5)
/* ADP_Ctrl_SemiHosting_GetState
 * -----------------------------
 * Summary: Reads whether or not semi-hosting is enabled.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word semihostingstate)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'semihostingstate' is zero if semi-hosting is enabled, non-zero otherwise.
 *
 * NOTE: This should only be called if ADP_Info_SemiHosting didn't return
 * an error.
 */


#define ADP_Ctrl_SemiHosting_SetVector  ADPSUBREASON(CI_HADP,6)
/* ADP_Ctrl_SemiHosting_SetVector
 * ------------------------------
 * Summary: Sets the semi-hosting vector.
 *
 * Arguments:
 * Send:   (word semihostingvector)
 * Return: (word status)
 *
 * 'semihostingvector' holds the value the vector is to be set to.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: This should only be called if ADP_Info_SemiHosting didn't return
 * an error.
 */


#define ADP_Ctrl_SemiHosting_GetVector  ADPSUBREASON(CI_HADP,7)
/* ADP_Ctrl_SemiHosting_GetVector
 * ------------------------------
 * Summary: Gets the value of the semi-hosting vector.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word semihostingvector)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'semihostingvector' holds the value of the vector.
 *
 * NOTE: This should only be called if ADP_Info_SemiHosting didn't return
 * an error.
 */


#define ADP_Ctrl_Log                    ADPSUBREASON(CI_HADP,8)
/* ADP_Ctrl_Log
 * ------------
 * Summary: Returns the logging state.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word logsetting)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'logsetting' is a bitmap specifying the level of logging desired,
 *  described in more detail below.  The bits can be ORed together
 */

/* 'logsetting':- */

/* No logging. */
#define ADP_Ctrl_Log_NoLogging    (0)
/* RDI level logging. */
#define ADP_Ctrl_Log_RDI          (1 << 0)
/* ADP byte level logging. */
#define ADP_Ctrl_Log_ADP          (1 << 1)


#define ADP_Ctrl_SetLog                 ADPSUBREASON(CI_HADP,9)
/* ADP_Ctrl_SetLog
 * ---------------
 * Summary: Sets the logging state.
 *
 * Arguments:
 * Send:   (word logsetting)
 * Return: (word status)
 *
 * 'logsetting' is the same as in ADP_Ctrl_Log above.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */

#define ADP_Ctrl_SemiHosting_SetARMSWI   ADPSUBREASON(CI_HADP,10)
/* ADP_Ctrl_SemiHosting_SetARMSWI
 * ------------------------------
 * Summary: Sets the number of the ARM SWI used for semihosting
 *
 * Arguments:
 * Send:   (word ARM_SWI_number)
 * Return: (word status)
 *
 * The debug agent will interpret ARM SWI's with the SWI number specified
 * as semihosting SWI's.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: This should only be called if ADP_Info_ChangeableSHSWI didn't return
 * an error.
 */


#define ADP_Ctrl_SemiHosting_GetARMSWI   ADPSUBREASON(CI_HADP,11)
/* ADP_Ctrl_SemiHosting_GetARMSWI
 * ------------------------------
 * Summary: Reads the number of the ARM SWI used for semihosting
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word ARM_SWI_number)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * ARM_SWI_number is the SWI number which is used for semihosting.
 *
 * NOTE: This should only be called if ADP_Info_SemiHosting didn't return
 * an error.
 */

#define ADP_Ctrl_SemiHosting_SetThumbSWI   ADPSUBREASON(CI_HADP,12)
/* ADP_Ctrl_SemiHosting_SetThumbSWI
 * --------------------------------
 * Summary: Sets the number of the Thumb SWI used for semihosting
 *
 * Arguments:
 * Send:   (word Thumb_SWI_number)
 * Return: (word status)
 *
 * The debug agent will interpret Thumb SWI's with the SWI number specified
 * as semihosting SWI's.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: This should only be called if ADP_Info_ChangeableSHSWI didn't return
 * an error.
 */


#define ADP_Ctrl_SemiHosting_GetThumbSWI   ADPSUBREASON(CI_HADP,13)
/* ADP_Ctrl_SemiHosting_GetThumbSWI
 * --------------------------------
 * Summary: Reads the number of the Thumb SWI used for semihosting
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word ARM_Thumb_number)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * Thumb_SWI_number is the SWI number which is used for semihosting.
 *
 * NOTE: This should only be called if ADP_Info_SemiHosting didn't return
 * an error.
 */


#define ADP_Ctrl_Download_Supported   ADPSUBREASON(CI_HADP,14)
/* ADP_Ctrl_Download_Supported
 * ---------------------------
 * Summary: Can configuration be downloaded?
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError if the configuration can be downloaded,
 * non-zero otherwise.
 *
 * NOTE: Equivalent to RDIInfo_DownLoad.
 */


#define ADP_Ctrl_Download_Data       ADPSUBREASON(CI_HADP,15)
/* ADP_Ctrl_Download_Data
 * ----------------------
 * Summary: Loads configuration data.
 *
 * Arguments:
 * Send:   (word nbytes, words data)
 * Return: (word status)
 *
 * 'nbytes' is the number of *bytes* being sent.
 * 'data' is the configuration data. NOTE: data must not cause the buffer
 * size to exceed the maximum allowed buffer size.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDP_LoadConfigData.  Should only be used if
 * ADP_ICEM_AddConfig didn't return an error.
 */


#define ADP_Ctrl_Download_Agent            ADPSUBREASON(CI_HADP,16)
/* ADP_Ctrl_Download_Agent
 * -----------------------
 * Summary: Prepares Debug Agent to receive configuration data which it
 * should interpret as a new version of the Debug Agent code.
 *
 * Arguments:
 * Send:   (word loadaddress, word size)
 * Return: (word status)
 *
 * 'loadaddress' is the address where the new Debug Agent code should be
 * loaded.
 * 'size' is the number of bytes of Debug Agent code to be loaded.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDP_LoadAgent.  The data will be downloaded using
 * ADP_Ctrl_Download_Data.  The new agent is started with ADP_Ctrl_Start_Agent
 */


#define ADP_Ctrl_Start_Agent                    ADPSUBREASON(CI_HADP,17)
/* ADP_Ctrl_Start_Agent
 * -----------------------
 * Summary: Instruct Debug Agent to begin execution of new agent,
 * which has been downloaded by ADP_Ctrl_Download_Agent.
 *
 * Arguments:
 * Send:   (word startaddress)
 * Return: (word status)
 *
 * 'startaddress' is the address where the new Debug Agent code should be
 *  entered, and must satisfy:
 *     (loadaddress <= startaddress <= (loadaddress + size))
 *  where 'loadaddress' and  'size' were specified in the
 *  ADP_Ctrl_Download_Agent message.
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */


#define ADP_Ctrl_SetTopMem                      ADPSUBREASON(CI_HADP,18)
/* ADP_Ctrl_SetTopMem
 * ------------------
 * Summary: Sets the top of memory for ICEman2 systems, so that the C Library
 * can allocate the stack in the correct place on startup.
 *
 * Arguments:
 * Send:   (word mem_top)
 * Return: (word status)
 *
 * This request should only be supported by ICEman2.  Standard Angel systems
 * should return an error (unrecognised is fine).
 */


#define ADP_Read                ADPREASON(CI_HADP,3)
#define ADP_ReadHeaderSize      (ADP_DEFAULT_HEADER_SIZE + 2*sizeof(word))

/* ADP_Read
 * --------
 * Summary: Request for a transer of memory contents from the target to the
 * debugger.
 *
 * Arguments:
 * Send:   (word address, word nbytes)
 * Return: (word status, word rnbytes [, bytes data])
 *
 * 'address' is the address from which memory transer should start.
 * 'nbytes' is the number of bytes to transfer.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'rnbytes' holds the number of requested bytes NOT read (i.e. zero
 * indicates success, non-zero indicates an error).
 * 'data' is the number of bytes requested minus 'rnbytes'.
 */



#define ADP_Write               ADPREASON(CI_HADP,4)
#define ADP_WriteHeaderSize     (ADP_DEFAULT_HEADER_SIZE + 2*sizeof(word))

/* ADP_Write
 * ---------
 * Summary: Request for a transfer of memory contents from the debugger to
 * the target.
 *
 * Arguments:
 * Send:   (word address, word nbytes, bytes data)
 * Return: (word status [, word rnbytes])
 *
 * 'address' is the address from which memory transer should start.
 * 'nbytes' is the number of bytes to transfer.
 * 'data' holds the bytes to be transferred.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'rnbytes' holds the number of requested bytes NOT written (i.e. zero
 * indicates success, non-zero indicates an error) if status indicated an
 * error.
 */



#define ADP_CPUread             ADPREASON(CI_HADP,5)
/* ADP_CPUread
 * -----------
 * Summary: This is a request to read values in the CPU.
 *
 * Arguments:
 * Send:   (byte mode, word mask)
 * Return: (word status, words data)
 *
 * 'mode' defines the processor mode from which the transfer should be made.
 * It is described in more detail below.
 * 'mask' indicates which registers should be transferred. Setting a bit to
 * one will cause the designated register to be transferred. The details
 * of mask are specified below.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'data' holds the values of the registers on successful completion,
 * otherwise it just holds rubbish.  The lowest numbered register is
 * transferred first.  NOTE: data must not cause the buffer size to exceed
 * the maximum allowed buffer size.
 */

/* 'mode':- */
/* The mode number is the same as the mode number used by an ARM; a value of
   255 indicates the current mode. */
#define ADP_CPUmode_Current     (255)

/* 26bit user mode. */
#define ADP_CPUread_26bitUser   (0x0)

/* 26bit FIQ mode. */
#define ADP_CPUread_26bitFIQ    (0x1)

/* 26bit IRQ mode. */
#define ADP_CPUread_26bitIRQ    (0x2)

/* 26bit Supervisor mode. */
#define ADP_CPUread_26bitSVC    (0x3)

/* 32bit user mode. */
#define ADP_CPUread_32bitUser   (0x10)

/* 32bit FIQ mode. */
#define ADP_CPUread_32bitFIQ    (0x11)

/* 32bit IRQ mode. */
#define ADP_CPUread_32bitIRQ    (0x12)

/* 32bit Supervisor mode. */
#define ADP_CPUread_32bitSVC    (0x13)

/* 32bit Abort mode. */
#define ADP_CPUread_32bitAbort  (0x17)

/* 32bit Undefined mode. */
#define ADP_CPUread_32bitUndef  (0x1B)

/* #32bit System mode - Added in Architecture 4 ARMs e.g.ARM7TDMI */
#define ADP_CPUread_32bitSystem (0x1F)

/* 'mask':- */
/* Request registers RO-R14. */
#define ADP_CPUread_RegsMask  (0x7FFF)

/* Request Program Counter (including mode and flag bits in 26-bit modes. */
#define ADP_CPUread_PCmode    (1 << 15)

/* Request Program Counter (without mode and flag bits in 26-bit modes. */
#define ADP_CPUread_PCnomode  (1 << 16)

/* Requests the transfer of the CPSR */
#define ADP_CPUread_CPSR      (1 << 17)

/* In processor modes with an SPSR(non-user modes), bit 19 requests its
   transfer */
#define ADP_CPUread_SPSR      (1 << 18)



#define ADP_CPUwrite            ADPREASON(CI_HADP,6)
/* ADP_CPUwrite
 * ------------
 * Summary: This is a request to write values to the CPU.
 *
 * Arguments:
 * Send:   (byte mode, word mask, words data)
 * Return: (word status)
 *
 * 'mode' defines the processor mode to which the transfer should be made.
 * The mode number is the same as the mode number used by ARM; a value of
 * 255 indicates the current mode. See ADP_CPUread above for more detail.
 * 'mask' indicates which registers should be transferred. Setting a bit to
 * one will cause the designated register to be transferred. The details
 * of mask are specified above in ADP_CPUread.
 * 'data' holds the values of the registers to be transferred.  The first
 * value is written to the lowest numbered register.  NOTE: data must not
 * cause the buffer size to exceed the maximum allowed buffer size.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */



#define ADP_CPread              ADPREASON(CI_HADP,7)
/* ADP_CPread
 * ----------
 * Summary: This message requests a co-processors internal state.
 *
 * Arguments:
 * Send:   (byte CPnum, word mask)
 * Return: (word status, words data)
 *
 * 'CPnum' is the number of the co-processor to transfer values from.
 * 'mask' specifies which registers to transfer and is co-processor
 * specific.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'data' holds the registers specified in 'mask' if successful, otherwise
 * just rubbish.  The lowest numbered register is transferred first.
 * NOTE: data must not cause the buffer size to exceed the maximum allowed
 * buffer size.
 */



#define ADP_CPwrite             ADPREASON(CI_HADP,8)
/* ADP_CPwrite
 * -----------
 * Summary: This message requests a write to a co-processors internal state.
 *
 * Arguments:
 * Send:   (byte CPnum, word mask, words data)
 * Return: (word status)
 *
 * 'CPnum' is the number of the co-processor to transfer values to.
 * 'mask' specifies which registers to transfer and is co-processor
 * specific.
 * 'data' holds the values to transfer to the registers specified in 'mask'.
 * The first value is written to the lowest numbered register.
 * NOTE: data must not cause the buffer size to exceed the maximum allowed
 * buffer size.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */



#define ADP_SetBreak            ADPREASON(CI_HADP,9)
/* ADP_SetBreak
 * ------------
 * Summary: Sets a breakpoint.
 *
 * Arguments:
 * Send:   (word address, byte type [, word bound])
 * Return: (word status, word pointhandle, word raddress, word rbound)
 *
 * 'address' is the address of the instruction to set the breakpoint on.
 * 'type' specifies the sort of breakpoint and is described in more detail
 * below.
 * 'bound' is included if the least significant 4 bits of type are set to
 * 5 or above (see below for more detail).
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'pointhandle'  returns a handle to the breakpoint, it will be valid if bit
 * 7 of 'type' is set.  See below for more detail.
 * 'raddress' is valid depending on 'type', see below for more detail.
 * 'rbound' is valid depending on 'type', see below for more detail.
 */

/* 'type':- */
/* The least significant 4 bits define the sort of breakpoint to set:- */
/* Halt if the pc is equal to 'address'. */
#define ADP_SetBreak_EqualsAddress         (0)

/* Halt if the pc is greater than 'address'. */
#define ADP_SetBreak_GreaterAddress        (1)

/* Halt if the pc is greater than or equal to 'address'. */
#define ADP_SetBreak_GEqualsAddress        (2)

/* Halt if the pc is less than 'address'. */
#define ADP_SetBreak_LessAddress           (3)

/* Halt if the pc is less than or equal to 'address'. */
#define ADP_SetBreak_LEqualsAddress        (4)

/* Halt if the pc is in the range from 'address' to 'bound' inclusive. */
#define ADP_SetBreak_Range                 (5)

/* Halt if the pc is not in the range from 'address' to 'bound' inclusive. */
#define ADP_SetBreak_NotRange              (6)

/* Halt if (pc & 'bound') = 'address'. */
#define ADP_SetBreak_AndBound              (7)

/* Bits 5,6 and 7 are used as follows :- */
/* If set this indicates that the breakpoint is on a 16bit (Thumb)
   instruction rather than a 32bit (ARM) instruction. */
#define ADP_SetBreak_Thumb                 (1 << 4)

/* This requests that the breakpoint should be conditional (execution halts
   only if the breakpointed instruction is executed, not if it is
   conditionally skipped).  If bit 5 is not set, execution halts whenever
   the breakpointed instruction is reached (whether executed or skipped). */
#define ADP_SetBreak_Cond                  (1 << 5)

/* This requests a dry run: the breakpoint is not set and the 'raddress', and
   if appropriate the 'rbound', that would be used, are returned (for
   comparison and range breakpoints the address and bound used need not be
   exactly as requested).  A RDIError_NoError 'status' byte indicates that
   resources are currently available to set the breakpoint, non-zero
   indicates an error. RDIError_NoMorePoints indicates that the required
   breakpoint resources are not currently available. */
#define ADP_SetBreak_DryRun                (1 << 6)

/* If the request is successful, but there are no more breakpoint registers
   (of the requested type), then the value RDIError_NoMorePoints is
   returned. */

/* If a breakpoint is set on a location which already has a breakpoint, the
   first breakpoint will be removed before the new breakpoint is set. */



#define ADP_ClearBreak          ADPREASON(CI_HADP,10)
/* ADP_ClearBreak
 * --------------
 * Summary: Clears a breakpoint.
 *
 * Arguments:
 * Send:   (word pointhandle)
 * Return: (word status)
 *
 * 'pointhandle' is a handle returned by a previous ADP_SetBreak.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */


#define ADP_SetWatch            ADPREASON(CI_HADP,11)
/* ADP_SetWatch
 * ------------
 * Summary: Sets a watchpoint.
 *
 * Arguments:
 * Send:   (word address, byte type, byte datatype [,word bound])
 * Return: (word status, word pointhandle, word raddress, word rbound)
 *
 * 'address' is the address at which to set the watchpoint.
 * 'type' is the type of watchpoint to set and is described in detail below.
 * 'datatype' defines the sort of data access to watch for and is described
 * in more detail below.
 * 'bound' is included depending on the value of type (see description of
 * type below).
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'pointhandle' is valid depending on the value of type (see description
 * of type below).
 * 'raddress' is valid depending on the value of type (see description
 * of type below).
 * 'rbound' is valid depending on the value of type (see description
 * of type below).
 */

/* 'type':- */
/* The least significant 4 bits of 'type' define the sort of watchpoint to
   set:- */
/* Halt on a data access to the address equal to 'address'. */
#define ADP_SetWatch_EqualsAddress          (0)

/* Halt on a data access to an address greater than 'address'. */
#define ADP_SetWatch_GreaterAddress         (1)

/* Halt on a data access to an address greater than or equal to 'address'. */
#define ADP_SetWatch_GEqualsAddress         (2)

/* Halt on a data access to an address less than 'address'. */
#define ADP_SetWatch_LessAddress            (3)

/* Halt on a data access to an address less than or equal to 'address'. */
#define ADP_SetWatch_LEqualsAddress         (4)

/* Halt on a data access to an address in the range from 'address' to
   'bound' inclusive. */
#define ADP_SetWatch_Range                  (5)

/* Halt on a data access to an address not in the range from 'address' to
   'bound' inclusive. */
#define ADP_SetWatch_NotRange               (6)

/* Halt if (data-access-address & 'bound')='address'. */
#define ADP_SetWatch_AndBound               (7)

/* Bits 6 and 7 of 'type' also have further significance:-
   NOTE: they must not be simulataneously set. */

/* Bit 6 of 'type' set:  Requests a dry run: the watchpoint is not set and
   the 'address' and, if appropriate, the 'bound', that would be used are
   returned (for range and comparison watchpoints, the 'address' and 'bound'
   used need not be exactly as requested).  A RDIError_NoError status byte
   indicates that resources are currently available to set the watchpoint;
   RDIError_NoMorePoints indicates that the required watchpoint resources
   are not currently available. */

/* Bit 7 of 'type' set:  Requests that a handle should be returned for the
   watchpoint by which it will be identified subsequently.  If bit 7 is
   set, a handle will be returned ('pointhandle'), whether or not the
   request succeeds or fails (but, obviously, it will only be meaningful
   if the request succeesd). */

/* 'datatype':- */
/* The 'datatype' argument defines the sort of data access to watch for,
   values can be summed or ORed together to halt on any set of sorts of
   memory access. */

/* Watch for byte reads. */
#define ADP_SetWatch_ByteReads           (1)

/* Watch for half-word reads. */
#define ADP_SetWatch_HalfWordReads       (2)

/* Watch for word reads. */
#define ADP_SetWatch_WordReads           (4)

/* Watch for half-word reads. */
#define ADP_SetWatch_ByteWrites          (8)

/* Watch for half-word reads. */
#define ADP_SetWatch_HalfWordWrites      (16)

/* Watch for half-word reads. */
#define ADP_SetWatch_WordWrites          (32)

/* On successful completion a RDIError_NoError 'status' byte is returned.  On
   unsuccessful completion, a non-zero error code byte is returned.  If the
   request is successful, but there are now no more watchpoint registers
   (of the requested type), then the value RDIError_NoMorePoints is
   returned. */

/* If a watchpoint is set on a location which already has a watchpoint, the
   first watchpoint will be removed before the new watchpoint is set. */


#define ADP_ClearWatch          ADPREASON(CI_HADP,12)
/* ADP_ClearWatch
 * --------------
 * Summary: Clears a watchpoint.
 *
 * Arguments:
 * Send:   (word pointhandle)
 * Return: (word status)
 *
 * 'pointhandle' is a handle to a watchpoint returned by a previous
 * ADP_SetWatch.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */



#define ADP_Execute             ADPREASON(CI_HADP,13)
/* ADP_Execute
 * -----------
 * Summary: This message requests that the target starts executing from
 * the stored CPU state.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * The message will *ALWAYS* respond immediately with an ACK (unlike the
 * old RDI definition, which allowed asynchronous message replies).
 *
 * Execution will stop when allowed system events occur. The host will
 * be notified via a ADP_Stopped message (described below).
 */



#define ADP_Step                ADPREASON(CI_HADP,14)
/* ADP_Step
 * --------
 * Summary: Execute 'ninstr' instructions.
 *
 * Arguments:
 * Send: (word ninstr)
 * Return: (word status)
 *
 * 'ninstr' is the number of instructions to execute, starting at the
 * address currently loaded into the CPU program counter.  If it is zero,
 * the target should execute instructions upto the next instruction that
 * explicitly alters the Program Counter. i.e. a branch or ALU operation
 * with the PC as the destination.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * The ADP_Step function (unlike the earlier RDI system) will *ALWAYS*
 * return an ACK immediately. A subsequent ADP_Stopped message will be
 * delivered from the target to the host when the ADP_Step operation
 * has completed.
 */



#define ADP_InterruptRequest    ADPREASON(CI_HADP,15)
/* ADP_InterruptRequest
 * --------------------
 * Summary: Interrupt execution.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * On receiving this message the target should attempt to stop execution.
 */



#define ADP_HW_Emulation             ADPREASON(CI_HADP,16)
/* ADP_HW_Emulation
 * ----------------
 * The first parameter to ADP_HW_Emulation is a Reason Subcode, and
 * subsequent parameters are defined by that subcode
 *
 * word         reason subcode
 * other arguments as reason subcode determines
 *
 */

/* ADP__HW_Emulation sub-reason codes: */

#define ADP_HW_Emul_Supported         ADPSUBREASON(CI_HADP,0)
/* ADP_HW_Emul_Supported
 * ---------------------
 * Summary: Enquires whether calls to the next 4 messages are available
 * (MemoryAccess, MemoryMap, Set_CPUspeed, ReadClock).
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate the messages are available,
 * non-zero otherwise.
 *
 * NOTE: Equivalent to RDI_Info_Memory_Stats.
 */


#define ADP_HW_Emul_MemoryAccess      ADPSUBREASON(CI_HADP,1)
/* ADP_HW_Emul_MemoryAccess
 * ------------------------
 * Summary: Get memory access information for memory block with specified
 * handle.
 *
 * Arguments:
 * Send:   (word handle)
 * Return: (word status, word nreads, word nwrites, word sreads,
 *          word swrites, word ns, word s)
 *
 * 'handle' is a handle to a memory block.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'nreads' is the number of non-sequential reads.
 * 'nwrites' is the number of non-sequential writes.
 * 'sreads' is the number of sequential reads.
 * 'swrites' is the number of sequential writes.
 * 'ns' is time in nano seconds.
 * 's' is time in seconds.
 *
 * NOTE: Equivalent to RDIMemory_Access.
 */


#define ADP_HW_Emul_MemoryMap         ADPSUBREASON(CI_HADP,2)
/* ADP_HW_Emul_MemoryMap
 * ---------------------
 * Summary: Sets memory characteristics.
 *
 * Arguments:
 * Send:   (word n,
    Then 'n' sets of arguments of the form:-
            word handle, word start, word limit, byte width,
            byte access, word Nread_ns, word Nwrite_ns, word Sread_ns,
            word Swrite_ns)
 * Return: (word status)
 *
 * 'n' is the number of sets of arguments.
 * 'handle' is a handle to the region.
 * 'start' is the start of this region.
 * 'limit' is the limit of this region.
 * 'width' is the memory width, described in detail below.
 * 'access' is described in detail below.
 * 'Nread_ns' is the access time for N read cycles in nano seconds.
 * 'Nwrite_ns' is the access time for N write cycles in nano seconds.
 * 'Sread_ns' is the access time for S read cycles in nano seconds.
 * 'Swrite_ns' is the access time for S write cycles in nano seconds.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * NOTE: Equivalent to RDIMemory_Map.
 */

/* 'width':- */
/* 8 bit memory width. */
#define ADP_HW_Emul_MemoryMap_Width8     (0)

/* 16 bit memory width. */
#define ADP_HW_Emul_MemoryMap_Width16    (1)

/* 32 bit memory width. */
#define ADP_HW_Emul_MemoryMap_Width32    (2)

/* 'access':- */
/* Bit 0 - read access. */
#define ADP_HW_Emul_MemoryMap_Access_Read      (1 << 0)

/* Bit 1 - write access. */
#define ADP_HW_Emul_MemoryMap_Access_Write     (1 << 1)

/* Bit 2 - latched 32 bit memory. */
#define ADP_HW_Emul_MemoryMap_Access_Latched   (1 << 2)


#define ADP_HW_Emul_SetCPUSpeed       ADPSUBREASON(CI_HADP,3)
/* ADP_HW_Emul_SetCPUSpeed
 * -----------------------
 * Summary: Sets the speed of the CPU.
 *
 * Arguments:
 * Send:   (word speed)
 * Return: (word status)
 *
 * 'speed' is the CPU speed in nano seconds.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDISet_CPUSpeed.
 */


#define ADP_HW_Emul_ReadClock         ADPSUBREASON(CI_HADP,4)
/* ADP_HW_Emul_ReadClock
 * ---------------------
 * Summary: Reads simulated time.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word ns, word s)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'ns' is time in nano seconds.
 * 's' is time in seconds.
 *
 * NOTE: Equivalent to RDIRead_Clock.
 */


#define ADP_ICEbreakerHADP            ADPREASON(CI_HADP,17)

/* The first parameter to ADP_ICEbreaker is a Reason Subcode, and
 * subsequent parameters are defined by that subcode
 *
 * word         reason subcode
 * other arguments as reason subcode determines
 *
 */

/* ADP_ICEbreaker sub-reason codes: */

#define ADP_ICEB_Exists               ADPSUBREASON(CI_HADP,0)
/* ADP_ICEB_Exists
 * ---------------
 * Summary: Is there an ICEbreaker in the system?
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate there is an ICEbreaker,
 * non-zero otherwise.
 */


#define ADP_ICEB_GetLocks             ADPSUBREASON(CI_HADP,1)
/* ADP_ICEB_GetLocks
 * -----------------
 * Summary: Returns which ICEbreaker registers are locked.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word lockedstate)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'lockedstate' is a bitmap if the ICEbreaker registers locked against use
 * by IceMan (because explicitly written by the user). Bit n represents
 * hardware breakpoint n, and if set the register is locked.
 *
 * NOTE: Equivalent to RDIIcebreaker_GetLocks.  Should only be used if
 * ADP_ICEB_Exists didn't return an error.
 */


#define ADP_ICEB_SetLocks             ADPSUBREASON(CI_HADP,2)
/* ADP_ICEB_SetLocks
 * -----------------
 * Summary: Sets which ICEbreaker registers are locked.
 *
 * Arguments:
 * Send:   (word lockedstate)
 * Return: (word status)
 *
 * 'lockedstate' is the same as in ADP_ICEB_GetLocks above.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDIIcebreaker_SetLocks.  Should only be used if
 * ADP_ICEB_Exists didn't return an error.
 */


#define ADP_ICEB_CC_Exists            ADPSUBREASON(CI_HADP,3)
/* ADP_ICEB_CC_Exists
 * ------------------
 * Summary: Is there an ICEbreaker Comms Channel?
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate there is a Comms Channel,
 * non-zero otherwise.
 *
 * NOTE: Should only be used if ADP_ICEB_Exists didn't return an error.
 */


#define ADP_ICEB_CC_Connect_ToHost    ADPSUBREASON(CI_HADP,4)
/* ADP_ICEB_CC_Connect_ToHost
 * --------------------------
 * Summary: Connect Comms Channel in ToHost direction.
 *
 * Arguments:
 * Send:   (byte connect)
 * Return: (word status)
 *
 * 'connect' !!!!!
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDICommsChannel_ToHost.  Should only be used if
 * ADP_ICEB_CC_Exists didn't return an error.
 */


#define ADP_ICEB_CC_Connect_FromHost  ADPSUBREASON(CI_HADP,5)
/* ADP_ICEB_CC_Connect_FromHost
 * ----------------------------
 * Summary: Connect Comms Channel in FromHost direction.
 *
 * Arguments:
 * Send:   (byte connect)
 * Return: (word status)
 *
 * 'connect' is the same as in ADP_ICEB_CC_Connect_ToHost above.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDICommsChannel_FromHost.  Should only be used if
 * ADP_ICEB_CC_Exists didn't return an error.
 */


#define ADP_ICEman                    ADPREASON(CI_HADP,18)

/* The first parameter to ADP_ICEman is a Reason Subcode, and
 * subsequent parameters are defined by that subcode
 *
 * word         reason subcode
 * other arguments as reason subcode determines
 *
 */

/* ADP_ICEman sub-reason codes: */


#define ADP_ICEM_AddConfig            ADPSUBREASON(CI_HADP,0)
/* ADP_ICEM_AddConfig
 * ------------------
 * Summary: Prepares target to receive configuration data block.
 *
 * Arguments:
 * Send:   (word nbytes)
 * Return: (word status)
 *
 * 'nbytes' is the number of bytes in the configuration block.
 * 'status' is RDIError_NoError to indicate success, non-zero if a
 * configuration block of this size can't be accepted.
 *
 * NOTE: Equivalent to RDP_AddConfig.
 */


#define ADP_ICEM_SelectConfig         ADPSUBREASON(CI_HADP,1)
/* ADP_ICEM_SelectConfig
 * ---------------------
 * Summary: Selects one of the sets of configuration data blocks and
 * reinitialises to use the new configuration.
 *
 * Arguments:
 * Send:   (byte aspect, byte namelen, byte matchtype, word vsn_req,
            bytes name)
 * Return: (word status, word vsn_sel)
 *
 * 'aspect' is one of two values defined below.
 * 'namelen' is the number of bytes in 'name'.
 * 'matchtype' specifies how the selected version must match that specified,
 * and takes one of the values defined below.
 * 'vsn_req' is the requested version of the named configuration.
 * 'name' is the name of the configuration.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'vsn_sel' is the version number of the configuration selected on success.
 *
 * NOTE: Equivalent to RDP_SelectConfig.
 */

/* 'aspect':- */
#define ADP_ICEM_SelectConfig_ConfigCPU       (0)
#define ADP_ICEM_SelectConfig_ConfigSystem    (1)

/* 'matchtype':- */
#define ADP_ICEM_SelectConfig_MatchAny        (0)
#define ADP_ICEM_SelectConfig_MatchExactly    (1)
#define ADP_ICEM_SelectConfig_MatchNoEarlier  (2)


#define ADP_ICEM_ConfigCount          ADPSUBREASON(CI_HADP,2)
/* ADP_ICEM_ConfigCount
 * --------------------
 * Summary: Return number of configurations.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status [, word count])
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'count' returns the number of configurations if status is zero.
 *
 * NOTE: Equivalent to RDIConfig_Count.
 */


#define ADP_ICEM_ConfigNth            ADPSUBREASON(CI_HADP,3)
/* ADP_ICEM_ConfigNth
 * ------------------
 * Summary: Gets the nth configuration details.
 *
 * Arguments:
 * Send:   (word confign)
 * Return: (word status, word version, byte namelen, bytes name)
 *
 * 'confign' is the number of the configuration.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'version' is the configuration version number.
 * 'namelen' is the number of bytes in 'name'.
 * 'name' is the name of the configuration.
 *
 * NOTE: Equivalent to RDIConfig_Nth.
 */



#define ADP_Profile                   ADPREASON(CI_HADP,19)

/* The first parameter to ADP_Profile is a Reason Subcode, and
 * subsequent parameters are defined by that subcode
 *
 * word         reason subcode
 * other arguments as reason subcode determines
 *
 */

/* ADP_Profile sub-reason codes: */


#define ADP_Profile_Supported         ADPSUBREASON(CI_HADP,0)
/* ADP_Profile_Supported
 * ---------------------
 * Summary: Checks whether profiling is supported.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError if profiling is supported, non-zero otherwise.
 *
 * NOTE: Can also be determined using Info_Target.
 */


#define ADP_Profile_Stop              ADPSUBREASON(CI_HADP,1)
/* ADP_Profile_Stop
 * ----------------
 * Summary: Stops profiling.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDIProfile_Stop.
 */


#define ADP_Profile_Start             ADPSUBREASON(CI_HADP,2)
/* ADP_Profile_Start
 * -----------------
 * Summary: Starts profiling (PC sampling).
 *
 * Arguments:
 * Send:   (word interval)
 * Return: (word status)
 *
 * 'interval' is the period of PC sampling in micro seconds.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDIProfile_Start.
 */


#define ADP_Profile_WriteMap          ADPSUBREASON(CI_HADP,3)
#define ADP_ProfileWriteHeaderSize    (ADP_DEFAULT_HEADER_SIZE + 4*sizeof(word))

/* ADP_Profile_WriteMap
 * --------------------
 * Summary: Downloads a map array, which describes the PC ranges for profiling.
 *
 * Arguments: A number of messages each of form:-
 * Send:   (word len, word size, word offset, words map_data)
 * Return: (word status)
 *
 * 'len' is the number of elements in the entire map array being downloaded.
 * 'size' is the number of words being downloaded in this message, i.e. the
 * length of 'map_data'.
 * 'offset' is the offset into the entire map array which this message starts
 * from, in words.
 * 'map_data' consists of 'size' words of map data.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDIProfile_WriteMap.
 */


#define ADP_Profile_ReadMap           ADPSUBREASON(CI_HADP,4)
#define ADP_ProfileReadHeaderSize     (ADP_DEFAULT_HEADER_SIZE + 2*sizeof(word))

/* ADP_Profile_ReadMap
 * -------------------
 * Summary: Uploads a set of profile counts which correspond to the current
 * profile map.
 *
 * Arguments: A number of messages, each of the form:
 * Send:   (word offset, word size)
 * Return: (word status, words counts)
 *
 * 'offset' is the offset in the entire array of counts that this message
 * starts from, in words.
 * 'size' is the number of words uploaded in this message (in counts).
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'counts' is 'size' words of profile counts.
 *
 * NOTE: Equivalent to RDIProfile_ReadMap.
 */


#define ADP_Profile_ClearCounts       ADPSUBREASON(CI_HADP,5)
/* ADP_Profile_ClearCounts
 * -----------------------
 * Summary: Requests that PC sample counts be set to zero.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDIProfile_ClearCounts.
 */

#define ADP_InitialiseApplication       ADPREASON(CI_HADP,20)
/* ADP_InitialiseApplication
 * -------------------------
 * Summary: Requests that OS setup up the thread/task so that it can be
 *          executed.
 *
 * Arguments:
 * Send: ()
 * Return: (word status)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 */

#define ADP_End                         ADPREASON(CI_HADP,21)
/* ADP_End
 * -------
 * Summary: Sent by the host debugger to tell angel this debugging session
 *          is is finished
 * Arguments:
 * Send: ()
 * Return: (word status)
 * status' is RDIError_NoError to indicate success, non-zero otherwise.
 */

/******************************************************************
 *
 * CI_TADP messages
 *
 */

#define ADP_TADPUnrecognised        ADPREASON(CI_TADP,0)
/* This message is unusual in that it is normally sent in reply to
 * another message which is not understood.  This is an exception
 * to the normal protocol which says that a reply must have the
 * same base reason code as the original.  There is a single reply
 * parameter which is the reason code which was not understood.
 *
 * As well as being a reply this message can also be sent and will
 * return as if this message were unrecognised!
 *
 * Parameters:
 *      none
 *
 * Reply:
 *      word    reason code which was not recognised
 */

/*-------------------------------------------------------------------------*/

#define ADP_Stopped             ADPREASON(CI_TADP,1)
/* ADP_Stopped
 * -----------
 * Summary: This message is sent to the host when the application stops,
 * either naturally or due to an exception.
 *
 * Parameters:
 *      word    reason subcode
 *      other arguments as reason subcode determines.
 *      Unless stated otherwise (below) there will be none.
 *
 * Reply:
 *      word status     unless reason subcode says otherwise
 *
 * This message is sent to the host when execution has stopped. This
 * can be when the end of the application has been reached, or as the
 * result of an exception. It can also be the return from an ADP_Step
 * process, when the requested number of instructions have been
 * executed., or a breakpoint or watchpoint has been hit etc.
 */

/* The first set of Stopped subreason codes are for the ARM hardware
 * vectors. These events will be raised if the
 * ADP_Control_Vector_Catch allows, or if the target application has
 * not provided its own handlers.
 */
#define ADP_Stopped_BranchThroughZero    ADPSUBREASON(CI_TADP,0)
#define ADP_Stopped_UndefinedInstr       ADPSUBREASON(CI_TADP,1)
#define ADP_Stopped_SoftwareInterrupt    ADPSUBREASON(CI_TADP,2)
#define ADP_Stopped_PrefetchAbort        ADPSUBREASON(CI_TADP,3)
#define ADP_Stopped_DataAbort            ADPSUBREASON(CI_TADP,4)
#define ADP_Stopped_AddressException     ADPSUBREASON(CI_TADP,5)
#define ADP_Stopped_IRQ                  ADPSUBREASON(CI_TADP,6)
#define ADP_Stopped_FIQ                  ADPSUBREASON(CI_TADP,7)

/* We leave the rest of what would be the bits in the VectorCatch
 * bitmask free for future expansion.
 */

/* The following are software reasons for execution stopping: */
#define ADP_Stopped_BreakPoint         ADPSUBREASON(CI_TADP,32)
/* Breakpoint was reached
 *   extra send parameter: word handle - indicates which breakpoint
 */

#define ADP_Stopped_WatchPoint         ADPSUBREASON(CI_TADP,33)
/* Watchpoint was triggered
 *   extra send parameter: word handle - indicates which watchpoint
 */

#define ADP_Stopped_StepComplete       ADPSUBREASON(CI_TADP,34)
/* End of ADP_Step request */

#define ADP_Stopped_RunTimeErrorUnknown ADPSUBREASON(CI_TADP,35)
/*
 * non-specific fatal runtime support error
 */

#define ADP_Stopped_InternalError      ADPSUBREASON(CI_TADP,36)
/*   extra send parameter: word error - indicates the nature of the error
 *
 * An Angel internal error has happened.  The error number should be
 * displayed for the user to report to his software supplier.  Once
 * this error has been received the internal state of Angel can no longer
 * be trusted.
 */

#define ADP_Stopped_UserInterruption   ADPSUBREASON(CI_TADP,37)
/* Host requested interruption */

#define ADP_Stopped_ApplicationExit    ADPSUBREASON(CI_TADP,38)
/*   extra send parameter: word exitcode
 * This indicates that the application has exited via exit(), an exitcode
 * of zero indiactes successful termination.
 */

#define ADP_Stopped_StackOverflow      ADPSUBREASON(CI_TADP, 39)
/*
 * Software stack overflow has occurred
 */

#define ADP_Stopped_DivisionByZero     ADPSUBREASON(CI_TADP, 40)
/*
 * Division by zero has occurred
 */

#define ADP_Stopped_OSSpecific         ADPSUBREASON(CI_TADP, 41)
/*
 * The OS has requested that execution stops.  The OS will know
 * why this has happened.
 */



/******************************************************************
 *
 * CI_TTDCC messages (Target-initiated debug comms channel)
 *
 */

#define ADP_TDCC_ToHost             ADPREASON(CI_TTDCC,0)
/* ADP_TDCC_ToHost
 * ------------------
 * Summary: Send Data down Comms Channel in ToHost direction.
 *
 * Arguments:
 * Send:   (word nbytes, words data)
 * Return: (word status)
 *
 * 'nbytes' is number of BYTES to be transferred from the target to the
 *  host via the Debug Comms channel.
 * 'data' is (nbytes/sizeof(word)) WORDS of data to be transferred from
 *  the target to the host via the Debug Comms channel.
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 *
 * NOTE: Equivalent to RDP_CCToHost and RDP_CCToHostReply (just set the
 * direction bit).
 * NOTE II: Current implementations only support single word transfers
 *          (nbytes = 4).
 */


#define ADP_TDCC_FromHost          ADPREASON(CI_TTDCC,1)
/* ADP_TDCC_FromHost
 * --------------------
 * Summary: Send Data down Comms Channel in FromHost direction.
 *
 * Arguments:
 * Send:   ()
 * Return: (word status, word nbytes, words data)
 *
 * 'status' is RDIError_NoError to indicate success, non-zero otherwise.
 * 'nbytes' is number of BYTES to be transferred from the host to the
 *  target via the Debug Comms channel, or zero if the host has no data
 *  to transfer.
 * 'data' is (nbytes/sizeof(word)) WORDS of transferred data.
 *
 * NOTE: Equivalent to RDP_CCFromHost and RDP_CCFromHostReply (just set the
 * direction bit).
 * NOTE II: Current implementations only support single word transfers
 *          (nbytes = 4).
 */


/*******************************************************************
 *
 * Error Codes
 *
 */

#define RDIError_NoError                0

#define RDIError_Reset                  1
#define RDIError_UndefinedInstruction   2
#define RDIError_SoftwareInterrupt      3
#define RDIError_PrefetchAbort          4
#define RDIError_DataAbort              5
#define RDIError_AddressException       6
#define RDIError_IRQ                    7
#define RDIError_FIQ                    8
#define RDIError_Error                  9
#define RDIError_BranchThrough0         10

#define RDIError_NotInitialised         128
#define RDIError_UnableToInitialise     129
#define RDIError_WrongByteSex           130
#define RDIError_UnableToTerminate      131
#define RDIError_BadInstruction         132
#define RDIError_IllegalInstruction     133
#define RDIError_BadCPUStateSetting     134
#define RDIError_UnknownCoPro           135
#define RDIError_UnknownCoProState      136
#define RDIError_BadCoProState          137
#define RDIError_BadPointType           138
#define RDIError_UnimplementedType      139
#define RDIError_BadPointSize           140
#define RDIError_UnimplementedSize      141
#define RDIError_NoMorePoints           142
#define RDIError_BreakpointReached      143
#define RDIError_WatchpointAccessed     144
#define RDIError_NoSuchPoint            145
#define RDIError_ProgramFinishedInStep  146
#define RDIError_UserInterrupt          147
#define RDIError_CantSetPoint           148
#define RDIError_IncompatibleRDILevels  149

#define RDIError_CantLoadConfig         150
#define RDIError_BadConfigData          151
#define RDIError_NoSuchConfig           152
#define RDIError_BufferFull             153
#define RDIError_OutOfStore             154
#define RDIError_NotInDownload          155
#define RDIError_PointInUse             156
#define RDIError_BadImageFormat         157
#define RDIError_TargetRunning          158
#define RDIError_DeviceWouldNotOpen     159
#define RDIError_NoSuchHandle           160
#define RDIError_ConflictingPoint       161

#define RDIError_LittleEndian           240
#define RDIError_BigEndian              241
#define RDIError_SoftInitialiseError    242

#define RDIError_InsufficientPrivilege  253
#define RDIError_UnimplementedMessage   254
#define RDIError_UndefinedMessage       255


#endif

/* EOF adp_h */
