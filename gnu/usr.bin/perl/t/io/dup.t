#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

my $test = 1;
print "1..8\n";
print "ok 1\n";

open(DUPOUT,">&STDOUT");
open(DUPERR,">&STDERR");

open(STDOUT,">Io.dup")  || die "Can't open stdout";
open(STDERR,">&STDOUT") || die "Can't open stderr";

select(STDERR); $| = 1;
select(STDOUT); $| = 1;

print STDOUT "ok 2\n";
print STDERR "ok 3\n";

# Since some systems don't have echo, we use Perl.
$echo = qq{$^X -le "print q(ok %d)"};

$cmd = sprintf $echo, 4;
print `$cmd`;

$cmd = sprintf "$echo 1>&2", 5;
$cmd = sprintf $echo, 5 if $^O eq 'MacOS';  # don't know if we can do this ...
print `$cmd`;

# KNOWN BUG system() does not honor STDOUT redirections on VMS.
if( $^O eq 'VMS' ) {
    print "not ok $_ # TODO system() not honoring STDOUT redirect on VMS\n" 
      for 6..7;
}
else {
    system sprintf $echo, 6;
    if ($^O eq 'MacOS') {
        system sprintf $echo, 7;
    }
    else {
        system sprintf "$echo 1>&2", 7;
    }
}

close(STDOUT) or die "Could not close: $!";
close(STDERR) or die "Could not close: $!";

open(STDOUT,">&DUPOUT") or die "Could not open: $!";
open(STDERR,">&DUPERR") or die "Could not open: $!";

if (($^O eq 'MSWin32') || ($^O eq 'NetWare') || ($^O eq 'VMS')) { print `type Io.dup` }
elsif ($^O eq 'MacOS') { system 'catenate Io.dup' }
else                   { system 'cat Io.dup' }
unlink 'Io.dup';

print STDOUT "ok 8\n";

