#	$OpenBSD: dot.profile,v 1.2 1996/04/25 21:32:06 niklas Exp $

PATH=/sbin:/bin:/usr/bin:/usr/sbin:/ export PATH
TERM=vt100 export TERM

# set up some sane defaults
echo 'erase ^?, werase ^H, kill ^U, intr ^C'
stty newcrt werase ^H intr ^C kill ^U erase ^?
echo
echo "Now when OpenBSD is booted you're on your own."
echo "Remember to write bootblocks and to make devices"
echo "in dev in your new root filesystem before booting."
echo "Also remember to copy /genbsd and /boot to the"
echo "new root; it's not there by default."
echo
echo "Good luck!"
echo
