/*
 * Common defintions used by several *.xg files
 */

/* $arla: common.h,v 1.19 2003/03/06 20:39:47 lha Exp $ */

%#ifndef _COMMON_
%#define _COMMON_

const LockRead = 0;
const LockWrite	= 1;
const LockExtend = 2;
const LockRelease = 3;

const AFSNAMEMAX = 256;

const RWVOL   = 0;
const ROVOL   = 1;
const BACKVOL = 2;

struct AFSFid {
     uint32_t Volume;
     uint32_t Vnode;
     uint32_t Unique;
};

struct VenusFid {
     int32_t Cell;
     AFSFid fid;
};

struct AFSCallBack {
     uint32_t CallBackVersion;
     uint32_t ExpirationTime;
     uint32_t CallBackType;
};

enum CallBackType { CBEXCLUSIVE = 1, CBSHARED = 2, CBDROPPED = 3};

const CALLBACK_VERSION = 1;

struct AFSVolSync {
     uint32_t spare1;
     uint32_t spare2;
     uint32_t spare3;
     uint32_t spare4;
     uint32_t spare5;
     uint32_t spare6;
};

const TYPE_FILE = 1;
const TYPE_DIR  = 2;
const TYPE_LINK = 3;

struct AFSFetchStatus {
     uint32_t InterfaceVersion;
     uint32_t FileType;
     uint32_t LinkCount;
     uint32_t Length;
     uint32_t DataVersion;
     uint32_t Author;
     uint32_t Owner;
     uint32_t CallerAccess;
     uint32_t AnonymousAccess;
     uint32_t UnixModeBits;
     uint32_t ParentVnode;
     uint32_t ParentUnique;
     uint32_t SegSize;
     uint32_t ClientModTime;
     uint32_t ServerModTime;
     uint32_t Group;
     uint32_t SyncCount;
     uint32_t DataVersionHigh;	/* For AFS/DFS translator */
     uint32_t LockCount;
     uint32_t LengthHigh;
     uint32_t ErrorCode;
};

/*
 * Things in AFSStoreStatus.mask
 */

const SS_MODTIME     = 0x01 ;
const SS_OWNER       = 0x02 ;
const SS_GROUP       = 0x04 ;
const SS_MODEBITS    = 0x08 ;
const SS_SEGSIZE     = 0x10 ;
const SS_FSYNC       = 0x400; /* 1024 */


struct AFSStoreStatus {
     uint32_t Mask;
     uint32_t ClientModTime;
     uint32_t Owner;
     uint32_t Group;
     uint32_t UnixModeBits;
     uint32_t SegSize;
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

const AFS_SETMINQUOTA = 1;
const AFS_SETMAXQUOTA = 2;

const AFSOPAQUEMAX = 1024;

typedef opaque AFSOpaque<AFSOPAQUEMAX>;

typedef int32_t ViceLockType;

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
    uint32_t  Vid;
    int32_t   Type;
    uint32_t  Type0;
    uint32_t  Type1;
    uint32_t  Type2;
    uint32_t  Type3;
    uint32_t  Type4;
    uint32_t  ServerCount;
    uint32_t  Server0;
    uint32_t  Server1;
    uint32_t  Server2;
    uint32_t  Server3;
    uint32_t  Server4;
    uint32_t  Server5;
    uint32_t  Server6;
    uint32_t  Server7;
    uint16_t  Port0;
    uint16_t  Port1;
    uint16_t  Port2;
    uint16_t  Port3;
    uint16_t  Port4;
    uint16_t  Port5;
    uint16_t  Port6;
    uint16_t  Port7;
};

#include "afsuuid.h"

const AFSCAPABILITIESMAX = 196;

typedef int32_t Capabilities<AFSCAPABILITIESMAX>;

%#endif /* _COMMON_ */
