#!/usr/local/bin/perl -w

use lib qw(t/lib);

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm
use lib qw(blib/lib blib/arch);

use strict;
use Test::More tests => 1;

# Can't do much with this other than make sure it loads properly
BEGIN { use_ok('CGI::Switch') };
