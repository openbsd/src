/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * ardi.h
 * ADP RDI interfaces
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 */

#include "host.h"

typedef unsigned32 ARMword;

#include "dbg_rdi.h"
#include "dbg_conf.h"

extern char *commandline;
extern ARMword last_vector_catch;

/* This is the size of buffers that are asked for by standard channels
 * Non standard channels may wish to copy this!
 */
extern int Armsd_BufferSize;

typedef int (*host_ChannelBufferFilledFnPtr)(unsigned int ,unsigned char ** ,void *);

int angel_RDI_open(
    unsigned type, Dbg_ConfigBlock const *config,
    Dbg_HostosInterface const *hostif, struct Dbg_MCState *dbg_state);
int angel_RDI_close(void);

int angel_RDI_read(ARMword source, void *dest, unsigned *nbytes);
int angel_RDI_write(const void *source, ARMword dest, unsigned *nbytes);

int angel_RDI_CPUread(unsigned mode, unsigned long mask, ARMword *buffer);
int angel_RDI_CPUwrite(unsigned mode, unsigned long mask,
                       ARMword const *buffer);

int angel_RDI_CPread(unsigned CPnum, unsigned long mask, ARMword *buffer);
int angel_RDI_CPwrite(unsigned CPnum, unsigned long mask,
                      ARMword const *buffer);

int angel_RDI_setbreak(ARMword address, unsigned type, ARMword bound,
                      PointHandle *handle);
int angel_RDI_clearbreak(PointHandle handle);

int angel_RDI_setwatch(ARMword address, unsigned type, unsigned datatype,
                      ARMword bound, PointHandle *handle);
int angel_RDI_clearwatch(PointHandle handle);

int angel_RDI_pointinq(ARMword *address, unsigned type, unsigned datatype,
                      ARMword *bound);

int angel_RDI_execute(PointHandle *handle);

void angel_RDI_stop_request(void);

int angel_RDI_step(unsigned ninstr, PointHandle *handle);

int angel_RDI_info(unsigned type, ARMword *arg1, ARMword *arg2);

int angel_RDI_AddConfig(unsigned long nbytes);

int angel_RDI_LoadConfigData(unsigned long nbytes, char const *data);

int angel_RDI_SelectConfig(RDI_ConfigAspect aspect, char const *name,
                          RDI_ConfigMatchType matchtype, unsigned versionreq,
                          unsigned *versionp);

RDI_NameList const *angel_RDI_drivernames(void);

int angel_RDI_LoadAgent(ARMword dest, unsigned long size, getbufferproc *getb,
                        void *getbarg);

extern const struct Dbg_HostosInterface *angel_hostif;

typedef int angel_RDI_TargetStoppedProc(unsigned stopped_reason, void *arg);

extern int angel_RDI_OnTargetStopping(angel_RDI_TargetStoppedProc *fn,
                                      void *arg);
