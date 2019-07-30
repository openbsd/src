#
# $Id: cow.t,v 1.1 2013/08/29 16:47:39 dankogai Exp $
#
use strict;
use Encode ();
use Test::More tests => 2;


my %a = ( "L\x{c3}\x{a9}on" => "acme" );
my ($k) = ( keys %a );
Encode::_utf8_on($k);
my %h = ( $k => "acme" );
is $h{"L\x{e9}on"} => 'acme';
($k) = ( keys %h );
Encode::_utf8_off($k);
%a = ( $k => "acme" );
is $h{"L\x{e9}on"} => 'acme';
# use Devel::Peek;
# Dump(\%h);

