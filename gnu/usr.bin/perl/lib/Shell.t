#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test::More tests => 4;

BEGIN { use_ok('Shell'); }

my $Is_VMS     = $^O eq 'VMS';
my $Is_MSWin32 = $^O eq 'MSWin32';
my $Is_NetWare = $^O eq 'NetWare';

$Shell::capture_stderr = 1; #

# Now test that that works ..

my $tmpfile = 'sht0001';

while ( -f $tmpfile )
{
  $tmpfile++;
}

END { -f $tmpfile && (open STDERR, '>&SAVERR' and unlink $tmpfile) };



open(SAVERR,">&STDERR") ;
open(STDERR, ">$tmpfile");

xXx();  # Ok someone could have a program called this :(

# On os2 the warning is on by default...
ok( ($^O eq 'os2' xor !(-s $tmpfile)) ,'$Shell::capture_stderr');

$Shell::capture_stderr = 0; #

# someone will have to fill in the blanks for other platforms

if ( $Is_VMS )
{
    ok(directory(),'Execute command');
    my @files = directory('*.*');
    ok(@files,'Quoted arguments');
}
elsif( $Is_MSWin32 )
{
  ok(dir(),'Execute command');

  my @files = dir('*.*');

  ok(@files, 'Quoted arguments');
}
else
{
  ok(ls(),'Execute command');

  my @files = ls('*');

  ok(@files,'Quoted arguments');

}
open(STDERR,">&SAVERR") ;
