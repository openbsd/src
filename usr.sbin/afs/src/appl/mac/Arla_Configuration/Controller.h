#import <Cocoa/Cocoa.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>
#import "DataSource.h"

@interface Controller : NSObject
{
    IBOutlet DataSource *dataSource;
    IBOutlet NSTableView *tableView;
    IBOutlet NSTabViewItem *cellSetupTab;
    IBOutlet NSTabViewItem *generalTab;
    IBOutlet NSTextField *authText;
    IBOutlet NSControl *saveButton;
    IBOutlet NSTextField *newCellName;
    IBOutlet NSControl *addButton;
    IBOutlet NSWindow *mainWindow;
    IBOutlet NSButton *startButton;
    IBOutlet NSControl *startText;
    IBOutlet NSControl *statusText;
    IBOutlet NSControl *startAtBoot;
    IBOutlet NSControl *maxBytes;
    IBOutlet NSControl *minBytes;
    IBOutlet NSControl *maxFiles;
    IBOutlet NSControl *minFiles;
    IBOutlet NSControl *defaultButton;
    IBOutlet NSControl *currentBytesText;
    IBOutlet NSProgressIndicator *currentBytesMeter;
    IBOutlet NSControl *maxBytesText;
    IBOutlet NSControl *currentFilesText;
    IBOutlet NSProgressIndicator *currentFilesMeter;
    IBOutlet NSControl *maxFilesText;
    IBOutlet NSButton *authButton;
    AuthorizationRef gAuthorization;
    Boolean gIsLocked;
    Boolean authColChanged;
    Boolean showColChanged;
    Boolean confChanged;
    int daemonState;
}
- (IBAction)addCell:(id)sender;
- (IBAction)auth:(id)sender;
- (IBAction)save:(id)sender;
- (IBAction)startButton:(id) sender;
- (IBAction)defaultButton:(id) sender;
- (IBAction)confChanged:(id) sender;
- (void) showChanged;
- (void) authChanged;
@end
