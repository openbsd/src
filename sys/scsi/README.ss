$OpenBSD: README.ss,v 1.1 1997/03/08 21:21:37 kstailey Exp $

If you think SCSI tape drives are quirky you haven't seen anything.

There are many SCSI scanners that don't use the SCSI Scanner protocol
with CDB's like SET_WINDOW.  Instead they send Esc-code sequences over
the SCSI bus using the READ and WRITE operations.  True brain death at
its finest.  The HP ScanJet, the Sharp JX-600S and the Epson ES-300C
are among these.

Ricoh, UMAX, Mustek, Fujitsu, Microtek on the other hand all try to
use the SCSI scanner protocol, but scanning, unlike reading a block
from a disk, is a multi-step operation.  Certain steps must be
performed in sequence and the protocol does not define this.  In
addition vendors are permitted by the SCSI spec. to have unique
additional features that the spec. does not fully define.  Last but not
least vendors make mistakes in implementing the spec.

My SCSI scanner driver architecture is designed to support scanners
two ways.

The first way is used if a scanner uses CDB's like SET_WINDOW. The
driver code should be in ss.c and quirk tables and sequence strings
etc. can battle it out.  This part is not fully implemented yet.
Work is being done on it from time to time.

The other way is used when the driver is used with an
Esc-code-over-SCSI case like a ScanJet it installs an "operations
switch" so that parts of the code in ss.c can be bypassed.  This
feature is implemented for ScanJets.  Currently some Mustek scanners
use this approach, as the Mustek scanners use MODE_SELECT and not
SET_WINDOW to send parameter data.  However it is possible that too
much code was farmed out to ss_mustek.c; it may be that common code
for ssread() in ss.c could be used.

