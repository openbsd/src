#!./perl
#
# Tests for perl exit codes, playing with $?, etc...


BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
}

# VMS and Windows need -e "...", most everything else works better with '
my $quote = $^O =~ /^(VMS|MSWin\d+)$/ ? q{"} : q{'};

# Run some code, return its wait status.
sub run {
    my($code) = shift;
    my $cmd = "$^X -e ";
    return system($cmd.$quote.$code.$quote);
}

BEGIN {
    # MacOS system() doesn't have good return value
    $numtests = ($^O eq 'VMS') ? 7 : ($^O eq 'MacOS') ? 0 : 3; 
}

require "test.pl";
plan(tests => $numtests);

if ($^O ne 'MacOS') {
my $exit, $exit_arg;

$exit = run('exit');
is( $exit >> 8, 0,              'Normal exit' );

if ($^O ne 'VMS') {

  $exit = run('exit 42');
  is( $exit >> 8, 42,             'Non-zero exit' );

} else {

# On VMS, successful returns from system() are always 0, warnings are 1,
# errors are 2, and fatal errors are 4.

  $exit = run("exit 196609"); # %CLI-S-NORMAL
  is( $exit >> 8, 0,             'success exit' );

  $exit = run("exit 196611");  # %CLI-I-NORMAL
  is( $exit >> 8, 0,             'informational exit' );

  $exit = run("exit 196608");  # %CLI-W-NORMAL
  is( $exit >> 8, 1,             'warning exit' );

  $exit = run("exit 196610");  # %CLI-E-NORMAL
  is( $exit >> 8, 2,             'error exit' );

  $exit = run("exit 196612");  # %CLI-F-NORMAL
  is( $exit >> 8, 4,             'fatal error exit' );
}

$exit_arg = 42;
$exit = run("END { \$? = $exit_arg }");

# On VMS, in the child process the actual exit status will be SS$_ABORT, 
# which is what you get from any non-zero value of $? that has been 
# dePOSIXified by STATUS_POSIX_SET.  In the parent process, all we'll 
# see are the severity bits (0-2) shifted left by 8.
$exit_arg = (44 & 7) if $^O eq 'VMS';  

is( $exit >> 8, $exit_arg,             'Changing $? in END block' );
}
