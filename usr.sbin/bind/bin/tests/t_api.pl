#!/usr/local/bin/perl
#
# Copyright (C) 1999-2001  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: t_api.pl,v 1.7 2001/01/09 21:41:43 bwelling Exp $

require "getopts.pl";

#
# a minimalistic test api in perl compatable with the C api
# used for the bind 9 regression tests
#

sub t_info {
	package t_api;
	local($format, @rest) = @_;
	printf("I:${format}%s", @rest);
}

sub t_result {
	package t_api;
	local($result) = @_;
	$T_inresult = 1;
	printf("R:$result\n");
}

sub t_assert {
	package t_api;
	local($component, $anum, $class, $what, @rest) = @_;
	printf("A:%s:%d:%s:$what\n", $component, $anum, $class, @rest);
}

sub t_getenv {
	package t_api;
	local($name) = @_;
	return($T_env{$name}) if (defined($T_env{$name}));
}

package t_api;

$| = 1;

sub t_on_abort {
	$T_aborted = 1;
	&t_info("got abort\n");
	die;
}

sub t_on_alarm {
	$T_timedout = 1;
	&t_info("got alarm\n");
	die;
}

sub t_on_int {
	$T_terminated = 1;
	&t_info("got int\n");
	die;
}

# initialize the test environment
sub t_initconf {
	local($cfile) = @_;
	local($name, $value);

	if ((-f $cfile) && (-s _)) {
		open(XXX, "< $cfile");
		while (<XXX>) {
			next if (/^\#/);
			next unless (/=/);
			chop;
			($name, $value) = split(/=/, $_, 2);
			$T_env{$name} = $value;
		}
		close(XXX);
	}
}

# dump the configuration to the journal
sub t_dumpconf {
	local($name, $value);

	foreach $name (sort keys %T_env) {
		&main't_info("%s\t%s\n", $name, $T_env{$name});
	}
}

# run a test
sub doTestN {
	package main;
	local($testnumber) = @_;
	local($status);

	if (defined($T_testlist[$testnumber])) {

		$t_api'T_inresult	= 0;
		$t_api'T_aborted	= 0;
		$t_api'T_timedout	= 0;
		$t_api'T_terminated	= 0;
		$t_api'T_unresolved	= 0;

		alarm($t_api'T_timeout);
		$status = eval($T_testlist[$testnumber]);
		alarm(0);

		if (! defined($status)) {
			&t_info("The test case timed out\n") if ($t_api'T_timedout);
			&t_info("The test case was terminated\n") if ($t_api'T_terminated);
			&t_info("The test case was aborted\n") if ($t_api'T_aborted);
			&t_result("UNRESOLVED");
		}
		elsif (! $t_api'T_inresult) {
			&t_result("NORESULT");
		}
	}
	else {
		&t_info("Test %d is not defined\n", $testnumber);
		&t_result("UNTESTED");
	}
}

$T_usage = "Usage:
	a               : run all tests
        b <dir>         : cd to dir before running tests
        c <configfile>  : use configfile instead of t_config
        d <level>       : set debug level to level
        h               : print test info                       (not implemented)
        u               : print usage info
        n <testnumber>  : run test number testnumber
        t <name>        : run test named testname		(not implemented)
        q <seconds>     : use seconds as the timeout value
        x               : don't execute tests in a subproc      (n/a)
";

# get command line args
&main'Getopts('ab:c:d:hun:t:q:x');

# if -u, print usage and exit
if (defined($main'opt_u)) {
	print $T_usage;
	exit(0);
}

# implement -h and -t after we add test descriptions to T_testlist ZZZ
if (defined($main'opt_h)) {
	print "the -h option is not implemented\n";
	exit(0);
}

if (defined($main'opt_t)) {
	print "the -t option is not implemented\n";
	exit(0);
}

#
# silently ignore the -x option
# this exists in the C version of the api
# to facilitate exception debugging with gdb
# and is not meaningful here
#

$T_configfile	= "t_config";
$T_debug	= 0;
$T_timeout	= 10;
$T_testnum	= -1;

$T_dir		= $main'opt_b if (defined($main'opt_b));
$T_debug	= $main'opt_d if (defined($main'opt_d));
$T_configfile	= $main'opt_c if (defined($main'opt_c));
$T_testnum	= $main'opt_n if (defined($main'opt_n));
$T_timeout	= $main'opt_q if (defined($main'opt_q));

$SIG{'ABRT'} = 't_api\'t_on_abort';
$SIG{'ALRM'} = 't_api\'t_on_alarm';
$SIG{'INT'}  = 't_api\'t_on_int';
$SIG{'QUIT'} = 't_api\'t_on_int';

# print the start line
$date = `date`;
chop $date;
($cmd = $0) =~ s/\.\///g;
printf("S:$cmd:$date\n");

# initialize the test environment
&t_initconf($T_configfile);
&t_dumpconf() if ($T_debug);

# establish working directory if requested
chdir("$T_dir") if (defined($T_dir) && (-d "$T_dir"));

# run the tests
if ($T_testnum == -1) {
	# run all tests
	$T_ntests = $#main'T_testlist + 1;
	for ($T_cnt = 0; $T_cnt < $T_ntests; ++$T_cnt) {
		&doTestN($T_cnt);
	}
}
else {
	# otherwise run the specified test
	&doTest($T_testnum);
}

# print the end line
$date = `date`;
chop $date;
printf("E:$cmd:$date\n");

1;

