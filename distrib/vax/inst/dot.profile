PATH=/sbin:/bin:/usr/bin:/usr/sbin:/ export PATH
TERM=vt100 export TERM

# set up some sane defaults
echo 'erase ^?, werase ^?, kill ^U, intr ^C, status ^T'
stty newcrt werase ^? intr ^C kill ^U erase ^? status ^T 9600
echo
echo "Now when OpenBSD is booted you're on your own."
echo "Remember to write bootblocks and to make devices"
echo "in dev in your new root filesystem before booting."
echo "Also remember to copy /bsd and /boot to the"
echo "new root; it's not there by default."
echo
echo "Good luck!"
echo
