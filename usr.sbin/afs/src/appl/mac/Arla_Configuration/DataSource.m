#import "DataSource.h"
#import "ReadCells.h"
#import "Controller.h"
#include <stdio.h>
#include <sys/ioctl.h>
#include <kafs.h>
#include "config.h"
#include <roken.h>
#include <parse_units.h>
#include <sys/wait.h>

static struct units size_units[] = {
    { "M", 1024 * 1024 },
    { "k", 1024 },
    { NULL, 0 }
};

static void
drainfile(FILE *f)
{
    char buffer[100];
    while(fread(buffer, 1, sizeof(buffer), f) != 0);
}

static void
drainproc()
{
    pid_t pid;
    int status;
    do {
	pid = wait(&status);
    } while (pid != -1);
}

static int
getdaemonpid(AuthorizationRef authorization, char *buffer, int len)
{
    char *argv[3];
    OSStatus status;
    FILE *output;
    int n;

    argv[0] = "/var/run/arlad.pid";
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/bin/cat", 0, argv, &output);
    if (status == noErr) {
        n = fread(buffer, 1, len - 1, output);
        if (n == 0) {
            fclose(output);
            return -1;
        }
        buffer[n] = '\0';
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
checkdaemonpid(AuthorizationRef authorization, char *pid)
{
    char *argv[4];
    OSStatus status;
    FILE *output;
    char buffer[1000];
    int n;

    argv[0] = "cocommand=";
    argv[1] = "-p";
    argv[2] = pid;
    argv[3] = NULL;
    status = AuthorizationExecuteWithPrivileges(authorization, "/bin/ps", 0, argv, &output);
    n = fread(buffer, 1, sizeof(buffer) - 1, output);
    if (n == 0) {
        fclose(output);
        return -1;
    }
    buffer[n] = '\0';
    fclose(output);
    drainproc();
    if (strcmp(buffer, "\narlad\n") == 0)
        return 1;
    return 0;
}

static int
xfs_umount(AuthorizationRef authorization)
{
    char *argv[2];
    OSStatus status;
    FILE *output;

    argv[0] = "/afs";
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/usr/arla/sbin/umount_xfs", 0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
xfs_mount(AuthorizationRef authorization)
{
    char *argv[3];
    OSStatus status;
    FILE *output;

    argv[0] = "/dev/xfs0";
    argv[1] = "/afs";
    argv[2] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/usr/arla/sbin/mount_xfs", 0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
disktool_refresh(AuthorizationRef authorization)
{
    char *argv[2];
    OSStatus status;
    FILE *output;

    argv[0] = "-r";
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/usr/sbin/disktool", 0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
stoparlad(AuthorizationRef authorization)
{
    char *argv[3];
    OSStatus status;
    FILE *output;
    char pid[100];
    int ret;

    ret = getdaemonpid(authorization, pid, sizeof(pid));
    if (ret == -1)
        return -1;

    argv[0] = pid;
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/bin/kill", 0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
startarlad(AuthorizationRef authorization)
{
    char *argv[3];
    OSStatus status;
    FILE *output;

    argv[0] = "-D";
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/usr/arla/libexec/arlad", 0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
kmodunload(AuthorizationRef authorization)
{
    char *argv[3];
    OSStatus status;
    FILE *output;

    argv[0] = "-n";
    argv[1] = "xfs";
    argv[2] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/sbin/kmodunload", 0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
kmodload(AuthorizationRef authorization)
{
    char *argv[3];
    OSStatus status;
    FILE *output;

    argv[0] = "/usr/arla/bin/xfs_mod.o";
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/sbin/kmodload", 0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
mkafsdir(AuthorizationRef authorization)
{
    char *argv[3];
    OSStatus status;
    FILE *output;

    argv[0] = "/afs";
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/bin/mkdir",
						0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }
}

static int
getcache(int *max_bytes, int *used_bytes, int *max_vnodes, int *used_vnodes,
	 int *min_bytes, int *min_vnodes)
{
    u_int32_t parms[16];
    struct ViceIoctl a_params;

    a_params.in_size  = 0;
    a_params.out_size = sizeof(parms);
    a_params.in       = NULL;
    a_params.out      = (char *) parms;

    memset (parms, 0, sizeof(parms));

    if (!k_hasafs())
        return ENOSYS;

    if (k_pioctl (NULL, VIOCGETCACHEPARAMS , &a_params, 0) == -1)
        return errno;

    /* param[0] and param[1] send maxbytes and usedbytes in kbytes */

    if (max_vnodes)
        *max_vnodes = parms[2];
    if (used_vnodes)
        *used_vnodes = parms[3];
    if (max_bytes)
        *max_bytes = parms[4];
    if (used_bytes)
        *used_bytes = parms[5];
    if (min_bytes)
	*min_bytes = parms[6];
    if (min_vnodes)
	*min_vnodes = parms[7];

    return 0;
}

static int
setcache(int max_bytes, int min_bytes, int max_vnodes, int min_vnodes,
	 AuthorizationRef authorization)
{
    char *argv[6];
    OSStatus status;
    FILE *output;

    argv[0] = "setcachesize";
    argv[1] = malloc(100);
    argv[2] = malloc(100);
    argv[3] = malloc(100);
    argv[4] = malloc(100);
    argv[5] = NULL;

    snprintf(argv[1], 100, "%d", min_vnodes);
    snprintf(argv[2], 100, "%d", max_vnodes);
    snprintf(argv[3], 100, "%d", min_bytes);
    snprintf(argv[4], 100, "%d", max_bytes);

    status = AuthorizationExecuteWithPrivileges(authorization,
						"/usr/arla/bin/fs",
						0, argv, &output);
    if (status == noErr) {
        drainfile(output);
        fclose(output);
	drainproc();
        return 0;
    } else {
        return -1;
    }

    return 0;
}

@implementation DataSource

NSMutableArray *authArray;
NSMutableArray *showArray;
NSMutableArray *cellArray;

- (int) numberOfRowsInTableView:(NSTableView *) aTableView {
    return [ authArray count ];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(int) row {
    if (tableColumn == authColumn)
        return [ authArray objectAtIndex: row ];
    else if (tableColumn == showColumn)
        return [ showArray objectAtIndex: row ];
    else if (tableColumn == cellNameColumn)
        return [ cellArray objectAtIndex: row ];
    else
        return nil;
}

- (void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn
row:(int)row {
    if (aTableColumn == authColumn) {
        NSTableView *t = tableView;
        NSNumber *value = anObject;
        [ authArray replaceObjectAtIndex: row withObject: anObject ];
        [controller authChanged];
        if ([value intValue] == NSOnState) {
            [ showArray replaceObjectAtIndex: row withObject: anObject ];
            [controller showChanged];
        }
        [ t reloadData ];
    }
    else if (aTableColumn == showColumn)  {
        [ showArray replaceObjectAtIndex: row withObject: anObject ];
        [controller showChanged];
    }
    else if (aTableColumn == cellNameColumn) {
        [ cellArray replaceObjectAtIndex: row withObject: anObject ];
    }
}

- (void) awakeFromNib {
    NSButtonCell *aCell = [ [NSButtonCell alloc] init ];
    NSTableView *t = tableView;

    [aCell setButtonType: NSSwitchButton ];
    [aCell setTitle: @""];

    [authColumn setDataCell: aCell];
    [showColumn setDataCell: aCell];
    [aCell release];
    
    authArray = [ [NSMutableArray alloc] init ];
    showArray = [ [NSMutableArray alloc] init ];
    cellArray = [ [NSMutableArray alloc] init ];
    
    [ReadCells auth: authArray show: showArray cell: cellArray];
    [ t reloadData ];
}

- (void)addRowWithAuth: (NSNumber*)auth show: (NSNumber*)show cell: (NSString*)cell {
    [ authArray addObject: auth ];
    [ showArray addObject: show ];
    [ cellArray addObject: cell ];
    [ controller authChanged];
    [ controller showChanged];
}

- (void)deleteRow:(unsigned)row {
    [ authArray removeObjectAtIndex: row ];
    [ showArray removeObjectAtIndex: row ];
    [ cellArray removeObjectAtIndex: row ];    
}

- (NSString *)getDataForArray: anArray {
    NSNumber *aNumber;
    NSString *resultString;
    int i;
    int count;

    count = [anArray count];

    resultString = @"";

    for (i = 0; i < count; i++) {
        aNumber = [anArray objectAtIndex: i];
        if ([aNumber intValue] == NSOnState) {
            resultString = [resultString stringByAppendingString: [cellArray objectAtIndex: i]];
            resultString = [resultString stringByAppendingString: @"\n"];
        }
    }
    
    return resultString;
}

- (OSStatus) fetchData: (NSMutableString *) data file: (char *) file auth: (AuthorizationRef) authorization {
    char *argv[2];
    OSStatus status;
    FILE *output;
    char buffer[1000];
    int n;

    argv[0] = file;
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/bin/cat", 0, argv, &output);
    if (status == noErr) {
        n = fread(buffer, 1, sizeof(buffer) - 1, output);
        if (n == 0) {
            fclose(output);
            return -1;
        }
        buffer[n] = '\0';
        fclose(output);
	drainproc();
	[data appendString: [[NSString alloc] initWithCString: buffer]];
        return 0;
    } else {
        return -1;
    }
}

- (OSStatus) storeData: (NSString *) data file: (char *) file auth: (AuthorizationRef) authorization {
    const char *s;
    char *argv[2];
    OSStatus status;
    FILE *output;

    argv[0] = file;
    argv[1] = NULL;

    status = AuthorizationExecuteWithPrivileges(authorization, "/usr/bin/tee", 0, argv, &output);
    if (status == noErr) {
        s = [data cString];
        fwrite(s, [data length], 1, output);
        fclose(output);
	drainproc();
        return noErr;
    } else {
        return status;
    }
}

- (OSStatus)saveShowData: (AuthorizationRef) authorization {
    NSString *data;
    data = [self getDataForArray: showArray];
    return [self storeData: data file: "/usr/arla/etc/DynRootDB" auth: authorization];
}

- (OSStatus)saveAuthData: (AuthorizationRef) authorization {
    NSString *data;
    data = [self getDataForArray: authArray];
    return [self storeData: data file: "/usr/arla/etc/TheseCells" auth: authorization];
}

- (NSDictionary *) parseConfLine: (const char *) line {
    char *save = NULL;
    char *n;
    char *v;
    unsigned val;
    char *endptr;
    char *line1;
    NSDictionary *dict;
    
    if (line[0] == '#')
	return nil;
    
    line1 = strdup(line);
    if (line1 == NULL)
	return nil;
    
    n = strtok_r (line1, " \t", &save);
    if (n == NULL) {
	free(line1);
	return nil;
    }
    
    v = strtok_r (NULL, " \t", &save);
    if (v == NULL) {
	free(line1);
	return nil;
    }
    
    val = parse_units(v, size_units, NULL);
    if(val == (unsigned)-1) {
	val = strtol(v, &endptr, 0);
	if (endptr == v) {
	    free(line1);
	    return nil;
	}
    }
    
    dict = [NSDictionary dictionaryWithObject: [NSNumber numberWithInt: val]
			 forKey: [NSString stringWithCString: n]];
    free(line1);
    return dict;
}

- (void)parseConf: (NSString *) data dict: (NSMutableDictionary *) dict {
    NSRange curRange;
    NSRange nextRange;
    NSMutableCharacterSet *newLine;
    NSDictionary *linedict;

    unsigned length = [data length];

    newLine = [[NSMutableCharacterSet alloc] init];
    [newLine addCharactersInString: @"\n"];

    curRange.location = 0;
    while(1) {
	curRange.length = length - curRange.location;
	nextRange = [data rangeOfCharacterFromSet: newLine
			  options: NSLiteralSearch
			  range: curRange];
	if (nextRange.length == 0)
	    break;
	curRange.length = nextRange.location - curRange.location;
	linedict = [self parseConfLine:
			     [[data substringWithRange: curRange] cString]];
	if (dict)
	    [dict addEntriesFromDictionary: linedict];

	curRange.location = nextRange.location + nextRange.length;
    }
}

- (void)writeConf: (NSMutableString *) data dict: (NSDictionary *) dict {
    
    NSEnumerator *enumerator = [dict keyEnumerator];
    NSString *key;
    
    while ((key = [enumerator nextObject])) {
	[data appendString: [NSString stringWithFormat: @"%s %d\n",
				      [key cString],
				      [[dict objectForKey: key] intValue]]];
    }
}

- (OSStatus)getCache: (AuthorizationRef) authorization
	    maxBytes: (int *) maxBytes minBytes: (int *) minBytes
	    maxFiles: (int *) maxFiles minFiles: (int *) minFiles
	    curBytes: (int *) curBytes curFiles: (int *) curFiles {
    NSMutableString *data;
    NSMutableDictionary *dict;

    if (getcache(maxBytes, curBytes, maxFiles, curFiles,
		 minBytes, minFiles) == 0)
	return 0;

    data = [[[NSMutableString alloc] init] autorelease];
    dict = [[[NSMutableDictionary alloc] init] autorelease];

    [self fetchData: data file: "/usr/arla/etc/arla.conf" auth: authorization];

    [self parseConf: data dict: dict];
    
    *maxBytes = [[dict objectForKey: @"high_bytes"] intValue];
    *minBytes = [[dict objectForKey: @"low_bytes"] intValue];
    *maxFiles = [[dict objectForKey: @"high_vnodes"] intValue];
    *minFiles = [[dict objectForKey: @"low_vnodes"] intValue];
    *curBytes = 0;
    *curFiles = 0;

    return 0;
}
    

- (OSStatus)saveConfData: (AuthorizationRef) authorization
		maxBytes: (int) maxBytes minBytes: (int) minBytes
		maxFiles: (int) maxFiles minFiles: (int) minFiles
	     startAtBoot: (int) startAtBoot {
    NSMutableString *data;
    NSMutableDictionary *dict;

    data = [[NSMutableString alloc] init];
    dict = [[NSMutableDictionary alloc] init];

    [self fetchData: data file: "/usr/arla/etc/arla.conf" auth: authorization];

    [self parseConf: data dict: dict];

    [data setString: @""];

    [dict setObject: [NSNumber numberWithInt: maxBytes]
	  forKey: @"high_bytes"];
    [dict setObject: [NSNumber numberWithInt: minBytes]
	  forKey: @"low_bytes"];
    [dict setObject: [NSNumber numberWithInt: maxFiles]
	  forKey: @"high_vnodes"];
    [dict setObject: [NSNumber numberWithInt: minFiles]
	  forKey: @"low_vnodes"];
    
    [self writeConf: data dict: dict];
    
    [self storeData: data file: "/usr/arla/etc/arla.conf" auth: authorization];

    setcache(maxBytes, minBytes, maxFiles, minFiles, authorization);

    if (startAtBoot)
	[self storeData: @"yes" file: "/usr/arla/etc/startatboot"
	      auth: authorization];
    else
	[self storeData: @"no" file: "/usr/arla/etc/startatboot"
	      auth: authorization];

    return 0;
}

- (int) getStartAtBoot: (AuthorizationRef) authorization {
    NSMutableString *data;
    OSStatus status;

    data = [[[NSMutableString alloc] init] autorelease];

    status = [self fetchData: data
		   file: "/usr/arla/etc/startatboot"
		   auth: authorization];
    if (status)
	return 0;
    if (strcmp([data cString], "yes") == 0)
	return 1;
    return 0;
}

+ (int) getDaemonStatus: (AuthorizationRef) authorization {
    int ret;
    char buffer[1000];

    ret = getdaemonpid(authorization, buffer, sizeof(buffer));
    if (ret == -1)
        return -1;

    ret = checkdaemonpid(authorization, buffer);
    return ret;
}

+ (void) startDaemon: (AuthorizationRef) authorization {
    int i;
    struct timeval t;

    mkafsdir(authorization);
    kmodload(authorization);
    startarlad(authorization);
    for (i = 0; i < 40; i++) {
	if ([self getDaemonStatus: authorization] == 1)
	    break;
	t.tv_sec = 0;
	t.tv_usec = 250000;
	select(0, NULL, NULL, NULL, &t);
    }
    xfs_mount(authorization);
    disktool_refresh(authorization);
}

+ (void) stopDaemon: (AuthorizationRef) authorization {
    xfs_umount(authorization);
    disktool_refresh(authorization);
    stoparlad(authorization);
    kmodunload(authorization);
}


@end
