BEGIN {
    chdir 't' if -d 't/lib';
    @INC = '../lib' if -d 'lib';
    require Config; import Config;
    if (-d 'lib' and $Config{'extensions'} !~ /\bOS2(::|\/)REXX\b/) {
	print "1..0\n";
	exit 0;
    }
}

use OS2::REXX;

#
# DLL
#
$ydba = load OS2::REXX "ydbautil" 
  or print "1..0 # skipped: cannot find YDBAUTIL.DLL\n" and exit;
print "1..5\n", "ok 1\n";

#
# function
#
@pid = $ydba->RxProcId();
@pid == 1 ? print "ok 2\n" : print "not ok 2\n";
@res = split " ", $pid[0];
print "ok 3\n" if $res[0] == $$;
@pid = $ydba->RxProcId();
@res = split " ", $pid[0];
print "ok 4\n" if $res[0] == $$;
print "# @pid\n";

eval { $ydba->nixda(); };
print "ok 5\n" if $@ =~ /^Can't find entry 'nixda\'/;

