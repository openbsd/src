# Source file used to test the abs macro.
foo:
	mtc1	$0,$f0
	cvt.d.w	$f0,$f0
        mtc1    $0,$f1
        cvt.d.w $f1,$f1
        .space	8

