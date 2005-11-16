/* $OpenBSD: mpt_mpilib.h,v 1.6 2005/11/16 04:31:32 marco Exp $ */

/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_type.h,v 1.7 2005/07/10 15:05:39 scottl Exp $ */
/*
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_TYPE.H
 *          Title:  MPI Basic type definitions
 *  Creation Date:  June 6, 2000
 *
 *    MPI Version:  01.02.01
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *  02-20-01  01.01.02  Added define and ifdef for MPI_POINTER.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_TYPE_H
#define MPI_TYPE_H


/*******************************************************************************
 * Define MPI_POINTER if it hasn't already been defined. By default MPI_POINTER
 * is defined to be a near pointer. MPI_POINTER can be defined as a far pointer
 * by defining MPI_POINTER as "far *" before this header file is included.
 */
#ifndef MPI_POINTER
#define MPI_POINTER     *
#endif


/*****************************************************************************
*
*               B a s i c    T y p e s
*
*****************************************************************************/

typedef int8_t   S8;
typedef uint8_t  U8;
typedef int16_t  S16;
typedef uint16_t U16;
typedef int32_t  S32;
typedef uint32_t U32;

typedef struct _S64
{
    U32          Low;
    S32          High;
} S64;

typedef struct _U64
{
    U32          Low;
    U32          High;
} U64;


/****************************************************************************/
/*  Pointers                                                                */
/****************************************************************************/

typedef S8      *PS8;
typedef U8      *PU8;
typedef S16     *PS16;
typedef U16     *PU16;
typedef S32     *PS32;
typedef U32     *PU32;
typedef S64     *PS64;
typedef U64     *PU64;


#endif

/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/fc_log.h,v 1.5 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *  NAME:           fc_log.h
 *  SUMMARY:        MPI IocLogInfo definitions for the SYMFC9xx chips
 *  DESCRIPTION:    Contains the enumerated list of values that may be returned
 *                  in the IOCLogInfo field of a MPI Default Reply Message.
 *
 *  CREATION DATE:  6/02/2000
 *  ID:             $Id: mpt_mpilib.h,v 1.6 2005/11/16 04:31:32 marco Exp $
 */

/*
 * MpiIocLogInfo_t enum
 *
 * These 32 bit values are used in the IOCLogInfo field of the MPI reply
 * messages.
 * The value is 0xabcccccc where
 *          a = The type of log info as per the MPI spec. Since these codes are
 *              all for Fibre Channel this value will always be 2.
 *          b = Specifies a subclass of the firmware where
 *                  0 = FCP Initiator
 *                  1 = FCP Target
 *                  2 = LAN
 *                  3 = MPI Message Layer
 *                  4 = FC Link
 *                  5 = Context Manager
 *                  6 = Invalid Field Offset
 *                  7 = State Change Info
 *                  all others are reserved for future use
 *          c = A specific value within the subclass.
 *
 * NOTE: Any new values should be added to the end of each subclass so that the
 *       codes remain consistent across firmware releases.
 */
typedef enum _MpiIocLogInfoFc
{
    MPI_IOCLOGINFO_FC_INIT_BASE                     = 0x20000000,
    MPI_IOCLOGINFO_FC_INIT_ERROR_OUT_OF_ORDER_FRAME = 0x20000001, /* received an out of order frame - unsupported */
    MPI_IOCLOGINFO_FC_INIT_ERROR_BAD_START_OF_FRAME = 0x20000002, /* Bad Rx Frame, bad start of frame primative */
    MPI_IOCLOGINFO_FC_INIT_ERROR_BAD_END_OF_FRAME   = 0x20000003, /* Bad Rx Frame, bad end of frame primative */
    MPI_IOCLOGINFO_FC_INIT_ERROR_OVER_RUN           = 0x20000004, /* Bad Rx Frame, overrun */
    MPI_IOCLOGINFO_FC_INIT_ERROR_RX_OTHER           = 0x20000005, /* Other errors caught by IOC which require retries */
    MPI_IOCLOGINFO_FC_INIT_ERROR_SUBPROC_DEAD       = 0x20000006, /* Main processor could not initialize sub-processor */
    MPI_IOCLOGINFO_FC_INIT_ERROR_RX_OVERRUN         = 0x20000007, /* Scatter Gather overrun  */
    MPI_IOCLOGINFO_FC_INIT_ERROR_RX_BAD_STATUS      = 0x20000008, /* Receiver detected context mismatch via invalid header */
    MPI_IOCLOGINFO_FC_INIT_ERROR_RX_UNEXPECTED_FRAME= 0x20000009, /* CtxMgr detected unsupported frame type  */
    MPI_IOCLOGINFO_FC_INIT_ERROR_LINK_FAILURE       = 0x2000000A, /* Link failure occurred  */
    MPI_IOCLOGINFO_FC_INIT_ERROR_TX_TIMEOUT         = 0x2000000B, /* Transmitter timeout error */

    MPI_IOCLOGINFO_FC_TARGET_BASE                   = 0x21000000,
    MPI_IOCLOGINFO_FC_TARGET_NO_PDISC               = 0x21000001, /* not sent because we are waiting for a PDISC from the initiator */
    MPI_IOCLOGINFO_FC_TARGET_NO_LOGIN               = 0x21000002, /* not sent because we are not logged in to the remote node */
    MPI_IOCLOGINFO_FC_TARGET_DOAR_KILLED_BY_LIP     = 0x21000003, /* Data Out, Auto Response, not sent due to a LIP */
    MPI_IOCLOGINFO_FC_TARGET_DIAR_KILLED_BY_LIP     = 0x21000004, /* Data In, Auto Response, not sent due to a LIP */
    MPI_IOCLOGINFO_FC_TARGET_DIAR_MISSING_DATA      = 0x21000005, /* Data In, Auto Response, missing data frames */
    MPI_IOCLOGINFO_FC_TARGET_DONR_KILLED_BY_LIP     = 0x21000006, /* Data Out, No Response, not sent due to a LIP */
    MPI_IOCLOGINFO_FC_TARGET_WRSP_KILLED_BY_LIP     = 0x21000007, /* Auto-response after a write not sent due to a LIP */
    MPI_IOCLOGINFO_FC_TARGET_DINR_KILLED_BY_LIP     = 0x21000008, /* Data In, No Response, not completed due to a LIP */
    MPI_IOCLOGINFO_FC_TARGET_DINR_MISSING_DATA      = 0x21000009, /* Data In, No Response, missing data frames */
    MPI_IOCLOGINFO_FC_TARGET_MRSP_KILLED_BY_LIP     = 0x2100000a, /* Manual Response not sent due to a LIP */
    MPI_IOCLOGINFO_FC_TARGET_NO_CLASS_3             = 0x2100000b, /* not sent because remote node does not support Class 3 */
    MPI_IOCLOGINFO_FC_TARGET_LOGIN_NOT_VALID        = 0x2100000c, /* not sent because login to remote node not validated */
    MPI_IOCLOGINFO_FC_TARGET_FROM_OUTBOUND          = 0x2100000e, /* cleared from the outbound queue after a logout */
    MPI_IOCLOGINFO_FC_TARGET_WAITING_FOR_DATA_IN    = 0x2100000f, /* cleared waiting for data after a logout */

    MPI_IOCLOGINFO_FC_LAN_BASE                      = 0x22000000,
    MPI_IOCLOGINFO_FC_LAN_TRANS_SGL_MISSING         = 0x22000001, /* Transaction Context Sgl Missing */
    MPI_IOCLOGINFO_FC_LAN_TRANS_WRONG_PLACE         = 0x22000002, /* Transaction Context found before an EOB */
    MPI_IOCLOGINFO_FC_LAN_TRANS_RES_BITS_SET        = 0x22000003, /* Transaction Context value has reserved bits set */
    MPI_IOCLOGINFO_FC_LAN_WRONG_SGL_FLAG            = 0x22000004, /* Invalid SGL Flags */

    MPI_IOCLOGINFO_FC_MSG_BASE                      = 0x23000000,

    MPI_IOCLOGINFO_FC_LINK_BASE                     = 0x24000000,
    MPI_IOCLOGINFO_FC_LINK_LOOP_INIT_TIMEOUT        = 0x24000001, /* Loop initialization timed out */
    MPI_IOCLOGINFO_FC_LINK_ALREADY_INITIALIZED      = 0x24000002, /* Another system controller already initialized the loop */
    MPI_IOCLOGINFO_FC_LINK_LINK_NOT_ESTABLISHED     = 0x24000003, /* Not synchronized to signal or still negotiating (possible cable problem) */
    MPI_IOCLOGINFO_FC_LINK_CRC_ERROR                = 0x24000004, /* CRC check detected error on received frame */

    MPI_IOCLOGINFO_FC_CTX_BASE                      = 0x25000000,

    MPI_IOCLOGINFO_FC_INVALID_FIELD_BYTE_OFFSET     = 0x26000000, /* The lower 24 bits give the byte offset of the field in the request message that is invalid */
    MPI_IOCLOGINFO_FC_INVALID_FIELD_MAX_OFFSET      = 0x26ffffff,

    MPI_IOCLOGINFO_FC_STATE_CHANGE                  = 0x27000000  /* The lower 24 bits give additional information concerning state change */

} MpiIocLogInfoFc_t;
/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi.h,v 1.6 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI.H
 *          Title:  MPI Message independent structures and definitions
 *  Creation Date:  July 27, 2000
 *
 *    MPI.H Version:  01.02.11
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  05-24-00  00.10.02  Added MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH definition.
 *  06-06-00  01.00.01  Update MPI_VERSION_MAJOR and MPI_VERSION_MINOR.
 *  06-22-00  01.00.02  Added MPI_IOCSTATUS_LAN_ definitions.
 *                      Removed LAN_SUSPEND function definition.
 *                      Added MPI_MSGFLAGS_CONTINUATION_REPLY definition.
 *  06-30-00  01.00.03  Added MPI_CONTEXT_REPLY_TYPE_LAN definition.
 *                      Added MPI_GET/SET_CONTEXT_REPLY_TYPE macros.
 *  07-27-00  01.00.04  Added MPI_FAULT_ definitions.
 *                      Removed MPI_IOCSTATUS_MSG/DATA_XFER_ERROR definitions.
 *                      Added MPI_IOCSTATUS_INTERNAL_ERROR definition.
 *                      Added MPI_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH.
 *  11-02-00  01.01.01  Original release for post 1.0 work.
 *  12-04-00  01.01.02  Added new function codes.
 *  01-09-01  01.01.03  Added more definitions to the system interface section
 *                      Added MPI_IOCSTATUS_TARGET_STS_DATA_NOT_SENT.
 *  01-25-01  01.01.04  Changed MPI_VERSION_MINOR from 0x00 to 0x01.
 *  02-20-01  01.01.05  Started using MPI_POINTER.
 *                      Fixed value for MPI_DIAG_RW_ENABLE.
 *                      Added defines for MPI_DIAG_PREVENT_IOC_BOOT and
 *                      MPI_DIAG_CLEAR_FLASH_BAD_SIG.
 *                      Obsoleted MPI_IOCSTATUS_TARGET_FC_ defines.
 *  02-27-01  01.01.06  Removed MPI_HOST_INDEX_REGISTER define.
 *                      Added function codes for RAID.
 *  04-09-01  01.01.07  Added alternate define for MPI_DOORBELL_ACTIVE,
 *                      MPI_DOORBELL_USED, to better match the spec.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *                      Changed MPI_VERSION_MINOR from 0x01 to 0x02.
 *                      Added define MPI_FUNCTION_TOOLBOX.
 *  09-28-01  01.02.02  New function code MPI_SCSI_ENCLOSURE_PROCESSOR.
 *  11-01-01  01.02.03  Changed name to MPI_FUNCTION_SCSI_ENCLOSURE_PROCESSOR.
 *  03-14-02  01.02.04  Added MPI_HEADER_VERSION_ defines.
 *  05-31-02  01.02.05  Bumped MPI_HEADER_VERSION_UNIT.
 *  07-12-02  01.02.06  Added define for MPI_FUNCTION_MAILBOX.
 *  09-16-02  01.02.07  Bumped value for MPI_HEADER_VERSION_UNIT.
 *  11-15-02  01.02.08  Added define MPI_IOCSTATUS_TARGET_INVALID_IO_INDEX and
 *                      obsoleted define MPI_IOCSTATUS_TARGET_INVALID_IOCINDEX.
 *  04-01-03  01.02.09  New IOCStatus code: MPI_IOCSTATUS_FC_EXCHANGE_CANCELED
 *  06-26-03  01.02.10  Bumped MPI_HEADER_VERSION_UNIT value.
 *  01-16-04  01.02.11  Added define for MPI_IOCLOGINFO_TYPE_SHIFT.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_H
#define MPI_H


/*****************************************************************************
*
*        M P I    V e r s i o n    D e f i n i t i o n s
*
*****************************************************************************/

#define MPI_VERSION_MAJOR                   (0x01)
#define MPI_VERSION_MINOR                   (0x02)
#define MPI_VERSION_MAJOR_MASK              (0xFF00)
#define MPI_VERSION_MAJOR_SHIFT             (8)
#define MPI_VERSION_MINOR_MASK              (0x00FF)
#define MPI_VERSION_MINOR_SHIFT             (0)
#define MPI_VERSION ((MPI_VERSION_MAJOR << MPI_VERSION_MAJOR_SHIFT) |   \
                                      MPI_VERSION_MINOR)

#define MPI_VERSION_01_00                   (0x0100)
#define MPI_VERSION_01_01                   (0x0101)
#define MPI_VERSION_01_02                   (0x0102)
/* Note: The major versions of 0xe0 through 0xff are reserved */

/* versioning for this MPI header set */
#define MPI_HEADER_VERSION_UNIT             (0x0D)
#define MPI_HEADER_VERSION_DEV              (0x00)
#define MPI_HEADER_VERSION_UNIT_MASK        (0xFF00)
#define MPI_HEADER_VERSION_UNIT_SHIFT       (8)
#define MPI_HEADER_VERSION_DEV_MASK         (0x00FF)
#define MPI_HEADER_VERSION_DEV_SHIFT        (0)
#define MPI_HEADER_VERSION ((MPI_HEADER_VERSION_UNIT << 8) | MPI_HEADER_VERSION_DEV)

/*****************************************************************************
*
*        I O C    S t a t e    D e f i n i t i o n s
*
*****************************************************************************/

#define MPI_IOC_STATE_RESET                 (0x00000000)
#define MPI_IOC_STATE_READY                 (0x10000000)
#define MPI_IOC_STATE_OPERATIONAL           (0x20000000)
#define MPI_IOC_STATE_FAULT                 (0x40000000)

#define MPI_IOC_STATE_MASK                  (0xF0000000)
#define MPI_IOC_STATE_SHIFT                 (28)

/* Fault state codes (product independent range 0x8000-0xFFFF) */

#define MPI_FAULT_REQUEST_MESSAGE_PCI_PARITY_ERROR  (0x8111)
#define MPI_FAULT_REQUEST_MESSAGE_PCI_BUS_FAULT     (0x8112)
#define MPI_FAULT_REPLY_MESSAGE_PCI_PARITY_ERROR    (0x8113)
#define MPI_FAULT_REPLY_MESSAGE_PCI_BUS_FAULT       (0x8114)
#define MPI_FAULT_DATA_SEND_PCI_PARITY_ERROR        (0x8115)
#define MPI_FAULT_DATA_SEND_PCI_BUS_FAULT           (0x8116)
#define MPI_FAULT_DATA_RECEIVE_PCI_PARITY_ERROR     (0x8117)
#define MPI_FAULT_DATA_RECEIVE_PCI_BUS_FAULT        (0x8118)


/*****************************************************************************
*
*        P C I    S y s t e m    I n t e r f a c e    R e g i s t e r s
*
*****************************************************************************/

/* S y s t e m    D o o r b e l l */
#define MPI_DOORBELL_OFFSET                 (0x00000000)
#define MPI_DOORBELL_ACTIVE                 (0x08000000) /* DoorbellUsed */
#define MPI_DOORBELL_USED                   (MPI_DOORBELL_ACTIVE)
#define MPI_DOORBELL_ACTIVE_SHIFT           (27)
#define MPI_DOORBELL_WHO_INIT_MASK          (0x07000000)
#define MPI_DOORBELL_WHO_INIT_SHIFT         (24)
#define MPI_DOORBELL_FUNCTION_MASK          (0xFF000000)
#define MPI_DOORBELL_FUNCTION_SHIFT         (24)
#define MPI_DOORBELL_ADD_DWORDS_MASK        (0x00FF0000)
#define MPI_DOORBELL_ADD_DWORDS_SHIFT       (16)
#define MPI_DOORBELL_DATA_MASK              (0x0000FFFF)


#define MPI_WRITE_SEQUENCE_OFFSET           (0x00000004)
#define MPI_WRSEQ_KEY_VALUE_MASK            (0x0000000F)
#define MPI_WRSEQ_1ST_KEY_VALUE             (0x04)
#define MPI_WRSEQ_2ND_KEY_VALUE             (0x0B)
#define MPI_WRSEQ_3RD_KEY_VALUE             (0x02)
#define MPI_WRSEQ_4TH_KEY_VALUE             (0x07)
#define MPI_WRSEQ_5TH_KEY_VALUE             (0x0D)

#define MPI_DIAGNOSTIC_OFFSET               (0x00000008)
#define MPI_DIAG_CLEAR_FLASH_BAD_SIG        (0x00000400)
#define MPI_DIAG_PREVENT_IOC_BOOT           (0x00000200)
#define MPI_DIAG_DRWE                       (0x00000080)
#define MPI_DIAG_FLASH_BAD_SIG              (0x00000040)
#define MPI_DIAG_RESET_HISTORY              (0x00000020)
#define MPI_DIAG_RW_ENABLE                  (0x00000010)
#define MPI_DIAG_RESET_ADAPTER              (0x00000004)
#define MPI_DIAG_DISABLE_ARM                (0x00000002)
#define MPI_DIAG_MEM_ENABLE                 (0x00000001)

#define MPI_TEST_BASE_ADDRESS_OFFSET        (0x0000000C)

#define MPI_DIAG_RW_DATA_OFFSET             (0x00000010)

#define MPI_DIAG_RW_ADDRESS_OFFSET          (0x00000014)

#define MPI_HOST_INTERRUPT_STATUS_OFFSET    (0x00000030)
#define MPI_HIS_IOP_DOORBELL_STATUS         (0x80000000)
#define MPI_HIS_REPLY_MESSAGE_INTERRUPT     (0x00000008)
#define MPI_HIS_DOORBELL_INTERRUPT          (0x00000001)

#define MPI_HOST_INTERRUPT_MASK_OFFSET      (0x00000034)
#define MPI_HIM_RIM                         (0x00000008)
#define MPI_HIM_DIM                         (0x00000001)

#define MPI_REQUEST_QUEUE_OFFSET            (0x00000040)
#define MPI_REQUEST_POST_FIFO_OFFSET        (0x00000040)

#define MPI_REPLY_QUEUE_OFFSET              (0x00000044)
#define MPI_REPLY_POST_FIFO_OFFSET          (0x00000044)
#define MPI_REPLY_FREE_FIFO_OFFSET          (0x00000044)



/*****************************************************************************
*
*        M e s s a g e    F r a m e    D e s c r i p t o r s
*
*****************************************************************************/

#define MPI_REQ_MF_DESCRIPTOR_NB_MASK       (0x00000003)
#define MPI_REQ_MF_DESCRIPTOR_F_BIT         (0x00000004)
#define MPI_REQ_MF_DESCRIPTOR_ADDRESS_MASK  (0xFFFFFFF8)

#define MPI_ADDRESS_REPLY_A_BIT             (0x80000000)
#define MPI_ADDRESS_REPLY_ADDRESS_MASK      (0x7FFFFFFF)

#define MPI_CONTEXT_REPLY_A_BIT             (0x80000000)
#define MPI_CONTEXT_REPLY_TYPE_MASK         (0x60000000)
#define MPI_CONTEXT_REPLY_TYPE_SCSI_INIT    (0x00)
#define MPI_CONTEXT_REPLY_TYPE_SCSI_TARGET  (0x01)
#define MPI_CONTEXT_REPLY_TYPE_LAN          (0x02)
#define MPI_CONTEXT_REPLY_TYPE_SHIFT        (29)
#define MPI_CONTEXT_REPLY_CONTEXT_MASK      (0x1FFFFFFF)


/****************************************************************************/
/* Context Reply macros                                                     */
/****************************************************************************/

#define MPI_GET_CONTEXT_REPLY_TYPE(x)  (((x) & MPI_CONTEXT_REPLY_TYPE_MASK) \
                                          >> MPI_CONTEXT_REPLY_TYPE_SHIFT)

#define MPI_SET_CONTEXT_REPLY_TYPE(x, typ)                                  \
            ((x) = ((x) & ~MPI_CONTEXT_REPLY_TYPE_MASK) |                   \
                            (((typ) << MPI_CONTEXT_REPLY_TYPE_SHIFT) &      \
                                        MPI_CONTEXT_REPLY_TYPE_MASK))


/*****************************************************************************
*
*        M e s s a g e    F u n c t i o n s
*              0x80 -> 0x8F reserved for private message use per product
*
*
*****************************************************************************/

#define MPI_FUNCTION_SCSI_IO_REQUEST                (0x00)
#define MPI_FUNCTION_SCSI_TASK_MGMT                 (0x01)
#define MPI_FUNCTION_IOC_INIT                       (0x02)
#define MPI_FUNCTION_IOC_FACTS                      (0x03)
#define MPI_FUNCTION_CONFIG                         (0x04)
#define MPI_FUNCTION_PORT_FACTS                     (0x05)
#define MPI_FUNCTION_PORT_ENABLE                    (0x06)
#define MPI_FUNCTION_EVENT_NOTIFICATION             (0x07)
#define MPI_FUNCTION_EVENT_ACK                      (0x08)
#define MPI_FUNCTION_FW_DOWNLOAD                    (0x09)
#define MPI_FUNCTION_TARGET_CMD_BUFFER_POST         (0x0A)
#define MPI_FUNCTION_TARGET_ASSIST                  (0x0B)
#define MPI_FUNCTION_TARGET_STATUS_SEND             (0x0C)
#define MPI_FUNCTION_TARGET_MODE_ABORT              (0x0D)
#define MPI_FUNCTION_TARGET_FC_BUF_POST_LINK_SRVC   (0x0E) /* obsolete name */
#define MPI_FUNCTION_TARGET_FC_RSP_LINK_SRVC        (0x0F) /* obsolete name */
#define MPI_FUNCTION_TARGET_FC_EX_SEND_LINK_SRVC    (0x10) /* obsolete name */
#define MPI_FUNCTION_TARGET_FC_ABORT                (0x11) /* obsolete name */
#define MPI_FUNCTION_FC_LINK_SRVC_BUF_POST          (0x0E)
#define MPI_FUNCTION_FC_LINK_SRVC_RSP               (0x0F)
#define MPI_FUNCTION_FC_EX_LINK_SRVC_SEND           (0x10)
#define MPI_FUNCTION_FC_ABORT                       (0x11)
#define MPI_FUNCTION_FW_UPLOAD                      (0x12)
#define MPI_FUNCTION_FC_COMMON_TRANSPORT_SEND       (0x13)
#define MPI_FUNCTION_FC_PRIMITIVE_SEND              (0x14)

#define MPI_FUNCTION_RAID_ACTION                    (0x15)
#define MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH       (0x16)

#define MPI_FUNCTION_TOOLBOX                        (0x17)

#define MPI_FUNCTION_SCSI_ENCLOSURE_PROCESSOR       (0x18)

#define MPI_FUNCTION_MAILBOX                        (0x19)

#define MPI_FUNCTION_LAN_SEND                       (0x20)
#define MPI_FUNCTION_LAN_RECEIVE                    (0x21)
#define MPI_FUNCTION_LAN_RESET                      (0x22)

#define MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET         (0x40)
#define MPI_FUNCTION_IO_UNIT_RESET                  (0x41)
#define MPI_FUNCTION_HANDSHAKE                      (0x42)
#define MPI_FUNCTION_REPLY_FRAME_REMOVAL            (0x43)



/*****************************************************************************
*
*        S c a t t e r    G a t h e r    E l e m e n t s
*
*****************************************************************************/

/****************************************************************************/
/*  Simple element structures                                               */
/****************************************************************************/

typedef struct _SGE_SIMPLE32
{
    U32                     FlagsLength;
    U32                     Address;
} SGE_SIMPLE32, MPI_POINTER PTR_SGE_SIMPLE32,
  SGESimple32_t, MPI_POINTER pSGESimple32_t;

typedef struct _SGE_SIMPLE64
{
    U32                     FlagsLength;
    U64                     Address;
} SGE_SIMPLE64, MPI_POINTER PTR_SGE_SIMPLE64,
  SGESimple64_t, MPI_POINTER pSGESimple64_t;

typedef struct _SGE_SIMPLE_UNION
{
    U32                     FlagsLength;
    union
    {
        U32                 Address32;
        U64                 Address64;
    }u;
} SGESimpleUnion_t, MPI_POINTER pSGESimpleUnion_t,
  SGE_SIMPLE_UNION, MPI_POINTER PTR_SGE_SIMPLE_UNION;

/****************************************************************************/
/*  Chain element structures                                                */
/****************************************************************************/

typedef struct _SGE_CHAIN32
{
    U16                     Length;
    U8                      NextChainOffset;
    U8                      Flags;
    U32                     Address;
} SGE_CHAIN32, MPI_POINTER PTR_SGE_CHAIN32,
  SGEChain32_t, MPI_POINTER pSGEChain32_t;

typedef struct _SGE_CHAIN64
{
    U16                     Length;
    U8                      NextChainOffset;
    U8                      Flags;
    U64                     Address;
} SGE_CHAIN64, MPI_POINTER PTR_SGE_CHAIN64,
  SGEChain64_t, MPI_POINTER pSGEChain64_t;

typedef struct _SGE_CHAIN_UNION
{
    U16                     Length;
    U8                      NextChainOffset;
    U8                      Flags;
    union
    {
        U32                 Address32;
        U64                 Address64;
    }u;
} SGE_CHAIN_UNION, MPI_POINTER PTR_SGE_CHAIN_UNION,
  SGEChainUnion_t, MPI_POINTER pSGEChainUnion_t;

/****************************************************************************/
/*  Transaction Context element                                             */
/****************************************************************************/

typedef struct _SGE_TRANSACTION32
{
    U8                      Reserved;
    U8                      ContextSize;
    U8                      DetailsLength;
    U8                      Flags;
    U32                     TransactionContext[1];
    U32                     TransactionDetails[1];
} SGE_TRANSACTION32, MPI_POINTER PTR_SGE_TRANSACTION32,
  SGETransaction32_t, MPI_POINTER pSGETransaction32_t;

typedef struct _SGE_TRANSACTION64
{
    U8                      Reserved;
    U8                      ContextSize;
    U8                      DetailsLength;
    U8                      Flags;
    U32                     TransactionContext[2];
    U32                     TransactionDetails[1];
} SGE_TRANSACTION64, MPI_POINTER PTR_SGE_TRANSACTION64,
  SGETransaction64_t, MPI_POINTER pSGETransaction64_t;

typedef struct _SGE_TRANSACTION96
{
    U8                      Reserved;
    U8                      ContextSize;
    U8                      DetailsLength;
    U8                      Flags;
    U32                     TransactionContext[3];
    U32                     TransactionDetails[1];
} SGE_TRANSACTION96, MPI_POINTER PTR_SGE_TRANSACTION96,
  SGETransaction96_t, MPI_POINTER pSGETransaction96_t;

typedef struct _SGE_TRANSACTION128
{
    U8                      Reserved;
    U8                      ContextSize;
    U8                      DetailsLength;
    U8                      Flags;
    U32                     TransactionContext[4];
    U32                     TransactionDetails[1];
} SGE_TRANSACTION128, MPI_POINTER PTR_SGE_TRANSACTION128,
  SGETransaction_t128, MPI_POINTER pSGETransaction_t128;

typedef struct _SGE_TRANSACTION_UNION
{
    U8                      Reserved;
    U8                      ContextSize;
    U8                      DetailsLength;
    U8                      Flags;
    union
    {
        U32                 TransactionContext32[1];
        U32                 TransactionContext64[2];
        U32                 TransactionContext96[3];
        U32                 TransactionContext128[4];
    }u;
    U32                     TransactionDetails[1];
} SGE_TRANSACTION_UNION, MPI_POINTER PTR_SGE_TRANSACTION_UNION,
  SGETransactionUnion_t, MPI_POINTER pSGETransactionUnion_t;


/****************************************************************************/
/*  SGE IO types union  for IO SGL's                                        */
/****************************************************************************/

typedef struct _SGE_IO_UNION
{
    union
    {
        SGE_SIMPLE_UNION    Simple;
        SGE_CHAIN_UNION     Chain;
    } u;
} SGE_IO_UNION, MPI_POINTER PTR_SGE_IO_UNION,
  SGEIOUnion_t, MPI_POINTER pSGEIOUnion_t;

/****************************************************************************/
/*  SGE union for SGL's with Simple and Transaction elements                */
/****************************************************************************/

typedef struct _SGE_TRANS_SIMPLE_UNION
{
    union
    {
        SGE_SIMPLE_UNION        Simple;
        SGE_TRANSACTION_UNION   Transaction;
    } u;
} SGE_TRANS_SIMPLE_UNION, MPI_POINTER PTR_SGE_TRANS_SIMPLE_UNION,
  SGETransSimpleUnion_t, MPI_POINTER pSGETransSimpleUnion_t;

/****************************************************************************/
/*  All SGE types union                                                     */
/****************************************************************************/

typedef struct _SGE_MPI_UNION
{
    union
    {
        SGE_SIMPLE_UNION        Simple;
        SGE_CHAIN_UNION         Chain;
        SGE_TRANSACTION_UNION   Transaction;
    } u;
} SGE_MPI_UNION, MPI_POINTER PTR_SGE_MPI_UNION,
  MPI_SGE_UNION_t, MPI_POINTER pMPI_SGE_UNION_t,
  SGEAllUnion_t, MPI_POINTER pSGEAllUnion_t;


/****************************************************************************/
/*  SGE field definition and masks                                          */
/****************************************************************************/

/* Flags field bit definitions */

#define MPI_SGE_FLAGS_LAST_ELEMENT              (0x80)
#define MPI_SGE_FLAGS_END_OF_BUFFER             (0x40)
#define MPI_SGE_FLAGS_ELEMENT_TYPE_MASK         (0x30)
#define MPI_SGE_FLAGS_LOCAL_ADDRESS             (0x08)
#define MPI_SGE_FLAGS_DIRECTION                 (0x04)
#define MPI_SGE_FLAGS_ADDRESS_SIZE              (0x02)
#define MPI_SGE_FLAGS_END_OF_LIST               (0x01)

#define MPI_SGE_FLAGS_SHIFT                     (24)

#define MPI_SGE_LENGTH_MASK                     (0x00FFFFFF)
#define MPI_SGE_CHAIN_LENGTH_MASK               (0x0000FFFF)

/* Element Type */

#define MPI_SGE_FLAGS_TRANSACTION_ELEMENT       (0x00)
#define MPI_SGE_FLAGS_SIMPLE_ELEMENT            (0x10)
#define MPI_SGE_FLAGS_CHAIN_ELEMENT             (0x30)
#define MPI_SGE_FLAGS_ELEMENT_MASK              (0x30)

/* Address location */

#define MPI_SGE_FLAGS_SYSTEM_ADDRESS            (0x00)

/* Direction */

#define MPI_SGE_FLAGS_IOC_TO_HOST               (0x00)
#define MPI_SGE_FLAGS_HOST_TO_IOC               (0x04)

/* Address Size */

#define MPI_SGE_FLAGS_32_BIT_ADDRESSING         (0x00)
#define MPI_SGE_FLAGS_64_BIT_ADDRESSING         (0x02)

/* Context Size */

#define MPI_SGE_FLAGS_32_BIT_CONTEXT            (0x00)
#define MPI_SGE_FLAGS_64_BIT_CONTEXT            (0x02)
#define MPI_SGE_FLAGS_96_BIT_CONTEXT            (0x04)
#define MPI_SGE_FLAGS_128_BIT_CONTEXT           (0x06)

#define MPI_SGE_CHAIN_OFFSET_MASK               (0x00FF0000)
#define MPI_SGE_CHAIN_OFFSET_SHIFT              (16)


/****************************************************************************/
/*  SGE operation Macros                                                    */
/****************************************************************************/

         /* SIMPLE FlagsLength manipulations... */
#define  MPI_SGE_SET_FLAGS(f)           ((U32)(f) << MPI_SGE_FLAGS_SHIFT)
#define  MPI_SGE_GET_FLAGS(fl)          (((fl) & ~MPI_SGE_LENGTH_MASK) >> MPI_SGE_FLAGS_SHIFT)
#define  MPI_SGE_LENGTH(fl)             ((fl) & MPI_SGE_LENGTH_MASK)
#define  MPI_SGE_CHAIN_LENGTH(fl)       ((fl) & MPI_SGE_CHAIN_LENGTH_MASK)

#define  MPI_SGE_SET_FLAGS_LENGTH(f,l)  (MPI_SGE_SET_FLAGS(f) | MPI_SGE_LENGTH(l))

#define  MPI_pSGE_GET_FLAGS(psg)        MPI_SGE_GET_FLAGS((psg)->FlagsLength)
#define  MPI_pSGE_GET_LENGTH(psg)       MPI_SGE_LENGTH((psg)->FlagsLength)
#define  MPI_pSGE_SET_FLAGS_LENGTH(psg,f,l)  (psg)->FlagsLength = MPI_SGE_SET_FLAGS_LENGTH(f,l)
         /* CAUTION - The following are READ-MODIFY-WRITE! */
#define  MPI_pSGE_SET_FLAGS(psg,f)      (psg)->FlagsLength |= MPI_SGE_SET_FLAGS(f)
#define  MPI_pSGE_SET_LENGTH(psg,l)     (psg)->FlagsLength |= MPI_SGE_LENGTH(l)

#define  MPI_GET_CHAIN_OFFSET(x) ((x&MPI_SGE_CHAIN_OFFSET_MASK)>>MPI_SGE_CHAIN_OFFSET_SHIFT)



/*****************************************************************************
*
*        S t a n d a r d    M e s s a g e    S t r u c t u r e s
*
*****************************************************************************/

/****************************************************************************/
/* Standard message request header for all request messages                 */
/****************************************************************************/

typedef struct _MSG_REQUEST_HEADER
{
    U8                      Reserved[2];      /* function specific */
    U8                      ChainOffset;
    U8                      Function;
    U8                      Reserved1[3];     /* function specific */
    U8                      MsgFlags;
    U32                     MsgContext;
} MSG_REQUEST_HEADER, MPI_POINTER PTR_MSG_REQUEST_HEADER,
  MPIHeader_t, MPI_POINTER pMPIHeader_t;


/****************************************************************************/
/*  Default Reply                                                           */
/****************************************************************************/

typedef struct _MSG_DEFAULT_REPLY
{
    U8                      Reserved[2];      /* function specific */
    U8                      MsgLength;
    U8                      Function;
    U8                      Reserved1[3];     /* function specific */
    U8                      MsgFlags;
    U32                     MsgContext;
    U8                      Reserved2[2];     /* function specific */
    U16                     IOCStatus;
    U32                     IOCLogInfo;
} MSG_DEFAULT_REPLY, MPI_POINTER PTR_MSG_DEFAULT_REPLY,
  MPIDefaultReply_t, MPI_POINTER pMPIDefaultReply_t;


/* MsgFlags definition for all replies */

#define MPI_MSGFLAGS_CONTINUATION_REPLY         (0x80)


/*****************************************************************************
*
*               I O C    S t a t u s   V a l u e s
*
*****************************************************************************/

/****************************************************************************/
/*  Common IOCStatus values for all replies                                 */
/****************************************************************************/

#define MPI_IOCSTATUS_SUCCESS                  (0x0000)
#define MPI_IOCSTATUS_INVALID_FUNCTION         (0x0001)
#define MPI_IOCSTATUS_BUSY                     (0x0002)
#define MPI_IOCSTATUS_INVALID_SGL              (0x0003)
#define MPI_IOCSTATUS_INTERNAL_ERROR           (0x0004)
#define MPI_IOCSTATUS_RESERVED                 (0x0005)
#define MPI_IOCSTATUS_INSUFFICIENT_RESOURCES   (0x0006)
#define MPI_IOCSTATUS_INVALID_FIELD            (0x0007)
#define MPI_IOCSTATUS_INVALID_STATE            (0x0008)

/****************************************************************************/
/*  Config IOCStatus values                                                 */
/****************************************************************************/

#define MPI_IOCSTATUS_CONFIG_INVALID_ACTION    (0x0020)
#define MPI_IOCSTATUS_CONFIG_INVALID_TYPE      (0x0021)
#define MPI_IOCSTATUS_CONFIG_INVALID_PAGE      (0x0022)
#define MPI_IOCSTATUS_CONFIG_INVALID_DATA      (0x0023)
#define MPI_IOCSTATUS_CONFIG_NO_DEFAULTS       (0x0024)
#define MPI_IOCSTATUS_CONFIG_CANT_COMMIT       (0x0025)

/****************************************************************************/
/*  SCSIIO Reply (SPI & FCP) initiator values                               */
/****************************************************************************/

#define MPI_IOCSTATUS_SCSI_RECOVERED_ERROR     (0x0040)
#define MPI_IOCSTATUS_SCSI_INVALID_BUS         (0x0041)
#define MPI_IOCSTATUS_SCSI_INVALID_TARGETID    (0x0042)
#define MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE    (0x0043)
#define MPI_IOCSTATUS_SCSI_DATA_OVERRUN        (0x0044)
#define MPI_IOCSTATUS_SCSI_DATA_UNDERRUN       (0x0045)
#define MPI_IOCSTATUS_SCSI_IO_DATA_ERROR       (0x0046)
#define MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR      (0x0047)
#define MPI_IOCSTATUS_SCSI_TASK_TERMINATED     (0x0048)
#define MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH   (0x0049)
#define MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED    (0x004A)
#define MPI_IOCSTATUS_SCSI_IOC_TERMINATED      (0x004B)
#define MPI_IOCSTATUS_SCSI_EXT_TERMINATED      (0x004C)

/****************************************************************************/
/*  SCSI (SPI & FCP) target values                                          */
/****************************************************************************/

#define MPI_IOCSTATUS_TARGET_PRIORITY_IO         (0x0060)
#define MPI_IOCSTATUS_TARGET_INVALID_PORT        (0x0061)
#define MPI_IOCSTATUS_TARGET_INVALID_IOCINDEX    (0x0062)   /* obsolete */
#define MPI_IOCSTATUS_TARGET_INVALID_IO_INDEX    (0x0062)
#define MPI_IOCSTATUS_TARGET_ABORTED             (0x0063)
#define MPI_IOCSTATUS_TARGET_NO_CONN_RETRYABLE   (0x0064)
#define MPI_IOCSTATUS_TARGET_NO_CONNECTION       (0x0065)
#define MPI_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH (0x006A)
#define MPI_IOCSTATUS_TARGET_STS_DATA_NOT_SENT   (0x006B)

/****************************************************************************/
/*  Additional FCP target values                                            */
/****************************************************************************/

#define MPI_IOCSTATUS_TARGET_FC_ABORTED         (0x0066)    /* obsolete */
#define MPI_IOCSTATUS_TARGET_FC_RX_ID_INVALID   (0x0067)    /* obsolete */
#define MPI_IOCSTATUS_TARGET_FC_DID_INVALID     (0x0068)    /* obsolete */
#define MPI_IOCSTATUS_TARGET_FC_NODE_LOGGED_OUT (0x0069)    /* obsolete */

/****************************************************************************/
/*  Fibre Channel Direct Access values                                      */
/****************************************************************************/

#define MPI_IOCSTATUS_FC_ABORTED                (0x0066)
#define MPI_IOCSTATUS_FC_RX_ID_INVALID          (0x0067)
#define MPI_IOCSTATUS_FC_DID_INVALID            (0x0068)
#define MPI_IOCSTATUS_FC_NODE_LOGGED_OUT        (0x0069)
#define MPI_IOCSTATUS_FC_EXCHANGE_CANCELED      (0x006C)

/****************************************************************************/
/*  LAN values                                                              */
/****************************************************************************/

#define MPI_IOCSTATUS_LAN_DEVICE_NOT_FOUND      (0x0080)
#define MPI_IOCSTATUS_LAN_DEVICE_FAILURE        (0x0081)
#define MPI_IOCSTATUS_LAN_TRANSMIT_ERROR        (0x0082)
#define MPI_IOCSTATUS_LAN_TRANSMIT_ABORTED      (0x0083)
#define MPI_IOCSTATUS_LAN_RECEIVE_ERROR         (0x0084)
#define MPI_IOCSTATUS_LAN_RECEIVE_ABORTED       (0x0085)
#define MPI_IOCSTATUS_LAN_PARTIAL_PACKET        (0x0086)
#define MPI_IOCSTATUS_LAN_CANCELED              (0x0087)


/****************************************************************************/
/*  IOCStatus flag to indicate that log info is available                   */
/****************************************************************************/

#define MPI_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE   (0x8000)
#define MPI_IOCSTATUS_MASK                      (0x7FFF)

/****************************************************************************/
/*  LogInfo Types                                                           */
/****************************************************************************/

#define MPI_IOCLOGINFO_TYPE_MASK                (0xF0000000)
#define MPI_IOCLOGINFO_TYPE_SHIFT               (28)
#define MPI_IOCLOGINFO_TYPE_NONE                (0x0)
#define MPI_IOCLOGINFO_TYPE_SCSI                (0x1)
#define MPI_IOCLOGINFO_TYPE_FC                  (0x2)
#define MPI_IOCLOGINFO_LOG_DATA_MASK            (0x0FFFFFFF)


#endif
/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_cnfg.h,v 1.6 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_CNFG.H
 *          Title:  MPI Config message, structures, and Pages
 *  Creation Date:  July 27, 2000
 *
 *    MPI_CNFG.H Version:  01.02.13
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-08-00  01.00.02  Added _PAGEVERSION definitions for all pages.
 *                      Added FcPhLowestVersion, FcPhHighestVersion, Reserved2
 *                      fields to FC_DEVICE_0 page, updated the page version.
 *                      Changed _FREE_RUNNING_CLOCK to _PACING_TRANSFERS in
 *                      SCSI_PORT_0, SCSI_DEVICE_0 and SCSI_DEVICE_1 pages
 *                      and updated the page versions.
 *                      Added _RESPONSE_ID_MASK definition to SCSI_PORT_1
 *                      page and updated the page version.
 *                      Added Information field and _INFO_PARAMS_NEGOTIATED
 *                      definitionto SCSI_DEVICE_0 page.
 *  06-22-00  01.00.03  Removed batch controls from LAN_0 page and updated the
 *                      page version.
 *                      Added BucketsRemaining to LAN_1 page, redefined the
 *                      state values, and updated the page version.
 *                      Revised bus width definitions in SCSI_PORT_0,
 *                      SCSI_DEVICE_0 and SCSI_DEVICE_1 pages.
 *  06-30-00  01.00.04  Added MaxReplySize to LAN_1 page and updated the page
 *                      version.
 *                      Moved FC_DEVICE_0 PageAddress description to spec.
 *  07-27-00  01.00.05  Corrected the SubsystemVendorID and SubsystemID field
 *                      widths in IOC_0 page and updated the page version.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *                      Added Manufacturing pages, IO Unit Page 2, SCSI SPI
 *                      Port Page 2, FC Port Page 4, FC Port Page 5
 *  11-15-00  01.01.02  Interim changes to match proposals
 *  12-04-00  01.01.03  Config page changes to match MPI rev 1.00.01.
 *  12-05-00  01.01.04  Modified config page actions.
 *  01-09-01  01.01.05  Added defines for page address formats.
 *                      Data size for Manufacturing pages 2 and 3 no longer
 *                      defined here.
 *                      Io Unit Page 2 size is fixed at 4 adapters and some
 *                      flags were changed.
 *                      SCSI Port Page 2 Device Settings modified.
 *                      New fields added to FC Port Page 0 and some flags
 *                      cleaned up.
 *                      Removed impedance flash from FC Port Page 1.
 *                      Added FC Port pages 6 and 7.
 *  01-25-01  01.01.06  Added MaxInitiators field to FcPortPage0.
 *  01-29-01  01.01.07  Changed some defines to make them 32 character unique.
 *                      Added some LinkType defines for FcPortPage0.
 *  02-20-01  01.01.08  Started using MPI_POINTER.
 *  02-27-01  01.01.09  Replaced MPI_CONFIG_PAGETYPE_SCSI_LUN with
 *                      MPI_CONFIG_PAGETYPE_RAID_VOLUME.
 *                      Added definitions and structures for IOC Page 2 and
 *                      RAID Volume Page 2.
 *  03-27-01  01.01.10  Added CONFIG_PAGE_FC_PORT_8 and CONFIG_PAGE_FC_PORT_9.
 *                      CONFIG_PAGE_FC_PORT_3 now supports persistent by DID.
 *                      Added VendorId and ProductRevLevel fields to
 *                      RAIDVOL2_IM_PHYS_ID struct.
 *                      Modified values for MPI_FCPORTPAGE0_FLAGS_ATTACH_
 *                      defines to make them compatible to MPI version 1.0.
 *                      Added structure offset comments.
 *  04-09-01  01.01.11  Added some new defines for the PageAddress field and
 *                      removed some obsolete ones.
 *                      Added IO Unit Page 3.
 *                      Modified defines for Scsi Port Page 2.
 *                      Modified RAID Volume Pages.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *                      Added SepID and SepBus to RVP2 IMPhysicalDisk struct.
 *                      Added defines for the SEP bits in RVP2 VolumeSettings.
 *                      Modified the DeviceSettings field in RVP2 to use the
 *                      proper structure.
 *                      Added defines for SES, SAF-TE, and cross channel for
 *                      IOCPage2 CapabilitiesFlags.
 *                      Removed define for MPI_IOUNITPAGE2_FLAGS_RAID_DISABLE.
 *                      Removed define for
 *                      MPI_SCSIPORTPAGE2_PORT_FLAGS_PARITY_ENABLE.
 *                      Added define for MPI_CONFIG_PAGEATTR_RO_PERSISTENT.
 *  08-29-01 01.02.02   Fixed value for MPI_MANUFACTPAGE_DEVID_53C1035.
 *                      Added defines for MPI_FCPORTPAGE1_FLAGS_HARD_ALPA_ONLY
 *                      and MPI_FCPORTPAGE1_FLAGS_IMMEDIATE_ERROR_REPLY.
 *                      Removed MPI_SCSIPORTPAGE0_CAP_PACING_TRANSFERS,
 *                      MPI_SCSIDEVPAGE0_NP_PACING_TRANSFERS, and
 *                      MPI_SCSIDEVPAGE1_RP_PACING_TRANSFERS, and
 *                      MPI_SCSIDEVPAGE1_CONF_PPR_ALLOWED.
 *                      Added defines for MPI_SCSIDEVPAGE1_CONF_WDTR_DISALLOWED
 *                      and MPI_SCSIDEVPAGE1_CONF_SDTR_DISALLOWED.
 *                      Added OnBusTimerValue to CONFIG_PAGE_SCSI_PORT_1.
 *                      Added rejected bits to SCSI Device Page 0 Information.
 *                      Increased size of ALPA array in FC Port Page 2 by one
 *                      and removed a one byte reserved field.
 *  09-28-01 01.02.03   Swapped NegWireSpeedLow and NegWireSpeedLow in
 *                      CONFIG_PAGE_LAN_1 to match preferred 64-bit ordering.
 *                      Added structures for Manufacturing Page 4, IO Unit
 *                      Page 3, IOC Page 3, IOC Page 4, RAID Volume Page 0, and
 *                      RAID PhysDisk Page 0.
 *  10-04-01 01.02.04   Added define for MPI_CONFIG_PAGETYPE_RAID_PHYSDISK.
 *                      Modified some of the new defines to make them 32
 *                      character unique.
 *                      Modified how variable length pages (arrays) are defined.
 *                      Added generic defines for hot spare pools and RAID
 *                      volume types.
 *  11-01-01 01.02.05   Added define for MPI_IOUNITPAGE1_DISABLE_IR.
 *  03-14-02 01.02.06   Added PCISlotNum field to CONFIG_PAGE_IOC_1 along with
 *                      related define, and bumped the page version define.
 *  05-31-02 01.02.07   Added a Flags field to CONFIG_PAGE_IOC_2_RAID_VOL in a
 *                      reserved byte and added a define.
 *                      Added define for
 *                      MPI_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE.
 *                      Added new config page: CONFIG_PAGE_IOC_5.
 *                      Added MaxAliases, MaxHardAliases, and NumCurrentAliases
 *                      fields to CONFIG_PAGE_FC_PORT_0.
 *                      Added AltConnector and NumRequestedAliases fields to
 *                      CONFIG_PAGE_FC_PORT_1.
 *                      Added new config page: CONFIG_PAGE_FC_PORT_10.
 *  07-12-02 01.02.08   Added more MPI_MANUFACTPAGE_DEVID_ defines.
 *                      Added additional MPI_SCSIDEVPAGE0_NP_ defines.
 *                      Added more MPI_SCSIDEVPAGE1_RP_ defines.
 *                      Added define for
 *                      MPI_SCSIDEVPAGE1_CONF_EXTENDED_PARAMS_ENABLE.
 *                      Added new config page: CONFIG_PAGE_SCSI_DEVICE_3.
 *                      Modified MPI_FCPORTPAGE5_FLAGS_ defines.
 *  09-16-02 01.02.09   Added MPI_SCSIDEVPAGE1_CONF_FORCE_PPR_MSG define.
 *  11-15-02 01.02.10   Added ConnectedID defines for CONFIG_PAGE_SCSI_PORT_0.
 *                      Added more Flags defines for CONFIG_PAGE_FC_PORT_1.
 *                      Added more Flags defines for CONFIG_PAGE_FC_DEVICE_0.
 *  04-01-03 01.02.11   Added RR_TOV field and additional Flags defines for
 *                      CONFIG_PAGE_FC_PORT_1.
 *                      Added define MPI_FCPORTPAGE5_FLAGS_DISABLE to disable
 *                      an alias.
 *                      Added more device id defines.
 *  06-26-03 01.02.12   Added MPI_IOUNITPAGE1_IR_USE_STATIC_VOLUME_ID define.
 *                      Added TargetConfig and IDConfig fields to
 *                      CONFIG_PAGE_SCSI_PORT_1.
 *                      Added more PortFlags defines for CONFIG_PAGE_SCSI_PORT_2
 *                      to control DV.
 *                      Added more Flags defines for CONFIG_PAGE_FC_PORT_1.
 *                      In CONFIG_PAGE_FC_DEVICE_0, replaced Reserved1 field
 *                      with ADISCHardALPA.
 *                      Added MPI_FC_DEVICE_PAGE0_PROT_FCP_RETRY define.
 *  01-16-04 01.02.13   Added InitiatorDeviceTimeout and InitiatorIoPendTimeout
 *                      fields and related defines to CONFIG_PAGE_FC_PORT_1.
 *                      Added define for
 *                      MPI_FCPORTPAGE1_FLAGS_SOFT_ALPA_FALLBACK.
 *                      Added new fields to the substructures of
 *                      CONFIG_PAGE_FC_PORT_10.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_CNFG_H
#define MPI_CNFG_H


/*****************************************************************************
*
*       C o n f i g    M e s s a g e    a n d    S t r u c t u r e s
*
*****************************************************************************/

typedef struct _CONFIG_PAGE_HEADER
{
    U8                      PageVersion;                /* 00h */
    U8                      PageLength;                 /* 01h */
    U8                      PageNumber;                 /* 02h */
    U8                      PageType;                   /* 03h */
} CONFIG_PAGE_HEADER, MPI_POINTER PTR_CONFIG_PAGE_HEADER,
  ConfigPageHeader_t, MPI_POINTER pConfigPageHeader_t;

typedef union _CONFIG_PAGE_HEADER_UNION
{
   ConfigPageHeader_t  Struct;
   U8                  Bytes[4];
   U16                 Word16[2];
   U32                 Word32;
} ConfigPageHeaderUnion, MPI_POINTER pConfigPageHeaderUnion,
  CONFIG_PAGE_HEADER_UNION, MPI_POINTER PTR_CONFIG_PAGE_HEADER_UNION;


/****************************************************************************
*   PageType field values
****************************************************************************/
#define MPI_CONFIG_PAGEATTR_READ_ONLY               (0x00)
#define MPI_CONFIG_PAGEATTR_CHANGEABLE              (0x10)
#define MPI_CONFIG_PAGEATTR_PERSISTENT              (0x20)
#define MPI_CONFIG_PAGEATTR_RO_PERSISTENT           (0x30)
#define MPI_CONFIG_PAGEATTR_MASK                    (0xF0)

#define MPI_CONFIG_PAGETYPE_IO_UNIT                 (0x00)
#define MPI_CONFIG_PAGETYPE_IOC                     (0x01)
#define MPI_CONFIG_PAGETYPE_BIOS                    (0x02)
#define MPI_CONFIG_PAGETYPE_SCSI_PORT               (0x03)
#define MPI_CONFIG_PAGETYPE_SCSI_DEVICE             (0x04)
#define MPI_CONFIG_PAGETYPE_FC_PORT                 (0x05)
#define MPI_CONFIG_PAGETYPE_FC_DEVICE               (0x06)
#define MPI_CONFIG_PAGETYPE_LAN                     (0x07)
#define MPI_CONFIG_PAGETYPE_RAID_VOLUME             (0x08)
#define MPI_CONFIG_PAGETYPE_MANUFACTURING           (0x09)
#define MPI_CONFIG_PAGETYPE_RAID_PHYSDISK           (0x0A)
#define MPI_CONFIG_PAGETYPE_MASK                    (0x0F)

#define MPI_CONFIG_TYPENUM_MASK                     (0x0FFF)


/****************************************************************************
*   PageAddress field values
****************************************************************************/
#define MPI_SCSI_PORT_PGAD_PORT_MASK                (0x000000FF)

#define MPI_SCSI_DEVICE_TARGET_ID_MASK              (0x000000FF)
#define MPI_SCSI_DEVICE_TARGET_ID_SHIFT             (0)
#define MPI_SCSI_DEVICE_BUS_MASK                    (0x0000FF00)
#define MPI_SCSI_DEVICE_BUS_SHIFT                   (8)

#define MPI_FC_PORT_PGAD_PORT_MASK                  (0xF0000000)
#define MPI_FC_PORT_PGAD_PORT_SHIFT                 (28)
#define MPI_FC_PORT_PGAD_FORM_MASK                  (0x0F000000)
#define MPI_FC_PORT_PGAD_FORM_INDEX                 (0x01000000)
#define MPI_FC_PORT_PGAD_INDEX_MASK                 (0x0000FFFF)
#define MPI_FC_PORT_PGAD_INDEX_SHIFT                (0)

#define MPI_FC_DEVICE_PGAD_PORT_MASK                (0xF0000000)
#define MPI_FC_DEVICE_PGAD_PORT_SHIFT               (28)
#define MPI_FC_DEVICE_PGAD_FORM_MASK                (0x0F000000)
#define MPI_FC_DEVICE_PGAD_FORM_NEXT_DID            (0x00000000)
#define MPI_FC_DEVICE_PGAD_ND_PORT_MASK             (0xF0000000)
#define MPI_FC_DEVICE_PGAD_ND_PORT_SHIFT            (28)
#define MPI_FC_DEVICE_PGAD_ND_DID_MASK              (0x00FFFFFF)
#define MPI_FC_DEVICE_PGAD_ND_DID_SHIFT             (0)
#define MPI_FC_DEVICE_PGAD_FORM_BUS_TID             (0x01000000)
#define MPI_FC_DEVICE_PGAD_BT_BUS_MASK              (0x0000FF00)
#define MPI_FC_DEVICE_PGAD_BT_BUS_SHIFT             (8)
#define MPI_FC_DEVICE_PGAD_BT_TID_MASK              (0x000000FF)
#define MPI_FC_DEVICE_PGAD_BT_TID_SHIFT             (0)

#define MPI_PHYSDISK_PGAD_PHYSDISKNUM_MASK          (0x000000FF)
#define MPI_PHYSDISK_PGAD_PHYSDISKNUM_SHIFT         (0)



/****************************************************************************
*   Config Request Message
****************************************************************************/
typedef struct _MSG_CONFIG
{
    U8                      Action;                     /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      Reserved2[8];               /* 0Ch */
    CONFIG_PAGE_HEADER      Header;                     /* 14h */
    U32                     PageAddress;                /* 18h */
    SGE_IO_UNION            PageBufferSGE;              /* 1Ch */
} MSG_CONFIG, MPI_POINTER PTR_MSG_CONFIG,
  Config_t, MPI_POINTER pConfig_t;


/****************************************************************************
*   Action field values
****************************************************************************/
#define MPI_CONFIG_ACTION_PAGE_HEADER               (0x00)
#define MPI_CONFIG_ACTION_PAGE_READ_CURRENT         (0x01)
#define MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT        (0x02)
#define MPI_CONFIG_ACTION_PAGE_DEFAULT              (0x03)
#define MPI_CONFIG_ACTION_PAGE_WRITE_NVRAM          (0x04)
#define MPI_CONFIG_ACTION_PAGE_READ_DEFAULT         (0x05)
#define MPI_CONFIG_ACTION_PAGE_READ_NVRAM           (0x06)


/* Config Reply Message */
typedef struct _MSG_CONFIG_REPLY
{
    U8                      Action;                     /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      Reserved2[2];               /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    CONFIG_PAGE_HEADER      Header;                     /* 14h */
} MSG_CONFIG_REPLY, MPI_POINTER PTR_MSG_CONFIG_REPLY,
  ConfigReply_t, MPI_POINTER pConfigReply_t;



/*****************************************************************************
*
*               C o n f i g u r a t i o n    P a g e s
*
*****************************************************************************/

/****************************************************************************
*   Manufacturing Config pages
****************************************************************************/
#define MPI_MANUFACTPAGE_VENDORID_LSILOGIC          (0x1000)
#define MPI_MANUFACTPAGE_VENDORID_TREBIA            (0x1783)

#define MPI_MANUFACTPAGE_DEVICEID_FC909             (0x0621)
#define MPI_MANUFACTPAGE_DEVICEID_FC919             (0x0624)
#define MPI_MANUFACTPAGE_DEVICEID_FC929             (0x0622)
#define MPI_MANUFACTPAGE_DEVICEID_FC919X            (0x0628)
#define MPI_MANUFACTPAGE_DEVICEID_FC929X            (0x0626)

#define MPI_MANUFACTPAGE_DEVID_53C1030              (0x0030)
#define MPI_MANUFACTPAGE_DEVID_53C1030ZC            (0x0031)
#define MPI_MANUFACTPAGE_DEVID_1030_53C1035         (0x0032)
#define MPI_MANUFACTPAGE_DEVID_1030ZC_53C1035       (0x0033)
#define MPI_MANUFACTPAGE_DEVID_53C1035              (0x0040)
#define MPI_MANUFACTPAGE_DEVID_53C1035ZC            (0x0041)

#define MPI_MANUFACTPAGE_DEVID_SA2010               (0x0804)
#define MPI_MANUFACTPAGE_DEVID_SA2010ZC             (0x0805)
#define MPI_MANUFACTPAGE_DEVID_SA2020               (0x0806)
#define MPI_MANUFACTPAGE_DEVID_SA2020ZC             (0x0807)

#define MPI_MANUFACTPAGE_DEVID_SNP1000              (0x0010)
#define MPI_MANUFACTPAGE_DEVID_SNP500               (0x0020)



typedef struct _CONFIG_PAGE_MANUFACTURING_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      ChipName[16];               /* 04h */
    U8                      ChipRevision[8];            /* 14h */
    U8                      BoardName[16];              /* 1Ch */
    U8                      BoardAssembly[16];          /* 2Ch */
    U8                      BoardTracerNumber[16];      /* 3Ch */

} CONFIG_PAGE_MANUFACTURING_0, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_0,
  ManufacturingPage0_t, MPI_POINTER pManufacturingPage0_t;

#define MPI_MANUFACTURING0_PAGEVERSION                 (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      VPD[256];                   /* 04h */
} CONFIG_PAGE_MANUFACTURING_1, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_1,
  ManufacturingPage1_t, MPI_POINTER pManufacturingPage1_t;

#define MPI_MANUFACTURING1_PAGEVERSION                 (0x00)


typedef struct _MPI_CHIP_REVISION_ID
{
    U16 DeviceID;                                       /* 00h */
    U8  PCIRevisionID;                                  /* 02h */
    U8  Reserved;                                       /* 03h */
} MPI_CHIP_REVISION_ID, MPI_POINTER PTR_MPI_CHIP_REVISION_ID,
  MpiChipRevisionId_t, MPI_POINTER pMpiChipRevisionId_t;


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_MAN_PAGE_2_HW_SETTINGS_WORDS
#define MPI_MAN_PAGE_2_HW_SETTINGS_WORDS    (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_2
{
    CONFIG_PAGE_HEADER      Header;                                 /* 00h */
    MPI_CHIP_REVISION_ID    ChipId;                                 /* 04h */
    U32                     HwSettings[MPI_MAN_PAGE_2_HW_SETTINGS_WORDS];/* 08h */
} CONFIG_PAGE_MANUFACTURING_2, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_2,
  ManufacturingPage2_t, MPI_POINTER pManufacturingPage2_t;

#define MPI_MANUFACTURING2_PAGEVERSION                  (0x00)


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_MAN_PAGE_3_INFO_WORDS
#define MPI_MAN_PAGE_3_INFO_WORDS           (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_3
{
    CONFIG_PAGE_HEADER                  Header;                     /* 00h */
    MPI_CHIP_REVISION_ID                ChipId;                     /* 04h */
    U32                                 Info[MPI_MAN_PAGE_3_INFO_WORDS];/* 08h */
} CONFIG_PAGE_MANUFACTURING_3, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_3,
  ManufacturingPage3_t, MPI_POINTER pManufacturingPage3_t;

#define MPI_MANUFACTURING3_PAGEVERSION                  (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_4
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U32                             Reserved1;          /* 04h */
    U8                              InfoOffset0;        /* 08h */
    U8                              InfoSize0;          /* 09h */
    U8                              InfoOffset1;        /* 0Ah */
    U8                              InfoSize1;          /* 0Bh */
    U8                              InquirySize;        /* 0Ch */
    U8                              Reserved2;          /* 0Dh */
    U16                             Reserved3;          /* 0Eh */
    U8                              InquiryData[56];    /* 10h */
    U32                             ISVolumeSettings;   /* 48h */
    U32                             IMEVolumeSettings;  /* 4Ch */
    U32                             IMVolumeSettings;   /* 50h */
} CONFIG_PAGE_MANUFACTURING_4, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_4,
  ManufacturingPage4_t, MPI_POINTER pManufacturingPage4_t;

#define MPI_MANUFACTURING4_PAGEVERSION                  (0x00)


/****************************************************************************
*   IO Unit Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_IO_UNIT_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U64                     UniqueValue;                /* 04h */
} CONFIG_PAGE_IO_UNIT_0, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_0,
  IOUnitPage0_t, MPI_POINTER pIOUnitPage0_t;

#define MPI_IOUNITPAGE0_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_IO_UNIT_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
} CONFIG_PAGE_IO_UNIT_1, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_1,
  IOUnitPage1_t, MPI_POINTER pIOUnitPage1_t;

#define MPI_IOUNITPAGE1_PAGEVERSION                     (0x00)

/* IO Unit Page 1 Flags defines */

#define MPI_IOUNITPAGE1_MULTI_FUNCTION                  (0x00000000)
#define MPI_IOUNITPAGE1_SINGLE_FUNCTION                 (0x00000001)
#define MPI_IOUNITPAGE1_MULTI_PATHING                   (0x00000002)
#define MPI_IOUNITPAGE1_SINGLE_PATHING                  (0x00000000)
#define MPI_IOUNITPAGE1_IR_USE_STATIC_VOLUME_ID         (0x00000004)
#define MPI_IOUNITPAGE1_DISABLE_IR                      (0x00000040)
#define MPI_IOUNITPAGE1_FORCE_32                        (0x00000080)


typedef struct _MPI_ADAPTER_INFO
{
    U8      PciBusNumber;                               /* 00h */
    U8      PciDeviceAndFunctionNumber;                 /* 01h */
    U16     AdapterFlags;                               /* 02h */
} MPI_ADAPTER_INFO, MPI_POINTER PTR_MPI_ADAPTER_INFO,
  MpiAdapterInfo_t, MPI_POINTER pMpiAdapterInfo_t;

#define MPI_ADAPTER_INFO_FLAGS_EMBEDDED                 (0x0001)
#define MPI_ADAPTER_INFO_FLAGS_INIT_STATUS              (0x0002)

typedef struct _CONFIG_PAGE_IO_UNIT_2
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U32                     BiosVersion;                /* 08h */
    MPI_ADAPTER_INFO        AdapterOrder[4];            /* 0Ch */
} CONFIG_PAGE_IO_UNIT_2, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_2,
  IOUnitPage2_t, MPI_POINTER pIOUnitPage2_t;

#define MPI_IOUNITPAGE2_PAGEVERSION                     (0x00)

#define MPI_IOUNITPAGE2_FLAGS_PAUSE_ON_ERROR            (0x00000002)
#define MPI_IOUNITPAGE2_FLAGS_VERBOSE_ENABLE            (0x00000004)
#define MPI_IOUNITPAGE2_FLAGS_COLOR_VIDEO_DISABLE       (0x00000008)
#define MPI_IOUNITPAGE2_FLAGS_DONT_HOOK_INT_40          (0x00000010)


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX
#define MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX     (1)
#endif

typedef struct _CONFIG_PAGE_IO_UNIT_3
{
    CONFIG_PAGE_HEADER      Header;                                   /* 00h */
    U8                      GPIOCount;                                /* 04h */
    U8                      Reserved1;                                /* 05h */
    U16                     Reserved2;                                /* 06h */
    U16                     GPIOVal[MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX]; /* 08h */
} CONFIG_PAGE_IO_UNIT_3, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_3,
  IOUnitPage3_t, MPI_POINTER pIOUnitPage3_t;

#define MPI_IOUNITPAGE3_PAGEVERSION                     (0x01)

#define MPI_IOUNITPAGE3_GPIO_FUNCTION_MASK              (0xFC)
#define MPI_IOUNITPAGE3_GPIO_FUNCTION_SHIFT             (2)
#define MPI_IOUNITPAGE3_GPIO_SETTING_OFF                (0x00)
#define MPI_IOUNITPAGE3_GPIO_SETTING_ON                 (0x01)


/****************************************************************************
*   IOC Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_IOC_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     TotalNVStore;               /* 04h */
    U32                     FreeNVStore;                /* 08h */
    U16                     VendorID;                   /* 0Ch */
    U16                     DeviceID;                   /* 0Eh */
    U8                      RevisionID;                 /* 10h */
    U8                      Reserved[3];                /* 11h */
    U32                     ClassCode;                  /* 14h */
    U16                     SubsystemVendorID;          /* 18h */
    U16                     SubsystemID;                /* 1Ah */
} CONFIG_PAGE_IOC_0, MPI_POINTER PTR_CONFIG_PAGE_IOC_0,
  IOCPage0_t, MPI_POINTER pIOCPage0_t;

#define MPI_IOCPAGE0_PAGEVERSION                        (0x01)


typedef struct _CONFIG_PAGE_IOC_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U32                     CoalescingTimeout;          /* 08h */
    U8                      CoalescingDepth;            /* 0Ch */
    U8                      PCISlotNum;                 /* 0Dh */
    U8                      Reserved[2];                /* 0Eh */
} CONFIG_PAGE_IOC_1, MPI_POINTER PTR_CONFIG_PAGE_IOC_1,
  IOCPage1_t, MPI_POINTER pIOCPage1_t;

#define MPI_IOCPAGE1_PAGEVERSION                        (0x01)

#define MPI_IOCPAGE1_REPLY_COALESCING                   (0x00000001)

#define MPI_IOCPAGE1_PCISLOTNUM_UNKNOWN                 (0xFF)


typedef struct _CONFIG_PAGE_IOC_2_RAID_VOL
{
    U8                          VolumeID;               /* 00h */
    U8                          VolumeBus;              /* 01h */
    U8                          VolumeIOC;              /* 02h */
    U8                          VolumePageNumber;       /* 03h */
    U8                          VolumeType;             /* 04h */
    U8                          Flags;                  /* 05h */
    U16                         Reserved3;              /* 06h */
} CONFIG_PAGE_IOC_2_RAID_VOL, MPI_POINTER PTR_CONFIG_PAGE_IOC_2_RAID_VOL,
  ConfigPageIoc2RaidVol_t, MPI_POINTER pConfigPageIoc2RaidVol_t;

/* IOC Page 2 Volume RAID Type values, also used in RAID Volume pages */

#define MPI_RAID_VOL_TYPE_IS                        (0x00)
#define MPI_RAID_VOL_TYPE_IME                       (0x01)
#define MPI_RAID_VOL_TYPE_IM                        (0x02)

/* IOC Page 2 Volume Flags values */

#define MPI_IOCPAGE2_FLAG_VOLUME_INACTIVE           (0x08)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_2_RAID_VOLUME_MAX
#define MPI_IOC_PAGE_2_RAID_VOLUME_MAX      (1)
#endif

typedef struct _CONFIG_PAGE_IOC_2
{
    CONFIG_PAGE_HEADER          Header;                              /* 00h */
    U32                         CapabilitiesFlags;                   /* 04h */
    U8                          NumActiveVolumes;                    /* 08h */
    U8                          MaxVolumes;                          /* 09h */
    U8                          NumActivePhysDisks;                  /* 0Ah */
    U8                          MaxPhysDisks;                        /* 0Bh */
    CONFIG_PAGE_IOC_2_RAID_VOL  RaidVolume[MPI_IOC_PAGE_2_RAID_VOLUME_MAX];/* 0Ch */
} CONFIG_PAGE_IOC_2, MPI_POINTER PTR_CONFIG_PAGE_IOC_2,
  IOCPage2_t, MPI_POINTER pIOCPage2_t;

#define MPI_IOCPAGE2_PAGEVERSION                        (0x02)

/* IOC Page 2 Capabilities flags */

#define MPI_IOCPAGE2_CAP_FLAGS_IS_SUPPORT               (0x00000001)
#define MPI_IOCPAGE2_CAP_FLAGS_IME_SUPPORT              (0x00000002)
#define MPI_IOCPAGE2_CAP_FLAGS_IM_SUPPORT               (0x00000004)
#define MPI_IOCPAGE2_CAP_FLAGS_SES_SUPPORT              (0x20000000)
#define MPI_IOCPAGE2_CAP_FLAGS_SAFTE_SUPPORT            (0x40000000)
#define MPI_IOCPAGE2_CAP_FLAGS_CROSS_CHANNEL_SUPPORT    (0x80000000)


typedef struct _IOC_3_PHYS_DISK
{
    U8                          PhysDiskID;             /* 00h */
    U8                          PhysDiskBus;            /* 01h */
    U8                          PhysDiskIOC;            /* 02h */
    U8                          PhysDiskNum;            /* 03h */
} IOC_3_PHYS_DISK, MPI_POINTER PTR_IOC_3_PHYS_DISK,
  Ioc3PhysDisk_t, MPI_POINTER pIoc3PhysDisk_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_3_PHYSDISK_MAX
#define MPI_IOC_PAGE_3_PHYSDISK_MAX         (1)
#endif

typedef struct _CONFIG_PAGE_IOC_3
{
    CONFIG_PAGE_HEADER          Header;                                /* 00h */
    U8                          NumPhysDisks;                          /* 04h */
    U8                          Reserved1;                             /* 05h */
    U16                         Reserved2;                             /* 06h */
    IOC_3_PHYS_DISK             PhysDisk[MPI_IOC_PAGE_3_PHYSDISK_MAX]; /* 08h */
} CONFIG_PAGE_IOC_3, MPI_POINTER PTR_CONFIG_PAGE_IOC_3,
  IOCPage3_t, MPI_POINTER pIOCPage3_t;

#define MPI_IOCPAGE3_PAGEVERSION                        (0x00)


typedef struct _IOC_4_SEP
{
    U8                          SEPTargetID;            /* 00h */
    U8                          SEPBus;                 /* 01h */
    U16                         Reserved;               /* 02h */
} IOC_4_SEP, MPI_POINTER PTR_IOC_4_SEP,
  Ioc4Sep_t, MPI_POINTER pIoc4Sep_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_4_SEP_MAX
#define MPI_IOC_PAGE_4_SEP_MAX              (1)
#endif

typedef struct _CONFIG_PAGE_IOC_4
{
    CONFIG_PAGE_HEADER          Header;                         /* 00h */
    U8                          ActiveSEP;                      /* 04h */
    U8                          MaxSEP;                         /* 05h */
    U16                         Reserved1;                      /* 06h */
    IOC_4_SEP                   SEP[MPI_IOC_PAGE_4_SEP_MAX];    /* 08h */
} CONFIG_PAGE_IOC_4, MPI_POINTER PTR_CONFIG_PAGE_IOC_4,
  IOCPage4_t, MPI_POINTER pIOCPage4_t;

#define MPI_IOCPAGE4_PAGEVERSION                        (0x00)


typedef struct _IOC_5_HOT_SPARE
{
    U8                          PhysDiskNum;            /* 00h */
    U8                          Reserved;               /* 01h */
    U8                          HotSparePool;           /* 02h */
    U8                          Flags;                   /* 03h */
} IOC_5_HOT_SPARE, MPI_POINTER PTR_IOC_5_HOT_SPARE,
  Ioc5HotSpare_t, MPI_POINTER pIoc5HotSpare_t;

/* IOC Page 5 HotSpare Flags */
#define MPI_IOC_PAGE_5_HOT_SPARE_ACTIVE                 (0x01)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_5_HOT_SPARE_MAX
#define MPI_IOC_PAGE_5_HOT_SPARE_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_IOC_5
{
    CONFIG_PAGE_HEADER          Header;                         /* 00h */
    U32                         Reserved1;                      /* 04h */
    U8                          NumHotSpares;                   /* 08h */
    U8                          Reserved2;                      /* 09h */
    U16                         Reserved3;                      /* 0Ah */
    IOC_5_HOT_SPARE             HotSpare[MPI_IOC_PAGE_5_HOT_SPARE_MAX]; /* 0Ch */
} CONFIG_PAGE_IOC_5, MPI_POINTER PTR_CONFIG_PAGE_IOC_5,
  IOCPage5_t, MPI_POINTER pIOCPage5_t;

#define MPI_IOCPAGE5_PAGEVERSION                        (0x00)



/****************************************************************************
*   SCSI Port Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_SCSI_PORT_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Capabilities;               /* 04h */
    U32                     PhysicalInterface;          /* 08h */
} CONFIG_PAGE_SCSI_PORT_0, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_0,
  SCSIPortPage0_t, MPI_POINTER pSCSIPortPage0_t;

#define MPI_SCSIPORTPAGE0_PAGEVERSION                   (0x01)

#define MPI_SCSIPORTPAGE0_CAP_IU                        (0x00000001)
#define MPI_SCSIPORTPAGE0_CAP_DT                        (0x00000002)
#define MPI_SCSIPORTPAGE0_CAP_QAS                       (0x00000004)
#define MPI_SCSIPORTPAGE0_CAP_MIN_SYNC_PERIOD_MASK      (0x0000FF00)
#define MPI_SCSIPORTPAGE0_CAP_MAX_SYNC_OFFSET_MASK      (0x00FF0000)
#define MPI_SCSIPORTPAGE0_CAP_WIDE                      (0x20000000)
#define MPI_SCSIPORTPAGE0_CAP_AIP                       (0x80000000)

#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_TYPE_MASK          (0x00000003)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_HVD                (0x01)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_SE                 (0x02)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_LVD                (0x03)
#define MPI_SCSIPORTPAGE0_PHY_MASK_CONNECTED_ID         (0xFF000000)
#define MPI_SCSIPORTPAGE0_PHY_SHIFT_CONNECTED_ID        (24)
#define MPI_SCSIPORTPAGE0_PHY_BUS_FREE_CONNECTED_ID     (0xFE)
#define MPI_SCSIPORTPAGE0_PHY_UNKNOWN_CONNECTED_ID      (0xFF)


typedef struct _CONFIG_PAGE_SCSI_PORT_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Configuration;              /* 04h */
    U32                     OnBusTimerValue;            /* 08h */
    U8                      TargetConfig;               /* 0Ch */
    U8                      Reserved1;                  /* 0Dh */
    U16                     IDConfig;                   /* 0Eh */
} CONFIG_PAGE_SCSI_PORT_1, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_1,
  SCSIPortPage1_t, MPI_POINTER pSCSIPortPage1_t;

#define MPI_SCSIPORTPAGE1_PAGEVERSION                   (0x03)

/* Configuration values */
#define MPI_SCSIPORTPAGE1_CFG_PORT_SCSI_ID_MASK         (0x000000FF)
#define MPI_SCSIPORTPAGE1_CFG_PORT_RESPONSE_ID_MASK     (0xFFFF0000)

/* TargetConfig values */
#define MPI_SCSIPORTPAGE1_TARGCONFIG_TARG_ONLY        (0x01)
#define MPI_SCSIPORTPAGE1_TARGCONFIG_INIT_TARG        (0x02)


typedef struct _MPI_DEVICE_INFO
{
    U8      Timeout;                                    /* 00h */
    U8      SyncFactor;                                 /* 01h */
    U16     DeviceFlags;                                /* 02h */
} MPI_DEVICE_INFO, MPI_POINTER PTR_MPI_DEVICE_INFO,
  MpiDeviceInfo_t, MPI_POINTER pMpiDeviceInfo_t;

typedef struct _CONFIG_PAGE_SCSI_PORT_2
{
    CONFIG_PAGE_HEADER  Header;                         /* 00h */
    U32                 PortFlags;                      /* 04h */
    U32                 PortSettings;                   /* 08h */
    MPI_DEVICE_INFO     DeviceSettings[16];             /* 0Ch */
} CONFIG_PAGE_SCSI_PORT_2, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_2,
  SCSIPortPage2_t, MPI_POINTER pSCSIPortPage2_t;

#define MPI_SCSIPORTPAGE2_PAGEVERSION                       (0x02)

/* PortFlags values */
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_SCAN_HIGH_TO_LOW       (0x00000001)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_AVOID_SCSI_RESET       (0x00000004)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_ALTERNATE_CHS          (0x00000008)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_TERMINATION_DISABLE    (0x00000010)

#define MPI_SCSIPORTPAGE2_PORT_FLAGS_DV_MASK                (0x00000060)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_FULL_DV                (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_BASIC_DV_ONLY          (0x00000020)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_OFF_DV                 (0x00000060)

/* PortSettings values */
#define MPI_SCSIPORTPAGE2_PORT_HOST_ID_MASK                 (0x0000000F)
#define MPI_SCSIPORTPAGE2_PORT_MASK_INIT_HBA                (0x00000030)
#define MPI_SCSIPORTPAGE2_PORT_DISABLE_INIT_HBA             (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_BIOS_INIT_HBA                (0x00000010)
#define MPI_SCSIPORTPAGE2_PORT_OS_INIT_HBA                  (0x00000020)
#define MPI_SCSIPORTPAGE2_PORT_BIOS_OS_INIT_HBA             (0x00000030)
#define MPI_SCSIPORTPAGE2_PORT_REMOVABLE_MEDIA              (0x000000C0)
#define MPI_SCSIPORTPAGE2_PORT_SPINUP_DELAY_MASK            (0x00000F00)
#define MPI_SCSIPORTPAGE2_PORT_MASK_NEGO_MASTER_SETTINGS    (0x00003000)
#define MPI_SCSIPORTPAGE2_PORT_NEGO_MASTER_SETTINGS         (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_NONE_MASTER_SETTINGS         (0x00001000)
#define MPI_SCSIPORTPAGE2_PORT_ALL_MASTER_SETTINGS          (0x00003000)

#define MPI_SCSIPORTPAGE2_DEVICE_DISCONNECT_ENABLE          (0x0001)
#define MPI_SCSIPORTPAGE2_DEVICE_ID_SCAN_ENABLE             (0x0002)
#define MPI_SCSIPORTPAGE2_DEVICE_LUN_SCAN_ENABLE            (0x0004)
#define MPI_SCSIPORTPAGE2_DEVICE_TAG_QUEUE_ENABLE           (0x0008)
#define MPI_SCSIPORTPAGE2_DEVICE_WIDE_DISABLE               (0x0010)
#define MPI_SCSIPORTPAGE2_DEVICE_BOOT_CHOICE                (0x0020)


/****************************************************************************
*   SCSI Target Device Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_SCSI_DEVICE_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     NegotiatedParameters;       /* 04h */
    U32                     Information;                /* 08h */
} CONFIG_PAGE_SCSI_DEVICE_0, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_0,
  SCSIDevicePage0_t, MPI_POINTER pSCSIDevicePage0_t;

#define MPI_SCSIDEVPAGE0_PAGEVERSION                    (0x03)

#define MPI_SCSIDEVPAGE0_NP_IU                          (0x00000001)
#define MPI_SCSIDEVPAGE0_NP_DT                          (0x00000002)
#define MPI_SCSIDEVPAGE0_NP_QAS                         (0x00000004)
#define MPI_SCSIDEVPAGE0_NP_HOLD_MCS                    (0x00000008)
#define MPI_SCSIDEVPAGE0_NP_WR_FLOW                     (0x00000010)
#define MPI_SCSIDEVPAGE0_NP_RD_STRM                     (0x00000020)
#define MPI_SCSIDEVPAGE0_NP_RTI                         (0x00000040)
#define MPI_SCSIDEVPAGE0_NP_PCOMP_EN                    (0x00000080)
#define MPI_SCSIDEVPAGE0_NP_NEG_SYNC_PERIOD_MASK        (0x0000FF00)
#define MPI_SCSIDEVPAGE0_NP_NEG_SYNC_OFFSET_MASK        (0x00FF0000)
#define MPI_SCSIDEVPAGE0_NP_WIDE                        (0x20000000)
#define MPI_SCSIDEVPAGE0_NP_AIP                         (0x80000000)

#define MPI_SCSIDEVPAGE0_INFO_PARAMS_NEGOTIATED         (0x00000001)
#define MPI_SCSIDEVPAGE0_INFO_SDTR_REJECTED             (0x00000002)
#define MPI_SCSIDEVPAGE0_INFO_WDTR_REJECTED             (0x00000004)
#define MPI_SCSIDEVPAGE0_INFO_PPR_REJECTED              (0x00000008)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     RequestedParameters;        /* 04h */
    U32                     Reserved;                   /* 08h */
    U32                     Configuration;              /* 0Ch */
} CONFIG_PAGE_SCSI_DEVICE_1, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_1,
  SCSIDevicePage1_t, MPI_POINTER pSCSIDevicePage1_t;

#define MPI_SCSIDEVPAGE1_PAGEVERSION                    (0x04)

#define MPI_SCSIDEVPAGE1_RP_IU                          (0x00000001)
#define MPI_SCSIDEVPAGE1_RP_DT                          (0x00000002)
#define MPI_SCSIDEVPAGE1_RP_QAS                         (0x00000004)
#define MPI_SCSIDEVPAGE1_RP_HOLD_MCS                    (0x00000008)
#define MPI_SCSIDEVPAGE1_RP_WR_FLOW                     (0x00000010)
#define MPI_SCSIDEVPAGE1_RP_RD_STRM                     (0x00000020)
#define MPI_SCSIDEVPAGE1_RP_RTI                         (0x00000040)
#define MPI_SCSIDEVPAGE1_RP_PCOMP_EN                    (0x00000080)
#define MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK        (0x0000FF00)
#define MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK        (0x00FF0000)
#define MPI_SCSIDEVPAGE1_RP_WIDE                        (0x20000000)
#define MPI_SCSIDEVPAGE1_RP_AIP                         (0x80000000)

#define MPI_SCSIDEVPAGE1_CONF_WDTR_DISALLOWED           (0x00000002)
#define MPI_SCSIDEVPAGE1_CONF_SDTR_DISALLOWED           (0x00000004)
#define MPI_SCSIDEVPAGE1_CONF_EXTENDED_PARAMS_ENABLE    (0x00000008)
#define MPI_SCSIDEVPAGE1_CONF_FORCE_PPR_MSG             (0x00000010)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_2
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     DomainValidation;           /* 04h */
    U32                     ParityPipeSelect;           /* 08h */
    U32                     DataPipeSelect;             /* 0Ch */
} CONFIG_PAGE_SCSI_DEVICE_2, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_2,
  SCSIDevicePage2_t, MPI_POINTER pSCSIDevicePage2_t;

#define MPI_SCSIDEVPAGE2_PAGEVERSION                    (0x01)

#define MPI_SCSIDEVPAGE2_DV_ISI_ENABLE                  (0x00000010)
#define MPI_SCSIDEVPAGE2_DV_SECONDARY_DRIVER_ENABLE     (0x00000020)
#define MPI_SCSIDEVPAGE2_DV_SLEW_RATE_CTRL              (0x00000380)
#define MPI_SCSIDEVPAGE2_DV_PRIM_DRIVE_STR_CTRL         (0x00001C00)
#define MPI_SCSIDEVPAGE2_DV_SECOND_DRIVE_STR_CTRL       (0x0000E000)
#define MPI_SCSIDEVPAGE2_DV_XCLKH_ST                    (0x10000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKS_ST                    (0x20000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKH_DT                    (0x40000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKS_DT                    (0x80000000)

#define MPI_SCSIDEVPAGE2_PPS_PPS_MASK                   (0x00000003)

#define MPI_SCSIDEVPAGE2_DPS_BIT_0_PL_SELECT_MASK       (0x00000003)
#define MPI_SCSIDEVPAGE2_DPS_BIT_1_PL_SELECT_MASK       (0x0000000C)
#define MPI_SCSIDEVPAGE2_DPS_BIT_2_PL_SELECT_MASK       (0x00000030)
#define MPI_SCSIDEVPAGE2_DPS_BIT_3_PL_SELECT_MASK       (0x000000C0)
#define MPI_SCSIDEVPAGE2_DPS_BIT_4_PL_SELECT_MASK       (0x00000300)
#define MPI_SCSIDEVPAGE2_DPS_BIT_5_PL_SELECT_MASK       (0x00000C00)
#define MPI_SCSIDEVPAGE2_DPS_BIT_6_PL_SELECT_MASK       (0x00003000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_7_PL_SELECT_MASK       (0x0000C000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_8_PL_SELECT_MASK       (0x00030000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_9_PL_SELECT_MASK       (0x000C0000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_10_PL_SELECT_MASK      (0x00300000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_11_PL_SELECT_MASK      (0x00C00000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_12_PL_SELECT_MASK      (0x03000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_13_PL_SELECT_MASK      (0x0C000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_14_PL_SELECT_MASK      (0x30000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_15_PL_SELECT_MASK      (0xC0000000)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_3
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U16                     MsgRejectCount;             /* 04h */
    U16                     PhaseErrorCount;            /* 06h */
    U16                     ParityErrorCount;           /* 08h */
    U16                     Reserved;                   /* 0Ah */
} CONFIG_PAGE_SCSI_DEVICE_3, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_3,
  SCSIDevicePage3_t, MPI_POINTER pSCSIDevicePage3_t;

#define MPI_SCSIDEVPAGE3_PAGEVERSION                    (0x00)

#define MPI_SCSIDEVPAGE3_MAX_COUNTER                    (0xFFFE)
#define MPI_SCSIDEVPAGE3_UNSUPPORTED_COUNTER            (0xFFFF)


/****************************************************************************
*   FC Port Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_FC_PORT_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U8                      MPIPortNumber;              /* 08h */
    U8                      LinkType;                   /* 09h */
    U8                      PortState;                  /* 0Ah */
    U8                      Reserved;                   /* 0Bh */
    U32                     PortIdentifier;             /* 0Ch */
    U64                     WWNN;                       /* 10h */
    U64                     WWPN;                       /* 18h */
    U32                     SupportedServiceClass;      /* 20h */
    U32                     SupportedSpeeds;            /* 24h */
    U32                     CurrentSpeed;               /* 28h */
    U32                     MaxFrameSize;               /* 2Ch */
    U64                     FabricWWNN;                 /* 30h */
    U64                     FabricWWPN;                 /* 38h */
    U32                     DiscoveredPortsCount;       /* 40h */
    U32                     MaxInitiators;              /* 44h */
    U8                      MaxAliasesSupported;        /* 48h */
    U8                      MaxHardAliasesSupported;    /* 49h */
    U8                      NumCurrentAliases;          /* 4Ah */
    U8                      Reserved1;                  /* 4Bh */
} CONFIG_PAGE_FC_PORT_0, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_0,
  FCPortPage0_t, MPI_POINTER pFCPortPage0_t;

#define MPI_FCPORTPAGE0_PAGEVERSION                     (0x02)

#define MPI_FCPORTPAGE0_FLAGS_PROT_MASK                 (0x0000000F)
#define MPI_FCPORTPAGE0_FLAGS_PROT_FCP_INIT             (MPI_PORTFACTS_PROTOCOL_INITIATOR)
#define MPI_FCPORTPAGE0_FLAGS_PROT_FCP_TARG             (MPI_PORTFACTS_PROTOCOL_TARGET)
#define MPI_FCPORTPAGE0_FLAGS_PROT_LAN                  (MPI_PORTFACTS_PROTOCOL_LAN)
#define MPI_FCPORTPAGE0_FLAGS_PROT_LOGBUSADDR           (MPI_PORTFACTS_PROTOCOL_LOGBUSADDR)

#define MPI_FCPORTPAGE0_FLAGS_ALIAS_ALPA_SUPPORTED      (0x00000010)
#define MPI_FCPORTPAGE0_FLAGS_ALIAS_WWN_SUPPORTED       (0x00000020)
#define MPI_FCPORTPAGE0_FLAGS_FABRIC_WWN_VALID          (0x00000040)

#define MPI_FCPORTPAGE0_FLAGS_ATTACH_TYPE_MASK          (0x00000F00)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_NO_INIT            (0x00000000)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_POINT_TO_POINT     (0x00000100)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_PRIVATE_LOOP       (0x00000200)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_FABRIC_DIRECT      (0x00000400)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_PUBLIC_LOOP        (0x00000800)

#define MPI_FCPORTPAGE0_LTYPE_RESERVED                  (0x00)
#define MPI_FCPORTPAGE0_LTYPE_OTHER                     (0x01)
#define MPI_FCPORTPAGE0_LTYPE_UNKNOWN                   (0x02)
#define MPI_FCPORTPAGE0_LTYPE_COPPER                    (0x03)
#define MPI_FCPORTPAGE0_LTYPE_SINGLE_1300               (0x04)
#define MPI_FCPORTPAGE0_LTYPE_SINGLE_1500               (0x05)
#define MPI_FCPORTPAGE0_LTYPE_50_LASER_MULTI            (0x06)
#define MPI_FCPORTPAGE0_LTYPE_50_LED_MULTI              (0x07)
#define MPI_FCPORTPAGE0_LTYPE_62_LASER_MULTI            (0x08)
#define MPI_FCPORTPAGE0_LTYPE_62_LED_MULTI              (0x09)
#define MPI_FCPORTPAGE0_LTYPE_MULTI_LONG_WAVE           (0x0A)
#define MPI_FCPORTPAGE0_LTYPE_MULTI_SHORT_WAVE          (0x0B)
#define MPI_FCPORTPAGE0_LTYPE_LASER_SHORT_WAVE          (0x0C)
#define MPI_FCPORTPAGE0_LTYPE_LED_SHORT_WAVE            (0x0D)
#define MPI_FCPORTPAGE0_LTYPE_1300_LONG_WAVE            (0x0E)
#define MPI_FCPORTPAGE0_LTYPE_1500_LONG_WAVE            (0x0F)

#define MPI_FCPORTPAGE0_PORTSTATE_UNKNOWN               (0x01)      /*(SNIA)HBA_PORTSTATE_UNKNOWN       1 Unknown */
#define MPI_FCPORTPAGE0_PORTSTATE_ONLINE                (0x02)      /*(SNIA)HBA_PORTSTATE_ONLINE        2 Operational */
#define MPI_FCPORTPAGE0_PORTSTATE_OFFLINE               (0x03)      /*(SNIA)HBA_PORTSTATE_OFFLINE       3 User Offline */
#define MPI_FCPORTPAGE0_PORTSTATE_BYPASSED              (0x04)      /*(SNIA)HBA_PORTSTATE_BYPASSED      4 Bypassed */
#define MPI_FCPORTPAGE0_PORTSTATE_DIAGNOST              (0x05)      /*(SNIA)HBA_PORTSTATE_DIAGNOSTICS   5 In diagnostics mode */
#define MPI_FCPORTPAGE0_PORTSTATE_LINKDOWN              (0x06)      /*(SNIA)HBA_PORTSTATE_LINKDOWN      6 Link Down */
#define MPI_FCPORTPAGE0_PORTSTATE_ERROR                 (0x07)      /*(SNIA)HBA_PORTSTATE_ERROR         7 Port Error */
#define MPI_FCPORTPAGE0_PORTSTATE_LOOPBACK              (0x08)      /*(SNIA)HBA_PORTSTATE_LOOPBACK      8 Loopback */

#define MPI_FCPORTPAGE0_SUPPORT_CLASS_1                 (0x00000001)
#define MPI_FCPORTPAGE0_SUPPORT_CLASS_2                 (0x00000002)
#define MPI_FCPORTPAGE0_SUPPORT_CLASS_3                 (0x00000004)

#define MPI_FCPORTPAGE0_SUPPORT_1GBIT_SPEED             (0x00000001) /* (SNIA)HBA_PORTSPEED_1GBIT 1  1 GBit/sec  */
#define MPI_FCPORTPAGE0_SUPPORT_2GBIT_SPEED             (0x00000002) /* (SNIA)HBA_PORTSPEED_2GBIT 2  2 GBit/sec  */
#define MPI_FCPORTPAGE0_SUPPORT_10GBIT_SPEED            (0x00000004) /* (SNIA)HBA_PORTSPEED_10GBIT 4 10 GBit/sec */

#define MPI_FCPORTPAGE0_CURRENT_SPEED_1GBIT             MPI_FCPORTPAGE0_SUPPORT_1GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_2GBIT             MPI_FCPORTPAGE0_SUPPORT_2GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_10GBIT            MPI_FCPORTPAGE0_SUPPORT_10GBIT_SPEED


typedef struct _CONFIG_PAGE_FC_PORT_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U64                     NoSEEPROMWWNN;              /* 08h */
    U64                     NoSEEPROMWWPN;              /* 10h */
    U8                      HardALPA;                   /* 18h */
    U8                      LinkConfig;                 /* 19h */
    U8                      TopologyConfig;             /* 1Ah */
    U8                      AltConnector;               /* 1Bh */
    U8                      NumRequestedAliases;        /* 1Ch */
    U8                      RR_TOV;                     /* 1Dh */
    U8                      InitiatorDeviceTimeout;     /* 1Eh */
    U8                      InitiatorIoPendTimeout;     /* 1Fh */
} CONFIG_PAGE_FC_PORT_1, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_1,
  FCPortPage1_t, MPI_POINTER pFCPortPage1_t;

#define MPI_FCPORTPAGE1_PAGEVERSION                     (0x06)

#define MPI_FCPORTPAGE1_FLAGS_EXT_FCP_STATUS_EN         (0x08000000)
#define MPI_FCPORTPAGE1_FLAGS_IMMEDIATE_ERROR_REPLY     (0x04000000)
#define MPI_FCPORTPAGE1_FLAGS_FORCE_USE_NOSEEPROM_WWNS  (0x02000000)
#define MPI_FCPORTPAGE1_FLAGS_VERBOSE_RESCAN_EVENTS     (0x01000000)
#define MPI_FCPORTPAGE1_FLAGS_TARGET_MODE_OXID          (0x00800000)
#define MPI_FCPORTPAGE1_FLAGS_PORT_OFFLINE              (0x00400000)
#define MPI_FCPORTPAGE1_FLAGS_SOFT_ALPA_FALLBACK        (0x00200000)
#define MPI_FCPORTPAGE1_FLAGS_MASK_RR_TOV_UNITS         (0x00000070)
#define MPI_FCPORTPAGE1_FLAGS_SUPPRESS_PROT_REG         (0x00000008)
#define MPI_FCPORTPAGE1_FLAGS_PLOGI_ON_LOGO             (0x00000004)
#define MPI_FCPORTPAGE1_FLAGS_MAINTAIN_LOGINS           (0x00000002)
#define MPI_FCPORTPAGE1_FLAGS_SORT_BY_DID               (0x00000001)
#define MPI_FCPORTPAGE1_FLAGS_SORT_BY_WWN               (0x00000000)

#define MPI_FCPORTPAGE1_FLAGS_PROT_MASK                 (0xF0000000)
#define MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT                (28)
#define MPI_FCPORTPAGE1_FLAGS_PROT_FCP_INIT             ((U32)MPI_PORTFACTS_PROTOCOL_INITIATOR << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_FCP_TARG             ((U32)MPI_PORTFACTS_PROTOCOL_TARGET << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_LAN                  ((U32)MPI_PORTFACTS_PROTOCOL_LAN << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_LOGBUSADDR           ((U32)MPI_PORTFACTS_PROTOCOL_LOGBUSADDR << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)

#define MPI_FCPORTPAGE1_FLAGS_NONE_RR_TOV_UNITS         (0x00000000)
#define MPI_FCPORTPAGE1_FLAGS_THOUSANDTH_RR_TOV_UNITS   (0x00000010)
#define MPI_FCPORTPAGE1_FLAGS_TENTH_RR_TOV_UNITS        (0x00000030)
#define MPI_FCPORTPAGE1_FLAGS_TEN_RR_TOV_UNITS          (0x00000050)

#define MPI_FCPORTPAGE1_HARD_ALPA_NOT_USED              (0xFF)

#define MPI_FCPORTPAGE1_LCONFIG_SPEED_MASK              (0x0F)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_1GIG              (0x00)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_2GIG              (0x01)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_4GIG              (0x02)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_10GIG             (0x03)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_AUTO              (0x0F)

#define MPI_FCPORTPAGE1_TOPOLOGY_MASK                   (0x0F)
#define MPI_FCPORTPAGE1_TOPOLOGY_NLPORT                 (0x01)
#define MPI_FCPORTPAGE1_TOPOLOGY_NPORT                  (0x02)
#define MPI_FCPORTPAGE1_TOPOLOGY_AUTO                   (0x0F)

#define MPI_FCPORTPAGE1_ALT_CONN_UNKNOWN                (0x00)

#define MPI_FCPORTPAGE1_INITIATOR_DEV_TIMEOUT_MASK      (0x7F)
#define MPI_FCPORTPAGE1_INITIATOR_DEV_UNIT_16           (0x80)


typedef struct _CONFIG_PAGE_FC_PORT_2
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      NumberActive;               /* 04h */
    U8                      ALPA[127];                  /* 05h */
} CONFIG_PAGE_FC_PORT_2, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_2,
  FCPortPage2_t, MPI_POINTER pFCPortPage2_t;

#define MPI_FCPORTPAGE2_PAGEVERSION                     (0x01)


typedef struct _WWN_FORMAT
{
    U64                     WWNN;                       /* 00h */
    U64                     WWPN;                       /* 08h */
} WWN_FORMAT, MPI_POINTER PTR_WWN_FORMAT,
  WWNFormat, MPI_POINTER pWWNFormat;

typedef union _FC_PORT_PERSISTENT_PHYSICAL_ID
{
    WWN_FORMAT              WWN;
    U32                     Did;
} FC_PORT_PERSISTENT_PHYSICAL_ID, MPI_POINTER PTR_FC_PORT_PERSISTENT_PHYSICAL_ID,
  PersistentPhysicalId_t, MPI_POINTER pPersistentPhysicalId_t;

typedef struct _FC_PORT_PERSISTENT
{
    FC_PORT_PERSISTENT_PHYSICAL_ID  PhysicalIdentifier; /* 00h */
    U8                              TargetID;           /* 10h */
    U8                              Bus;                /* 11h */
    U16                             Flags;              /* 12h */
} FC_PORT_PERSISTENT, MPI_POINTER PTR_FC_PORT_PERSISTENT,
  PersistentData_t, MPI_POINTER pPersistentData_t;

#define MPI_PERSISTENT_FLAGS_SHIFT                      (16)
#define MPI_PERSISTENT_FLAGS_ENTRY_VALID                (0x0001)
#define MPI_PERSISTENT_FLAGS_SCAN_ID                    (0x0002)
#define MPI_PERSISTENT_FLAGS_SCAN_LUNS                  (0x0004)
#define MPI_PERSISTENT_FLAGS_BOOT_DEVICE                (0x0008)
#define MPI_PERSISTENT_FLAGS_BY_DID                     (0x0080)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_FC_PORT_PAGE_3_ENTRY_MAX
#define MPI_FC_PORT_PAGE_3_ENTRY_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_FC_PORT_3
{
    CONFIG_PAGE_HEADER      Header;                                 /* 00h */
    FC_PORT_PERSISTENT      Entry[MPI_FC_PORT_PAGE_3_ENTRY_MAX];    /* 04h */
} CONFIG_PAGE_FC_PORT_3, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_3,
  FCPortPage3_t, MPI_POINTER pFCPortPage3_t;

#define MPI_FCPORTPAGE3_PAGEVERSION                     (0x01)


typedef struct _CONFIG_PAGE_FC_PORT_4
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     PortFlags;                  /* 04h */
    U32                     PortSettings;               /* 08h */
} CONFIG_PAGE_FC_PORT_4, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_4,
  FCPortPage4_t, MPI_POINTER pFCPortPage4_t;

#define MPI_FCPORTPAGE4_PAGEVERSION                     (0x00)

#define MPI_FCPORTPAGE4_PORT_FLAGS_ALTERNATE_CHS        (0x00000008)

#define MPI_FCPORTPAGE4_PORT_MASK_INIT_HBA              (0x00000030)
#define MPI_FCPORTPAGE4_PORT_DISABLE_INIT_HBA           (0x00000000)
#define MPI_FCPORTPAGE4_PORT_BIOS_INIT_HBA              (0x00000010)
#define MPI_FCPORTPAGE4_PORT_OS_INIT_HBA                (0x00000020)
#define MPI_FCPORTPAGE4_PORT_BIOS_OS_INIT_HBA           (0x00000030)
#define MPI_FCPORTPAGE4_PORT_REMOVABLE_MEDIA            (0x000000C0)
#define MPI_FCPORTPAGE4_PORT_SPINUP_DELAY_MASK          (0x00000F00)


typedef struct _CONFIG_PAGE_FC_PORT_5_ALIAS_INFO
{
    U8      Flags;                                      /* 00h */
    U8      AliasAlpa;                                  /* 01h */
    U16     Reserved;                                   /* 02h */
    U64     AliasWWNN;                                  /* 04h */
    U64     AliasWWPN;                                  /* 0Ch */
} CONFIG_PAGE_FC_PORT_5_ALIAS_INFO,
  MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_5_ALIAS_INFO,
  FcPortPage5AliasInfo_t, MPI_POINTER pFcPortPage5AliasInfo_t;

typedef struct _CONFIG_PAGE_FC_PORT_5
{
    CONFIG_PAGE_HEADER                  Header;         /* 00h */
    CONFIG_PAGE_FC_PORT_5_ALIAS_INFO    AliasInfo;      /* 04h */
} CONFIG_PAGE_FC_PORT_5, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_5,
  FCPortPage5_t, MPI_POINTER pFCPortPage5_t;

#define MPI_FCPORTPAGE5_PAGEVERSION                     (0x02)

#define MPI_FCPORTPAGE5_FLAGS_ALPA_ACQUIRED             (0x01)
#define MPI_FCPORTPAGE5_FLAGS_HARD_ALPA                 (0x02)
#define MPI_FCPORTPAGE5_FLAGS_HARD_WWNN                 (0x04)
#define MPI_FCPORTPAGE5_FLAGS_HARD_WWPN                 (0x08)
#define MPI_FCPORTPAGE5_FLAGS_DISABLE                   (0x10)

typedef struct _CONFIG_PAGE_FC_PORT_6
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Reserved;                   /* 04h */
    U64                     TimeSinceReset;             /* 08h */
    U64                     TxFrames;                   /* 10h */
    U64                     RxFrames;                   /* 18h */
    U64                     TxWords;                    /* 20h */
    U64                     RxWords;                    /* 28h */
    U64                     LipCount;                   /* 30h */
    U64                     NosCount;                   /* 38h */
    U64                     ErrorFrames;                /* 40h */
    U64                     DumpedFrames;               /* 48h */
    U64                     LinkFailureCount;           /* 50h */
    U64                     LossOfSyncCount;            /* 58h */
    U64                     LossOfSignalCount;          /* 60h */
    U64                     PrimativeSeqErrCount;       /* 68h */
    U64                     InvalidTxWordCount;         /* 70h */
    U64                     InvalidCrcCount;            /* 78h */
    U64                     FcpInitiatorIoCount;        /* 80h */
} CONFIG_PAGE_FC_PORT_6, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_6,
  FCPortPage6_t, MPI_POINTER pFCPortPage6_t;

#define MPI_FCPORTPAGE6_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_FC_PORT_7
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Reserved;                   /* 04h */
    U8                      PortSymbolicName[256];      /* 08h */
} CONFIG_PAGE_FC_PORT_7, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_7,
  FCPortPage7_t, MPI_POINTER pFCPortPage7_t;

#define MPI_FCPORTPAGE7_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_FC_PORT_8
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     BitVector[8];               /* 04h */
} CONFIG_PAGE_FC_PORT_8, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_8,
  FCPortPage8_t, MPI_POINTER pFCPortPage8_t;

#define MPI_FCPORTPAGE8_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_FC_PORT_9
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Reserved;                   /* 04h */
    U64                     GlobalWWPN;                 /* 08h */
    U64                     GlobalWWNN;                 /* 10h */
    U32                     UnitType;                   /* 18h */
    U32                     PhysicalPortNumber;         /* 1Ch */
    U32                     NumAttachedNodes;           /* 20h */
    U16                     IPVersion;                  /* 24h */
    U16                     UDPPortNumber;              /* 26h */
    U8                      IPAddress[16];              /* 28h */
    U16                     Reserved1;                  /* 38h */
    U16                     TopologyDiscoveryFlags;     /* 3Ah */
} CONFIG_PAGE_FC_PORT_9, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_9,
  FCPortPage9_t, MPI_POINTER pFCPortPage9_t;

#define MPI_FCPORTPAGE9_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_FC_PORT_10_BASE_SFP_DATA
{
    U8                      Id;                         /* 10h */
    U8                      ExtId;                      /* 11h */
    U8                      Connector;                  /* 12h */
    U8                      Transceiver[8];             /* 13h */
    U8                      Encoding;                   /* 1Bh */
    U8                      BitRate_100mbs;             /* 1Ch */
    U8                      Reserved1;                  /* 1Dh */
    U8                      Length9u_km;                /* 1Eh */
    U8                      Length9u_100m;              /* 1Fh */
    U8                      Length50u_10m;              /* 20h */
    U8                      Length62p5u_10m;            /* 21h */
    U8                      LengthCopper_m;             /* 22h */
    U8                      Reseverved2;                /* 22h */
    U8                      VendorName[16];             /* 24h */
    U8                      Reserved3;                  /* 34h */
    U8                      VendorOUI[3];               /* 35h */
    U8                      VendorPN[16];               /* 38h */
    U8                      VendorRev[4];               /* 48h */
    U16                     Wavelength;                 /* 4Ch */
    U8                      Reserved4;                  /* 4Eh */
    U8                      CC_BASE;                    /* 4Fh */
} CONFIG_PAGE_FC_PORT_10_BASE_SFP_DATA,
  MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_10_BASE_SFP_DATA,
  FCPortPage10BaseSfpData_t, MPI_POINTER pFCPortPage10BaseSfpData_t;

#define MPI_FCPORT10_BASE_ID_UNKNOWN        (0x00)
#define MPI_FCPORT10_BASE_ID_GBIC           (0x01)
#define MPI_FCPORT10_BASE_ID_FIXED          (0x02)
#define MPI_FCPORT10_BASE_ID_SFP            (0x03)
#define MPI_FCPORT10_BASE_ID_SFP_MIN        (0x04)
#define MPI_FCPORT10_BASE_ID_SFP_MAX        (0x7F)
#define MPI_FCPORT10_BASE_ID_VEND_SPEC_MASK (0x80)

#define MPI_FCPORT10_BASE_EXTID_UNKNOWN     (0x00)
#define MPI_FCPORT10_BASE_EXTID_MODDEF1     (0x01)
#define MPI_FCPORT10_BASE_EXTID_MODDEF2     (0x02)
#define MPI_FCPORT10_BASE_EXTID_MODDEF3     (0x03)
#define MPI_FCPORT10_BASE_EXTID_SEEPROM     (0x04)
#define MPI_FCPORT10_BASE_EXTID_MODDEF5     (0x05)
#define MPI_FCPORT10_BASE_EXTID_MODDEF6     (0x06)
#define MPI_FCPORT10_BASE_EXTID_MODDEF7     (0x07)
#define MPI_FCPORT10_BASE_EXTID_VNDSPC_MASK (0x80)

#define MPI_FCPORT10_BASE_CONN_UNKNOWN      (0x00)
#define MPI_FCPORT10_BASE_CONN_SC           (0x01)
#define MPI_FCPORT10_BASE_CONN_COPPER1      (0x02)
#define MPI_FCPORT10_BASE_CONN_COPPER2      (0x03)
#define MPI_FCPORT10_BASE_CONN_BNC_TNC      (0x04)
#define MPI_FCPORT10_BASE_CONN_COAXIAL      (0x05)
#define MPI_FCPORT10_BASE_CONN_FIBERJACK    (0x06)
#define MPI_FCPORT10_BASE_CONN_LC           (0x07)
#define MPI_FCPORT10_BASE_CONN_MT_RJ        (0x08)
#define MPI_FCPORT10_BASE_CONN_MU           (0x09)
#define MPI_FCPORT10_BASE_CONN_SG           (0x0A)
#define MPI_FCPORT10_BASE_CONN_OPT_PIGT     (0x0B)
#define MPI_FCPORT10_BASE_CONN_RSV1_MIN     (0x0C)
#define MPI_FCPORT10_BASE_CONN_RSV1_MAX     (0x1F)
#define MPI_FCPORT10_BASE_CONN_HSSDC_II     (0x20)
#define MPI_FCPORT10_BASE_CONN_CPR_PIGT     (0x21)
#define MPI_FCPORT10_BASE_CONN_RSV2_MIN     (0x22)
#define MPI_FCPORT10_BASE_CONN_RSV2_MAX     (0x7F)
#define MPI_FCPORT10_BASE_CONN_VNDSPC_MASK  (0x80)

#define MPI_FCPORT10_BASE_ENCODE_UNSPEC     (0x00)
#define MPI_FCPORT10_BASE_ENCODE_8B10B      (0x01)
#define MPI_FCPORT10_BASE_ENCODE_4B5B       (0x02)
#define MPI_FCPORT10_BASE_ENCODE_NRZ        (0x03)
#define MPI_FCPORT10_BASE_ENCODE_MANCHESTER (0x04)


typedef struct _CONFIG_PAGE_FC_PORT_10_EXTENDED_SFP_DATA
{
    U8                      Options[2];                 /* 50h */
    U8                      BitRateMax;                 /* 52h */
    U8                      BitRateMin;                 /* 53h */
    U8                      VendorSN[16];               /* 54h */
    U8                      DateCode[8];                /* 64h */
    U8                      DiagMonitoringType;         /* 6Ch */
    U8                      EnhancedOptions;            /* 6Dh */
    U8                      SFF8472Compliance;          /* 6Eh */
    U8                      CC_EXT;                     /* 6Fh */
} CONFIG_PAGE_FC_PORT_10_EXTENDED_SFP_DATA,
  MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_10_EXTENDED_SFP_DATA,
  FCPortPage10ExtendedSfpData_t, MPI_POINTER pFCPortPage10ExtendedSfpData_t;

#define MPI_FCPORT10_EXT_OPTION1_RATESEL    (0x20)
#define MPI_FCPORT10_EXT_OPTION1_TX_DISABLE (0x10)
#define MPI_FCPORT10_EXT_OPTION1_TX_FAULT   (0x08)
#define MPI_FCPORT10_EXT_OPTION1_LOS_INVERT (0x04)
#define MPI_FCPORT10_EXT_OPTION1_LOS        (0x02)


typedef struct _CONFIG_PAGE_FC_PORT_10
{
    CONFIG_PAGE_HEADER                          Header;             /* 00h */
    U8                                          Flags;              /* 04h */
    U8                                          Reserved1;          /* 05h */
    U16                                         Reserved2;          /* 06h */
    U32                                         HwConfig1;          /* 08h */
    U32                                         HwConfig2;          /* 0Ch */
    CONFIG_PAGE_FC_PORT_10_BASE_SFP_DATA        Base;               /* 10h */
    CONFIG_PAGE_FC_PORT_10_EXTENDED_SFP_DATA    Extended;           /* 50h */
    U8                                          VendorSpecific[32]; /* 70h */
} CONFIG_PAGE_FC_PORT_10, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_10,
  FCPortPage10_t, MPI_POINTER pFCPortPage10_t;

#define MPI_FCPORTPAGE10_PAGEVERSION                    (0x01)

/* standard MODDEF pin definitions (from GBIC spec.) */
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_MASK              (0x00000007)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF2                  (0x00000001)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF1                  (0x00000002)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF0                  (0x00000004)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_NOGBIC            (0x00000007)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_CPR_IEEE_CX       (0x00000006)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_COPPER            (0x00000005)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_OPTICAL_LW        (0x00000004)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_SEEPROM           (0x00000003)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_SW_OPTICAL        (0x00000002)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_LX_IEEE_OPT_LW    (0x00000001)
#define MPI_FCPORTPAGE10_FLAGS_MODDEF_SX_IEEE_OPT_SW    (0x00000000)

#define MPI_FCPORTPAGE10_FLAGS_CC_BASE_OK               (0x00000010)
#define MPI_FCPORTPAGE10_FLAGS_CC_EXT_OK                (0x00000020)


/****************************************************************************
*   FC Device Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_FC_DEVICE_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U64                     WWNN;                       /* 04h */
    U64                     WWPN;                       /* 0Ch */
    U32                     PortIdentifier;             /* 14h */
    U8                      Protocol;                   /* 18h */
    U8                      Flags;                      /* 19h */
    U16                     BBCredit;                   /* 1Ah */
    U16                     MaxRxFrameSize;             /* 1Ch */
    U8                      ADISCHardALPA;              /* 1Eh */
    U8                      PortNumber;                 /* 1Fh */
    U8                      FcPhLowestVersion;          /* 20h */
    U8                      FcPhHighestVersion;         /* 21h */
    U8                      CurrentTargetID;            /* 22h */
    U8                      CurrentBus;                 /* 23h */
} CONFIG_PAGE_FC_DEVICE_0, MPI_POINTER PTR_CONFIG_PAGE_FC_DEVICE_0,
  FCDevicePage0_t, MPI_POINTER pFCDevicePage0_t;

#define MPI_FC_DEVICE_PAGE0_PAGEVERSION                 (0x03)

#define MPI_FC_DEVICE_PAGE0_FLAGS_TARGETID_BUS_VALID    (0x01)
#define MPI_FC_DEVICE_PAGE0_FLAGS_PLOGI_INVALID         (0x02)
#define MPI_FC_DEVICE_PAGE0_FLAGS_PRLI_INVALID          (0x04)

#define MPI_FC_DEVICE_PAGE0_PROT_IP                     (0x01)
#define MPI_FC_DEVICE_PAGE0_PROT_FCP_TARGET             (0x02)
#define MPI_FC_DEVICE_PAGE0_PROT_FCP_INITIATOR          (0x04)
#define MPI_FC_DEVICE_PAGE0_PROT_FCP_RETRY              (0x08)

#define MPI_FC_DEVICE_PAGE0_PGAD_PORT_MASK      (MPI_FC_DEVICE_PGAD_PORT_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_FORM_MASK      (MPI_FC_DEVICE_PGAD_FORM_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_FORM_NEXT_DID  (MPI_FC_DEVICE_PGAD_FORM_NEXT_DID)
#define MPI_FC_DEVICE_PAGE0_PGAD_FORM_BUS_TID   (MPI_FC_DEVICE_PGAD_FORM_BUS_TID)
#define MPI_FC_DEVICE_PAGE0_PGAD_DID_MASK       (MPI_FC_DEVICE_PGAD_ND_DID_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_BUS_MASK       (MPI_FC_DEVICE_PGAD_BT_BUS_MASK)
#define MPI_FC_DEVICE_PAGE0_PGAD_BUS_SHIFT      (MPI_FC_DEVICE_PGAD_BT_BUS_SHIFT)
#define MPI_FC_DEVICE_PAGE0_PGAD_TID_MASK       (MPI_FC_DEVICE_PGAD_BT_TID_MASK)

#define MPI_FC_DEVICE_PAGE0_HARD_ALPA_UNKNOWN   (0xFF)

/****************************************************************************
*   RAID Volume Config Pages
****************************************************************************/

typedef struct _RAID_VOL0_PHYS_DISK
{
    U16                         Reserved;               /* 00h */
    U8                          PhysDiskMap;            /* 02h */
    U8                          PhysDiskNum;            /* 03h */
} RAID_VOL0_PHYS_DISK, MPI_POINTER PTR_RAID_VOL0_PHYS_DISK,
  RaidVol0PhysDisk_t, MPI_POINTER pRaidVol0PhysDisk_t;

#define MPI_RAIDVOL0_PHYSDISK_PRIMARY                   (0x01)
#define MPI_RAIDVOL0_PHYSDISK_SECONDARY                 (0x02)

typedef struct _RAID_VOL0_STATUS
{
    U8                          Flags;                  /* 00h */
    U8                          State;                  /* 01h */
    U16                         Reserved;               /* 02h */
} RAID_VOL0_STATUS, MPI_POINTER PTR_RAID_VOL0_STATUS,
  RaidVol0Status_t, MPI_POINTER pRaidVol0Status_t;

/* RAID Volume Page 0 VolumeStatus defines */

#define MPI_RAIDVOL0_STATUS_FLAG_ENABLED                (0x01)
#define MPI_RAIDVOL0_STATUS_FLAG_QUIESCED               (0x02)
#define MPI_RAIDVOL0_STATUS_FLAG_RESYNC_IN_PROGRESS     (0x04)
#define MPI_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE        (0x08)

#define MPI_RAIDVOL0_STATUS_STATE_OPTIMAL               (0x00)
#define MPI_RAIDVOL0_STATUS_STATE_DEGRADED              (0x01)
#define MPI_RAIDVOL0_STATUS_STATE_FAILED                (0x02)

typedef struct _RAID_VOL0_SETTINGS
{
    U16                         Settings;       /* 00h */
    U8                          HotSparePool;   /* 01h */ /* MPI_RAID_HOT_SPARE_POOL_ */
    U8                          Reserved;       /* 02h */
} RAID_VOL0_SETTINGS, MPI_POINTER PTR_RAID_VOL0_SETTINGS,
  RaidVol0Settings, MPI_POINTER pRaidVol0Settings;

/* RAID Volume Page 0 VolumeSettings defines */

#define MPI_RAIDVOL0_SETTING_WRITE_CACHING_ENABLE       (0x0001)
#define MPI_RAIDVOL0_SETTING_OFFLINE_ON_SMART           (0x0002)
#define MPI_RAIDVOL0_SETTING_AUTO_CONFIGURE             (0x0004)
#define MPI_RAIDVOL0_SETTING_PRIORITY_RESYNC            (0x0008)
#define MPI_RAIDVOL0_SETTING_USE_PRODUCT_ID_SUFFIX      (0x0010)
#define MPI_RAIDVOL0_SETTING_USE_DEFAULTS               (0x8000)

/* RAID Volume Page 0 HotSparePool defines, also used in RAID Physical Disk */
#define MPI_RAID_HOT_SPARE_POOL_0                       (0x01)
#define MPI_RAID_HOT_SPARE_POOL_1                       (0x02)
#define MPI_RAID_HOT_SPARE_POOL_2                       (0x04)
#define MPI_RAID_HOT_SPARE_POOL_3                       (0x08)
#define MPI_RAID_HOT_SPARE_POOL_4                       (0x10)
#define MPI_RAID_HOT_SPARE_POOL_5                       (0x20)
#define MPI_RAID_HOT_SPARE_POOL_6                       (0x40)
#define MPI_RAID_HOT_SPARE_POOL_7                       (0x80)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX
#define MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_RAID_VOL_0
{
    CONFIG_PAGE_HEADER      Header;         /* 00h */
    U8                      VolumeID;       /* 04h */
    U8                      VolumeBus;      /* 05h */
    U8                      VolumeIOC;      /* 06h */
    U8                      VolumeType;     /* 07h */ /* MPI_RAID_VOL_TYPE_ */
    RAID_VOL0_STATUS        VolumeStatus;   /* 08h */
    RAID_VOL0_SETTINGS      VolumeSettings; /* 0Ch */
    U32                     MaxLBA;         /* 10h */
    U32                     Reserved1;      /* 14h */
    U32                     StripeSize;     /* 18h */
    U32                     Reserved2;      /* 1Ch */
    U32                     Reserved3;      /* 20h */
    U8                      NumPhysDisks;   /* 24h */
    U8                      DataScrubRate;  /* 25h */
    U8                      ResyncRate;     /* 26h */
    U8                      InactiveStatus; /* 27h */
    RAID_VOL0_PHYS_DISK     PhysDisk[MPI_RAID_VOL_PAGE_0_PHYSDISK_MAX];/* 28h */
} CONFIG_PAGE_RAID_VOL_0, MPI_POINTER PTR_CONFIG_PAGE_RAID_VOL_0,
  RaidVolumePage0_t, MPI_POINTER pRaidVolumePage0_t;

#define MPI_RAIDVOLPAGE0_PAGEVERSION                    (0x01)


/****************************************************************************
*   RAID Physical Disk Config Pages
****************************************************************************/

typedef struct _RAID_PHYS_DISK0_ERROR_DATA
{
    U8                      ErrorCdbByte;               /* 00h */
    U8                      ErrorSenseKey;              /* 01h */
    U16                     Reserved;                   /* 02h */
    U16                     ErrorCount;                 /* 04h */
    U8                      ErrorASC;                   /* 06h */
    U8                      ErrorASCQ;                  /* 07h */
    U16                     SmartCount;                 /* 08h */
    U8                      SmartASC;                   /* 0Ah */
    U8                      SmartASCQ;                  /* 0Bh */
} RAID_PHYS_DISK0_ERROR_DATA, MPI_POINTER PTR_RAID_PHYS_DISK0_ERROR_DATA,
  RaidPhysDisk0ErrorData_t, MPI_POINTER pRaidPhysDisk0ErrorData_t;

typedef struct _RAID_PHYS_DISK_INQUIRY_DATA
{
    U8                          VendorID[8];            /* 00h */
    U8                          ProductID[16];          /* 08h */
    U8                          ProductRevLevel[4];     /* 18h */
    U8                          Info[32];               /* 1Ch */
} RAID_PHYS_DISK0_INQUIRY_DATA, MPI_POINTER PTR_RAID_PHYS_DISK0_INQUIRY_DATA,
  RaidPhysDisk0InquiryData, MPI_POINTER pRaidPhysDisk0InquiryData;

typedef struct _RAID_PHYS_DISK0_SETTINGS
{
    U8              SepID;              /* 00h */
    U8              SepBus;             /* 01h */
    U8              HotSparePool;       /* 02h */ /* MPI_RAID_HOT_SPARE_POOL_ */
    U8              PhysDiskSettings;   /* 03h */
} RAID_PHYS_DISK0_SETTINGS, MPI_POINTER PTR_RAID_PHYS_DISK0_SETTINGS,
  RaidPhysDiskSettings_t, MPI_POINTER pRaidPhysDiskSettings_t;

typedef struct _RAID_PHYS_DISK0_STATUS
{
    U8                              Flags;              /* 00h */
    U8                              State;              /* 01h */
    U16                             Reserved;           /* 02h */
} RAID_PHYS_DISK0_STATUS, MPI_POINTER PTR_RAID_PHYS_DISK0_STATUS,
  RaidPhysDiskStatus_t, MPI_POINTER pRaidPhysDiskStatus_t;

/* RAID Volume 2 IM Physical Disk DiskStatus flags */

#define MPI_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC           (0x01)
#define MPI_PHYSDISK0_STATUS_FLAG_QUIESCED              (0x02)

#define MPI_PHYSDISK0_STATUS_ONLINE                     (0x00)
#define MPI_PHYSDISK0_STATUS_MISSING                    (0x01)
#define MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE             (0x02)
#define MPI_PHYSDISK0_STATUS_FAILED                     (0x03)
#define MPI_PHYSDISK0_STATUS_INITIALIZING               (0x04)
#define MPI_PHYSDISK0_STATUS_OFFLINE_REQUESTED          (0x05)
#define MPI_PHYSDISK0_STATUS_FAILED_REQUESTED           (0x06)
#define MPI_PHYSDISK0_STATUS_OTHER_OFFLINE              (0xFF)

typedef struct _CONFIG_PAGE_RAID_PHYS_DISK_0
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U8                              PhysDiskID;         /* 04h */
    U8                              PhysDiskBus;        /* 05h */
    U8                              PhysDiskIOC;        /* 06h */
    U8                              PhysDiskNum;        /* 07h */
    RAID_PHYS_DISK0_SETTINGS        PhysDiskSettings;   /* 08h */
    U32                             Reserved1;          /* 0Ch */
    U32                             Reserved2;          /* 10h */
    U32                             Reserved3;          /* 14h */
    U8                              DiskIdentifier[16]; /* 18h */
    RAID_PHYS_DISK0_INQUIRY_DATA    InquiryData;        /* 28h */
    RAID_PHYS_DISK0_STATUS          PhysDiskStatus;     /* 64h */
    U32                             MaxLBA;             /* 68h */
    RAID_PHYS_DISK0_ERROR_DATA      ErrorData;          /* 6Ch */
} CONFIG_PAGE_RAID_PHYS_DISK_0, MPI_POINTER PTR_CONFIG_PAGE_RAID_PHYS_DISK_0,
  RaidPhysDiskPage0_t, MPI_POINTER pRaidPhysDiskPage0_t;

#define MPI_RAIDPHYSDISKPAGE0_PAGEVERSION           (0x00)


/****************************************************************************
*   LAN Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_LAN_0
{
    ConfigPageHeader_t      Header;                     /* 00h */
    U16                     TxRxModes;                  /* 04h */
    U16                     Reserved;                   /* 06h */
    U32                     PacketPrePad;               /* 08h */
} CONFIG_PAGE_LAN_0, MPI_POINTER PTR_CONFIG_PAGE_LAN_0,
  LANPage0_t, MPI_POINTER pLANPage0_t;

#define MPI_LAN_PAGE0_PAGEVERSION                       (0x01)

#define MPI_LAN_PAGE0_RETURN_LOOPBACK                   (0x0000)
#define MPI_LAN_PAGE0_SUPPRESS_LOOPBACK                 (0x0001)
#define MPI_LAN_PAGE0_LOOPBACK_MASK                     (0x0001)

typedef struct _CONFIG_PAGE_LAN_1
{
    ConfigPageHeader_t      Header;                     /* 00h */
    U16                     Reserved;                   /* 04h */
    U8                      CurrentDeviceState;         /* 06h */
    U8                      Reserved1;                  /* 07h */
    U32                     MinPacketSize;              /* 08h */
    U32                     MaxPacketSize;              /* 0Ch */
    U32                     HardwareAddressLow;         /* 10h */
    U32                     HardwareAddressHigh;        /* 14h */
    U32                     MaxWireSpeedLow;            /* 18h */
    U32                     MaxWireSpeedHigh;           /* 1Ch */
    U32                     BucketsRemaining;           /* 20h */
    U32                     MaxReplySize;               /* 24h */
    U32                     NegWireSpeedLow;            /* 28h */
    U32                     NegWireSpeedHigh;           /* 2Ch */
} CONFIG_PAGE_LAN_1, MPI_POINTER PTR_CONFIG_PAGE_LAN_1,
  LANPage1_t, MPI_POINTER pLANPage1_t;

#define MPI_LAN_PAGE1_PAGEVERSION                       (0x03)

#define MPI_LAN_PAGE1_DEV_STATE_RESET                   (0x00)
#define MPI_LAN_PAGE1_DEV_STATE_OPERATIONAL             (0x01)

#endif

/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_fc.h,v 1.5 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_FC.H
 *          Title:  MPI Fibre Channel messages and structures
 *  Creation Date:  June 12, 2000
 *
 *    MPI_FC.H Version:  01.02.04
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-12-00  01.00.02  Added _MSG_FC_ABORT_REPLY structure.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *  12-04-00  01.01.02  Added messages for Common Transport Send and
 *                      Primitive Send.
 *  01-09-01  01.01.03  Modifed some of the new flags to have an MPI prefix
 *                      and modified the FcPrimitiveSend flags.
 *  01-25-01  01.01.04  Move InitiatorIndex in LinkServiceRsp reply to a larger
 *                      field.
 *                      Added FC_ABORT_TYPE_CT_SEND_REQUEST and
 *                      FC_ABORT_TYPE_EXLINKSEND_REQUEST for FcAbort request.
 *                      Added MPI_FC_PRIM_SEND_FLAGS_STOP_SEND.
 *  02-20-01  01.01.05  Started using MPI_POINTER.
 *  03-27-01  01.01.06  Added Flags field to MSG_LINK_SERVICE_BUFFER_POST_REPLY
 *                      and defined MPI_LS_BUF_POST_REPLY_FLAG_NO_RSP_NEEDED.
 *                      Added MPI_FC_PRIM_SEND_FLAGS_RESET_LINK define.
 *                      Added structure offset comments.
 *  04-09-01  01.01.07  Added RspLength field to MSG_LINK_SERVICE_RSP_REQUEST.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  09-28-01  01.02.02  Change name of reserved field in
 *                      MSG_LINK_SERVICE_RSP_REPLY.
 *  05-31-02  01.02.03  Adding AliasIndex to FC Direct Access requests.
 *  01-16-04  01.02.04  Added define for MPI_FC_PRIM_SEND_FLAGS_ML_RESET_LINK.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_FC_H
#define MPI_FC_H


/*****************************************************************************
*
*        F C    T a r g e t    M o d e    M e s s a g e s
*
*****************************************************************************/

/****************************************************************************/
/* Link Service Buffer Post messages                                        */
/****************************************************************************/

typedef struct _MSG_LINK_SERVICE_BUFFER_POST_REQUEST
{
    U8                      BufferPostFlags;    /* 00h */
    U8                      BufferCount;        /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved;           /* 04h */
    U8                      Reserved1;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    SGE_TRANS_SIMPLE_UNION  SGL;
} MSG_LINK_SERVICE_BUFFER_POST_REQUEST,
 MPI_POINTER PTR_MSG_LINK_SERVICE_BUFFER_POST_REQUEST,
  LinkServiceBufferPostRequest_t, MPI_POINTER pLinkServiceBufferPostRequest_t;

#define LINK_SERVICE_BUFFER_POST_FLAGS_PORT_MASK (0x01)

typedef struct _WWNFORMAT
{
    U32                     PortNameHigh;       /* 00h */
    U32                     PortNameLow;        /* 04h */
    U32                     NodeNameHigh;       /* 08h */
    U32                     NodeNameLow;        /* 0Ch */
} WWNFORMAT,
  WwnFormat_t;

/* Link Service Buffer Post Reply */
typedef struct _MSG_LINK_SERVICE_BUFFER_POST_REPLY
{
    U8                      Flags;              /* 00h */
    U8                      Reserved;           /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      PortNumber;         /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved2;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     TransferLength;     /* 14h */
    U32                     TransactionContext; /* 18h */
    U32                     Rctl_Did;           /* 1Ch */
    U32                     Csctl_Sid;          /* 20h */
    U32                     Type_Fctl;          /* 24h */
    U16                     SeqCnt;             /* 28h */
    U8                      Dfctl;              /* 2Ah */
    U8                      SeqId;              /* 2Bh */
    U16                     Rxid;               /* 2Ch */
    U16                     Oxid;               /* 2Eh */
    U32                     Parameter;          /* 30h */
    WWNFORMAT               Wwn;                /* 34h */
} MSG_LINK_SERVICE_BUFFER_POST_REPLY, MPI_POINTER PTR_MSG_LINK_SERVICE_BUFFER_POST_REPLY,
  LinkServiceBufferPostReply_t, MPI_POINTER pLinkServiceBufferPostReply_t;

#define MPI_LS_BUF_POST_REPLY_FLAG_NO_RSP_NEEDED    (0x80)

#define MPI_FC_DID_MASK                             (0x00FFFFFF)
#define MPI_FC_DID_SHIFT                            (0)
#define MPI_FC_RCTL_MASK                            (0xFF000000)
#define MPI_FC_RCTL_SHIFT                           (24)
#define MPI_FC_SID_MASK                             (0x00FFFFFF)
#define MPI_FC_SID_SHIFT                            (0)
#define MPI_FC_CSCTL_MASK                           (0xFF000000)
#define MPI_FC_CSCTL_SHIFT                          (24)
#define MPI_FC_FCTL_MASK                            (0x00FFFFFF)
#define MPI_FC_FCTL_SHIFT                           (0)
#define MPI_FC_TYPE_MASK                            (0xFF000000)
#define MPI_FC_TYPE_SHIFT                           (24)

/* obsolete name for the above */
#define FCP_TARGET_DID_MASK                         (0x00FFFFFF)
#define FCP_TARGET_DID_SHIFT                        (0)
#define FCP_TARGET_RCTL_MASK                        (0xFF000000)
#define FCP_TARGET_RCTL_SHIFT                       (24)
#define FCP_TARGET_SID_MASK                         (0x00FFFFFF)
#define FCP_TARGET_SID_SHIFT                        (0)
#define FCP_TARGET_CSCTL_MASK                       (0xFF000000)
#define FCP_TARGET_CSCTL_SHIFT                      (24)
#define FCP_TARGET_FCTL_MASK                        (0x00FFFFFF)
#define FCP_TARGET_FCTL_SHIFT                       (0)
#define FCP_TARGET_TYPE_MASK                        (0xFF000000)
#define FCP_TARGET_TYPE_SHIFT                       (24)


/****************************************************************************/
/* Link Service Response messages                                           */
/****************************************************************************/

typedef struct _MSG_LINK_SERVICE_RSP_REQUEST
{
    U8                      RspFlags;           /* 00h */
    U8                      RspLength;          /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     Rctl_Did;           /* 0Ch */
    U32                     Csctl_Sid;          /* 10h */
    U32                     Type_Fctl;          /* 14h */
    U16                     SeqCnt;             /* 18h */
    U8                      Dfctl;              /* 1Ah */
    U8                      SeqId;              /* 1Bh */
    U16                     Rxid;               /* 1Ch */
    U16                     Oxid;               /* 1Eh */
    U32                     Parameter;          /* 20h */
    SGE_SIMPLE_UNION        SGL;                /* 24h */
} MSG_LINK_SERVICE_RSP_REQUEST, MPI_POINTER PTR_MSG_LINK_SERVICE_RSP_REQUEST,
  LinkServiceRspRequest_t, MPI_POINTER pLinkServiceRspRequest_t;

#define LINK_SERVICE_RSP_FLAGS_IMMEDIATE        (0x80)
#define LINK_SERVICE_RSP_FLAGS_PORT_MASK        (0x01)


/* Link Service Response Reply  */
typedef struct _MSG_LINK_SERVICE_RSP_REPLY
{
    U16                     Reserved;           /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      Reserved_0100_InitiatorIndex; /* 06h */ /* obsolete InitiatorIndex */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     InitiatorIndex;     /* 14h */
} MSG_LINK_SERVICE_RSP_REPLY, MPI_POINTER PTR_MSG_LINK_SERVICE_RSP_REPLY,
  LinkServiceRspReply_t, MPI_POINTER pLinkServiceRspReply_t;


/****************************************************************************/
/* Extended Link Service Send messages                                      */
/****************************************************************************/

typedef struct _MSG_EXLINK_SERVICE_SEND_REQUEST
{
    U8                      SendFlags;          /* 00h */
    U8                      AliasIndex;         /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U32                     MsgFlags_Did;       /* 04h */
    U32                     MsgContext;         /* 08h */
    U32                     ElsCommandCode;     /* 0Ch */
    SGE_SIMPLE_UNION        SGL;                /* 10h */
} MSG_EXLINK_SERVICE_SEND_REQUEST, MPI_POINTER PTR_MSG_EXLINK_SERVICE_SEND_REQUEST,
  ExLinkServiceSendRequest_t, MPI_POINTER pExLinkServiceSendRequest_t;

#define EX_LINK_SERVICE_SEND_DID_MASK           (0x00FFFFFF)
#define EX_LINK_SERVICE_SEND_DID_SHIFT          (0)
#define EX_LINK_SERVICE_SEND_MSGFLAGS_MASK      (0xFF000000)
#define EX_LINK_SERVICE_SEND_MSGFLAGS_SHIFT     (24)


/* Extended Link Service Send Reply */
typedef struct _MSG_EXLINK_SERVICE_SEND_REPLY
{
    U8                      Reserved;           /* 00h */
    U8                      AliasIndex;         /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     ResponseLength;     /* 14h */
} MSG_EXLINK_SERVICE_SEND_REPLY, MPI_POINTER PTR_MSG_EXLINK_SERVICE_SEND_REPLY,
  ExLinkServiceSendReply_t, MPI_POINTER pExLinkServiceSendReply_t;

/****************************************************************************/
/* FC Abort messages                                                        */
/****************************************************************************/

typedef struct _MSG_FC_ABORT_REQUEST
{
    U8                      AbortFlags;                 /* 00h */
    U8                      AbortType;                  /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     TransactionContextToAbort;  /* 0Ch */
} MSG_FC_ABORT_REQUEST, MPI_POINTER PTR_MSG_FC_ABORT_REQUEST,
  FcAbortRequest_t, MPI_POINTER pFcAbortRequest_t;

#define FC_ABORT_FLAG_PORT_MASK                 (0x01)

#define FC_ABORT_TYPE_ALL_FC_BUFFERS            (0x00)
#define FC_ABORT_TYPE_EXACT_FC_BUFFER           (0x01)
#define FC_ABORT_TYPE_CT_SEND_REQUEST           (0x02)
#define FC_ABORT_TYPE_EXLINKSEND_REQUEST        (0x03)

/* FC Abort Reply */
typedef struct _MSG_FC_ABORT_REPLY
{
    U16                     Reserved;           /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
} MSG_FC_ABORT_REPLY, MPI_POINTER PTR_MSG_FC_ABORT_REPLY,
  FcAbortReply_t, MPI_POINTER pFcAbortReply_t;


/****************************************************************************/
/* FC Common Transport Send messages                                        */
/****************************************************************************/

typedef struct _MSG_FC_COMMON_TRANSPORT_SEND_REQUEST
{
    U8                      SendFlags;          /* 00h */
    U8                      AliasIndex;         /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U32                     MsgFlags_Did;       /* 04h */
    U32                     MsgContext;         /* 08h */
    U16                     CTCommandCode;      /* 0Ch */
    U8                      FsType;             /* 0Eh */
    U8                      Reserved1;          /* 0Fh */
    SGE_SIMPLE_UNION        SGL;                /* 10h */
} MSG_FC_COMMON_TRANSPORT_SEND_REQUEST,
 MPI_POINTER PTR_MSG_FC_COMMON_TRANSPORT_SEND_REQUEST,
  FcCommonTransportSendRequest_t, MPI_POINTER pFcCommonTransportSendRequest_t;

#define MPI_FC_CT_SEND_DID_MASK                 (0x00FFFFFF)
#define MPI_FC_CT_SEND_DID_SHIFT                (0)
#define MPI_FC_CT_SEND_MSGFLAGS_MASK            (0xFF000000)
#define MPI_FC_CT_SEND_MSGFLAGS_SHIFT           (24)


/* FC Common Transport Send Reply */
typedef struct _MSG_FC_COMMON_TRANSPORT_SEND_REPLY
{
    U8                      Reserved;           /* 00h */
    U8                      AliasIndex;         /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     ResponseLength;     /* 14h */
} MSG_FC_COMMON_TRANSPORT_SEND_REPLY, MPI_POINTER PTR_MSG_FC_COMMON_TRANSPORT_SEND_REPLY,
  FcCommonTransportSendReply_t, MPI_POINTER pFcCommonTransportSendReply_t;


/****************************************************************************/
/* FC Primitive Send messages                                               */
/****************************************************************************/

typedef struct _MSG_FC_PRIMITIVE_SEND_REQUEST
{
    U8                      SendFlags;          /* 00h */
    U8                      Reserved;           /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      FcPrimitive[4];     /* 0Ch */
} MSG_FC_PRIMITIVE_SEND_REQUEST, MPI_POINTER PTR_MSG_FC_PRIMITIVE_SEND_REQUEST,
  FcPrimitiveSendRequest_t, MPI_POINTER pFcPrimitiveSendRequest_t;

#define MPI_FC_PRIM_SEND_FLAGS_PORT_MASK       (0x01)
#define MPI_FC_PRIM_SEND_FLAGS_ML_RESET_LINK   (0x02)
#define MPI_FC_PRIM_SEND_FLAGS_RESET_LINK      (0x04)
#define MPI_FC_PRIM_SEND_FLAGS_STOP_SEND       (0x08)
#define MPI_FC_PRIM_SEND_FLAGS_SEND_ONCE       (0x10)
#define MPI_FC_PRIM_SEND_FLAGS_SEND_AROUND     (0x20)
#define MPI_FC_PRIM_SEND_FLAGS_UNTIL_FULL      (0x40)
#define MPI_FC_PRIM_SEND_FLAGS_FOREVER         (0x80)

/* FC Primitive Send Reply */
typedef struct _MSG_FC_PRIMITIVE_SEND_REPLY
{
    U8                      SendFlags;          /* 00h */
    U8                      Reserved;           /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved1;          /* 04h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
} MSG_FC_PRIMITIVE_SEND_REPLY, MPI_POINTER PTR_MSG_FC_PRIMITIVE_SEND_REPLY,
  FcPrimitiveSendReply_t, MPI_POINTER pFcPrimitiveSendReply_t;

#endif

/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_init.h,v 1.6 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_INIT.H
 *          Title:  MPI initiator mode messages and structures
 *  Creation Date:  June 8, 2000
 *
 *    MPI_INIT.H Version:  01.02.07
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  05-24-00  00.10.02  Added SenseBufferLength to _MSG_SCSI_IO_REPLY.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-08-00  01.00.02  Added MPI_SCSI_RSP_INFO_ definitions.
 *  11-02-00  01.01.01  Original release for post 1.0 work.
 *  12-04-00  01.01.02  Added MPI_SCSIIO_CONTROL_NO_DISCONNECT.
 *  02-20-01  01.01.03  Started using MPI_POINTER.
 *  03-27-01  01.01.04  Added structure offset comments.
 *  04-10-01  01.01.05  Added new MsgFlag for MSG_SCSI_TASK_MGMT.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  08-29-01  01.02.02  Added MPI_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET.
 *                      Added MPI_SCSI_STATE_QUEUE_TAG_REJECTED for
 *                      MSG_SCSI_IO_REPLY.
 *  09-28-01  01.02.03  Added structures and defines for SCSI Enclosure
 *                      Processor messages.
 *  10-04-01  01.02.04  Added defines for SEP request Action field.
 *  05-31-02  01.02.05  Added MPI_SCSIIO_MSGFLGS_CMD_DETERMINES_DATA_DIR define
 *                      for SCSI IO requests.
 *  11-15-02  01.02.06  Added special extended SCSI Status defines for FCP.
 *  06-26-03  01.02.07  Added MPI_SCSI_STATUS_FCPEXT_UNASSIGNED define.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_INIT_H
#define MPI_INIT_H


/*****************************************************************************
*
*               S C S I    I n i t i a t o r    M e s s a g e s
*
*****************************************************************************/

/****************************************************************************/
/*  SCSI IO messages and assocaited structures                              */
/****************************************************************************/

typedef struct _MSG_SCSI_IO_REQUEST
{
    U8                      TargetID;           /* 00h */
    U8                      Bus;                /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U8                      CDBLength;          /* 04h */
    U8                      SenseBufferLength;  /* 05h */
    U8                      Reserved;           /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      LUN[8];             /* 0Ch */
    U32                     Control;            /* 14h */
    U8                      CDB[16];            /* 18h */
    U32                     DataLength;         /* 28h */
    U32                     SenseBufferLowAddr; /* 2Ch */
    SGE_IO_UNION            SGL;                /* 30h */
} MSG_SCSI_IO_REQUEST, MPI_POINTER PTR_MSG_SCSI_IO_REQUEST,
  SCSIIORequest_t, MPI_POINTER pSCSIIORequest_t;


/* SCSI IO MsgFlags bits */

#define MPI_SCSIIO_MSGFLGS_SENSE_WIDTH              (0x01)
#define MPI_SCSIIO_MSGFLGS_SENSE_WIDTH_32           (0x00)
#define MPI_SCSIIO_MSGFLGS_SENSE_WIDTH_64           (0x01)
#define MPI_SCSIIO_MSGFLGS_SENSE_LOCATION           (0x02)
#define MPI_SCSIIO_MSGFLGS_SENSE_LOC_HOST           (0x00)
#define MPI_SCSIIO_MSGFLGS_SENSE_LOC_IOC            (0x02)
#define MPI_SCSIIO_MSGFLGS_CMD_DETERMINES_DATA_DIR  (0x04)

/* SCSI IO LUN fields */

#define MPI_SCSIIO_LUN_FIRST_LEVEL_ADDRESSING   (0x0000FFFF)
#define MPI_SCSIIO_LUN_SECOND_LEVEL_ADDRESSING  (0xFFFF0000)
#define MPI_SCSIIO_LUN_THIRD_LEVEL_ADDRESSING   (0x0000FFFF)
#define MPI_SCSIIO_LUN_FOURTH_LEVEL_ADDRESSING  (0xFFFF0000)
#define MPI_SCSIIO_LUN_LEVEL_1_WORD             (0xFF00)
#define MPI_SCSIIO_LUN_LEVEL_1_DWORD            (0x0000FF00)

/* SCSI IO Control bits */

#define MPI_SCSIIO_CONTROL_DATADIRECTION_MASK   (0x03000000)
#define MPI_SCSIIO_CONTROL_NODATATRANSFER       (0x00000000)
#define MPI_SCSIIO_CONTROL_WRITE                (0x01000000)
#define MPI_SCSIIO_CONTROL_READ                 (0x02000000)

#define MPI_SCSIIO_CONTROL_ADDCDBLEN_MASK       (0x3C000000)
#define MPI_SCSIIO_CONTROL_ADDCDBLEN_SHIFT      (26)

#define MPI_SCSIIO_CONTROL_TASKATTRIBUTE_MASK   (0x00000700)
#define MPI_SCSIIO_CONTROL_SIMPLEQ              (0x00000000)
#define MPI_SCSIIO_CONTROL_HEADOFQ              (0x00000100)
#define MPI_SCSIIO_CONTROL_ORDEREDQ             (0x00000200)
#define MPI_SCSIIO_CONTROL_ACAQ                 (0x00000400)
#define MPI_SCSIIO_CONTROL_UNTAGGED             (0x00000500)
#define MPI_SCSIIO_CONTROL_NO_DISCONNECT        (0x00000700)

#define MPI_SCSIIO_CONTROL_TASKMANAGE_MASK      (0x00FF0000)
#define MPI_SCSIIO_CONTROL_OBSOLETE             (0x00800000)
#define MPI_SCSIIO_CONTROL_CLEAR_ACA_RSV        (0x00400000)
#define MPI_SCSIIO_CONTROL_TARGET_RESET         (0x00200000)
#define MPI_SCSIIO_CONTROL_LUN_RESET_RSV        (0x00100000)
#define MPI_SCSIIO_CONTROL_RESERVED             (0x00080000)
#define MPI_SCSIIO_CONTROL_CLR_TASK_SET_RSV     (0x00040000)
#define MPI_SCSIIO_CONTROL_ABORT_TASK_SET       (0x00020000)
#define MPI_SCSIIO_CONTROL_RESERVED2            (0x00010000)


/* SCSI IO reply structure */
typedef struct _MSG_SCSI_IO_REPLY
{
    U8                      TargetID;           /* 00h */
    U8                      Bus;                /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      CDBLength;          /* 04h */
    U8                      SenseBufferLength;  /* 05h */
    U8                      Reserved;           /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      SCSIStatus;         /* 0Ch */
    U8                      SCSIState;          /* 0Dh */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     TransferCount;      /* 14h */
    U32                     SenseCount;         /* 18h */
    U32                     ResponseInfo;       /* 1Ch */
} MSG_SCSI_IO_REPLY, MPI_POINTER PTR_MSG_SCSI_IO_REPLY,
  SCSIIOReply_t, MPI_POINTER pSCSIIOReply_t;


/* SCSI IO Reply SCSIStatus values (SAM-2 status codes) */

#define MPI_SCSI_STATUS_SUCCESS                 (0x00)
#define MPI_SCSI_STATUS_CHECK_CONDITION         (0x02)
#define MPI_SCSI_STATUS_CONDITION_MET           (0x04)
#define MPI_SCSI_STATUS_BUSY                    (0x08)
#define MPI_SCSI_STATUS_INTERMEDIATE            (0x10)
#define MPI_SCSI_STATUS_INTERMEDIATE_CONDMET    (0x14)
#define MPI_SCSI_STATUS_RESERVATION_CONFLICT    (0x18)
#define MPI_SCSI_STATUS_COMMAND_TERMINATED      (0x22)
#define MPI_SCSI_STATUS_TASK_SET_FULL           (0x28)
#define MPI_SCSI_STATUS_ACA_ACTIVE              (0x30)

#define MPI_SCSI_STATUS_FCPEXT_DEVICE_LOGGED_OUT    (0x80)
#define MPI_SCSI_STATUS_FCPEXT_NO_LINK              (0x81)
#define MPI_SCSI_STATUS_FCPEXT_UNASSIGNED           (0x82)


/* SCSI IO Reply SCSIState values */

#define MPI_SCSI_STATE_AUTOSENSE_VALID          (0x01)
#define MPI_SCSI_STATE_AUTOSENSE_FAILED         (0x02)
#define MPI_SCSI_STATE_NO_SCSI_STATUS           (0x04)
#define MPI_SCSI_STATE_TERMINATED               (0x08)
#define MPI_SCSI_STATE_RESPONSE_INFO_VALID      (0x10)
#define MPI_SCSI_STATE_QUEUE_TAG_REJECTED       (0x20)

/* SCSI IO Reply ResponseInfo values */
/* (FCP-1 RSP_CODE values and SPI-3 Packetized Failure codes) */

#define MPI_SCSI_RSP_INFO_FUNCTION_COMPLETE     (0x00000000)
#define MPI_SCSI_RSP_INFO_FCP_BURST_LEN_ERROR   (0x01000000)
#define MPI_SCSI_RSP_INFO_CMND_FIELDS_INVALID   (0x02000000)
#define MPI_SCSI_RSP_INFO_FCP_DATA_RO_ERROR     (0x03000000)
#define MPI_SCSI_RSP_INFO_TASK_MGMT_UNSUPPORTED (0x04000000)
#define MPI_SCSI_RSP_INFO_TASK_MGMT_FAILED      (0x05000000)
#define MPI_SCSI_RSP_INFO_SPI_LQ_INVALID_TYPE   (0x06000000)


/****************************************************************************/
/*  SCSI Task Management messages                                           */
/****************************************************************************/

typedef struct _MSG_SCSI_TASK_MGMT
{
    U8                      TargetID;           /* 00h */
    U8                      Bus;                /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U8                      Reserved;           /* 04h */
    U8                      TaskType;           /* 05h */
    U8                      Reserved1;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      LUN[8];             /* 0Ch */
    U32                     Reserved2[7];       /* 14h */
    U32                     TaskMsgContext;     /* 30h */
} MSG_SCSI_TASK_MGMT, MPI_POINTER PTR_SCSI_TASK_MGMT,
  SCSITaskMgmt_t, MPI_POINTER pSCSITaskMgmt_t;

/* TaskType values */

#define MPI_SCSITASKMGMT_TASKTYPE_ABORT_TASK            (0x01)
#define MPI_SCSITASKMGMT_TASKTYPE_ABRT_TASK_SET         (0x02)
#define MPI_SCSITASKMGMT_TASKTYPE_TARGET_RESET          (0x03)
#define MPI_SCSITASKMGMT_TASKTYPE_RESET_BUS             (0x04)
#define MPI_SCSITASKMGMT_TASKTYPE_LOGICAL_UNIT_RESET    (0x05)

/* MsgFlags bits */
#define MPI_SCSITASKMGMT_MSGFLAGS_TARGET_RESET_OPTION   (0x00)
#define MPI_SCSITASKMGMT_MSGFLAGS_LIP_RESET_OPTION      (0x02)
#define MPI_SCSITASKMGMT_MSGFLAGS_LIPRESET_RESET_OPTION (0x04)

/* SCSI Task Management Reply */
typedef struct _MSG_SCSI_TASK_MGMT_REPLY
{
    U8                      TargetID;           /* 00h */
    U8                      Bus;                /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      Reserved;           /* 04h */
    U8                      TaskType;           /* 05h */
    U8                      Reserved1;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      Reserved2[2];       /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     TerminationCount;   /* 14h */
} MSG_SCSI_TASK_MGMT_REPLY, MPI_POINTER PTR_MSG_SCSI_TASK_MGMT_REPLY,
  SCSITaskMgmtReply_t, MPI_POINTER pSCSITaskMgmtReply_t;


/****************************************************************************/
/*  SCSI Enclosure Processor messages                                       */
/****************************************************************************/

typedef struct _MSG_SEP_REQUEST
{
    U8                      TargetID;           /* 00h */
    U8                      Bus;                /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U8                      Action;             /* 04h */
    U8                      Reserved1;          /* 05h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     SlotStatus;         /* 0Ch */
} MSG_SEP_REQUEST, MPI_POINTER PTR_MSG_SEP_REQUEST,
  SEPRequest_t, MPI_POINTER pSEPRequest_t;

/* Action defines */
#define MPI_SEP_REQ_ACTION_WRITE_STATUS                 (0x00)
#define MPI_SEP_REQ_ACTION_READ_STATUS                  (0x01)

/* SlotStatus bits for MSG_SEP_REQUEST */
#define MPI_SEP_REQ_SLOTSTATUS_NO_ERROR                 (0x00000001)
#define MPI_SEP_REQ_SLOTSTATUS_DEV_FAULTY               (0x00000002)
#define MPI_SEP_REQ_SLOTSTATUS_DEV_REBUILDING           (0x00000004)
#define MPI_SEP_REQ_SLOTSTATUS_IN_FAILED_ARRAY          (0x00000008)
#define MPI_SEP_REQ_SLOTSTATUS_IN_CRITICAL_ARRAY        (0x00000010)
#define MPI_SEP_REQ_SLOTSTATUS_PARITY_CHECK             (0x00000020)
#define MPI_SEP_REQ_SLOTSTATUS_PREDICTED_FAULT          (0x00000040)
#define MPI_SEP_REQ_SLOTSTATUS_UNCONFIGURED             (0x00000080)
#define MPI_SEP_REQ_SLOTSTATUS_HOT_SPARE                (0x00000100)
#define MPI_SEP_REQ_SLOTSTATUS_REBUILD_STOPPED          (0x00000200)
#define MPI_SEP_REQ_SLOTSTATUS_IDENTIFY_REQUEST         (0x00020000)
#define MPI_SEP_REQ_SLOTSTATUS_REQUEST_REMOVE           (0x00040000)
#define MPI_SEP_REQ_SLOTSTATUS_REQUEST_INSERT           (0x00080000)
#define MPI_SEP_REQ_SLOTSTATUS_DO_NOT_MOVE              (0x00400000)
#define MPI_SEP_REQ_SLOTSTATUS_B_ENABLE_BYPASS          (0x04000000)
#define MPI_SEP_REQ_SLOTSTATUS_A_ENABLE_BYPASS          (0x08000000)
#define MPI_SEP_REQ_SLOTSTATUS_DEV_OFF                  (0x10000000)
#define MPI_SEP_REQ_SLOTSTATUS_SWAP_RESET               (0x80000000)


typedef struct _MSG_SEP_REPLY
{
    U8                      TargetID;           /* 00h */
    U8                      Bus;                /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      Action;             /* 04h */
    U8                      Reserved1;          /* 05h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     SlotStatus;         /* 14h */
} MSG_SEP_REPLY, MPI_POINTER PTR_MSG_SEP_REPLY,
  SEPReply_t, MPI_POINTER pSEPReply_t;

/* SlotStatus bits for MSG_SEP_REPLY */
#define MPI_SEP_REPLY_SLOTSTATUS_NO_ERROR               (0x00000001)
#define MPI_SEP_REPLY_SLOTSTATUS_DEV_FAULTY             (0x00000002)
#define MPI_SEP_REPLY_SLOTSTATUS_DEV_REBUILDING         (0x00000004)
#define MPI_SEP_REPLY_SLOTSTATUS_IN_FAILED_ARRAY        (0x00000008)
#define MPI_SEP_REPLY_SLOTSTATUS_IN_CRITICAL_ARRAY      (0x00000010)
#define MPI_SEP_REPLY_SLOTSTATUS_PARITY_CHECK           (0x00000020)
#define MPI_SEP_REPLY_SLOTSTATUS_PREDICTED_FAULT        (0x00000040)
#define MPI_SEP_REPLY_SLOTSTATUS_UNCONFIGURED           (0x00000080)
#define MPI_SEP_REPLY_SLOTSTATUS_HOT_SPARE              (0x00000100)
#define MPI_SEP_REPLY_SLOTSTATUS_REBUILD_STOPPED        (0x00000200)
#define MPI_SEP_REPLY_SLOTSTATUS_REPORT                 (0x00010000)
#define MPI_SEP_REPLY_SLOTSTATUS_IDENTIFY_REQUEST       (0x00020000)
#define MPI_SEP_REPLY_SLOTSTATUS_REMOVE_READY           (0x00040000)
#define MPI_SEP_REPLY_SLOTSTATUS_INSERT_READY           (0x00080000)
#define MPI_SEP_REPLY_SLOTSTATUS_DO_NOT_REMOVE          (0x00400000)
#define MPI_SEP_REPLY_SLOTSTATUS_B_BYPASS_ENABLED       (0x01000000)
#define MPI_SEP_REPLY_SLOTSTATUS_A_BYPASS_ENABLED       (0x02000000)
#define MPI_SEP_REPLY_SLOTSTATUS_B_ENABLE_BYPASS        (0x04000000)
#define MPI_SEP_REPLY_SLOTSTATUS_A_ENABLE_BYPASS        (0x08000000)
#define MPI_SEP_REPLY_SLOTSTATUS_DEV_OFF                (0x10000000)
#define MPI_SEP_REPLY_SLOTSTATUS_FAULT_SENSED           (0x40000000)
#define MPI_SEP_REPLY_SLOTSTATUS_SWAPPED                (0x80000000)

#endif
/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_ioc.h,v 1.6 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_IOC.H
 *          Title:  MPI IOC, Port, Event, FW Download, and FW Upload messages
 *  Creation Date:  August 11, 2000
 *
 *    MPI_IOC.H Version:  01.02.08
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  05-24-00  00.10.02  Added _MSG_IOC_INIT_REPLY structure.
 *  06-06-00  01.00.01  Added CurReplyFrameSize field to _MSG_IOC_FACTS_REPLY.
 *  06-12-00  01.00.02  Added _MSG_PORT_ENABLE_REPLY structure.
 *                      Added _MSG_EVENT_ACK_REPLY structure.
 *                      Added _MSG_FW_DOWNLOAD_REPLY structure.
 *                      Added _MSG_TOOLBOX_REPLY structure.
 *  06-30-00  01.00.03  Added MaxLanBuckets to _PORT_FACT_REPLY structure.
 *  07-27-00  01.00.04  Added _EVENT_DATA structure definitions for _SCSI,
 *                      _LINK_STATUS, _LOOP_STATE and _LOGOUT.
 *  08-11-00  01.00.05  Switched positions of MsgLength and Function fields in
 *                      _MSG_EVENT_ACK_REPLY structure to match specification.
 *  11-02-00  01.01.01  Original release for post 1.0 work.
 *                      Added a value for Manufacturer to WhoInit.
 *  12-04-00  01.01.02  Modified IOCFacts reply, added FWUpload messages, and
 *                      removed toolbox message.
 *  01-09-01  01.01.03  Added event enabled and disabled defines.
 *                      Added structures for FwHeader and DataHeader.
 *                      Added ImageType to FwUpload reply.
 *  02-20-01  01.01.04  Started using MPI_POINTER.
 *  02-27-01  01.01.05  Added event for RAID status change and its event data.
 *                      Added IocNumber field to MSG_IOC_FACTS_REPLY.
 *  03-27-01  01.01.06  Added defines for ProductId field of MPI_FW_HEADER.
 *                      Added structure offset comments.
 *  04-09-01  01.01.07  Added structure EVENT_DATA_EVENT_CHANGE.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *                      New format for FWVersion and ProductId in
 *                      MSG_IOC_FACTS_REPLY and MPI_FW_HEADER.
 *  08-31-01  01.02.02  Addded event MPI_EVENT_SCSI_DEVICE_STATUS_CHANGE and
 *                      related structure and defines.
 *                      Added event MPI_EVENT_ON_BUS_TIMER_EXPIRED.
 *                      Added MPI_IOCINIT_FLAGS_DISCARD_FW_IMAGE.
 *                      Replaced a reserved field in MSG_IOC_FACTS_REPLY with
 *                      IOCExceptions and changed DataImageSize to reserved.
 *                      Added MPI_FW_DOWNLOAD_ITYPE_NVSTORE_DATA and
 *                      MPI_FW_UPLOAD_ITYPE_NVDATA.
 *  09-28-01  01.02.03  Modified Event Data for Integrated RAID.
 *  11-01-01  01.02.04  Added defines for MPI_EXT_IMAGE_HEADER ImageType field.
 *  03-14-02  01.02.05  Added HeaderVersion field to MSG_IOC_FACTS_REPLY.
 *  05-31-02  01.02.06  Added define for
 *                      MPI_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID.
 *                      Added AliasIndex to EVENT_DATA_LOGOUT structure.
 *  04-01-03  01.02.07  Added defines for MPI_FW_HEADER_SIGNATURE_.
 *  06-26-03  01.02.08  Added new values to the product family defines.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_IOC_H
#define MPI_IOC_H


/*****************************************************************************
*
*               I O C    M e s s a g e s
*
*****************************************************************************/

/****************************************************************************/
/*  IOCInit message                                                         */
/****************************************************************************/

typedef struct _MSG_IOC_INIT
{
    U8                      WhoInit;                    /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Flags;                      /* 04h */
    U8                      MaxDevices;                 /* 05h */
    U8                      MaxBuses;                   /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     ReplyFrameSize;             /* 0Ch */
    U8                      Reserved1[2];               /* 0Eh */
    U32                     HostMfaHighAddr;            /* 10h */
    U32                     SenseBufferHighAddr;        /* 14h */
} MSG_IOC_INIT, MPI_POINTER PTR_MSG_IOC_INIT,
  IOCInit_t, MPI_POINTER pIOCInit_t;

/* WhoInit values */
#define MPI_WHOINIT_NO_ONE                      (0x00)
#define MPI_WHOINIT_SYSTEM_BIOS                 (0x01)
#define MPI_WHOINIT_ROM_BIOS                    (0x02)
#define MPI_WHOINIT_PCI_PEER                    (0x03)
#define MPI_WHOINIT_HOST_DRIVER                 (0x04)
#define MPI_WHOINIT_MANUFACTURER                (0x05)

/* Flags values */
#define MPI_IOCINIT_FLAGS_DISCARD_FW_IMAGE      (0x01)

typedef struct _MSG_IOC_INIT_REPLY
{
    U8                      WhoInit;                    /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Flags;                      /* 04h */
    U8                      MaxDevices;                 /* 05h */
    U8                      MaxBuses;                   /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved2;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
} MSG_IOC_INIT_REPLY, MPI_POINTER PTR_MSG_IOC_INIT_REPLY,
  IOCInitReply_t, MPI_POINTER pIOCInitReply_t;



/****************************************************************************/
/*  IOC Facts message                                                       */
/****************************************************************************/

typedef struct _MSG_IOC_FACTS
{
    U8                      Reserved[2];                /* 00h */
    U8                      ChainOffset;                /* 01h */
    U8                      Function;                   /* 02h */
    U8                      Reserved1[3];               /* 03h */
    U8                      MsgFlags;                   /* 04h */
    U32                     MsgContext;                 /* 08h */
} MSG_IOC_FACTS, MPI_POINTER PTR_IOC_FACTS,
  IOCFacts_t, MPI_POINTER pIOCFacts_t;

typedef struct _MPI_FW_VERSION_STRUCT
{
    U8                      Dev;                        /* 00h */
    U8                      Unit;                       /* 01h */
    U8                      Minor;                      /* 02h */
    U8                      Major;                      /* 03h */
} MPI_FW_VERSION_STRUCT;

typedef union _MPI_FW_VERSION
{
    MPI_FW_VERSION_STRUCT   Struct;
    U32                     Word;
} MPI_FW_VERSION;

/* IOC Facts Reply */
typedef struct _MSG_IOC_FACTS_REPLY
{
    U16                     MsgVersion;                 /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     HeaderVersion;              /* 04h */
    U8                      IOCNumber;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     IOCExceptions;              /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U8                      MaxChainDepth;              /* 14h */
    U8                      WhoInit;                    /* 15h */
    U8                      BlockSize;                  /* 16h */
    U8                      Flags;                      /* 17h */
    U16                     ReplyQueueDepth;            /* 18h */
    U16                     RequestFrameSize;           /* 1Ah */
    U16                     Reserved_0101_FWVersion;    /* 1Ch */ /* obsolete 16-bit FWVersion */
    U16                     ProductID;                  /* 1Eh */
    U32                     CurrentHostMfaHighAddr;     /* 20h */
    U16                     GlobalCredits;              /* 24h */
    U8                      NumberOfPorts;              /* 26h */
    U8                      EventState;                 /* 27h */
    U32                     CurrentSenseBufferHighAddr; /* 28h */
    U16                     CurReplyFrameSize;          /* 2Ch */
    U8                      MaxDevices;                 /* 2Eh */
    U8                      MaxBuses;                   /* 2Fh */
    U32                     FWImageSize;                /* 30h */
    U32                     Reserved4;                  /* 34h */
    MPI_FW_VERSION          FWVersion;                  /* 38h */
} MSG_IOC_FACTS_REPLY, MPI_POINTER PTR_MSG_IOC_FACTS_REPLY,
  IOCFactsReply_t, MPI_POINTER pIOCFactsReply_t;

#define MPI_IOCFACTS_MSGVERSION_MAJOR_MASK          (0xFF00)
#define MPI_IOCFACTS_MSGVERSION_MINOR_MASK          (0x00FF)

#define MPI_IOCFACTS_HEADERVERSION_UNIT_MASK        (0xFF00)
#define MPI_IOCFACTS_HEADERVERSION_DEV_MASK         (0x00FF)

#define MPI_IOCFACTS_EXCEPT_CONFIG_CHECKSUM_FAIL    (0x0001)
#define MPI_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID     (0x0002)

#define MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT         (0x01)

#define MPI_IOCFACTS_EVENTSTATE_DISABLED            (0x00)
#define MPI_IOCFACTS_EVENTSTATE_ENABLED             (0x01)



/*****************************************************************************
*
*               P o r t    M e s s a g e s
*
*****************************************************************************/

/****************************************************************************/
/*  Port Facts message and Reply                                            */
/****************************************************************************/

typedef struct _MSG_PORT_FACTS
{
     U8                     Reserved[2];                /* 00h */
     U8                     ChainOffset;                /* 02h */
     U8                     Function;                   /* 03h */
     U8                     Reserved1[2];               /* 04h */
     U8                     PortNumber;                 /* 06h */
     U8                     MsgFlags;                   /* 07h */
     U32                    MsgContext;                 /* 08h */
} MSG_PORT_FACTS, MPI_POINTER PTR_MSG_PORT_FACTS,
  PortFacts_t, MPI_POINTER pPortFacts_t;

typedef struct _MSG_PORT_FACTS_REPLY
{
     U16                    Reserved;                   /* 00h */
     U8                     MsgLength;                  /* 02h */
     U8                     Function;                   /* 03h */
     U16                    Reserved1;                  /* 04h */
     U8                     PortNumber;                 /* 06h */
     U8                     MsgFlags;                   /* 07h */
     U32                    MsgContext;                 /* 08h */
     U16                    Reserved2;                  /* 0Ch */
     U16                    IOCStatus;                  /* 0Eh */
     U32                    IOCLogInfo;                 /* 10h */
     U8                     Reserved3;                  /* 14h */
     U8                     PortType;                   /* 15h */
     U16                    MaxDevices;                 /* 16h */
     U16                    PortSCSIID;                 /* 18h */
     U16                    ProtocolFlags;              /* 1Ah */
     U16                    MaxPostedCmdBuffers;        /* 1Ch */
     U16                    MaxPersistentIDs;           /* 1Eh */
     U16                    MaxLanBuckets;              /* 20h */
     U16                    Reserved4;                  /* 22h */
     U32                    Reserved5;                  /* 24h */
} MSG_PORT_FACTS_REPLY, MPI_POINTER PTR_MSG_PORT_FACTS_REPLY,
  PortFactsReply_t, MPI_POINTER pPortFactsReply_t;


/* PortTypes values */

#define MPI_PORTFACTS_PORTTYPE_INACTIVE         (0x00)
#define MPI_PORTFACTS_PORTTYPE_SCSI             (0x01)
#define MPI_PORTFACTS_PORTTYPE_FC               (0x10)

/* ProtocolFlags values */

#define MPI_PORTFACTS_PROTOCOL_LOGBUSADDR       (0x01)
#define MPI_PORTFACTS_PROTOCOL_LAN              (0x02)
#define MPI_PORTFACTS_PROTOCOL_TARGET           (0x04)
#define MPI_PORTFACTS_PROTOCOL_INITIATOR        (0x08)


/****************************************************************************/
/*  Port Enable Message                                                     */
/****************************************************************************/

typedef struct _MSG_PORT_ENABLE
{
    U8                      Reserved[2];                /* 00h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[2];               /* 04h */
    U8                      PortNumber;                 /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
} MSG_PORT_ENABLE, MPI_POINTER PTR_MSG_PORT_ENABLE,
  PortEnable_t, MPI_POINTER pPortEnable_t;

typedef struct _MSG_PORT_ENABLE_REPLY
{
    U8                      Reserved[2];                /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[2];               /* 04h */
    U8                      PortNumber;                 /* 05h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved2;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
} MSG_PORT_ENABLE_REPLY, MPI_POINTER PTR_MSG_PORT_ENABLE_REPLY,
  PortEnableReply_t, MPI_POINTER pPortEnableReply_t;


/*****************************************************************************
*
*               E v e n t    M e s s a g e s
*
*****************************************************************************/

/****************************************************************************/
/*  Event Notification messages                                             */
/****************************************************************************/

typedef struct _MSG_EVENT_NOTIFY
{
    U8                      Switch;                     /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
} MSG_EVENT_NOTIFY, MPI_POINTER PTR_MSG_EVENT_NOTIFY,
  EventNotification_t, MPI_POINTER pEventNotification_t;

/* Event Notification Reply */

typedef struct _MSG_EVENT_NOTIFY_REPLY
{
     U16                    EventDataLength;            /* 00h */
     U8                     MsgLength;                  /* 02h */
     U8                     Function;                   /* 03h */
     U8                     Reserved1[2];               /* 04h */
     U8                     AckRequired;                /* 06h */
     U8                     MsgFlags;                   /* 07h */
     U32                    MsgContext;                 /* 08h */
     U8                     Reserved2[2];               /* 0Ch */
     U16                    IOCStatus;                  /* 0Eh */
     U32                    IOCLogInfo;                 /* 10h */
     U32                    Event;                      /* 14h */
     U32                    EventContext;               /* 18h */
     U32                    Data[1];                    /* 1Ch */
} MSG_EVENT_NOTIFY_REPLY, MPI_POINTER PTR_MSG_EVENT_NOTIFY_REPLY,
  EventNotificationReply_t, MPI_POINTER pEventNotificationReply_t;

/* Event Acknowledge */

typedef struct _MSG_EVENT_ACK
{
    U8                      Reserved[2];                /* 00h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     Event;                      /* 0Ch */
    U32                     EventContext;               /* 10h */
} MSG_EVENT_ACK, MPI_POINTER PTR_MSG_EVENT_ACK,
  EventAck_t, MPI_POINTER pEventAck_t;

typedef struct _MSG_EVENT_ACK_REPLY
{
    U8                      Reserved[2];                /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved2;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
} MSG_EVENT_ACK_REPLY, MPI_POINTER PTR_MSG_EVENT_ACK_REPLY,
  EventAckReply_t, MPI_POINTER pEventAckReply_t;

/* Switch */

#define MPI_EVENT_NOTIFICATION_SWITCH_OFF   (0x00)
#define MPI_EVENT_NOTIFICATION_SWITCH_ON    (0x01)

/* Event */

#define MPI_EVENT_NONE                      (0x00000000)
#define MPI_EVENT_LOG_DATA                  (0x00000001)
#define MPI_EVENT_STATE_CHANGE              (0x00000002)
#define MPI_EVENT_UNIT_ATTENTION            (0x00000003)
#define MPI_EVENT_IOC_BUS_RESET             (0x00000004)
#define MPI_EVENT_EXT_BUS_RESET             (0x00000005)
#define MPI_EVENT_RESCAN                    (0x00000006)
#define MPI_EVENT_LINK_STATUS_CHANGE        (0x00000007)
#define MPI_EVENT_LOOP_STATE_CHANGE         (0x00000008)
#define MPI_EVENT_LOGOUT                    (0x00000009)
#define MPI_EVENT_EVENT_CHANGE              (0x0000000A)
#define MPI_EVENT_INTEGRATED_RAID           (0x0000000B)
#define MPI_EVENT_SCSI_DEVICE_STATUS_CHANGE (0x0000000C)
#define MPI_EVENT_ON_BUS_TIMER_EXPIRED      (0x0000000D)

/* AckRequired field values */

#define MPI_EVENT_NOTIFICATION_ACK_NOT_REQUIRED (0x00)
#define MPI_EVENT_NOTIFICATION_ACK_REQUIRED     (0x01)

/* EventChange Event data */

typedef struct _EVENT_DATA_EVENT_CHANGE
{
    U8                      EventState;                 /* 00h */
    U8                      Reserved;                   /* 01h */
    U16                     Reserved1;                  /* 02h */
} EVENT_DATA_EVENT_CHANGE, MPI_POINTER PTR_EVENT_DATA_EVENT_CHANGE,
  EventDataEventChange_t, MPI_POINTER pEventDataEventChange_t;

/* SCSI Event data for Port, Bus and Device forms */

typedef struct _EVENT_DATA_SCSI
{
    U8                      TargetID;                   /* 00h */
    U8                      BusPort;                    /* 01h */
    U16                     Reserved;                   /* 02h */
} EVENT_DATA_SCSI, MPI_POINTER PTR_EVENT_DATA_SCSI,
  EventDataScsi_t, MPI_POINTER pEventDataScsi_t;

/* SCSI Device Status Change Event data */

typedef struct _EVENT_DATA_SCSI_DEVICE_STATUS_CHANGE
{
    U8                      TargetID;                   /* 00h */
    U8                      Bus;                        /* 01h */
    U8                      ReasonCode;                 /* 02h */
    U8                      LUN;                        /* 03h */
    U8                      ASC;                        /* 04h */
    U8                      ASCQ;                       /* 05h */
    U16                     Reserved;                   /* 06h */
} EVENT_DATA_SCSI_DEVICE_STATUS_CHANGE,
  MPI_POINTER PTR_EVENT_DATA_SCSI_DEVICE_STATUS_CHANGE,
  MpiEventDataScsiDeviceStatusChange_t,
  MPI_POINTER pMpiEventDataScsiDeviceStatusChange_t;

/* MPI SCSI Device Status Change Event data ReasonCode values */
#define MPI_EVENT_SCSI_DEV_STAT_RC_ADDED                (0x03)
#define MPI_EVENT_SCSI_DEV_STAT_RC_NOT_RESPONDING       (0x04)
#define MPI_EVENT_SCSI_DEV_STAT_RC_SMART_DATA           (0x05)

/* MPI Link Status Change Event data */

typedef struct _EVENT_DATA_LINK_STATUS
{
    U8                      State;                      /* 00h */
    U8                      Reserved;                   /* 01h */
    U16                     Reserved1;                  /* 02h */
    U8                      Reserved2;                  /* 04h */
    U8                      Port;                       /* 05h */
    U16                     Reserved3;                  /* 06h */
} EVENT_DATA_LINK_STATUS, MPI_POINTER PTR_EVENT_DATA_LINK_STATUS,
  EventDataLinkStatus_t, MPI_POINTER pEventDataLinkStatus_t;

#define MPI_EVENT_LINK_STATUS_FAILURE       (0x00000000)
#define MPI_EVENT_LINK_STATUS_ACTIVE        (0x00000001)

/* MPI Loop State Change Event data */

typedef struct _EVENT_DATA_LOOP_STATE
{
    U8                      Character4;                 /* 00h */
    U8                      Character3;                 /* 01h */
    U8                      Type;                       /* 02h */
    U8                      Reserved;                   /* 03h */
    U8                      Reserved1;                  /* 04h */
    U8                      Port;                       /* 05h */
    U16                     Reserved2;                  /* 06h */
} EVENT_DATA_LOOP_STATE, MPI_POINTER PTR_EVENT_DATA_LOOP_STATE,
  EventDataLoopState_t, MPI_POINTER pEventDataLoopState_t;

#define MPI_EVENT_LOOP_STATE_CHANGE_LIP     (0x0001)
#define MPI_EVENT_LOOP_STATE_CHANGE_LPE     (0x0002)
#define MPI_EVENT_LOOP_STATE_CHANGE_LPB     (0x0003)

/* MPI LOGOUT Event data */

typedef struct _EVENT_DATA_LOGOUT
{
    U32                     NPortID;                    /* 00h */
    U8                      AliasIndex;                 /* 04h */
    U8                      Port;                       /* 05h */
    U16                     Reserved1;                  /* 06h */
} EVENT_DATA_LOGOUT, MPI_POINTER PTR_EVENT_DATA_LOGOUT,
  EventDataLogout_t, MPI_POINTER pEventDataLogout_t;

#define MPI_EVENT_LOGOUT_ALL_ALIASES        (0xFF)


/* MPI Integrated RAID Event data */

typedef struct _EVENT_DATA_RAID
{
    U8                      VolumeID;                   /* 00h */
    U8                      VolumeBus;                  /* 01h */
    U8                      ReasonCode;                 /* 02h */
    U8                      PhysDiskNum;                /* 03h */
    U8                      ASC;                        /* 04h */
    U8                      ASCQ;                       /* 05h */
    U16                     Reserved;                   /* 06h */
    U32                     SettingsStatus;             /* 08h */
} EVENT_DATA_RAID, MPI_POINTER PTR_EVENT_DATA_RAID,
  MpiEventDataRaid_t, MPI_POINTER pMpiEventDataRaid_t;

/* MPI Integrated RAID Event data ReasonCode values */
#define MPI_EVENT_RAID_RC_VOLUME_CREATED                (0x00)
#define MPI_EVENT_RAID_RC_VOLUME_DELETED                (0x01)
#define MPI_EVENT_RAID_RC_VOLUME_SETTINGS_CHANGED       (0x02)
#define MPI_EVENT_RAID_RC_VOLUME_STATUS_CHANGED         (0x03)
#define MPI_EVENT_RAID_RC_VOLUME_PHYSDISK_CHANGED       (0x04)
#define MPI_EVENT_RAID_RC_PHYSDISK_CREATED              (0x05)
#define MPI_EVENT_RAID_RC_PHYSDISK_DELETED              (0x06)
#define MPI_EVENT_RAID_RC_PHYSDISK_SETTINGS_CHANGED     (0x07)
#define MPI_EVENT_RAID_RC_PHYSDISK_STATUS_CHANGED       (0x08)
#define MPI_EVENT_RAID_RC_DOMAIN_VAL_NEEDED             (0x09)
#define MPI_EVENT_RAID_RC_SMART_DATA                    (0x0A)
#define MPI_EVENT_RAID_RC_REPLACE_ACTION_STARTED        (0x0B)


/*****************************************************************************
*
*               F i r m w a r e    L o a d    M e s s a g e s
*
*****************************************************************************/

/****************************************************************************/
/*  Firmware Download message and associated structures                     */
/****************************************************************************/

typedef struct _MSG_FW_DOWNLOAD
{
    U8                      ImageType;                  /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    SGE_MPI_UNION           SGL;                        /* 0Ch */
} MSG_FW_DOWNLOAD, MPI_POINTER PTR_MSG_FW_DOWNLOAD,
  FWDownload_t, MPI_POINTER pFWDownload_t;

#define MPI_FW_DOWNLOAD_ITYPE_RESERVED      (0x00)
#define MPI_FW_DOWNLOAD_ITYPE_FW            (0x01)
#define MPI_FW_DOWNLOAD_ITYPE_BIOS          (0x02)
#define MPI_FW_DOWNLOAD_ITYPE_NVDATA        (0x03)


typedef struct _FWDownloadTCSGE
{
    U8                      Reserved;                   /* 00h */
    U8                      ContextSize;                /* 01h */
    U8                      DetailsLength;              /* 02h */
    U8                      Flags;                      /* 03h */
    U32                     Reserved_0100_Checksum;     /* 04h */ /* obsolete Checksum */
    U32                     ImageOffset;                /* 08h */
    U32                     ImageSize;                  /* 0Ch */
} FW_DOWNLOAD_TCSGE, MPI_POINTER PTR_FW_DOWNLOAD_TCSGE,
  FWDownloadTCSGE_t, MPI_POINTER pFWDownloadTCSGE_t;

/* Firmware Download reply */
typedef struct _MSG_FW_DOWNLOAD_REPLY
{
    U8                      ImageType;                  /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved2;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
} MSG_FW_DOWNLOAD_REPLY, MPI_POINTER PTR_MSG_FW_DOWNLOAD_REPLY,
  FWDownloadReply_t, MPI_POINTER pFWDownloadReply_t;


/****************************************************************************/
/*  Firmware Upload message and associated structures                       */
/****************************************************************************/

typedef struct _MSG_FW_UPLOAD
{
    U8                      ImageType;                  /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    SGE_MPI_UNION           SGL;                        /* 0Ch */
} MSG_FW_UPLOAD, MPI_POINTER PTR_MSG_FW_UPLOAD,
  FWUpload_t, MPI_POINTER pFWUpload_t;

#define MPI_FW_UPLOAD_ITYPE_FW_IOC_MEM      (0x00)
#define MPI_FW_UPLOAD_ITYPE_FW_FLASH        (0x01)
#define MPI_FW_UPLOAD_ITYPE_BIOS_FLASH      (0x02)
#define MPI_FW_UPLOAD_ITYPE_NVDATA          (0x03)

typedef struct _FWUploadTCSGE
{
    U8                      Reserved;                   /* 00h */
    U8                      ContextSize;                /* 01h */
    U8                      DetailsLength;              /* 02h */
    U8                      Flags;                      /* 03h */
    U32                     Reserved1;                  /* 04h */
    U32                     ImageOffset;                /* 08h */
    U32                     ImageSize;                  /* 0Ch */
} FW_UPLOAD_TCSGE, MPI_POINTER PTR_FW_UPLOAD_TCSGE,
  FWUploadTCSGE_t, MPI_POINTER pFWUploadTCSGE_t;

/* Firmware Upload reply */
typedef struct _MSG_FW_UPLOAD_REPLY
{
    U8                      ImageType;                  /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      Reserved1[3];               /* 04h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved2;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     ActualImageSize;            /* 14h */
} MSG_FW_UPLOAD_REPLY, MPI_POINTER PTR_MSG_FW_UPLOAD_REPLY,
  FWUploadReply_t, MPI_POINTER pFWUploadReply_t;


typedef struct _MPI_FW_HEADER
{
    U32                     ArmBranchInstruction0;      /* 00h */
    U32                     Signature0;                 /* 04h */
    U32                     Signature1;                 /* 08h */
    U32                     Signature2;                 /* 0Ch */
    U32                     ArmBranchInstruction1;      /* 10h */
    U32                     ArmBranchInstruction2;      /* 14h */
    U32                     Reserved;                   /* 18h */
    U32                     Checksum;                   /* 1Ch */
    U16                     VendorId;                   /* 20h */
    U16                     ProductId;                  /* 22h */
    MPI_FW_VERSION          FWVersion;                  /* 24h */
    U32                     SeqCodeVersion;             /* 28h */
    U32                     ImageSize;                  /* 2Ch */
    U32                     NextImageHeaderOffset;      /* 30h */
    U32                     LoadStartAddress;           /* 34h */
    U32                     IopResetVectorValue;        /* 38h */
    U32                     IopResetRegAddr;            /* 3Ch */
    U32                     VersionNameWhat;            /* 40h */
    U8                      VersionName[32];            /* 44h */
    U32                     VendorNameWhat;             /* 64h */
    U8                      VendorName[32];             /* 68h */
} MPI_FW_HEADER, MPI_POINTER PTR_MPI_FW_HEADER,
  MpiFwHeader_t, MPI_POINTER pMpiFwHeader_t;

#define MPI_FW_HEADER_WHAT_SIGNATURE        (0x29232840)

/* defines for using the ProductId field */
#define MPI_FW_HEADER_PID_TYPE_MASK             (0xF000)
#define MPI_FW_HEADER_PID_TYPE_SCSI             (0x0000)
#define MPI_FW_HEADER_PID_TYPE_FC               (0x1000)

#define MPI_FW_HEADER_SIGNATURE_0               (0x5AEAA55A)
#define MPI_FW_HEADER_SIGNATURE_1               (0xA55AEAA5)
#define MPI_FW_HEADER_SIGNATURE_2               (0x5AA55AEA)

#define MPI_FW_HEADER_PID_PROD_MASK                     (0x0F00)
#define MPI_FW_HEADER_PID_PROD_INITIATOR_SCSI           (0x0100)
#define MPI_FW_HEADER_PID_PROD_TARGET_INITIATOR_SCSI    (0x0200)
#define MPI_FW_HEADER_PID_PROD_TARGET_SCSI              (0x0300)
#define MPI_FW_HEADER_PID_PROD_IM_SCSI                  (0x0400)
#define MPI_FW_HEADER_PID_PROD_IS_SCSI                  (0x0500)
#define MPI_FW_HEADER_PID_PROD_CTX_SCSI                 (0x0600)

#define MPI_FW_HEADER_PID_FAMILY_MASK           (0x00FF)
#define MPI_FW_HEADER_PID_FAMILY_1030A0_SCSI    (0x0001)
#define MPI_FW_HEADER_PID_FAMILY_1030B0_SCSI    (0x0002)
#define MPI_FW_HEADER_PID_FAMILY_1030B1_SCSI    (0x0003)
#define MPI_FW_HEADER_PID_FAMILY_1030C0_SCSI    (0x0004)
#define MPI_FW_HEADER_PID_FAMILY_1020A0_SCSI    (0x0005)
#define MPI_FW_HEADER_PID_FAMILY_1020B0_SCSI    (0x0006)
#define MPI_FW_HEADER_PID_FAMILY_1020B1_SCSI    (0x0007)
#define MPI_FW_HEADER_PID_FAMILY_1020C0_SCSI    (0x0008)
#define MPI_FW_HEADER_PID_FAMILY_1035A0_SCSI    (0x0009)
#define MPI_FW_HEADER_PID_FAMILY_1035B0_SCSI    (0x000A)
#define MPI_FW_HEADER_PID_FAMILY_1030TA0_SCSI   (0x000B)
#define MPI_FW_HEADER_PID_FAMILY_1020TA0_SCSI   (0x000C)
#define MPI_FW_HEADER_PID_FAMILY_909_FC         (0x0000)
#define MPI_FW_HEADER_PID_FAMILY_919_FC         (0x0001)
#define MPI_FW_HEADER_PID_FAMILY_919X_FC        (0x0002)

typedef struct _MPI_EXT_IMAGE_HEADER
{
    U8                      ImageType;                  /* 00h */
    U8                      Reserved;                   /* 01h */
    U16                     Reserved1;                  /* 02h */
    U32                     Checksum;                   /* 04h */
    U32                     ImageSize;                  /* 08h */
    U32                     NextImageHeaderOffset;      /* 0Ch */
    U32                     LoadStartAddress;           /* 10h */
    U32                     Reserved2;                  /* 14h */
} MPI_EXT_IMAGE_HEADER, MPI_POINTER PTR_MPI_EXT_IMAGE_HEADER,
  MpiExtImageHeader_t, MPI_POINTER pMpiExtImageHeader_t;

/* defines for the ImageType field */
#define MPI_EXT_IMAGE_TYPE_UNSPECIFIED          (0x00)
#define MPI_EXT_IMAGE_TYPE_FW                   (0x01)
#define MPI_EXT_IMAGE_TYPE_NVDATA               (0x03)

#endif
/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_lan.h,v 1.5 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_LAN.H
 *          Title:  MPI LAN messages and structures
 *  Creation Date:  June 30, 2000
 *
 *    MPI_LAN.H Version:  01.02.01
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  05-24-00  00.10.02  Added LANStatus field to _MSG_LAN_SEND_REPLY.
 *                      Added LANStatus field to _MSG_LAN_RECEIVE_POST_REPLY.
 *                      Moved ListCount field in _MSG_LAN_RECEIVE_POST_REPLY.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-12-00  01.00.02  Added MPI_ to BUCKETSTATUS_ definitions.
 *  06-22-00  01.00.03  Major changes to match new LAN definition in 1.0 spec.
 *  06-30-00  01.00.04  Added Context Reply definitions per revised proposal.
 *                      Changed transaction context usage to bucket/buffer.
 *  07-05-00  01.00.05  Removed LAN_RECEIVE_POST_BUCKET_CONTEXT_MASK definition
 *                      to lan private header file
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *  02-20-01  01.01.02  Started using MPI_POINTER.
 *  03-27-01  01.01.03  Added structure offset comments.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_LAN_H
#define MPI_LAN_H


/******************************************************************************
*
*               L A N    M e s s a g e s
*
*******************************************************************************/

/* LANSend messages */

typedef struct _MSG_LAN_SEND_REQUEST
{
    U16                     Reserved;           /* 00h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      PortNumber;         /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    SGE_MPI_UNION           SG_List[1];         /* 0Ch */
} MSG_LAN_SEND_REQUEST, MPI_POINTER PTR_MSG_LAN_SEND_REQUEST,
  LANSendRequest_t, MPI_POINTER pLANSendRequest_t;


typedef struct _MSG_LAN_SEND_REPLY
{
    U16                     Reserved;           /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      Reserved2;          /* 04h */
    U8                      NumberOfContexts;   /* 05h */
    U8                      PortNumber;         /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     BufferContext;      /* 14h */
} MSG_LAN_SEND_REPLY, MPI_POINTER PTR_MSG_LAN_SEND_REPLY,
  LANSendReply_t, MPI_POINTER pLANSendReply_t;


/* LANReceivePost */

typedef struct _MSG_LAN_RECEIVE_POST_REQUEST
{
    U16                     Reserved;           /* 00h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      PortNumber;         /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     BucketCount;        /* 0Ch */
    SGE_MPI_UNION           SG_List[1];         /* 10h */
} MSG_LAN_RECEIVE_POST_REQUEST, MPI_POINTER PTR_MSG_LAN_RECEIVE_POST_REQUEST,
  LANReceivePostRequest_t, MPI_POINTER pLANReceivePostRequest_t;


typedef struct _MSG_LAN_RECEIVE_POST_REPLY
{
    U16                     Reserved;           /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      Reserved2;          /* 04h */
    U8                      NumberOfContexts;   /* 05h */
    U8                      PortNumber;         /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     BucketsRemaining;   /* 14h */
    U32                     PacketOffset;       /* 18h */
    U32                     PacketLength;       /* 1Ch */
    U32                     BucketContext[1];   /* 20h */
} MSG_LAN_RECEIVE_POST_REPLY, MPI_POINTER PTR_MSG_LAN_RECEIVE_POST_REPLY,
  LANReceivePostReply_t, MPI_POINTER pLANReceivePostReply_t;


/* LANReset */

typedef struct _MSG_LAN_RESET_REQUEST
{
    U16                     Reserved;           /* 00h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      PortNumber;         /* 05h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
} MSG_LAN_RESET_REQUEST, MPI_POINTER PTR_MSG_LAN_RESET_REQUEST,
  LANResetRequest_t, MPI_POINTER pLANResetRequest_t;


typedef struct _MSG_LAN_RESET_REPLY
{
    U16                     Reserved;           /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      PortNumber;         /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     Reserved3;          /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
} MSG_LAN_RESET_REPLY, MPI_POINTER PTR_MSG_LAN_RESET_REPLY,
  LANResetReply_t, MPI_POINTER pLANResetReply_t;


/****************************************************************************/
/* LAN Context Reply defines and macros                                     */
/****************************************************************************/

#define LAN_REPLY_PACKET_LENGTH_MASK            (0x0000FFFF)
#define LAN_REPLY_PACKET_LENGTH_SHIFT           (0)
#define LAN_REPLY_BUCKET_CONTEXT_MASK           (0x07FF0000)
#define LAN_REPLY_BUCKET_CONTEXT_SHIFT          (16)
#define LAN_REPLY_BUFFER_CONTEXT_MASK           (0x07FFFFFF)
#define LAN_REPLY_BUFFER_CONTEXT_SHIFT          (0)
#define LAN_REPLY_FORM_MASK                     (0x18000000)
#define LAN_REPLY_FORM_RECEIVE_SINGLE           (0x00)
#define LAN_REPLY_FORM_RECEIVE_MULTIPLE         (0x01)
#define LAN_REPLY_FORM_SEND_SINGLE              (0x02)
#define LAN_REPLY_FORM_MESSAGE_CONTEXT          (0x03)
#define LAN_REPLY_FORM_SHIFT                    (27)

#define GET_LAN_PACKET_LENGTH(x)    (((x) & LAN_REPLY_PACKET_LENGTH_MASK)   \
                                        >> LAN_REPLY_PACKET_LENGTH_SHIFT)

#define SET_LAN_PACKET_LENGTH(x, lth)                                       \
            ((x) = ((x) & ~LAN_REPLY_PACKET_LENGTH_MASK) |                  \
                            (((lth) << LAN_REPLY_PACKET_LENGTH_SHIFT) &     \
                                        LAN_REPLY_PACKET_LENGTH_MASK))

#define GET_LAN_BUCKET_CONTEXT(x)   (((x) & LAN_REPLY_BUCKET_CONTEXT_MASK)  \
                                        >> LAN_REPLY_BUCKET_CONTEXT_SHIFT)

#define SET_LAN_BUCKET_CONTEXT(x, ctx)                                      \
            ((x) = ((x) & ~LAN_REPLY_BUCKET_CONTEXT_MASK) |                 \
                            (((ctx) << LAN_REPLY_BUCKET_CONTEXT_SHIFT) &    \
                                        LAN_REPLY_BUCKET_CONTEXT_MASK))

#define GET_LAN_BUFFER_CONTEXT(x)   (((x) & LAN_REPLY_BUFFER_CONTEXT_MASK)  \
                                        >> LAN_REPLY_BUFFER_CONTEXT_SHIFT)

#define SET_LAN_BUFFER_CONTEXT(x, ctx)                                      \
            ((x) = ((x) & ~LAN_REPLY_BUFFER_CONTEXT_MASK) |                 \
                            (((ctx) << LAN_REPLY_BUFFER_CONTEXT_SHIFT) &    \
                                        LAN_REPLY_BUFFER_CONTEXT_MASK))

#define GET_LAN_FORM(x)             (((x) & LAN_REPLY_FORM_MASK)            \
                                        >> LAN_REPLY_FORM_SHIFT)

#define SET_LAN_FORM(x, frm)                                                \
            ((x) = ((x) & ~LAN_REPLY_FORM_MASK) |                           \
                            (((frm) << LAN_REPLY_FORM_SHIFT) &              \
                                        LAN_REPLY_FORM_MASK))


/****************************************************************************/
/* LAN Current Device State defines                                         */
/****************************************************************************/

#define MPI_LAN_DEVICE_STATE_RESET                     (0x00)
#define MPI_LAN_DEVICE_STATE_OPERATIONAL               (0x01)


/****************************************************************************/
/* LAN Loopback defines                                                     */
/****************************************************************************/

#define MPI_LAN_TX_MODES_ENABLE_LOOPBACK_SUPPRESSION   (0x01)

#endif

/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_raid.h,v 1.6 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_RAID.H
 *          Title:  MPI RAID message and structures
 *  Creation Date:  February 27, 2001
 *
 *    MPI_RAID.H Version:  01.02.09
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  02-27-01  01.01.01  Original release for this file.
 *  03-27-01  01.01.02  Added structure offset comments.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  09-28-01  01.02.02  Major rework for MPI v1.2 Integrated RAID changes.
 *  10-04-01  01.02.03  Added ActionData defines for
 *                      MPI_RAID_ACTION_DELETE_VOLUME action.
 *  11-01-01  01.02.04  Added define for MPI_RAID_ACTION_ADATA_DO_NOT_SYNC.
 *  03-14-02  01.02.05  Added define for MPI_RAID_ACTION_ADATA_LOW_LEVEL_INIT.
 *  05-07-02  01.02.06  Added define for MPI_RAID_ACTION_ACTIVATE_VOLUME,
 *                      MPI_RAID_ACTION_INACTIVATE_VOLUME, and
 *                      MPI_RAID_ACTION_ADATA_INACTIVATE_ALL.
 *  07-12-02  01.02.07  Added structures for Mailbox request and reply.
 *  11-15-02  01.02.08  Added missing MsgContext field to MSG_MAILBOX_REQUEST.
 *  04-01-03  01.02.09  New action data option flag for
 *                      MPI_RAID_ACTION_DELETE_VOLUME.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_RAID_H
#define MPI_RAID_H


/******************************************************************************
*
*        R A I D    M e s s a g e s
*
*******************************************************************************/


/****************************************************************************/
/* RAID Volume Request                                                      */
/****************************************************************************/

typedef struct _MSG_RAID_ACTION
{
    U8                      Action;             /* 00h */
    U8                      Reserved1;          /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U8                      VolumeID;           /* 04h */
    U8                      VolumeBus;          /* 05h */
    U8                      PhysDiskNum;        /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U32                     Reserved2;          /* 0Ch */
    U32                     ActionDataWord;     /* 10h */
    SGE_SIMPLE_UNION        ActionDataSGE;      /* 14h */
} MSG_RAID_ACTION_REQUEST, MPI_POINTER PTR_MSG_RAID_ACTION_REQUEST,
  MpiRaidActionRequest_t , MPI_POINTER pMpiRaidActionRequest_t;


/* RAID Action request Action values */

#define MPI_RAID_ACTION_STATUS                      (0x00)
#define MPI_RAID_ACTION_INDICATOR_STRUCT            (0x01)
#define MPI_RAID_ACTION_CREATE_VOLUME               (0x02)
#define MPI_RAID_ACTION_DELETE_VOLUME               (0x03)
#define MPI_RAID_ACTION_DISABLE_VOLUME              (0x04)
#define MPI_RAID_ACTION_ENABLE_VOLUME               (0x05)
#define MPI_RAID_ACTION_QUIESCE_PHYS_IO             (0x06)
#define MPI_RAID_ACTION_ENABLE_PHYS_IO              (0x07)
#define MPI_RAID_ACTION_CHANGE_VOLUME_SETTINGS      (0x08)
#define MPI_RAID_ACTION_PHYSDISK_OFFLINE            (0x0A)
#define MPI_RAID_ACTION_PHYSDISK_ONLINE             (0x0B)
#define MPI_RAID_ACTION_CHANGE_PHYSDISK_SETTINGS    (0x0C)
#define MPI_RAID_ACTION_CREATE_PHYSDISK             (0x0D)
#define MPI_RAID_ACTION_DELETE_PHYSDISK             (0x0E)
#define MPI_RAID_ACTION_FAIL_PHYSDISK               (0x0F)
#define MPI_RAID_ACTION_REPLACE_PHYSDISK            (0x10)
#define MPI_RAID_ACTION_ACTIVATE_VOLUME             (0x11)
#define MPI_RAID_ACTION_INACTIVATE_VOLUME           (0x12)
#define MPI_RAID_ACTION_SET_RESYNC_RATE             (0x13)
#define MPI_RAID_ACTION_SET_DATA_SCRUB_RATE         (0x14)

/* ActionDataWord defines for use with MPI_RAID_ACTION_CREATE_VOLUME action */
#define MPI_RAID_ACTION_ADATA_DO_NOT_SYNC           (0x00000001)
#define MPI_RAID_ACTION_ADATA_LOW_LEVEL_INIT        (0x00000002)

/* ActionDataWord defines for use with MPI_RAID_ACTION_DELETE_VOLUME action */
#define MPI_RAID_ACTION_ADATA_KEEP_PHYS_DISKS       (0x00000000)
#define MPI_RAID_ACTION_ADATA_DEL_PHYS_DISKS        (0x00000001)

#define MPI_RAID_ACTION_ADATA_KEEP_LBA0             (0x00000000)
#define MPI_RAID_ACTION_ADATA_ZERO_LBA0             (0x00000002)

/* ActionDataWord defines for use with MPI_RAID_ACTION_ACTIVATE_VOLUME action */
#define MPI_RAID_ACTION_ADATA_INACTIVATE_ALL        (0x00000001)


/* RAID Action reply message */

typedef struct _MSG_RAID_ACTION_REPLY
{
    U8                      Action;             /* 00h */
    U8                      Reserved;           /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      VolumeID;           /* 04h */
    U8                      VolumeBus;          /* 05h */
    U8                      PhysDiskNum;        /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     ActionStatus;       /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     VolumeStatus;       /* 14h */
    U32                     ActionData;         /* 18h */
} MSG_RAID_ACTION_REPLY, MPI_POINTER PTR_MSG_RAID_ACTION_REPLY,
  MpiRaidActionReply_t, MPI_POINTER pMpiRaidActionReply_t;


/* RAID Volume reply ActionStatus values */

#define MPI_RAID_ACTION_ASTATUS_SUCCESS             (0x0000)
#define MPI_RAID_ACTION_ASTATUS_INVALID_ACTION      (0x0001)
#define MPI_RAID_ACTION_ASTATUS_FAILURE             (0x0002)
#define MPI_RAID_ACTION_ASTATUS_IN_PROGRESS         (0x0003)


/* RAID Volume reply RAID Volume Indicator structure */

typedef struct _MPI_RAID_VOL_INDICATOR
{
    U64                     TotalBlocks;        /* 00h */
    U64                     BlocksRemaining;    /* 08h */
} MPI_RAID_VOL_INDICATOR, MPI_POINTER PTR_MPI_RAID_VOL_INDICATOR,
  MpiRaidVolIndicator_t, MPI_POINTER pMpiRaidVolIndicator_t;


/****************************************************************************/
/* SCSI IO RAID Passthrough Request                                         */
/****************************************************************************/

typedef struct _MSG_SCSI_IO_RAID_PT_REQUEST
{
    U8                      PhysDiskNum;        /* 00h */
    U8                      Reserved1;          /* 01h */
    U8                      ChainOffset;        /* 02h */
    U8                      Function;           /* 03h */
    U8                      CDBLength;          /* 04h */
    U8                      SenseBufferLength;  /* 05h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      LUN[8];             /* 0Ch */
    U32                     Control;            /* 14h */
    U8                      CDB[16];            /* 18h */
    U32                     DataLength;         /* 28h */
    U32                     SenseBufferLowAddr; /* 2Ch */
    SGE_IO_UNION            SGL;                /* 30h */
} MSG_SCSI_IO_RAID_PT_REQUEST, MPI_POINTER PTR_MSG_SCSI_IO_RAID_PT_REQUEST,
  SCSIIORaidPassthroughRequest_t, MPI_POINTER pSCSIIORaidPassthroughRequest_t;


/* SCSI IO RAID Passthrough reply structure */

typedef struct _MSG_SCSI_IO_RAID_PT_REPLY
{
    U8                      PhysDiskNum;        /* 00h */
    U8                      Reserved1;          /* 01h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U8                      CDBLength;          /* 04h */
    U8                      SenseBufferLength;  /* 05h */
    U8                      Reserved2;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U8                      SCSIStatus;         /* 0Ch */
    U8                      SCSIState;          /* 0Dh */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     TransferCount;      /* 14h */
    U32                     SenseCount;         /* 18h */
    U32                     ResponseInfo;       /* 1Ch */
} MSG_SCSI_IO_RAID_PT_REPLY, MPI_POINTER PTR_MSG_SCSI_IO_RAID_PT_REPLY,
  SCSIIORaidPassthroughReply_t, MPI_POINTER pSCSIIORaidPassthroughReply_t;


/****************************************************************************/
/* Mailbox reqeust structure */
/****************************************************************************/

typedef struct _MSG_MAILBOX_REQUEST
{
    U16                     Reserved1;
    U8                      ChainOffset;
    U8                      Function;
    U16                     Reserved2;
    U8                      Reserved3;
    U8                      MsgFlags;
    U32                     MsgContext;
    U8                      Command[10];
    U16                     Reserved4;
    SGE_IO_UNION            SGL;
} MSG_MAILBOX_REQUEST, MPI_POINTER PTR_MSG_MAILBOX_REQUEST,
  MailboxRequest_t, MPI_POINTER pMailboxRequest_t;


/* Mailbox reply structure */
typedef struct _MSG_MAILBOX_REPLY
{
    U16                     Reserved1;          /* 00h */
    U8                      MsgLength;          /* 02h */
    U8                      Function;           /* 03h */
    U16                     Reserved2;          /* 04h */
    U8                      Reserved3;          /* 06h */
    U8                      MsgFlags;           /* 07h */
    U32                     MsgContext;         /* 08h */
    U16                     MailboxStatus;      /* 0Ch */
    U16                     IOCStatus;          /* 0Eh */
    U32                     IOCLogInfo;         /* 10h */
    U32                     Reserved4;          /* 14h */
} MSG_MAILBOX_REPLY, MPI_POINTER PTR_MSG_MAILBOX_REPLY,
  MailboxReply_t, MPI_POINTER pMailboxReply_t;

#endif



/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/mpt/mpilib/mpi_targ.h,v 1.5 2005/07/10 15:05:39 scottl Exp $ */
/*-
 * Copyright (c) 2000-2005, LSI Logic Corporation and its contributors.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon including
 *    a substantially similar Disclaimer requirement for further binary
 *    redistribution.
 * 3. Neither the name of the LSI Logic Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF THE COPYRIGHT
 * OWNER OR CONTRIBUTOR IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 *           Name:  MPI_TARG.H
 *          Title:  MPI Target mode messages and structures
 *  Creation Date:  June 22, 2000
 *
 *    MPI_TARG.H Version:  01.02.09
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-22-00  01.00.02  Added _MSG_TARGET_CMD_BUFFER_POST_REPLY structure.
 *                      Corrected DECSRIPTOR typo to DESCRIPTOR.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *                      Modified target mode to use IoIndex instead of
 *                      HostIndex and IocIndex. Added Alias.
 *  01-09-01  01.01.02  Added defines for TARGET_ASSIST_FLAGS_REPOST_CMD_BUFFER
 *                      and TARGET_STATUS_SEND_FLAGS_REPOST_CMD_BUFFER.
 *  02-20-01  01.01.03  Started using MPI_POINTER.
 *                      Added structures for MPI_TARGET_SCSI_SPI_CMD_BUFFER and
 *                      MPI_TARGET_FCP_CMD_BUFFER.
 *  03-27-01  01.01.04  Added structure offset comments.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *  09-28-01  01.02.02  Added structure for MPI_TARGET_SCSI_SPI_STATUS_IU.
 *                      Added PriorityReason field to some replies and
 *                      defined more PriorityReason codes.
 *                      Added some defines for to support previous version
 *                      of MPI.
 *  10-04-01  01.02.03  Added PriorityReason to MSG_TARGET_ERROR_REPLY.
 *  11-01-01  01.02.04  Added define for TARGET_STATUS_SEND_FLAGS_HIGH_PRIORITY.
 *  03-14-02  01.02.05  Modified MPI_TARGET_FCP_RSP_BUFFER to get the proper
 *                      byte ordering.
 *  05-31-02  01.02.06  Modified TARGET_MODE_REPLY_ALIAS_MASK to only include
 *                      one bit.
 *                      Added AliasIndex field to MPI_TARGET_FCP_CMD_BUFFER.
 *  09-16-02  01.02.07  Added flags for confirmed completion.
 *                      Added PRIORITY_REASON_TARGET_BUSY.
 *  11-15-02  01.02.08  Added AliasID field to MPI_TARGET_SCSI_SPI_CMD_BUFFER.
 *  04-01-03  01.02.09  Added OptionalOxid field to MPI_TARGET_FCP_CMD_BUFFER.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_TARG_H
#define MPI_TARG_H


/******************************************************************************
*
*        S C S I    T a r g e t    M e s s a g e s
*
*******************************************************************************/

typedef struct _CMD_BUFFER_DESCRIPTOR
{
    U16                     IoIndex;                    /* 00h */
    U16                     Reserved;                   /* 02h */
    union                                               /* 04h */
    {
        U32                 PhysicalAddress32;
        U64                 PhysicalAddress64;
    } u;
} CMD_BUFFER_DESCRIPTOR, MPI_POINTER PTR_CMD_BUFFER_DESCRIPTOR,
  CmdBufferDescriptor_t, MPI_POINTER pCmdBufferDescriptor_t;


/****************************************************************************/
/* Target Command Buffer Post Request                                       */
/****************************************************************************/

typedef struct _MSG_TARGET_CMD_BUFFER_POST_REQUEST
{
    U8                      BufferPostFlags;            /* 00h */
    U8                      BufferCount;                /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U8                      BufferLength;               /* 04h */
    U8                      Reserved;                   /* 05h */
    U8                      Reserved1;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    CMD_BUFFER_DESCRIPTOR   Buffer[1];                  /* 0Ch */
} MSG_TARGET_CMD_BUFFER_POST_REQUEST, MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_REQUEST,
  TargetCmdBufferPostRequest_t, MPI_POINTER pTargetCmdBufferPostRequest_t;

#define CMD_BUFFER_POST_FLAGS_PORT_MASK         (0x01)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_MASK    (0x80)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_32      (0)
#define CMD_BUFFER_POST_FLAGS_ADDR_MODE_64      (1)
#define CMD_BUFFER_POST_FLAGS_64_BIT_ADDR       (0x80)

#define CMD_BUFFER_POST_IO_INDEX_MASK           (0x00003FFF)
#define CMD_BUFFER_POST_IO_INDEX_MASK_0100      (0x000003FF) /* obsolete */


typedef struct _MSG_TARGET_CMD_BUFFER_POST_REPLY
{
    U8                      BufferPostFlags;            /* 00h */
    U8                      BufferCount;                /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U8                      BufferLength;               /* 04h */
    U8                      Reserved;                   /* 05h */
    U8                      Reserved1;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved2;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
} MSG_TARGET_CMD_BUFFER_POST_REPLY, MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_REPLY,
  TargetCmdBufferPostReply_t, MPI_POINTER pTargetCmdBufferPostReply_t;

/* the following structure is obsolete as of MPI v1.2 */
typedef struct _MSG_PRIORITY_CMD_RECEIVED_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      PriorityReason;             /* 0Ch */
    U8                      Reserved3;                  /* 0Dh */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     ReplyWord;                  /* 14h */
} MSG_PRIORITY_CMD_RECEIVED_REPLY, MPI_POINTER PTR_MSG_PRIORITY_CMD_RECEIVED_REPLY,
  PriorityCommandReceivedReply_t, MPI_POINTER pPriorityCommandReceivedReply_t;

#define PRIORITY_REASON_NO_DISCONNECT           (0x00)
#define PRIORITY_REASON_SCSI_TASK_MANAGEMENT    (0x01)
#define PRIORITY_REASON_CMD_PARITY_ERR          (0x02)
#define PRIORITY_REASON_MSG_OUT_PARITY_ERR      (0x03)
#define PRIORITY_REASON_LQ_CRC_ERR              (0x04)
#define PRIORITY_REASON_CMD_CRC_ERR             (0x05)
#define PRIORITY_REASON_PROTOCOL_ERR            (0x06)
#define PRIORITY_REASON_DATA_OUT_PARITY_ERR     (0x07)
#define PRIORITY_REASON_DATA_OUT_CRC_ERR        (0x08)
#define PRIORITY_REASON_TARGET_BUSY             (0x09)
#define PRIORITY_REASON_UNKNOWN                 (0xFF)


typedef struct _MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      PriorityReason;             /* 0Ch */
    U8                      Reserved3;                  /* 0Dh */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     ReplyWord;                  /* 14h */
} MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY,
  MPI_POINTER PTR_MSG_TARGET_CMD_BUFFER_POST_ERROR_REPLY,
  TargetCmdBufferPostErrorReply_t, MPI_POINTER pTargetCmdBufferPostErrorReply_t;


typedef struct _MPI_TARGET_FCP_CMD_BUFFER
{
    U8      FcpLun[8];                                  /* 00h */
    U8      FcpCntl[4];                                 /* 08h */
    U8      FcpCdb[16];                                 /* 0Ch */
    U32     FcpDl;                                      /* 1Ch */
    U8      AliasIndex;                                 /* 20h */
    U8      Reserved1;                                  /* 21h */
    U16     OptionalOxid;                               /* 22h */
} MPI_TARGET_FCP_CMD_BUFFER, MPI_POINTER PTR_MPI_TARGET_FCP_CMD_BUFFER,
  MpiTargetFcpCmdBuffer, MPI_POINTER pMpiTargetFcpCmdBuffer;


typedef struct _MPI_TARGET_SCSI_SPI_CMD_BUFFER
{
    /* SPI L_Q information unit */
    U8      L_QType;                                    /* 00h */
    U8      Reserved;                                   /* 01h */
    U16     Tag;                                        /* 02h */
    U8      LogicalUnitNumber[8];                       /* 04h */
    U32     DataLength;                                 /* 0Ch */
    /* SPI command information unit */
    U8      ReservedFirstByteOfCommandIU;               /* 10h */
    U8      TaskAttribute;                              /* 11h */
    U8      TaskManagementFlags;                        /* 12h */
    U8      AdditionalCDBLength;                        /* 13h */
    U8      CDB[16];                                    /* 14h */
    /* Alias ID */
    U8      AliasID;                                    /* 24h */
    U8      Reserved1;                                  /* 25h */
    U16     Reserved2;                                  /* 26h */
} MPI_TARGET_SCSI_SPI_CMD_BUFFER,
  MPI_POINTER PTR_MPI_TARGET_SCSI_SPI_CMD_BUFFER,
  MpiTargetScsiSpiCmdBuffer, MPI_POINTER pMpiTargetScsiSpiCmdBuffer;


/****************************************************************************/
/* Target Assist Request                                                    */
/****************************************************************************/

typedef struct _MSG_TARGET_ASSIST_REQUEST
{
    U8                      StatusCode;                 /* 00h */
    U8                      TargetAssistFlags;          /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     QueueTag;                   /* 04h */
    U8                      Reserved;                   /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     ReplyWord;                  /* 0Ch */
    U8                      LUN[8];                     /* 10h */
    U32                     RelativeOffset;             /* 18h */
    U32                     DataLength;                 /* 1Ch */
    SGE_IO_UNION            SGL[1];                     /* 20h */
} MSG_TARGET_ASSIST_REQUEST, MPI_POINTER PTR_MSG_TARGET_ASSIST_REQUEST,
  TargetAssistRequest_t, MPI_POINTER pTargetAssistRequest_t;

#define TARGET_ASSIST_FLAGS_DATA_DIRECTION          (0x01)
#define TARGET_ASSIST_FLAGS_AUTO_STATUS             (0x02)
#define TARGET_ASSIST_FLAGS_HIGH_PRIORITY           (0x04)
#define TARGET_ASSIST_FLAGS_CONFIRMED               (0x08)
#define TARGET_ASSIST_FLAGS_REPOST_CMD_BUFFER       (0x80)


typedef struct _MSG_TARGET_ERROR_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      PriorityReason;             /* 0Ch */
    U8                      Reserved3;                  /* 0Dh */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     ReplyWord;                  /* 14h */
    U32                     TransferCount;              /* 18h */
} MSG_TARGET_ERROR_REPLY, MPI_POINTER PTR_MSG_TARGET_ERROR_REPLY,
  TargetErrorReply_t, MPI_POINTER pTargetErrorReply_t;


/****************************************************************************/
/* Target Status Send Request                                               */
/****************************************************************************/

typedef struct _MSG_TARGET_STATUS_SEND_REQUEST
{
    U8                      StatusCode;                 /* 00h */
    U8                      StatusFlags;                /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     QueueTag;                   /* 04h */
    U8                      Reserved;                   /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     ReplyWord;                  /* 0Ch */
    U8                      LUN[8];                     /* 10h */
    SGE_SIMPLE_UNION        StatusDataSGE;              /* 18h */
} MSG_TARGET_STATUS_SEND_REQUEST, MPI_POINTER PTR_MSG_TARGET_STATUS_SEND_REQUEST,
  TargetStatusSendRequest_t, MPI_POINTER pTargetStatusSendRequest_t;

#define TARGET_STATUS_SEND_FLAGS_AUTO_GOOD_STATUS   (0x01)
#define TARGET_STATUS_SEND_FLAGS_HIGH_PRIORITY      (0x04)
#define TARGET_STATUS_SEND_FLAGS_CONFIRMED          (0x08)
#define TARGET_STATUS_SEND_FLAGS_REPOST_CMD_BUFFER  (0x80)

/*
 * NOTE: FCP_RSP data is big-endian. When used on a little-endian system, this
 * structure properly orders the bytes.
 */
typedef struct _MPI_TARGET_FCP_RSP_BUFFER
{
    U8      Reserved0[8];                               /* 00h */
    U8      Reserved1[2];                               /* 08h */
    U8      FcpFlags;                                   /* 0Ah */
    U8      FcpStatus;                                  /* 0Bh */
    U32     FcpResid;                                   /* 0Ch */
    U32     FcpSenseLength;                             /* 10h */
    U32     FcpResponseLength;                          /* 14h */
    U8      FcpResponseData[8];                         /* 18h */
    U8      FcpSenseData[32]; /* Pad to 64 bytes */     /* 20h */
} MPI_TARGET_FCP_RSP_BUFFER, MPI_POINTER PTR_MPI_TARGET_FCP_RSP_BUFFER,
  MpiTargetFcpRspBuffer, MPI_POINTER pMpiTargetFcpRspBuffer;

/*
 * NOTE: The SPI status IU is big-endian. When used on a little-endian system,
 * this structure properly orders the bytes.
 */
typedef struct _MPI_TARGET_SCSI_SPI_STATUS_IU
{
    U8      Reserved0;                                  /* 00h */
    U8      Reserved1;                                  /* 01h */
    U8      Valid;                                      /* 02h */
    U8      Status;                                     /* 03h */
    U32     SenseDataListLength;                        /* 04h */
    U32     PktFailuresListLength;                      /* 08h */
    U8      SenseData[52]; /* Pad the IU to 64 bytes */ /* 0Ch */
} MPI_TARGET_SCSI_SPI_STATUS_IU, MPI_POINTER PTR_MPI_TARGET_SCSI_SPI_STATUS_IU,
  TargetScsiSpiStatusIU_t, MPI_POINTER pTargetScsiSpiStatusIU_t;

/****************************************************************************/
/* Target Mode Abort Request                                                */
/****************************************************************************/

typedef struct _MSG_TARGET_MODE_ABORT_REQUEST
{
    U8                      AbortType;                  /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     ReplyWord;                  /* 0Ch */
    U32                     MsgContextToAbort;          /* 10h */
} MSG_TARGET_MODE_ABORT, MPI_POINTER PTR_MSG_TARGET_MODE_ABORT,
  TargetModeAbort_t, MPI_POINTER pTargetModeAbort_t;

#define TARGET_MODE_ABORT_TYPE_ALL_CMD_BUFFERS      (0x00)
#define TARGET_MODE_ABORT_TYPE_ALL_IO               (0x01)
#define TARGET_MODE_ABORT_TYPE_EXACT_IO             (0x02)
#define TARGET_MODE_ABORT_TYPE_EXACT_IO_REQUEST     (0x03)

/* Target Mode Abort Reply */

typedef struct _MSG_TARGET_MODE_ABORT_REPLY
{
    U16                     Reserved;                   /* 00h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved3;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    U32                     AbortCount;                 /* 14h */
} MSG_TARGET_MODE_ABORT_REPLY, MPI_POINTER PTR_MSG_TARGET_MODE_ABORT_REPLY,
  TargetModeAbortReply_t, MPI_POINTER pTargetModeAbortReply_t;


/****************************************************************************/
/* Target Mode Context Reply                                                */
/****************************************************************************/

#define TARGET_MODE_REPLY_IO_INDEX_MASK         (0x00003FFF)
#define TARGET_MODE_REPLY_IO_INDEX_SHIFT        (0)
#define TARGET_MODE_REPLY_INITIATOR_INDEX_MASK  (0x03FFC000)
#define TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT (14)
#define TARGET_MODE_REPLY_ALIAS_MASK            (0x04000000)
#define TARGET_MODE_REPLY_ALIAS_SHIFT           (26)
#define TARGET_MODE_REPLY_PORT_MASK             (0x10000000)
#define TARGET_MODE_REPLY_PORT_SHIFT            (28)


#define GET_IO_INDEX(x)     (((x) & TARGET_MODE_REPLY_IO_INDEX_MASK)           \
                                    >> TARGET_MODE_REPLY_IO_INDEX_SHIFT)

#define SET_IO_INDEX(t, i)                                                     \
            ((t) = ((t) & ~TARGET_MODE_REPLY_IO_INDEX_MASK) |                  \
                              (((i) << TARGET_MODE_REPLY_IO_INDEX_SHIFT) &     \
                                             TARGET_MODE_REPLY_IO_INDEX_MASK))

#define GET_INITIATOR_INDEX(x) (((x) & TARGET_MODE_REPLY_INITIATOR_INDEX_MASK) \
                                   >> TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT)

#define SET_INITIATOR_INDEX(t, ii)                                             \
        ((t) = ((t) & ~TARGET_MODE_REPLY_INITIATOR_INDEX_MASK) |               \
                        (((ii) << TARGET_MODE_REPLY_INITIATOR_INDEX_SHIFT) &   \
                                      TARGET_MODE_REPLY_INITIATOR_INDEX_MASK))

#define GET_ALIAS(x) (((x) & TARGET_MODE_REPLY_ALIAS_MASK)                     \
                                               >> TARGET_MODE_REPLY_ALIAS_SHIFT)

#define SET_ALIAS(t, a)  ((t) = ((t) & ~TARGET_MODE_REPLY_ALIAS_MASK) |        \
                                    (((a) << TARGET_MODE_REPLY_ALIAS_SHIFT) &  \
                                                 TARGET_MODE_REPLY_ALIAS_MASK))

#define GET_PORT(x) (((x) & TARGET_MODE_REPLY_PORT_MASK)                       \
                                               >> TARGET_MODE_REPLY_PORT_SHIFT)

#define SET_PORT(t, p)  ((t) = ((t) & ~TARGET_MODE_REPLY_PORT_MASK) |          \
                                    (((p) << TARGET_MODE_REPLY_PORT_SHIFT) &   \
                                                  TARGET_MODE_REPLY_PORT_MASK))

/* the following obsolete values are for MPI v1.0 support */
#define TARGET_MODE_REPLY_0100_MASK_HOST_INDEX       (0x000003FF)
#define TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX      (0)
#define TARGET_MODE_REPLY_0100_MASK_IOC_INDEX        (0x001FF800)
#define TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX       (11)
#define TARGET_MODE_REPLY_0100_PORT_MASK             (0x00400000)
#define TARGET_MODE_REPLY_0100_PORT_SHIFT            (22)
#define TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX  (0x1F800000)
#define TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX (23)

#define GET_HOST_INDEX_0100(x) (((x) & TARGET_MODE_REPLY_0100_MASK_HOST_INDEX) \
                                  >> TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX)

#define SET_HOST_INDEX_0100(t, hi)                                             \
            ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_HOST_INDEX) |           \
                         (((hi) << TARGET_MODE_REPLY_0100_SHIFT_HOST_INDEX) &  \
                                      TARGET_MODE_REPLY_0100_MASK_HOST_INDEX))

#define GET_IOC_INDEX_0100(x)   (((x) & TARGET_MODE_REPLY_0100_MASK_IOC_INDEX) \
                                  >> TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX)

#define SET_IOC_INDEX_0100(t, ii)                                              \
            ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_IOC_INDEX) |            \
                        (((ii) << TARGET_MODE_REPLY_0100_SHIFT_IOC_INDEX) &    \
                                     TARGET_MODE_REPLY_0100_MASK_IOC_INDEX))

#define GET_INITIATOR_INDEX_0100(x)                                            \
            (((x) & TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX)               \
                              >> TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX)

#define SET_INITIATOR_INDEX_0100(t, ii)                                        \
        ((t) = ((t) & ~TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX) |          \
                   (((ii) << TARGET_MODE_REPLY_0100_SHIFT_INITIATOR_INDEX) &   \
                                TARGET_MODE_REPLY_0100_MASK_INITIATOR_INDEX))


#endif
