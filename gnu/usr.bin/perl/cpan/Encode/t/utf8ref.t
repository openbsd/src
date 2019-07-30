#
# $Id: utf8ref.t,v 1.1 2010/09/18 18:39:51 dankogai Exp $
#

use strict;
use warnings;
use Encode;
use Test::More;
plan tests => 4;
#plan 'no_plan';

# my $a = find_encoding('ASCII');
my $u = find_encoding('UTF-8');
my $r = [];
no warnings 'uninitialized';
is encode_utf8($r), ''.$r;
is $u->encode($r), '';
$r = {};
is decode_utf8($r), ''.$r;
is $u->decode($r), '';
