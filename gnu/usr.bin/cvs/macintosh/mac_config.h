/*
 * mac_config.h - Macintosh-specific definitions and configuration settings
 *
 * MDLadwig <mike@twinpeaks.prc.com> --- July 1996
 */
// MacCVS with AppleEvent IO and no console support.  If this is not defined then IO will be
// via the SIOUX console
//#define AE_IO_HANDLERS
// Setup includes to use MSL instead of Plum-Hall ANSI library
//#define MSL_LIBRARY
#define AE_OUTBUF_SIZE			32000		// Maximum size of output Apple Events
#define AE_TIMEOUT_SECONDS		30			// Timeout for AppleEvents command recipt
#define ArgMax						512	// Maximum number of Args passed via AE command
#define EnvMax						512	// Maximum number of Env Vars passed via AE command
#define STACK_SIZE_68K			98305		// Stack size for 68k version (PPC set in CW prefs)
