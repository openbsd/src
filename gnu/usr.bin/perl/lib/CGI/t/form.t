#!/usr/local/bin/perl -w

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm
use lib qw(. ./blib/lib ./blib/arch);

use Test::More tests => 18;

BEGIN { use_ok('CGI'); };
use CGI (':standard','-no_debug','-tabindex');

my $CRLF = "\015\012";
if ($^O eq 'VMS') {
    $CRLF = "\n";  # via web server carriage is inserted automatically
}
if (ord("\t") != 9) { # EBCDIC?
    $CRLF = "\r\n";
}


# Set up a CGI environment
$ENV{REQUEST_METHOD}  = 'GET';
$ENV{QUERY_STRING}    = 'game=chess&game=checkers&weather=dull';
$ENV{PATH_INFO}       = '/somewhere/else';
$ENV{PATH_TRANSLATED} = '/usr/local/somewhere/else';
$ENV{SCRIPT_NAME}     ='/cgi-bin/foo.cgi';
$ENV{SERVER_PROTOCOL} = 'HTTP/1.0';
$ENV{SERVER_PORT}     = 8080;
$ENV{SERVER_NAME}     = 'the.good.ship.lollypop.com';

is(start_form(-action=>'foobar',-method=>'get'),
   qq(<form method="get" action="foobar" enctype="multipart/form-data">\n),
   "start_form()");

is(submit(),
   qq(<input type="submit" tabindex="1" name=".submit" />),
   "submit()");

is(submit(-name  => 'foo',
	  -value => 'bar'),
   qq(<input type="submit" tabindex="2" name="foo" value="bar" />),
   "submit(-name,-value)");

is(submit({-name  => 'foo',
	   -value => 'bar'}),
   qq(<input type="submit" tabindex="3" name="foo" value="bar" />),
   "submit({-name,-value})");

is(textfield(-name => 'weather'),
   qq(<input type="text" name="weather" tabindex="4" value="dull" />),
   "textfield({-name})");

is(textfield(-name  => 'weather',
	     -value => 'nice'),
   qq(<input type="text" name="weather" tabindex="5" value="dull" />),
   "textfield({-name,-value})");

is(textfield(-name     => 'weather',
	     -value    => 'nice',
	     -override => 1),
   qq(<input type="text" name="weather" tabindex="6" value="nice" />),
   "textfield({-name,-value,-override})");

is(checkbox(-name  => 'weather',
	    -value => 'nice'),
   qq(<label><input type="checkbox" name="weather" value="nice" tabindex="7" />weather</label>),
   "checkbox()");

is(checkbox(-name  => 'weather',
	    -value => 'nice',
	    -label => 'forecast'),
   qq(<label><input type="checkbox" name="weather" value="nice" tabindex="8" />forecast</label>),
   "checkbox()");

is(checkbox(-name     => 'weather',
	    -value    => 'nice',
	    -label    => 'forecast',
	    -checked  => 1,
	    -override => 1),
   qq(<label><input type="checkbox" name="weather" value="nice" tabindex="9" checked="checked" />forecast</label>),
   "checkbox()");

is(checkbox(-name  => 'weather',
	    -value => 'dull',
	    -label => 'forecast'),
   qq(<label><input type="checkbox" name="weather" value="dull" tabindex="10" checked="checked" />forecast</label>),
   "checkbox()");

is(radio_group(-name => 'game'),
   qq(<label><input type="radio" name="game" value="chess" checked="checked" tabindex="11" />chess</label> <label><input type="radio" name="game" value="checkers" tabindex="12" />checkers</label>),
   'radio_group()');

is(radio_group(-name   => 'game',
	       -labels => {'chess' => 'ping pong'}),
   qq(<label><input type="radio" name="game" value="chess" checked="checked" tabindex="13" />ping pong</label> <label><input type="radio" name="game" value="checkers" tabindex="14" />checkers</label>),
   'radio_group()');

is(checkbox_group(-name   => 'game',
		  -Values => [qw/checkers chess cribbage/]),
   qq(<label><input type="checkbox" name="game" value="checkers" checked="checked" tabindex="15" />checkers</label> <label><input type="checkbox" name="game" value="chess" checked="checked" tabindex="16" />chess</label> <label><input type="checkbox" name="game" value="cribbage" tabindex="17" />cribbage</label>),
   'checkbox_group()');

is(checkbox_group(-name       => 'game',
		  '-values'   => [qw/checkers chess cribbage/],
		  '-defaults' => ['cribbage'],
		  -override=>1),
   qq(<label><input type="checkbox" name="game" value="checkers" tabindex="18" />checkers</label> <label><input type="checkbox" name="game" value="chess" tabindex="19" />chess</label> <label><input type="checkbox" name="game" value="cribbage" checked="checked" tabindex="20" />cribbage</label>),
   'checkbox_group()');

is(popup_menu(-name     => 'game',
	      '-values' => [qw/checkers chess cribbage/],
	      -default  => 'cribbage',
	      -override => 1),
   '<select name="game" tabindex="21" >
<option value="checkers">checkers</option>
<option value="chess">chess</option>
<option selected="selected" value="cribbage">cribbage</option>
</select>',
   'popup_menu()');
is(scrolling_list(-name => 'game',
		  '-values' => [qw/checkers chess cribbage/],
		  -default => 'cribbage',
		  -override=>1),
   '<select name="game" tabindex="22"  size="3">
<option value="checkers">checkers</option>
<option value="chess">chess</option>
<option selected="selected" value="cribbage">cribbage</option>
</select>',
  'scrolling_list()');
