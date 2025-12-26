#!perl

# Test interaction of threads and directory handles.

BEGIN {
     chdir 't' if -d 't';
     @INC = '../lib';
     require './test.pl';
     $| = 1;

     require Config;
     skip_all_without_config('useithreads');
     skip_all_if_miniperl("no dynamic loading on miniperl, no threads");
     skip_all("runs out of memory on some EBCDIC") if $ENV{PERL_SKIP_BIG_MEM_TESTS};

     plan(1);
}

use strict;
use warnings;
use threads;

# Basic sanity check: make sure this does not crash
fresh_perl_is <<'# this is no comment', 'ok', {}, 'crash when duping dirh';
   use threads;
   opendir dir, 'op';
   async{}->join for 1..2;
   print "ok";
# this is no comment
