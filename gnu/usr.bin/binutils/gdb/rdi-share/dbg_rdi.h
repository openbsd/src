/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * ARM debugger toolbox : dbg_rdi.h
 */

/*
 * RCS $Revision: 1.3 $
 * Checkin $Date: 2004/12/27 14:00:54 $
 */

#ifndef dbg_rdi__h
#define dbg_rdi__h

/***************************************************************************\
*                            Other RDI values                               *
\***************************************************************************/

#define RDISex_Little           0 /* the byte sex of the debuggee       */
#define RDISex_Big              1
#define RDISex_DontCare         2

#define RDIPoint_EQ             0 /* the different types of break/watchpoints */
#define RDIPoint_GT             1
#define RDIPoint_GE             2
#define RDIPoint_LT             3
#define RDIPoint_LE             4
#define RDIPoint_IN             5
#define RDIPoint_OUT            6
#define RDIPoint_MASK           7

#define RDIPoint_16Bit          16  /* 16-bit breakpoint                */
#define RDIPoint_Conditional    32

/* ORRed with point type in extended RDP break and watch messages       */
#define RDIPoint_Inquiry        64
#define RDIPoint_Handle         128 /* messages                         */

#define RDIWatch_ByteRead       1 /* types of data accesses to watch for*/
#define RDIWatch_HalfRead       2
#define RDIWatch_WordRead       4
#define RDIWatch_ByteWrite      8
#define RDIWatch_HalfWrite      16
#define RDIWatch_WordWrite      32

#define RDIReg_R15              (1L << 15) /* mask values for CPU       */
#define RDIReg_PC               (1L << 16)
#define RDIReg_CPSR             (1L << 17)
#define RDIReg_SPSR             (1L << 18)
#define RDINumCPURegs           19

#define RDINumCPRegs            10 /* current maximum                   */

#define RDIMode_Curr            255

/* RDI_Info subcodes */
/* rdp in parameters are all preceded by                                */
/*   in byte = RDP_Info, word = info subcode                            */
/*     out parameters are all preceded by                               */
/*   out byte = RDP_Return                                              */

#define RDIInfo_Target          0
/* rdi: out ARMword *targetflags, out ARMword *processor id             */
/* rdp: in none, out word targetflags, word processorid, byte status    */
/* the following bits are defined in targetflags                        */
#  define RDITarget_LogSpeed                  0x0f
#  define RDITarget_HW                        0x10    /* else emulator  */
#  define RDITarget_AgentMaxLevel             0xe0
#  define RDITarget_AgentLevelShift       5
#  define RDITarget_DebuggerMinLevel         0x700
#  define RDITarget_DebuggerLevelShift    8
#  define RDITarget_CanReloadAgent           0x800
#  define RDITarget_CanInquireLoadSize      0x1000
#  define RDITarget_UnderstandsRDPInterrupt 0x2000
#  define RDITarget_CanProfile              0x4000
#  define RDITarget_Code16                  0x8000
#  define RDITarget_HasCommsChannel        0x10000

#define RDIInfo_Points          1
/* rdi: out ARMword *pointcapabilities                                  */
/* rdp: in none, out word pointcapabilities, byte status                */
/* the following bits are defined in pointcapabilities                  */
#  define RDIPointCapability_Comparison   1
#  define RDIPointCapability_Range        2
/* 4 to 128 are RDIWatch_xx{Read,Write} left-shifted by two */
#  define RDIPointCapability_Mask         0x100
#  define RDIPointCapability_ThreadBreak  0x200
#  define RDIPointCapability_ThreadWatch  0x400
#  define RDIPointCapability_CondBreak    0x800
#  define RDIPointCapability_Status       0x1000 /* status enquiries available */

#define RDIInfo_Step            2
/* rdi: out ARMword *stepcapabilities                                   */
/* rdp: in none, out word stepcapabilities, byte status                 */
/* the following bits are defined in stepcapabilities                   */
#  define RDIStep_Multiple      1
#  define RDIStep_PCChange      2
#  define RDIStep_Single        4

#define RDIInfo_MMU             3
/* rdi: out ARMword *mmuidentity                                        */
/* rdp: in none, out word mmuidentity, byte status                      */

#define RDIInfo_DownLoad        4
/* Inquires whether configuration download and selection is available.  */
/* rdp: in none, out byte status                                        */
/* No argument, no return value. status == ok if available              */

#define RDIInfo_SemiHosting     5
/* Inquires whether RDISemiHosting_* RDI_Info calls are available.      */
/* rdp: in none, out byte status                                        */
/* No argument, no return value. status == ok if available              */

#define RDIInfo_CoPro           6
/* Inquires whether CoPro RDI_Info calls are available.                 */
/* rdp: in none, out byte status                                        */
/* No argument, no return value. status == ok if available              */

#define RDIInfo_Icebreaker      7
/* Inquires whether debuggee controlled by IceBreaker.                  */
/* rdp: in none, out byte status                                        */
/* No argument, no return value. status == ok if available              */

#define RDIMemory_Access        8
/* rdi: out RDI_MemAccessStats *p, in ARMword *handle                   */
/* rdp: in word handle                                                  */
/*      out word nreads, word nwrites, word sreads, word swrites,       */
/*          word ns, word s, byte status                                */

/* Get memory access information for memory block with specified handle */

#define RDIMemory_Map           9
/* rdi: in  RDI_MemDescr md[n], in ARMword *n                           */
/* rdp: in word n, n * {                                                */
/*           word handle, word start, word limit,                       */
/*           byte width, byte access                                    */
/*           word Nread_ns, word Nwrite_ns,                             */
/*           word Sread_ns, word Swrite_ns}                             */
/*      out byte status                                                 */
/* Sets memory characteristics.                                         */

#define RDISet_CPUSpeed         10
/* rdi: in  ARMword *speed                                              */
/* rdp: in word speed, out byte status                                  */
/* Sets CPU speed (in ns)                                               */

#define RDIRead_Clock           12
/* rdi: out ARMword *ns, out ARMword *s                                 */
/* rdp: in none, out word ns, word s, byte status                       */
/* Reads simulated time                                                 */

#define RDIInfo_Memory_Stats    13
/* Inquires whether RDI_Info codes 8-10 are available                   */
/* rdp: in none, out byte status                                        */
/* No argument, no return value. status == ok if available              */

/* The next two are only to be used if RDIInfo_DownLoad returned no     */
/* error                                                                */
#define RDIConfig_Count         14
/* rdi: out ARMword *count                                              */
/* rdp: out byte status, word count (if status == OK)                   */

/* In addition, the next one is only to be used if RDIConfig_Count      */
/* returned no error                                                    */
typedef struct { unsigned32 version; char name[32]; } RDI_ConfigDesc;
#define RDIConfig_Nth           15
/* rdi: in ARMword *n, out RDI_ConfigDesc *                             */
/* rdp: in word n                                                       */
/*      out word version, byte namelen, bytes * bytelen name,           */
/*          byte status                                                 */

/* Set a front-end polling function to be used from within driver poll  */
/* loops                                                                */
typedef void RDI_PollProc(void *);
typedef struct { RDI_PollProc *p; void *arg; } RDI_PollDesc;
#define RDISet_PollProc         16
/* rdi: in RDI_PollDesc const *from, RDI_PollDesc *to                   */
/*      if from non-NULL, sets the polling function from it             */
/*      if to non-NULL, returns the previous polling function to it     */
/* No corresponding RDP operation                                       */

/* Called on debugger startup to see if the target is ready to execute  */
#define RDIInfo_CanTargetExecute 20
/* rdi: in  void
 *      out byte status (RDIError_NoError => Yes, Otherwise No)
 */

/* Called to set the top of target memory in an ICEman2 system
 * This is then used by ICEman to tell the C Library via the INFOHEAP
 * SWI where the stack should start.
 * Note that only ICEman2 supports this call.  Other systems eg.
 * Demon, Angel, will simply return an error, which means that setting
 * the top of memory in this fashion is not supported.
 */
#define RDIInfo_SetTopMem        21
/* rdi: in  word mem_top
 *      out byte status (RDIError_NoError => Done, Other => Not supported
 */

/* Called before performing a loadagent to determine the endianess of
 * the debug agent, so that images of the wrong bytesex can be
 * complained about
 */
#define RDIInfo_AgentEndianess   22
/* rdi: in void
 *      out byte status
 *      status should be RDIError_LittleEndian or RDIError_BigEndian
 *      any other value indicates the target does not support this
 *      request, so the debugger will have to make a best guess, which
 *      probably means only allow little endian loadagenting.
 */

/* The next two are only to be used if the value returned by            */
/* RDIInfo_Points has RDIPointCapability_Status set.                    */
#define RDIPointStatus_Watch    0x80
#define RDIPointStatus_Break    0x81
/* rdi: inout ARMword * (in handle, out hwresource), out ARMword *type  */
/* rdp: in word handle, out word hwresource, word type, byte status     */

#define RDISignal_Stop          0x100
/* Requests that the debuggee stop                                      */
/* No arguments, no return value                                        */
/* rdp: no reply (when the debuggee stops, there will be a reply to the */
/*      step or execute request which started it)                       */

#define RDIVector_Catch         0x180
/* rdi: in ARMword *bitmap                                              */
/* rdp: int word bitmap, out byte status                                */
/* bit i in bitmap set to cause vector i to cause entry to debugger     */

/* The next four are only to be used if RDIInfo_Semihosting returned    */
/* no error                                                             */
#define RDISemiHosting_SetState 0x181
/* rdi: in ARMword *semihostingstate                                    */
/* rdp: in word semihostingstate, out byte status                       */
#define RDISemiHosting_GetState 0x182
/* rdi: out ARMword *semihostingstate                                   */
/* rdp: in none, out word semihostingstate, byte status                 */
#define RDISemiHosting_SetVector 0x183
/* rdi: in ARMword *semihostingvector                                   */
/* rdp: in word semihostingvector, out byte status                      */
#define RDISemiHosting_GetVector 0x184
/* rdi: out ARMword *semihostingvector                                  */
/* rdp: in none, out word semihostingvector, byte status                */

/* The next two are only to be used if RDIInfo_Icebreaker returned      */
/* no error                                                             */
#define RDIIcebreaker_GetLocks  0x185
/* rdi: out ARMword *lockedstate                                        */
/* rdp: in none, out word lockedstate, byte status                      */

#define RDIIcebreaker_SetLocks  0x186
/* rdi: in ARMword *lockedstate                                         */
/* rdp: in word lockedstate, out byte status                            */

/* lockedstate is a bitmap of the icebreaker registers locked against   */
/* use by IceMan (because explicitly written by the user)               */

#define RDIInfo_GetLoadSize     0x187
/* rdi: out ARMword *maxloadsize                                        */
/* rdp: in none, out word maxloadsize, byte status                      */
/* Inquires the maximum length of data transfer the agent is prepared   */
/* to receive                                                           */
/* Only usable if RDIInfo_Target returned RDITarget_CanInquireLoadSize  */
/* rdi: out ARMword *size                                               */

/* Only to be used if the value returned by RDIInfo_Target had          */
/* RDITarget_HasCommsChannel set                                        */
typedef void RDICCProc_ToHost(void *arg, ARMword data);
typedef void RDICCProc_FromHost(void *arg, ARMword *data, int *valid);

#define RDICommsChannel_ToHost  0x188
/* rdi: in RDICCProc_ToHost *, in void *arg                             */
/* rdp: in byte connect, out byte status                                */
#define RDICommsChannel_FromHost 0x189
/* rdi: in RDICCProc_FromHost *, in void *arg                           */
/* rdp: in byte connect, out byte status                                */

/* These 4 are only to be used if RDIInfo_Semihosting returns no error  */
#define RDISemiHosting_SetARMSWI 0x190
/* rdi: in ARMword ARM_SWI_number                                       */
/* rdp: in ARMword ARM_SWI_number, out byte status                      */

#define RDISemiHosting_GetARMSWI 0x191
/* rdi: out ARMword ARM_SWI_number                                      */
/* rdp: out ARMword ARM_SWI_number, byte status                         */

#define RDISemiHosting_SetThumbSWI 0x192
/* rdi: in ARMword Thumb_SWI_number                                     */
/* rdp: in ARMword Thumb_SWI_number, out byte status                    */

#define RDISemiHosting_GetThumbSWI 0x193
/* rdi: out ARMword ARM_Thumb_number                                    */
/* rdp: out ARMword ARM_Thumb_number, byte status                       */


#define RDICycles               0x200
/* rdi: out ARMword cycles[12]                                          */
/* rdp: in none, out 6 words cycles, byte status                        */
/* the rdi result represents 6 big-endian doublewords; the rdp results  */
/* return values for the ls halves of these                             */
#  define RDICycles_Size        48

#define RDIErrorP               0x201
/* rdi: out ARMaddress *errorp                                          */
/* rdp: in none, out word errorp, byte status                           */
/* Returns the error pointer associated with the last return from step  */
/* or execute with status RDIError_Error.                               */

#define RDISet_Cmdline          0x300
/* rdi: in char *commandline (a null-terminated string)                 */
/* No corresponding RDP operation (cmdline is sent to the agent in      */
/* response to SWI_GetEnv)                                              */

#define RDISet_RDILevel         0x301
/* rdi: in ARMword *level                                               */
/* rdp: in word level, out byte status                                  */
/* Sets the RDI/RDP protocol level to be used (must lie between the     */
/* limits returned by RDIInfo_Target).                                  */

#define RDISet_Thread           0x302
/* rdi: in ARMword *threadhandle                                        */
/* rdp: in word threadhandle, out byte status                           */
/* Sets the thread context for subsequent thread-sensitive operations   */
/* (null value sets no thread)                                          */

/* The next two are only to be used if RDI_read or RDI_write returned   */
/* RDIError_LittleEndian or RDIError_BigEndian, to signify that the     */
/* debugger has noticed.                                                */
#define RDIInfo_AckByteSex  0x303
/* rdi: in ARMword *sex (RDISex_Little or RDISex_Big)                   */

/* The next two are only to be used if RDIInfo_CoPro returned no error  */
#define RDIInfo_DescribeCoPro   0x400
/* rdi: in int *cpno, Dbg_CoProDesc *cpd                                */
/* rdp: in byte cpno,                                                   */
/*         cpd->entries * {                                             */
/*           byte rmin, byte rmax, byte nbytes, byte access,            */
/*           byte cprt_r_b0, cprt_r_b1, cprt_w_b0, cprt_w_b1}           */
/*         byte = 255                                                   */
/*      out byte status                                                 */

#define RDIInfo_RequestCoProDesc 0x401
/* rdi: in int *cpno, out Dbg_CoProDesc *cpd                            */
/* rpd: in byte cpno                                                    */
/*      out nentries * {                                                */
/*            byte rmin, byte rmax, byte nbytes, byte access,           */
/*          }                                                           */
/*          byte = 255, byte status                                     */

#define RDIInfo_Log             0x800
/* rdi: out ARMword *logsetting                                         */
/* No corresponding RDP operation                                       */
#define RDIInfo_SetLog          0x801
/* rdi: in ARMword *logsetting                                          */
/* No corresponding RDP operation                                       */

#define RDIProfile_Stop         0x500
/* No arguments, no return value                                        */
/* rdp: in none, out byte status                                        */
/* Requests that pc sampling stop                                       */

#define RDIProfile_Start        0x501
/* rdi: in ARMword *interval                                            */
/* rdp: in word interval, out byte status                               */
/* Requests that pc sampling start, with period <interval> usec         */

#define RDIProfile_WriteMap     0x502
/* rdi: in ARMword map[]                                                */
/* map[0] is the length of the array, subsequent elements are sorted    */
/* and are the base of ranges for pc sampling (so if the sampled pc     */
/* lies between map[i] and map[i+1], count[i] is incremented).          */
/* rdp: a number of messages, each of form:                             */
/*        in word len, word size, word offset, <size> words map data    */
/*        out status                                                    */
/* len, size and offset are all word counts.                            */

#define RDIProfile_ReadMap      0x503
/* rdi: in ARMword *len, out ARMword counts[len]                        */
/* Requests that the counts array be set to the accumulated pc sample   */
/* counts                                                               */
/* rdp: a number of messages, each of form:                             */
/*        in word offset, word size                                     */
/*        out <size> words, status                                      */
/* len, size and offset are all word counts.                            */

#define RDIProfile_ClearCounts  0x504
/* No arguments, no return value                                        */
/* rdp: in none, out byte status                                        */
/* Requests that pc sample counts be set to zero                        */

#define RDIInfo_RequestReset    0x900
/* Request reset of the target environment                              */
/* No arguments, no return value                                        */
/* No RDP equivalent, sends an RDP reset                                */

#define RDIInfo_CapabilityRequest 0x8000
/* Request whether the interface supports the named capability. The     */
/* capability is specified by or'ing the RDIInfo number with this, and  */
/* sending that request                                                 */
/* rdi: in none                                                         */
/* rdp: in none, out byte status                                        */

typedef struct {
  ARMword len;
  ARMword map[1];
} RDI_ProfileMap;

typedef unsigned32 PointHandle;
typedef unsigned32 ThreadHandle;
#define RDINoPointHandle        ((PointHandle)-1L)
#define RDINoHandle             ((ThreadHandle)-1L)

struct Dbg_ConfigBlock;
struct Dbg_HostosInterface;
struct Dbg_MCState;
typedef int rdi_open_proc(unsigned type, struct Dbg_ConfigBlock const *config,
                          struct Dbg_HostosInterface const *i,
                          struct Dbg_MCState *dbg_state);
typedef int rdi_close_proc(void);
typedef int rdi_read_proc(ARMword source, void *dest, unsigned *nbytes);
typedef int rdi_write_proc(const void *source, ARMword dest, unsigned *nbytes);
typedef int rdi_CPUread_proc(unsigned mode, unsigned32 mask, ARMword *state);
typedef int rdi_CPUwrite_proc(unsigned mode, unsigned32 mask, ARMword const *state);
typedef int rdi_CPread_proc(unsigned CPnum, unsigned32 mask, ARMword *state);
typedef int rdi_CPwrite_proc(unsigned CPnum, unsigned32 mask, ARMword const *state);
typedef int rdi_setbreak_proc(ARMword address, unsigned type, ARMword bound,
                              PointHandle *handle);
typedef int rdi_clearbreak_proc(PointHandle handle);
typedef int rdi_setwatch_proc(ARMword address, unsigned type, unsigned datatype,
                              ARMword bound, PointHandle *handle);
typedef int rdi_clearwatch_proc(PointHandle handle);
typedef int rdi_execute_proc(PointHandle *handle);
typedef int rdi_step_proc(unsigned ninstr, PointHandle *handle);
typedef int rdi_info_proc(unsigned type, ARMword *arg1, ARMword *arg2);
typedef int rdi_pointinq_proc(ARMword *address, unsigned type,
                              unsigned datatype, ARMword *bound);

typedef enum {
    RDI_ConfigCPU,
    RDI_ConfigSystem
} RDI_ConfigAspect;

typedef enum {
    RDI_MatchAny,
    RDI_MatchExactly,
    RDI_MatchNoEarlier
} RDI_ConfigMatchType;

typedef int rdi_addconfig_proc(unsigned32 nbytes);
typedef int rdi_loadconfigdata_proc(unsigned32 nbytes, char const *data);
typedef int rdi_selectconfig_proc(RDI_ConfigAspect aspect, char const *name,
                                  RDI_ConfigMatchType matchtype, unsigned versionreq,
                                  unsigned *versionp);

typedef char *getbufferproc(void *getbarg, unsigned32 *sizep);
typedef int rdi_loadagentproc(ARMword dest, unsigned32 size, getbufferproc *getb, void *getbarg);
typedef int rdi_targetisdead(void);

typedef struct {
    int itemmax;
    char const * const *names;
} RDI_NameList;

typedef RDI_NameList const *rdi_namelistproc(void);

typedef int rdi_errmessproc(char *buf, int buflen, int errnum);

struct RDIProcVec {
    char rditypename[12];

    rdi_open_proc       *open;
    rdi_close_proc      *close;
    rdi_read_proc       *read;
    rdi_write_proc      *write;
    rdi_CPUread_proc    *CPUread;
    rdi_CPUwrite_proc   *CPUwrite;
    rdi_CPread_proc     *CPread;
    rdi_CPwrite_proc    *CPwrite;
    rdi_setbreak_proc   *setbreak;
    rdi_clearbreak_proc *clearbreak;
    rdi_setwatch_proc   *setwatch;
    rdi_clearwatch_proc *clearwatch;
    rdi_execute_proc    *execute;
    rdi_step_proc       *step;
    rdi_info_proc       *info;
    /* V2 RDI */
    rdi_pointinq_proc   *pointinquiry;

    /* These three useable only if RDIInfo_DownLoad returns no error */
    rdi_addconfig_proc  *addconfig;
    rdi_loadconfigdata_proc *loadconfigdata;
    rdi_selectconfig_proc *selectconfig;

    rdi_namelistproc    *drivernames;
    rdi_namelistproc    *cpunames;

    rdi_errmessproc     *errmess;

    /* Only if RDIInfo_Target returns a value with RDITarget_LoadAgent set */
    rdi_loadagentproc   *loadagent;
    rdi_targetisdead    *targetisdead;
};

#endif
