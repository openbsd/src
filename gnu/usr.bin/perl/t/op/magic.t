#!./perl

# $RCSfile: magic.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:05 $

$| = 1;		# command buffering

print "1..6\n";

eval '$ENV{"foo"} = "hi there";';	# check that ENV is inited inside eval
if (`echo \$foo` eq "hi there\n") {print "ok 1\n";} else {print "not ok 1\n";}

unlink 'ajslkdfpqjsjfk';
$! = 0;
open(foo,'ajslkdfpqjsjfk');
if ($!) {print "ok 2\n";} else {print "not ok 2\n";}

# the next tests are embedded inside system simply because sh spits out
# a newline onto stderr when a child process kills itself with SIGINT.

system './perl', '-e', <<'END';

    $| = 1;		# command buffering

    $SIG{"INT"} = "ok3"; kill "INT",$$;
    $SIG{"INT"} = "IGNORE"; kill 2,$$; print "ok 4\n";
    $SIG{"INT"} = "DEFAULT"; kill 2,$$; print "not ok\n";

    sub ok3 {
	if (($x = pop(@_)) eq "INT") {
	    print "ok 3\n";
	}
	else {
	    print "not ok 3 $a\n";
	}
    }

END

@val1 = @ENV{keys(%ENV)};	# can we slice ENV?
@val2 = values(%ENV);

print join(':',@val1) eq join(':',@val2) ? "ok 5\n" : "not ok 5\n";

print @val1 > 1 ? "ok 6\n" : "not ok 6\n";

