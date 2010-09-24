#!perl -w

# Form-related tests for CGI.pm
# If you are adding or updated tests, please put tests for each methods in
# their own file, rather than growing this file any larger. 

use Test::More 'no_plan';
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

is(checkbox_group(-name   => 'game',
		  -Values => [qw/checkers chess cribbage/],
		 -disabled => ['checkers']),
   qq(<label><input type="checkbox" name="game" value="checkers" checked="checked" tabindex="23" disabled='1'/><span style="color:gray">checkers</span></label> <label><input type="checkbox" name="game" value="chess" checked="checked" tabindex="24" />chess</label> <label><input type="checkbox" name="game" value="cribbage" tabindex="25" />cribbage</label>),
   'checkbox_group()');

my $optgroup = optgroup(-name=>'optgroup_name',
                        -Values => ['moe','catch'],
                        -attributes=>{'catch'=>{'class'=>'red'}});

is($optgroup, 
    qq(<optgroup label="optgroup_name">
<option value="moe">moe</option>
<option class="red" value="catch">catch</option>
</optgroup>),
    'optgroup()');

is(popup_menu(-name=>'menu_name',
              -Values=>[qw/eenie meenie minie/, $optgroup],
              -labels=>{'eenie'=>'one',
                        'meenie'=>'two',
                        'minie'=>'three'},
              -default=>'meenie'),
    qq(<select name="menu_name" tabindex="26" >
<option value="eenie">one</option>
<option selected="selected" value="meenie">two</option>
<option value="minie">three</option>
<optgroup label="optgroup_name">
<option value="moe">moe</option>
<option class="red" value="catch">catch</option>
</optgroup>
</select>),
    'popup_menu() + optgroup()');

is(scrolling_list(-name=>'menu_name',
              -Values=>[qw/eenie meenie minie/, $optgroup],
              -labels=>{'eenie'=>'one',
                        'meenie'=>'two',
                        'minie'=>'three'},
              -default=>'meenie'),
    qq(<select name="menu_name" tabindex="27"  size="4">
<option value="eenie">one</option>
<option selected="selected" value="meenie">two</option>
<option value="minie">three</option>
<optgroup label="optgroup_name">
<option value="moe">moe</option>
<option class="red" value="catch">catch</option>
</optgroup>
</select>),
    'scrolling_list() + optgroup()');

# ---------- START 22046 ----------
# The following tests were added for
# https://rt.cpan.org/Public/Bug/Display.html?id=22046
#     SHCOREY at cpan.org
# Saved whether working with XHTML because need to test both
# with it and without.
my $saved_XHTML = $CGI::XHTML;

# set XHTML
$CGI::XHTML = 1;

is(start_form("GET","/foobar"),
    qq{<form method="get" action="/foobar" enctype="multipart/form-data">
},
    'start_form() + XHTML');

is(start_form("GET", "/foobar",&CGI::URL_ENCODED),
    qq{<form method="get" action="/foobar" enctype="application/x-www-form-urlencoded">
},
    'start_form() + XHTML + URL_ENCODED');

is(start_form("GET", "/foobar",&CGI::MULTIPART),
    qq{<form method="get" action="/foobar" enctype="multipart/form-data">
},
    'start_form() + XHTML + MULTIPART');

is(start_multipart_form("GET", "/foobar"),
    qq{<form method="get" action="/foobar" enctype="multipart/form-data">
},
    'start_multipart_form() + XHTML');

is(start_multipart_form("GET", "/foobar","name=\"foobar\""),
    qq{<form method="get" action="/foobar" enctype="multipart/form-data" name="foobar">
},
    'start_multipart_form() + XHTML + additional args');

# set no XHTML
$CGI::XHTML = 0;

is(start_form("GET","/foobar"),
    qq{<form method="get" action="/foobar" enctype="application/x-www-form-urlencoded">
},
    'start_form() + NO_XHTML');

is(start_form("GET", "/foobar",&CGI::URL_ENCODED),
    qq{<form method="get" action="/foobar" enctype="application/x-www-form-urlencoded">
},
    'start_form() + NO_XHTML + URL_ENCODED');

is(start_form("GET", "/foobar",&CGI::MULTIPART),
    qq{<form method="get" action="/foobar" enctype="multipart/form-data">
},
    'start_form() + NO_XHTML + MULTIPART');

is(start_multipart_form("GET", "/foobar"),
    qq{<form method="get" action="/foobar" enctype="multipart/form-data">
},
    'start_multipart_form() + NO_XHTML');

is(start_multipart_form("GET", "/foobar","name=\"foobar\""),
    qq{<form method="get" action="/foobar" enctype="multipart/form-data" name="foobar">
},
    'start_multipart_form() + NO_XHTML + additional args');

# restoring value
$CGI::XHTML = $saved_XHTML;
