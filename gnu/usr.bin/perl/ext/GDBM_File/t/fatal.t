#!./perl -w
use strict;

use Test::More;
use Config;

BEGIN {
    plan(skip_all => "GDBM_File was not built")
	unless $Config{extensions} =~ /\bGDBM_File\b/;

    plan(tests => 8);
    use_ok('GDBM_File');
}

unlink <Op_dbmx*>;

open my $fh, $^X or die "Can't open $^X: $!";
my $fileno = fileno $fh;
isnt($fileno, undef, "Can find next available file descriptor");
close $fh or die $!;

is((open $fh, "<&=$fileno"), undef,
   "Check that we cannot open fileno $fileno. \$! is $!");

umask(0);
my %h;
isa_ok(tie(%h, 'GDBM_File', 'Op_dbmx', GDBM_WRCREAT, 0640), 'GDBM_File');

isnt((open $fh, "<&=$fileno"), undef, "dup fileno $fileno")
    or diag("\$! = $!");
isnt(close $fh, undef,
     "close fileno $fileno, out from underneath the GDBM_File");
is(eval {
    $h{Perl} = 'Rules';
    untie %h;
    1;
}, undef, 'Trapped error when attempting to write to knobbled GDBM_File');

# Observed "File write error" and "lseek error" from two different systems.
# So there might be more variants. Important part was that we trapped the error
# via croak.
like($@, qr/ at .*\bfatal\.t line \d+\.\n\z/,
     'expected error message from GDBM_File');

unlink <Op_dbmx*>;
