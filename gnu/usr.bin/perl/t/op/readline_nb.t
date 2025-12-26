#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
    require Config; Config->import;

    skip_all_if_miniperl();
}

$^O eq "MSWin32"
    and skip_all("Win32 can't make a pipe non-blocking");

use strict;
use IO::Select;

$Config{d_pipe}
  or skip_all("No pipe");

my ($in, $out);
pipe($in, $out)
  or skip_all("Cannot pipe: $!");

$in->blocking(0)
  or skip_all("Cannot make pipe non-blocking");

my $line = <$in>;
is($line, undef, "error reading");
ok(!$in->error, "but did not set error flag");
close $out;
$line = <$in>;
is($line, undef, "nothing to read, but eof");
ok(!$in->error, "still did not set error flag");
ok($in->eof, "did set eof");
ok(close($in), "close success");


done_testing();
