#!/usr/local/bin/perl -w

use Test::More tests => 3;

BEGIN { use_ok('CGI'); };
use CGI (':standard','-no_debug','-no_xhtml');

# no_xhtml test on checkbox_group()
is(checkbox_group(-name       => 'game',
		  '-values'   => [qw/checkers chess cribbage/],
                  '-defaults' => ['cribbage']),
   qq(<input type="checkbox" name="game" value="checkers" >checkers <input type="checkbox" name="game" value="chess" >chess <input type="checkbox" name="game" value="cribbage" checked >cribbage),
   'checkbox_group()');

#  xhtml test on checkbox_group()
$CGI::XHTML = 1;
is(checkbox_group(-name       => 'game',
		  '-values'   => [qw/checkers chess cribbage/],
                  '-defaults' => ['cribbage']),
   qq(<label><input type="checkbox" name="game" value="checkers" />checkers</label> <label><input type="checkbox" name="game" value="chess" />chess</label> <label><input type="checkbox" name="game" value="cribbage" checked="checked" />cribbage</label>),
   'checkbox_group()');
