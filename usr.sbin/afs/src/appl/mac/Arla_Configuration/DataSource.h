#import <Cocoa/Cocoa.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

@interface DataSource : NSObject
{
    IBOutlet id authColumn;
    IBOutlet id cellNameColumn;
    IBOutlet id showColumn;
    IBOutlet id tableView;
    IBOutlet id controller;
}
- (void)addRowWithAuth: (NSNumber*)auth show: (NSNumber*)show cell: (NSString*)cell;
- (OSStatus)saveShowData: (AuthorizationRef) gAuthorization;
- (OSStatus)saveAuthData: (AuthorizationRef) gAuthorization;
- (OSStatus)saveConfData: (AuthorizationRef) gAuthorization
		maxBytes: (int) maxBytes minBytes: (int) minBytes
		maxFiles: (int) maxFiles minFiles: (int) minFiles
	     startAtBoot: (int) startAtBoot;
- (int) getStartAtBoot: (AuthorizationRef) authorization;
+ (int) getDaemonStatus: (AuthorizationRef) gAuthorization;
+ (void) startDaemon: (AuthorizationRef) gAuthorization;
+ (void) stopDaemon: (AuthorizationRef) gAuthorization;
- (OSStatus)getCache: (AuthorizationRef) authorization
maxBytes: (int *) maxBytes minBytes: (int *) minBytes
maxFiles: (int *) maxFiles minFiles: (int *) minFiles
curBytes: (int *) curBytes curFiles: (int *) curFiles;
@end
