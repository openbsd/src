# Target: VxWorks SPARC
TDEPFILES= sparc-tdep.o \
	remote-vx.o remote-vxsparc.o xdr_ld.o xdr_ptrace.o xdr_rdb.o
DEPRECATED_TM_FILE= config/tm-vxworks.h
