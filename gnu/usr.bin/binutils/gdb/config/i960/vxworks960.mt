# Target: VxWorks running on an Intel 960
TDEPFILES= i960-tdep.o remote-vx.o remote-vx960.o xdr_ld.o xdr_ptrace.o xdr_rdb.o
TM_FILE= tm-vx960.h

# Define this for the vx-share routines, which don't see param.h.
MT_CFLAGS= -DI80960

# Don't use remote.o; it doesn't compile (and won't work) due to lack of
# BREAKPOINT.
REMOTE_OBS= dcache.o remote-utils.o 
