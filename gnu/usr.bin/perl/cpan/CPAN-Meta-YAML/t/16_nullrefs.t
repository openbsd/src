#!/usr/bin/perl

# Testing for null references

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use t::lib::Test;
use Test::More tests(1);
use CPAN::Meta::YAML;





#####################################################################
# Example Empty References

yaml_ok(
	<<'END_YAML',
--- []
--- {}
END_YAML
	[ [], {} ],
	'Empty references',
);
