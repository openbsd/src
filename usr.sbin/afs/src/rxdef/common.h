/*	$OpenBSD: common.h,v 1.1.1.1 1998/09/14 21:53:18 art Exp $	*/
/*
 * Common defintions for cb.xg and fs.xg
 */

/* $KTH: common.h,v 1.8 1998/09/03 18:47:29 lha Exp $ */

%#ifndef _COMMON_
%#define _COMMON_

const LockRead = 0;
const LockWrite	= 1;
const LockExtend = 2;
const LockRelease = 3;

const AFSNAMEMAX = 256;

struct AFSFid {
     unsigned long Volume;
     unsigned long Vnode;
     unsigned long Unique;
};

struct VenusFid {
     long Cell;
     AFSFid fid;
};

struct AFSCallBack {
     unsigned long CallBackVersion;
     unsigned long ExpirationTime;
     unsigned long CallBackType;
};

enum CallBackType { CBEXCLUSIVE = 1, CBSHARED = 2, CBDROPPED = 3};

struct AFSVolSync {
     unsigned spare1;
     unsigned spare2;
     unsigned spare3;
     unsigned spare4;
     unsigned spare5;
     unsigned spare6;
};

const TYPE_FILE = 1;
const TYPE_DIR  = 2;
const TYPE_LINK = 3;

struct AFSFetchStatus {
     unsigned long InterfaceVersion;
     unsigned long FileType;
     unsigned long LinkCount;
     unsigned long Length;
     unsigned long DataVersion;
     unsigned long Author;
     unsigned long Owner;
     unsigned long CallerAccess;
     unsigned long AnonymousAccess;
     unsigned long UnixModeBits;
     unsigned long ParentVnode;
     unsigned long ParentUnique;
     unsigned long SegSize;
     unsigned long ClientModTime;
     unsigned long ServerModTime;
     unsigned long Group;
     unsigned long SyncCount;
     unsigned spare1;
     unsigned spare2;
     unsigned spare3;
     unsigned spare4;
};

/*
 * Things in AFSStoreStatus.mask
 */

const SS_MODTIME     = 0x01 ;
const SS_OWNER       = 0x02 ;
const SS_GROUP       = 0x04 ;
const SS_MODEBITS    = 0x08 ;
const SS_SEGSIZE     = 0x0F ;

struct AFSStoreStatus {
     unsigned long Mask;
     unsigned long ClientModTime;
     unsigned long Owner;
     unsigned long Group;
     unsigned long UnixModeBits;
     unsigned long SegSize;
};

struct AFSFetchVolumeStatus {
    int32_t   Vid;
    int32_t   ParentId;
    char      Online;
    char      InService;
    char      Blessed;
    char      NeedsSalvage;
    int32_t   Type;
    int32_t   MinQuota;
    int32_t   MaxQuota;
    int32_t   BlocksInUse;
    int32_t   PartBlocksAvail;
    int32_t   PartMaxBlocks;
};

struct AFSStoreVolumeStatus {
    int32_t   Mask;
    int32_t   MinQuota;
    int32_t   MaxQuota;
};

const AFSOPAQUEMAX = 1024;

typedef opaque AFSOpaque<AFSOPAQUEMAX>;

typedef long ViceLockType;

const AFSCBMAX = 50;

typedef AFSCallBack AFSCBs<AFSCBMAX>;
typedef AFSFetchStatus AFSBulkStats<AFSCBMAX>;
typedef AFSFid AFSCBFids<AFSCBMAX>;

/* Definitions for ACLs */

const PRSFS_READ       =  1 ; /* Read files */
const PRSFS_WRITE      =  2 ; /* Write files & write-lock existing files */
const PRSFS_INSERT     =  4 ; /* Insert & write-lock new files */
const PRSFS_LOOKUP     =  8 ; /* Enumerate files and examine ACL */
const PRSFS_DELETE     = 16 ; /* Remove files */
const PRSFS_LOCK       = 32 ; /* Read-lock files */
const PRSFS_ADMINISTER = 64 ; /* Set access list of directory */

struct AFSVolumeInfo {
    u_long  Vid;
    long    Type;
    u_long  Type0;
    u_long  Type1;
    u_long  Type2;
    u_long  Type3;
    u_long  Type4;
    u_long  ServerCount;
    u_long  Server0;
    u_long  Server1;
    u_long  Server2;
    u_long  Server3;
    u_long  Server4;
    u_long  Server5;
    u_long  Server6;
    u_long  Server7;
    unsigned short Port0;
    unsigned short Port1;
    unsigned short Port2;
    unsigned short Port3;
    unsigned short Port4;
    unsigned short Port5;
    unsigned short Port6;
    unsigned short Port7;
};




%#endif /* _COMMON_ */
