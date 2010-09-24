#!/usr/local/bin/perl -w

use Test::More tests => 2;

BEGIN{ use_ok('CGI'); }

can_ok('CGI', qw/cookie param/);
