#ifndef	_SYS_PIOCTL_H_
#define	_SYS_PIOCTL_H_

/*
 */
#define AFSCALL_PIOCTL 20
#define AFSCALL_SETPAG 21

#ifndef _VICEIOCTL
#define _VICEIOCTL(id)  ((unsigned int ) _IOW('V', id, struct ViceIoctl))
#endif /* _VICEIOCTL */

#define VIOCSETAL               _VICEIOCTL(1)
#define VIOCGETAL               _VICEIOCTL(2)
#define VIOCSETTOK              _VICEIOCTL(3)
#define VIOCGETVOLSTAT          _VICEIOCTL(4)
#define VIOCSETVOLSTAT          _VICEIOCTL(5)
#define VIOCFLUSH               _VICEIOCTL(6)
#define VIOCGETTOK              _VICEIOCTL(8)
#define VIOCUNLOG               _VICEIOCTL(9)
#define VIOCCKSERV              _VICEIOCTL(10)
#define VIOCCKBACK              _VICEIOCTL(11)
#define VIOCCKCONN              _VICEIOCTL(12)
#define VIOCWHEREIS             _VICEIOCTL(14)
#define VIOCACCESS              _VICEIOCTL(20)
#define VIOCUNPAG               _VICEIOCTL(21)
#define VIOCGETFID              _VICEIOCTL(22)
#define VIOCSETCACHESIZE        _VICEIOCTL(24)
#define VIOCFLUSHCB             _VICEIOCTL(25)
#define VIOCNEWCELL             _VICEIOCTL(26)
#define VIOCGETCELL             _VICEIOCTL(27)
#define VIOC_AFS_DELETE_MT_PT   _VICEIOCTL(28)
#define VIOC_AFS_STAT_MT_PT     _VICEIOCTL(29)
#define VIOC_FILE_CELL_NAME     _VICEIOCTL(30)
#define VIOC_GET_WS_CELL        _VICEIOCTL(31)
#define VIOC_AFS_MARINER_HOST   _VICEIOCTL(32)
#define VIOC_GET_PRIMARY_CELL   _VICEIOCTL(33)
#define VIOC_VENUSLOG           _VICEIOCTL(34)
#define VIOC_GETCELLSTATUS      _VICEIOCTL(35)
#define VIOC_SETCELLSTATUS      _VICEIOCTL(36)
#define VIOC_FLUSHVOLUME        _VICEIOCTL(37)
#define VIOC_AFS_SYSNAME        _VICEIOCTL(38)
#define VIOC_EXPORTAFS          _VICEIOCTL(39)
#define VIOCGETCACHEPARAMS      _VICEIOCTL(40)

struct ViceIoctl {
  caddr_t in, out;
  short in_size;
  short out_size;
};

struct ClearToken {
  int32_t AuthHandle;
  char HandShakeKey[8];
  int32_t ViceId;
  int32_t BeginTimestamp;
  int32_t EndTimestamp;
};

#endif
