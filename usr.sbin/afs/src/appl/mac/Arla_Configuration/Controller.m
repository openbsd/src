#import "Controller.h"

@implementation Controller

- (IBAction)addCell:(id)sender
{
    const char *s;

    if ([[newCellName stringValue] length] == 0)
        return;
    s = [[newCellName stringValue] cString];
    if (strchr(s, ' ')) {
        NSBeginAlertSheet(@"Error", @"OK", nil, nil, [tableView window], nil, nil, nil, NULL,
            @"Cell cannot contain spaces");
        return;
    }
    [ dataSource addRowWithAuth: [[NSNumber alloc] initWithInt: NSOnState]
                 show: [[NSNumber alloc] initWithInt: NSOnState]
                 cell: [newCellName stringValue]];
    [ tableView reloadData ];
    [ tableView scrollRowToVisible: ([tableView numberOfRows] - 1)];
}

- (BOOL) isChanged {
    if (authColChanged || showColChanged || confChanged)
        return TRUE;
    else
        return FALSE;
}

- (void) updateSaveButton {
    if ([self isChanged] &&
        !gIsLocked) {
        [saveButton setEnabled: TRUE];
    } else {
        [saveButton setEnabled: FALSE];    
    }
}

- (void) showChanged {
    showColChanged = TRUE;
    [self updateSaveButton];
}

- (void) authChanged {
    authColChanged = TRUE;
    [self updateSaveButton];
}

- (void) confChanged {
    confChanged = TRUE;
    [self updateSaveButton];
}

- (void) setEnabled: (BOOL) status {
    int i;
    NSArray *cols;
    
    cols = [tableView tableColumns];
    for (i = 0; i < [cols count]; i++) {
        [[[cols objectAtIndex: i] dataCell] setEnabled: status];
    }
    [tableView setNeedsDisplay];
    [addButton setEnabled: status];
    [newCellName setEnabled: status];
    [maxBytes setEnabled: status];
    [minBytes setEnabled: status];
    [maxFiles setEnabled: status];
    [minFiles setEnabled: status];
    [defaultButton setEnabled: status];
    [startAtBoot setEnabled: status];
}

- (void) drawAuthState {
    NSImage *lockImage;
    NSBundle *mainBundle;
    NSString *lockImageName;

    mainBundle = [NSBundle mainBundle];
    if (gIsLocked)
	lockImageName = [mainBundle pathForResource: @"lock" ofType: @"tiff"];
    else
	lockImageName = [mainBundle pathForResource: @"unlock" ofType: @"tiff"];

    lockImage = [[NSImage alloc] initWithContentsOfFile: lockImageName];

    if (gIsLocked) {
        [authText setStringValue: @"Click the lock to make changes."];
        [self setEnabled: FALSE];
    } else {
        [authText setStringValue: @"Click the lock to prevent further changes."];
        [self setEnabled: TRUE];
    }
    [self updateSaveButton];
    [authButton setImage: lockImage];
}

- (void) drawDaemonState {
    if (gIsLocked) {
        [statusText setStringValue: @"Status unknown"];
        [startText setStringValue: @"You are unauthenticated"];
        [startButton setTitle: @"Start"];
        [startButton setEnabled: FALSE];
	return;
    }
    if (daemonState == 1) {
        [statusText setStringValue: @"AFS Client On"];
        [startText setStringValue: @"Click Stop to turn off the Arla AFS Client"];
        [startButton setTitle: @"Stop"];
        [startButton setEnabled: TRUE];
    } else {
        [statusText setStringValue: @"AFS Client Off"];
        [startText setStringValue: @"Click Start to turn on the Arla AFS Client"];
        [startButton setTitle: @"Start"];
        [startButton setEnabled: TRUE];
    }
}

- (void) setAuthState: (AuthorizationFlags) flags {
    AuthorizationItem rights[] =
    {	
        { kAuthorizationRightExecute, 0, NULL, 0 }
    };
    AuthorizationRights rightSet = { 1, rights };
    OSStatus 		status;

    status = AuthorizationCopyRights(gAuthorization, &rightSet, kAuthorizationEmptyEnvironment, flags, NULL);
    if (status == errAuthorizationSuccess)
        gIsLocked = false;
    else
        gIsLocked = true;

    [self drawAuthState];
}

- (void) destroyAuthState {
    OSStatus 		status;

    status = AuthorizationFree(gAuthorization, kAuthorizationFlagDestroyRights);
    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &gAuthorization);
    gIsLocked = true;
    [self drawAuthState];
}

- (void) setBytesMeter: (int) current max: (int) max {
    [currentBytesMeter setMaxValue: max];
    [currentBytesMeter setDoubleValue: current];
    [currentBytesText setStringValue: [NSString stringWithFormat: @"%d megabytes", current]];
    [maxBytesText setIntValue: max];
}

- (void) setFilesMeter: (int) current max: (int) max {
    [currentFilesMeter setMaxValue: max];
    [currentFilesMeter setDoubleValue: current];
    [currentFilesText setStringValue: [NSString stringWithFormat: @"%d files", current]];
    [maxFilesText setIntValue: max];
}

- (void) initCacheValues {
    int maxbytes, curbytes, maxfiles, curfiles, minbytes, minfiles;

    [dataSource getCache: gAuthorization
		maxBytes: &maxbytes minBytes: &minbytes
		maxFiles: &maxfiles minFiles: &minfiles
		curBytes: &curbytes curFiles: &curfiles];

    [self setBytesMeter: curbytes / (1024*1024) max: maxbytes / (1024*1024)];
    [self setFilesMeter: curfiles max: maxfiles];

    if (!confChanged) {
	[maxBytes setIntValue: maxbytes / (1024*1024)];
	[minBytes setIntValue: minbytes / (1024*1024)];
	[maxFiles setIntValue: maxfiles];
	[minFiles setIntValue: minfiles];
    }
}

- (void) awakeFromNib {
    AuthorizationFlags 	flags;
    OSStatus 		status;

    flags = kAuthorizationFlagDefaults;

    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, flags, &gAuthorization);

    if (status == errAuthorizationSuccess) {     
        [self setAuthState: flags];
    } else {
        gIsLocked = true;
        [self drawAuthState];
    }
    authColChanged = FALSE;
    showColChanged = FALSE;
    confChanged = FALSE;
    if (gIsLocked) {
	daemonState = -1;
	[self drawDaemonState];
	[startAtBoot setIntValue: 0];
    } else {
	daemonState = [DataSource getDaemonStatus: gAuthorization];
	[self drawDaemonState];
	[startAtBoot setIntValue: [dataSource getStartAtBoot: gAuthorization]];
	[self initCacheValues];
    }
}

- (IBAction)startButton: (id) sender {
    BOOL newDaemonState;

    [self setAuthState: kAuthorizationFlagDefaults];

    if (gIsLocked)
        return;

    if (daemonState == 1)
        [DataSource stopDaemon: gAuthorization];
    else {
        [DataSource startDaemon: gAuthorization];
        [self initCacheValues];
    }

    newDaemonState = [DataSource getDaemonStatus: gAuthorization];
    if ((newDaemonState == daemonState) || (newDaemonState == -1)) {
        if (daemonState)
            NSBeginAlertSheet(@"Error", @"OK", nil, nil, mainWindow, nil, nil, nil, NULL,
                @"Cannot stop Arla");
        else
            NSBeginAlertSheet(@"Error", @"OK", nil, nil, mainWindow, nil, nil, nil, NULL,
                @"Cannot start Arla");
    }
    daemonState = newDaemonState;
    [self drawDaemonState];
}

- (IBAction)auth:(id)sender {
    AuthorizationFlags 	flags;

    flags = kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights;

    [self setAuthState: kAuthorizationFlagDefaults];

    if (gIsLocked) {
        [self setAuthState: flags];
	if (gIsLocked)
	    return;
	daemonState = [DataSource getDaemonStatus: gAuthorization];
	[self drawDaemonState];
	[self initCacheValues];
	[startAtBoot setIntValue: [dataSource getStartAtBoot: gAuthorization]];
    } else {
        [self destroyAuthState];
	daemonState = -1;
	[self drawDaemonState];
	[self initCacheValues];
	[startAtBoot setIntValue: 0];
    }
}


- (IBAction)save:(id)sender {
    OSStatus status;

    [self setAuthState: kAuthorizationFlagDefaults];

    if (gIsLocked)
        return;
    
    if (authColChanged) {
        status = [dataSource saveAuthData: gAuthorization];
        if (status != noErr) {
            return;
        }
        authColChanged = FALSE;
    }
    
    if (showColChanged) {
        status = [dataSource saveShowData: gAuthorization];
        if (status != noErr) {
            return;
        }
        showColChanged = FALSE;
    }

    if (confChanged) {
	if ([maxBytes intValue] == 0) {
	    NSBeginAlertSheet(@"Error", @"OK", nil, nil, mainWindow,
			      nil, nil, nil, NULL,
			      @"Maximum bytes must be non-zero");
	    return;
	}
	if ([minBytes intValue] == 0) {
	    NSBeginAlertSheet(@"Error", @"OK", nil, nil, mainWindow,
			      nil, nil, nil, NULL,
			      @"Minimum bytes must be non-zero");
	    return;
	}
	if ([maxFiles intValue] == 0) {
	    NSBeginAlertSheet(@"Error", @"OK", nil, nil, mainWindow,
			      nil, nil, nil, NULL,
			      @"Maximum files must be non-zero");
	    return;
	}
	if ([minFiles intValue] == 0) {
	    NSBeginAlertSheet(@"Error", @"OK", nil, nil, mainWindow,
			      nil, nil, nil, NULL,
			      @"Minimum files must be non-zero");
	    return;
	}
        status = [dataSource saveConfData: gAuthorization
			     maxBytes: [maxBytes intValue] * 1024 * 1024
			     minBytes: [minBytes intValue] * 1024 * 1024
			     maxFiles: [maxFiles intValue]
			     minFiles: [minFiles intValue]
			     startAtBoot: [startAtBoot intValue]
	];
        if (status != noErr) {
            return;
        }
        confChanged = FALSE;
        [self initCacheValues];
    }
    [self updateSaveButton];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)theApplication {
    return YES;
}

- (void)windowWillClose:(NSNotification *)aNotification {
    mainWindow = nil;
}

- (void)willEndCloseSheet:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
    NSWindow *window = contextInfo;

    if (returnCode == NSAlertAlternateReturn) {         /* "Don't Save" */
        [window close];
        [NSApp replyToApplicationShouldTerminate:YES];
    }
}

- (void)didEndCloseSheet:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo {
    NSWindow *window = contextInfo;

    if (returnCode == NSAlertDefaultReturn) {           /* "Save" */
        [self save: self];
        [window close];
        [NSApp replyToApplicationShouldTerminate:YES];
    } else if (returnCode == NSAlertOtherReturn) {      /* "Cancel" */
        [NSApp replyToApplicationShouldTerminate:NO];
    }
}

- (BOOL) windowShouldClose: (id) sender {
    [self setAuthState: kAuthorizationFlagDefaults];

    if ([self isChanged] && !gIsLocked) {
        NSBeginAlertSheet(@"Close", @"Save", @"Don't save", @"Cancel", sender, self,
            @selector(willEndCloseSheet:returnCode:contextInfo:),
            @selector(didEndCloseSheet:returnCode:contextInfo:),
            sender,
            @"Do you want to save changes before closing?");
        return NO;
    } else {
        [NSApp replyToApplicationShouldTerminate:YES];
        return YES;
    }
}

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *) app {
    if (mainWindow != nil) {
        [mainWindow performClose: self];
        return NSTerminateLater;
    } else
        return NSTerminateNow;
}

- (IBAction)defaultButton:(id) sender {
    [maxBytes abortEditing];
    [maxBytes setIntValue: 100];
    [minBytes abortEditing];
    [minBytes setIntValue: 90];
    [maxFiles abortEditing];
    [maxFiles setIntValue: 4000];
    [minFiles abortEditing];
    [minFiles setIntValue: 3000];
    [self confChanged];
}

- (IBAction)confChanged:(id) sender {
    [self confChanged];
}

@end
