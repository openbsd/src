BEGIN {
    chdir 't' if -d 't/lib';
    @INC = '../lib' if -d 'lib';
    require Config; import Config;
    if (-d 'lib' and $Config{'extensions'} !~ /\bOS2(::|\/)REXX\b/) {
	print "1..0\n";
	exit 0;
    }
}

print "1..5\n";

require OS2::DLL;
print "ok 1\n";
$emx_dll = OS2::DLL->load('emx');
print "ok 2\n";
$emx_version = $emx_dll->emx_revision();
print "ok 3\n";
$emx_version >= 40 or print "not ";	# We cannot work with old EMXs
print "ok 4\n";

$reason = '';
$emx_version >= 99 and $reason = ' # skipped: version of EMX 100 or more';	# Be safe
print "ok 5$reason\n";
