$!  Test.Com - DCL driver for perl5 regression tests
$!
$!  Version 1.1   4-Dec-1995
$!  Charles Bailey  bailey@genetics.upenn.edu
$
$!  A little basic setup
$   On Error Then Goto wrapup
$   olddef = F$Environment("Default")
$   If F$Search("t.dir").nes.""
$   Then
$       Set Default [.t]
$   Else
$       If F$TrnLNm("Perl_Root").nes.""
$       Then 
$           Set Default Perl_Root:[t]
$       Else
$           Write Sys$Error "Can't find test directory"
$           Exit 44
$       EndIf
$   EndIf
$
$!  Pick up a copy of perl to use for the tests
$   Delete/Log/NoConfirm Perl.;*
$   Copy/Log/NoConfirm [-]Perl.Exe []Perl.
$
$!  Make the environment look a little friendlier to tests which assume Unix
$   cat = "Type"
$   Macro/NoDebug/Object=Echo.Obj Sys$Input
		.title echo
		.psect data,wrt,noexe
	dsc:
		.word 0
		.byte 14 ; DSC$K_DTYPE_T
		.byte 2  ; DSC$K_CLASS_D
		.long 0
		.psect code,nowrt,exe
		.entry	echo,^m<r2,r3>
		movab	dsc,r2
		pushab	(r2)
		calls	#1,G^LIB$GET_FOREIGN
		movl	4(r2),r3
		movzwl	(r2),r0
		addl2	4(r2),r0
		cmpl	r3,r0
		bgtru	sym.3
		nop	
	sym.1:
		movb	(r3),r0
		cmpb	r0,#65
		blss	sym.2
		cmpb	r0,#90
		bgtr	sym.2
		cvtbl	r0,r0
		addl2	#32,r0
		cvtlb	r0,(r3)
	sym.2:
		incl	r3
		movzwl	(r2),r0
		addl2	4(r2),r0
		cmpl	r3,r0
		blequ	sym.1
	sym.3:
		pushab	(r2)
		calls	#1,G^LIB$PUT_OUTPUT
		movl	#1,r0
		ret	
		.end echo
$   Link/NoTrace Echo.Obj;
$   Delete/Log/NoConfirm Echo.Obj;*
$   echo = "$" + F$Parse("Echo.Exe")
$
$!  And do it
$   testdir = "Directory/NoHead/NoTrail/Column=1"
$   Define/User Perlshr Sys$Disk:[-]PerlShr.Exe
$   MCR Sys$Disk:[]Perl. "''p1'" "''p2'" "''p3'" "''p4'" "''p5'" "''p6'"
$   Deck/Dollar=$$END-OF-TEST$$
# $RCSfile: TEST,v $$Revision: 4.1 $$Date: 92/08/07 18:27:00 $
# Modified for VMS 30-Sep-1994  Charles Bailey  bailey@genetics.upenn.edu
#
# This is written in a peculiar style, since we're trying to avoid
# most of the constructs we'll be testing for.

# skip those tests we know will fail entirely or cause perl to hang bacause
# of Unixisms
@compexcl=('cpp.t','script.t');
@ioexcl=('argv.t','dup.t','fs.t','inplace.t','pipe.t');
@libexcl=('anydbm.t','db-btree.t','db-hash.t','db-recno.t',
          'gdbm.t','ndbm.t','odbm.t','sdbm.t','posix.t','soundex.t');
@opexcl=('exec.t','fork.t','glob.t','magic.t','misc.t','stat.t');
@exclist=(@compexcl,@ioexcl,@libexcl,@opexcl);
foreach $file (@exclist) { $skip{$file}++; }

$| = 1;

@ARGV = grep($_,@ARGV);  # remove empty elements due to "''p1'" syntax

if ($ARGV[0] eq '-v') {
    $verbose = 1;
    shift;
}

chdir 't' if -f 't/TEST';

if ($ARGV[0] eq '') {
    foreach (<[.*]*.t>) {
      s/.*[\[.]t./[./;
      ($fname = $_) =~ s/.*\]//;
      if ($skip{"\L$fname"}) { push(@skipped,$_); }
      else { push(@ARGV,$_); }
    }
}

if (@skipped) {
  print "The following tests were skipped because they rely extensively on\n";
  print " Unixisms not compatible with the current version of perl for VMS:\n";
  print "\t",join("\n\t",@skipped),"\n\n";
}

$bad = 0;
$good = 0;
$total = @ARGV;
while ($test = shift) {
    if ($test =~ /^$/) {
	next;
    }
    $te = $test;
    chop($te);
    $te .= '.' x (24 - length($te));
	open(script,"$test") || die "Can't run $test.\n";
	$_ = <script>;
	close(script);
	if (/#!..perl(.*)/) {
	    $switch = $1;
	} else {
	    $switch = '';
	}
	open(results,"\$ MCR Sys\$Disk:[]Perl. $switch $test |") || (print "can't run.\n");
    $ok = 0;
    $next = 0;
    while (<results>) {
	if ($verbose) {
	    print "$te$_";
	    $te = '';
	}
	unless (/^#/) {
	    if (/^1\.\.([0-9]+)/) {
		$max = $1;
		$totmax += $max;
		$files += 1;
		$next = 1;
		$ok = 1;
	    } else {
		$next = $1, $ok = 0, last if /^not ok ([0-9]*)/;
		next if /^\s*$/; # our 'echo' substitute produces one more \n than Unix'
		if (/^ok (.*)/ && $1 == $next) {
		    $next = $next + 1;
		} else {
		    $ok = 0;
		}
	    }
	}
    }
    $next = $next - 1;
    if ($ok && $next == $max) {
	print "${te}ok\n";
	$good = $good + 1;
    } else {
	$next += 1;
	print "${te}FAILED on test $next\n";
	$bad = $bad + 1;
	$_ = $test;
	if (/^base/) {
	    die "Failed a basic test--cannot continue.\n";
	}
    }
}

if ($bad == 0) {
    if ($ok) {
	print "All tests successful.\n";
    } else {
	die "FAILED--no tests were run for some reason.\n";
    }
} else {
    $pct = sprintf("%.2f", $good / $total * 100);
    if ($bad == 1) {
	warn "Failed 1 test, $pct% okay.\n";
    } else {
	warn "Failed $bad/$total tests, $pct% okay.\n";
    }
}
($user,$sys,$cuser,$csys) = times;
print sprintf("u=%g  s=%g  cu=%g  cs=%g  files=%d  tests=%d\n",
    $user,$sys,$cuser,$csys,$files,$totmax);
$$END-OF-TEST$$
$ wrapup:
$   If F$Search("Echo.Exe").nes."" Then Delete/Log/NoConfirm Echo.Exe;*
$   Set Default &olddef
$   Exit
