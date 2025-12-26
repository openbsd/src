#!perl
################################################################################
#
#  $Revision: 6 $
#  $Author: mhx $
#  $Date: 2010/03/07 16:01:42 +0100 $
#
################################################################################
#
#  Version 2.x, Copyright (C) 2007-2010, Marcus Holland-Moritz <mhx@cpan.org>.
#  Version 1.x, Copyright (C) 1999, Graham Barr <gbarr@pobox.com>.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

BEGIN {
  chdir 't' if -d 't';
  require "./test.pl";
  set_up_inc('../lib') if -d '../lib' && -d '../ext';

  require Config; Config->import;

  if ($ENV{'PERL_CORE'} && $Config{'extensions'} !~ m[\bIPC/SysV\b]) {
    skip_all('-- IPC::SysV was not built');
  }
  skip_all_if_miniperl();
  if ($Config{'d_shm'} ne 'define') {
    skip_all('-- $Config{d_shm} undefined');
  }
}


use sigtrap qw/die normal-signals error-signals/;
use IPC::SysV qw/ IPC_PRIVATE S_IRWXU IPC_RMID /;

my $key;
END { shmctl $key, IPC_RMID, 0 if defined $key }

{
	local $SIG{SYS} = sub { skip_all("SIGSYS caught") } if exists $SIG{SYS};
	$key = shmget IPC_PRIVATE, 8, S_IRWXU;
}

if (not defined $key) {
  my $info = "shmget() failed: $!";
  if ($! == &IPC::SysV::ENOSPC || $! == &IPC::SysV::ENOSYS ||
      $! == &IPC::SysV::ENOMEM || $! == &IPC::SysV::EACCES) {
    skip_all($info);
  }
  else {
    die $info;
  }
}
else {
	plan(tests => 33);
	pass('acquired shared mem');
}

ok(shmwrite($key, pack("N", 4711), 0, 4), 'write(offs=0)');
ok(shmwrite($key, pack("N", 210577), 4, 4), 'write(offs=4)');

my $var;
ok(shmread($key, $var, 0, 4), 'read(offs=0) returned ok');
is($var, pack("N", 4711), 'read(offs=0) correct');
ok(shmread($key, $var, 4, 4), 'read(offs=4) returned ok');
is($var, pack("N", 210577), 'read(offs=4) correct');

ok(shmwrite($key, "Shared", 1, 6), 'write(offs=1)');

ok(shmread($key, $var, 1, 6), 'read(offs=1) returned ok');
is($var, 'Shared', 'read(offs=1) correct');
ok(shmwrite($key,"Memory", 0, 6), 'write(offs=0)');

my $number = 1;
my $int = 2;
shmwrite $key, $int, 0, 1;
shmread $key, $number, 0, 1;
is("$number", $int, qq{"\$id" eq "$int"});
cmp_ok($number + 0, '==', $int, "\$id + 0 == $int");

my ($fetch, $store) = (0, 0);
{ package Counted;
  sub TIESCALAR { bless [undef] }
  sub FETCH     { ++$fetch; $_[0][0] }
  sub STORE     { ++$store; $_[0][0] = $_[1] } }
tie my $ct, 'Counted';
shmread $key, $ct, 0, 1;
is($fetch, 0, "shmread FETCH none");
is($store, 1, "shmread STORE once");
($fetch, $store) = (0, 0);
shmwrite $key, $ct, 0, 1;
is($fetch, 1, "shmwrite FETCH once");
is($store, 0, "shmwrite STORE none");

{
    # check reading into an upgraded buffer is sane
    my $text = "\xC0\F0AB";
    ok(shmwrite($key, $text, 0, 4), "setup text");
    my $rdbuf = "\x{101}";
    ok(shmread($key, $rdbuf, 0, 4), "read it back");
    is($rdbuf, $text, "check we got back the expected");

    # check writing from an upgraded buffer
    utf8::upgrade(my $utext = $text);
    ok(shmwrite($key, $utext, 0, 4), "setup text (upgraded source)");
    $rdbuf = "";
    ok(shmread($key, $rdbuf, 0, 4), "read it back (upgraded source)");
    is($rdbuf, $text, "check we got back the expected (upgraded source)");
}

# GH #22898 - reading into reference is sane
{
    my $rdbuf = [];
    builtin::weaken(my $wref = $rdbuf);

    my $text = 'A';
    ok(shmwrite($key, $text, 0, 1), "wrote 'A' to shared segment");
    ok(shmread($key, $rdbuf, 0, 1), "read 1 byte into buffer that previously stored a ref");
    is(ref($rdbuf), '', "buffer is not a reference anymore");
    is($rdbuf, $text, "buffer contains expected string");
    is($wref, undef, "no leak: referenced object had refcount decremented");
}

# GH #22895 - 2^31 boundary
SKIP: {
    skip("need at least 5GB of memory for this test", 5)
        unless ($ENV{PERL_TEST_MEMORY} // 0) >= 5;

    # delete previous allocation
    shmctl $key, IPC_RMID, 0;
    $key = undef;

    my $int32_max = 0x7fff_ffff;
    $key = shmget(IPC_PRIVATE, $int32_max+2, S_IRWXU) // die "shmget(2GB+1) failed: $!";
    my $bigbuf = 'A' x $int32_max;
    ok(shmwrite($key, $bigbuf, 0, length($bigbuf)), "wrote $int32_max bytes");
    $bigbuf .= 'X';
    ok(shmwrite($key, $bigbuf, 0, length($bigbuf)), "wrote $int32_max+1 bytes");
    my $smallbuf = 'X';
    ok(shmwrite($key, $smallbuf, $int32_max, 1), "wrote 1 byte at offset $int32_max");
    ok(shmwrite($key, $smallbuf, $int32_max+1, 1), "wrote 1 byte at offset $int32_max+1");
    my $int30x = 0x4000_0000;
    ok(shmwrite($key, $bigbuf, $int30x, $int30x), "wrote $int30x bytes at offset $int30x");
}
