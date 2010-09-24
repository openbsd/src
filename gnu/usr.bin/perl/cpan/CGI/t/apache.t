#!/usr/local/bin/perl -w

use strict;
use Test::More tests => 1;

# Can't do much with this other than make sure it loads properly
BEGIN { use_ok('CGI::Apache') };
