$OpenBSD: README.ss,v 1.6 1997/03/11 15:46:01 kstailey Exp $

If you think SCSI tape drives are quirky you haven't seen anything.

There are many SCSI scanners that don't use the SCSI Scanner protocol with
CDB's like SET_WINDOW.  Instead they send Esc-code sequences over the SCSI
bus using the READ and WRITE operations.  True brain death at its finest.
The HP ScanJet, the Sharp JX-600S and the Epson ES-300C are among these.

Ricoh, UMAX, Mustek, Fujitsu, Microtek on the other hand all try to use the
SCSI scanner protocol, but scanning, unlike reading a block from a disk, is
a multi-step operation.  Certain steps must be performed in sequence and
the protocol does not define this.  In addition vendors are permitted by
the SCSI spec. to have unique additional features that the spec. does not
fully define.  Last but not least vendors make mistakes in implementing the
spec.

My SCSI scanner driver architecture is designed to support scanners
two ways.

The first way is used if a scanner uses CDB's like SET_WINDOW. The driver
code should be in ss.c and quirk tables and sequence strings etc. can
battle it out.  This part is not fully implemented yet.  Work is being done
on it from time to time.

The other way is used when the driver is used with an Esc-code-over-SCSI
case like a ScanJet it installs an "operations switch" so that parts of the
code in ss.c can be bypassed.  This feature is implemented for ScanJets.
Currently some Mustek scanners use this approach, as the Mustek scanners
use MODE_SELECT and not SET_WINDOW to send parameter data.  However it is
possible that too much code was farmed out to ss_mustek.c; it may be that
common code for ssread() in ss.c could be used.

Other Considerations

SCSI disconnect is missing from many scanners.  Sucks huh?  A slow
peripheral that also monopolizes the bus.  This means that if your
scanner does not support disconnect you need a second SCSI controller
for it since access of the controller by any other devices will be
locked out while you are scanning.  Scanners that do this include
MUSTEK flatbed scanners MFS 06000CX and MFS 12000CX, UMAX UC-630 &
UG-630.  Over time, as multi-tasking becomes more important to
commoners^H^H^H^H^H^H^H^H^HWindoze users, scanner vendors often supply
new ROMs that can do disconnect.

The image data from the scanner driver is currently supposed to resemble
headerless PBM "rawbits".  Depending on this is probably a bad idea
especially because it cannot always be attained.  The Fujutisu M3096G
grayscale data is photo-negative with repect to PBM and this cannot be
changed.  It would be better to extend the ioctl() interface to be able
to describe the kind of data that is available.

Halftone control of scanners is missing, save for one pre-defined
selection.  This also should be in the ioctl() interface.

Basic workflow for scanning

1. Open driver.
2. ioctl to get parameters (this fills in default values and generally makes
   step 3 easier.)
3. Modify parameters.
4. ioctl to set parameters.
5. ioctl to get data size (same as step 2, but values will be different if
   the image size, resolution, or image data type was set.)
6. Read data based on size from scanner retrieved in step 5 (the driver
   delivers an EOF if you overread.)
7. Close driver (or use ioctl to reset it so you can scan again.)
