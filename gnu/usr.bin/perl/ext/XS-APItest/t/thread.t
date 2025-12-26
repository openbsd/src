#!perl
use warnings;
use strict;
use Test2::Tools::Basic;
use Config;

BEGIN {
    skip_all "Not pthreads or is win32"
      if !$Config{usethreads} || $^O eq "MSWin32";
}

use XS::APItest qw(thread_id_matches);

ok(thread_id_matches(),
   "check main thread id saved and is current thread");

done_testing();
