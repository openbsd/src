# Digest::MD5 2.07 and older used to trigger a core dump when
# passed an illegal file handle that failed to open.

print "1..3\n";

use Digest::MD5 ();
use Config;

$md5 = Digest::MD5->new;

eval {
   use vars qw(*FOO);
   $md5->addfile(*FOO);
};
print "not " unless $@ =~ /^Bad filehandle: FOO at/;
print "ok 1\n";

open(BAR, "no-existing-file.$$");
eval {
    $md5->addfile(*BAR);
};
print "not " unless $@ =~ /^No filehandle passed at/;
print "ok 2\n";

open(BAR, ">no-existing-file.$$") || die;
eval {
    $md5->addfile(*BAR);
};
# Some stdio implementations don't find reading from
# write-only filehandle to be a problem.  Therefore we
# cannot expect this to fail reliably with stdio.
my $stdio = !exists  $Config{useperlio} ||
	    !defined $Config{useperlio} ||
	    (exists $ENV{PERLIO} && $ENV{PERLIO} eq 'stdio') ||
	    defined $Config{usefaststdio};
print "not " unless $@ =~ /^Reading from filehandle failed at/ || $stdio;
print "ok 3\n";

close(BAR);
unlink("no-existing-file.$$");
