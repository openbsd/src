#!/usr/local/bin/perl -w

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm

use lib qw(blib/lib blib/arch);

use Test::More tests => 2;

BEGIN{ use_ok('CGI'); }

can_ok('CGI', qw/cookie param/);