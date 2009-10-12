#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

print "1..28\n";

$TST = 'TST';

$Is_Dosish = ($^O eq 'MSWin32' or $^O eq 'NetWare' or $^O eq 'dos' or
              $^O eq 'os2' or $^O eq 'mint' or $^O eq 'cygwin' or
              $^O =~ /^uwin/);

open($TST, 'harness') || (die "Can't open harness");
binmode $TST if $Is_Dosish;
if (eof(TST)) { print "not ok 1\n"; } else { print "ok 1\n"; }

$firstline = <$TST>;
$secondpos = tell;

$x = 0;
while (<TST>) {
    if (eof) {$x++;}
}
if ($x == 1) { print "ok 2\n"; } else { print "not ok 2\n"; }

$lastpos = tell;

unless (eof) { print "not ok 3\n"; } else { print "ok 3\n"; }

if (seek($TST,0,0)) { print "ok 4\n"; } else { print "not ok 4\n"; }

if (eof) { print "not ok 5\n"; } else { print "ok 5\n"; }

if ($firstline eq <TST>) { print "ok 6\n"; } else { print "not ok 6\n"; }

if ($secondpos == tell) { print "ok 7\n"; } else { print "not ok 7\n"; }

if (seek(TST,0,1)) { print "ok 8\n"; } else { print "not ok 8\n"; }

if (eof($TST)) { print "not ok 9\n"; } else { print "ok 9\n"; }

if ($secondpos == tell) { print "ok 10\n"; } else { print "not ok 10\n"; }

if (seek(TST,0,2)) { print "ok 11\n"; } else { print "not ok 11\n"; }

if ($lastpos == tell) { print "ok 12\n"; } else { print "not ok 12\n"; }

unless (eof) { print "not ok 13\n"; } else { print "ok 13\n"; }

if ($. == 0) { print "not ok 14\n"; } else { print "ok 14\n"; }

$curline = $.;
open(OTHER, 'harness') || (die "Can't open harness: $!");
binmode OTHER if (($^O eq 'MSWin32') || ($^O eq 'NetWare'));

{
    local($.);

    if ($. == 0) { print "not ok 15\n"; } else { print "ok 15\n"; }

    tell OTHER;
    if ($. == 0) { print "ok 16\n"; } else { print "not ok 16\n"; }

    $. = 5;
    scalar <OTHER>;
    if ($. == 6) { print "ok 17\n"; } else { print "not ok 17\n"; }
}

if ($. == $curline) { print "ok 18\n"; } else { print "not ok 18\n"; }

{
    local($.);

    scalar <OTHER>;
    if ($. == 7) { print "ok 19\n"; } else { print "not ok 19\n"; }
}

if ($. == $curline) { print "ok 20\n"; } else { print "not ok 20\n"; }

{
    local($.);

    tell OTHER;
    if ($. == 7) { print "ok 21\n"; } else { print "not ok 21\n"; }
}

close(OTHER);
{
    no warnings 'closed';
    if (tell(OTHER) == -1)  { print "ok 22\n"; } else { print "not ok 22\n"; }
}
{
    no warnings 'unopened';
    if (tell(ETHER) == -1)  { print "ok 23\n"; } else { print "not ok 23\n"; }
}

# ftell(STDIN) (or any std streams) is undefined, it can return -1 or
# something else.  ftell() on pipes, fifos, and sockets is defined to
# return -1.

my $written = tempfile();

close($TST);
open($tst,">$written")  || die "Cannot open $written:$!";
binmode $tst if $Is_Dosish;

if (tell($tst) == 0) { print "ok 24\n"; } else { print "not ok 24\n"; }

print $tst "fred\n";

if (tell($tst) == 5) { print "ok 25\n"; } else { print "not ok 25\n"; }

print $tst "more\n";

if (tell($tst) == 10) { print "ok 26\n"; } else { print "not ok 26\n"; }

close($tst);

open($tst,"+>>$written")  || die "Cannot open $written:$!";
binmode $tst if $Is_Dosish;

if (0) 
{
 # :stdio does not pass these so ignore them for now 

if (tell($tst) == 0) { print "ok 27\n"; } else { print "not ok 27\n"; }

$line = <$tst>;

if ($line eq "fred\n") { print "ok 29\n"; } else { print "not ok 29\n"; }

if (tell($tst) == 5) { print "ok 30\n"; } else { print "not ok 30\n"; }

}

print $tst "xxxx\n";

if (tell($tst) == 15 ||
    tell($tst) == 5) # unset PERLIO or PERLIO=stdio (e.g. HP-UX, Solaris)
{ print "ok 27\n"; } else { print "not ok 27\n"; }

close($tst);

open($tst,">$written")  || die "Cannot open $written:$!";
print $tst "foobar";
close $tst;
open($tst,">>$written")  || die "Cannot open $written:$!";

# This test makes a questionable assumption that the file pointer will
# be at eof after opening a file but before seeking, reading, or writing.
# Only known failure is on cygwin.
my $todo = $^O eq "cygwin" && &PerlIO::get_layers($tst) eq 'stdio'
    && ' # TODO: file pointer not at eof';

if (tell($tst) == 6)
{ print "ok 28$todo\n"; } else { print "not ok 28$todo\n"; }
close $tst;

