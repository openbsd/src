# -*- Mode: cperl; coding: utf-8; cperl-indent-level: 2 -*-
#!/usr/local/bin/perl -w

use strict;
use lib qw(t/lib);

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm
use lib qw(blib/lib blib/arch);

use Test::More tests => 47;
use IO::Handle;

BEGIN { use_ok('CGI::Carp') };

#-----------------------------------------------------------------------------
# Test id
#-----------------------------------------------------------------------------

# directly invoked
my $expect_f = __FILE__;
my $expect_l = __LINE__ + 1;
my ($file, $line, $id) = CGI::Carp::id(0);
is($file, $expect_f, "file");
is($line, $expect_l, "line");
is($id, "carp.t", "id");

# one level of indirection
sub id1 { my $level = shift; return CGI::Carp::id($level); };

$expect_l = __LINE__ + 1;
($file, $line, $id) = id1(1);
is($file, $expect_f, "file");
is($line, $expect_l, "line");
is($id, "carp.t", "id");

# two levels of indirection
sub id2 { my $level = shift; return id1($level); };

$expect_l = __LINE__ + 1;
($file, $line, $id) = id2(2);
is($file, $expect_f, "file");
is($line, $expect_l, "line");
is($id, "carp.t", "id");

#-----------------------------------------------------------------------------
# Test stamp
#-----------------------------------------------------------------------------

my $stamp = "/^\\[
      ([a-z]{3}\\s){2}\\s?
      [\\s\\d:]+
      \\]\\s$id:/ix";

like(CGI::Carp::stamp(),
     $stamp,
     "Time in correct format");

sub stamp1 {return CGI::Carp::stamp()};
sub stamp2 {return stamp1()};

like(stamp2(), $stamp, "Time in correct format");

#-----------------------------------------------------------------------------
# Test warn and _warn
#-----------------------------------------------------------------------------

# set some variables to control what's going on.
$CGI::Carp::WARN = 0;
$CGI::Carp::EMIT_WARNINGS = 0;
@CGI::Carp::WARNINGS = ();
my $q_file = quotemeta($file);


# Test that realwarn is called
{
  local $^W = 0;
  eval "sub CGI::Carp::realwarn {return 'Called realwarn'};";
}

$expect_l = __LINE__ + 1;
is(CGI::Carp::warn("There is a problem"),
   "Called realwarn",
   "CGI::Carp::warn calls CORE::warn");
is(@CGI::Carp::WARNINGS, 0, "_warn not called");

# Test that message is constructed correctly
eval 'sub CGI::Carp::realwarn {my $mess = shift; return $mess};';

$expect_l = __LINE__ + 1;
like(CGI::Carp::warn("There is a problem"),
   "/] $id: There is a problem at $q_file line $expect_l.".'$/',
   "CGI::Carp::warn builds correct message");
is(@CGI::Carp::WARNINGS, 0, "_warn not called");

# Test that _warn is called at the correct time
$CGI::Carp::WARN = 1;

$expect_l = __LINE__ + 1;
like(CGI::Carp::warn("There is a problem"),
   "/] $id: There is a problem at $q_file line $expect_l.".'$/',
   "CGI::Carp::warn builds correct message");

is(@CGI::Carp::WARNINGS, 1, "_warn now called");
like($CGI::Carp::WARNINGS[0],
   "/There is a problem at $q_file line $expect_l.".'$/',
   "CGI::Carp::WARNINGS has correct message (without stamp)");

#-----------------------------------------------------------------------------
# Test ineval
#-----------------------------------------------------------------------------

ok(!CGI::Carp::ineval, 'ineval returns false when not in eval');
eval {ok(CGI::Carp::ineval, 'ineval returns true when in eval');};

#-----------------------------------------------------------------------------
# Test die
#-----------------------------------------------------------------------------

# set some variables to control what's going on.
$CGI::Carp::WRAP = 0;

$expect_l = __LINE__ + 1;
eval { CGI::Carp::die('There is a problem'); };
like($@,
     '/^There is a problem/',
     'CGI::Carp::die calls CORE::die without altering argument in eval');

# Test that realwarn is called
{
  local $^W = 0;
  eval 'sub CGI::Carp::realdie {my $mess = shift; return $mess};';
}

like(CGI::Carp::die('There is a problem'),
     $stamp,
     'CGI::Carp::die calls CORE::die, but adds stamp');

#-----------------------------------------------------------------------------
# Test set_message
#-----------------------------------------------------------------------------

is(CGI::Carp::set_message('My new Message'),
   'My new Message',
   'CGI::Carp::set_message returns new message');

is($CGI::Carp::CUSTOM_MSG,
   'My new Message',
   'CGI::Carp::set_message message set correctly');

# set the message back to the empty string so that the tests later
# work properly.
CGI::Carp::set_message(''),

#-----------------------------------------------------------------------------
# Test set_progname
#-----------------------------------------------------------------------------

import CGI::Carp qw(name=new_progname);
is($CGI::Carp::PROGNAME,
     'new_progname',
     'CGI::Carp::import set program name correctly');

is(CGI::Carp::set_progname('newer_progname'),
   'newer_progname',
   'CGI::Carp::set_progname returns new program name');

is($CGI::Carp::PROGNAME,
   'newer_progname',
   'CGI::Carp::set_progname program name set correctly');

# set the message back to the empty string so that the tests later
# work properly.
is (CGI::Carp::set_progname(undef),undef,"CGI::Carp::set_progname returns unset name correctly");
is ($CGI::Carp::PROGNAME,undef,"CGI::Carp::set_progname program name unset correctly");

#-----------------------------------------------------------------------------
# Test warnings_to_browser
#-----------------------------------------------------------------------------

CGI::Carp::warningsToBrowser(0);
is($CGI::Carp::EMIT_WARNINGS, 0, "Warnings turned off");
unless( is(@CGI::Carp::WARNINGS, 1, "_warn not called") ) {
  print join "\n", map "'$_'", @CGI::Carp::WARNINGS;
}

# turn off STDOUT (prevents spurious warnings to screen
tie *STDOUT, 'StoreStuff' or die "Can't tie STDOUT";
CGI::Carp::warningsToBrowser(1);
my $fake_out = join '', <STDOUT>;
untie *STDOUT;

open(STDOUT, ">&REAL_STDOUT");
my $fname = $0;
$fname =~ tr/<>-/\253\273\255/; # _warn does this so we have to also
is( $fake_out, "<!-- warning: There is a problem at $fname line 100. -->\n",
                        'warningsToBrowser() on' );

is($CGI::Carp::EMIT_WARNINGS, 1, "Warnings turned off");
is(@CGI::Carp::WARNINGS, 0, "_warn is called");

#-----------------------------------------------------------------------------
# Test fatals_to_browser
#-----------------------------------------------------------------------------

package StoreStuff;

sub TIEHANDLE {
  my $class = shift;
  bless [], $class;
}

sub PRINT {
  my $self = shift;
  push @$self, @_;
}

sub READLINE {
  my $self = shift;
  shift @$self;
}

package main;

tie *STDOUT, "StoreStuff";

# do tests
my @result;

CGI::Carp::fatalsToBrowser();
$result[0] .= $_ while (<STDOUT>);

CGI::Carp::fatalsToBrowser('Message to the world');
$result[1] .= $_ while (<STDOUT>);

$ENV{SERVER_ADMIN} = 'foo@bar.com';
CGI::Carp::fatalsToBrowser();
$result[2] .= $_ while (<STDOUT>);

CGI::Carp::set_message('Override the message passed in'),

CGI::Carp::fatalsToBrowser('Message to the world');
$result[3] .= $_ while (<STDOUT>);
CGI::Carp::set_message(''),
delete $ENV{SERVER_ADMIN};

# now restore STDOUT
untie *STDOUT;


like($result[0],
     '/Content-type: text/html/',
     "Default string has header");

ok($result[0] !~ /Message to the world/, "Custom message not in default string");

like($result[1],
    '/Message to the world/',
    "Custom Message appears in output");

ok($result[0] !~ /foo\@bar.com/, "Server Admin does not appear in default message");

like($result[2],
    '/foo@bar.com/',
    "Server Admin appears in output");

like($result[3],
     '/Message to the world/',
     "Custom message not in result");

like($result[3],
     '/Override the message passed in/',
     "Correct message in string");

#-----------------------------------------------------------------------------
# Test to_filehandle
#-----------------------------------------------------------------------------

sub buffer {
  CGI::Carp::to_filehandle (@_);
}

tie *STORE, "StoreStuff";

require FileHandle;
my $fh = FileHandle->new;

ok( defined buffer(\*STORE),       '\*STORE returns proper filehandle');
ok( defined buffer( $fh ),         '$fh returns proper filehandle');
ok( defined buffer('::STDOUT'),    'STDIN returns proper filehandle');
ok( defined buffer(*main::STDOUT), 'STDIN returns proper filehandle');
ok(!defined buffer("WIBBLE"),      '"WIBBLE" doesn\'t returns proper filehandle');
