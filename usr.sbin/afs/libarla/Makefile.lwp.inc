### lwp stuff
SRCS		+= lwp.c lock.c iomgr.c timer.c fasttime.c preempt.c q.c
OBJS		+= process.o
CLEANFILES	+= process.o
.PATH:	${.CURDIR}/../src/lwp
CFLAGS	+= -DFD_SPEED_HACK -I${.CURDIR}/../src/lwp


process.o: process.S
	HOST_CPU=${MACHINE_ARCH}; \
	case "$$HOST_CPU" in \
		sparc) CPUDEF="-Dsparc" ;; \
		i*86*) CPUDEF="-DAFS_386i_ENV" ;; \
		m68k)  CPUDEF="-Dmc68000" ;; \
		mips)  CPUDEF="-Dmips" ;; \
		alpha) CPUDEF="-Dalpha" ;; \
		hppa*) CPUDEF="" ;; \
		powerpc) CPUDEF="-D__powerpc__" ;; \
		rs6000) CPUDEF="-DRIOS" ;; \
		*)  echo "Unknown host_cpu, good luck" ;; \
	esac; \
	OSDEF="-DAFS_NETBSD_ENV"; \
	${CC} $$CPUDEF $$OSDEF -c ${.CURDIR}/../src/lwp/process.S
