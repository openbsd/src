#define SystemSevenOrLater 1

#include "Types.r"
#include "SysTypes.r"
#include "BalloonTypes.r"
#include "AEUserTermTypes.r"
#include "AERegistry.r"
#include "AEObjects.r"

#define __kPrefSize	512
#define __kMinSize	512

#define GUSI_PREF_VERSION '0150'

#include "GUSI.r"

resource 'GU·I' (GUSIRsrcID) {
	'TEXT', 'CWIE', noAutoSpin, useChdir, approxStat, 
	noTCPDaemon, noUDPDaemon, 
	noConsole,
	{};
};

resource 'aete' (0, "MacCVS Suite") {
	0x01, 0x00, english, roman,
	{
		"MacCVS Suite", "Custom events", 'MCVS', 1, 1,
		{
			"Do Command", "Execute a CVS command", 'misc', 'dosc',
			'null', "", replyOptional, singleItem, notEnumerated, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, 
			'TEXT', "Command to execute", directParamRequired, singleItem, notEnumerated,
			changesState, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, 
			{
				"Mode", 'MODE', 'MODE', "Mode (AE, File).", optional, singleItem, enumerated, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, 
				"Environment", 'ENVT', 'TEXT', "Environment variables.", optional, listOfItems, notEnumerated, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved,
				"Filename", 'FILE', 'TEXT', "Output file path.", optional, singleItem, notEnumerated, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, 
				"Pathway", 'SPWD', 'TEXT', "Starting pathway.", optional, singleItem, notEnumerated, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved,
				"NoLineBuffer", 'LBUF', 'bool', "if true, send each result line as separate AE.", optional, singleItem, notEnumerated, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, reserved, 
			}
		},
		{},
		{},
		{
			'MODE',
			{
				"AE", 'TOAE', "Redirect standard output to apple events",
				"File", 'FILE', "Redirect standard output to a file",
			},
		},
	}
};

