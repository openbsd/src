$! generates options file for vms link
$! p1 is filename and mode to open file (filename/write or filename/append)
$! p2 is delimiter separating elements of list in p3
$! p3 is list of items to be written, one per line, into options file
$
$ open file 'p1'
$ element=0
$loop:
$ x=f$element(element,p2,p3)
$ if x .eqs. p2 then goto out
$ y=f$edit(x,"COLLAPSE")  ! lose spaces
$ if y .nes. "" then write file y
$ element=element+1
$ goto loop
$
$out:
$ close file
$ exit
