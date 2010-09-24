#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

plan 2;

my($dev_tty, $dev_null) = qw(/dev/tty /dev/null);
  ($dev_tty, $dev_null) = qw(con      nul      ) if $^O =~ /^(MSWin32|os2)$/;
  ($dev_tty, $dev_null) = qw(TT:      _NLA0:   ) if $^O eq "VMS";

SKIP: {
    open(my $tty, "<", $dev_tty)
	or skip("Can't open terminal '$dev_tty': $!");
    if ($^O eq 'VMS') {
        # TT might be a mailbox or other non-terminal device
        my $tt_dev = VMS::Filespec::vmspath('TT');
        skip("'$tt_dev' is probably not a terminal") if $tt_dev !~ m/^_(tt|ft|rt)/i;
    }
    ok(-t $tty, "'$dev_tty' is a TTY");
}
SKIP: {
    open(my $null, "<", $dev_null)
	or skip("Can't open null device '$dev_null': $!");
    ok(!-t $null, "'$dev_null' is not a TTY");
}
