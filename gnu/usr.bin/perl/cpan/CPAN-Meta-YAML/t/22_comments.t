#!/usr/bin/perl

# Testing of inline comments. These comments can be quite useful in config
# files and people will expect them to work.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use t::lib::Test;
use Test::More tests(2);
use CPAN::Meta::YAML;





#####################################################################
# Main Tests

yaml_ok(
	<<'END_YAML',
---
a: b#content
c: d #comment
e:
- f #comment
- g# content
h: 'single'                 # comment
h2: 'single # content'      # comment
i: "double"                 # comment
i2: "double # content"      # comment
j: |        # comment
    literal # content
    block   # content
k: {}       # comment
l: []       # comment
m:      # comment
  n: o
END_YAML
	[
		{
			a => 'b#content',
			c => 'd',
			e => [
				'f',
				'g# content',
			],
			h => 'single',
			h2 => 'single # content',
			i => 'double',
			i2 => 'double # content',
			j => "literal # content\nblock   # content\n",
			k => {},
			l => [],
			m => {
				n => 'o',
			},
		},
	],
	'Properly ignore comments',
	noyamlpm => 1,
);

# Repeat, with otherwise illegal characters in the comments
yaml_ok(
	<<'END_YAML',
---
a: b#content
c: d #comment '"!&@%`
e:
- f #comment '"!&@%`
- g# content
h: 'single'                 # comment '"!&@%`
h2: 'single # content'      # comment '"!&@%`
i: "double"                 # comment '"!&@%`
i2: "double # content"      # comment '"!&@%`
j: |        # comment '"!&@%`
    literal # content
    block   # content
k: {}       # comment '"!&@%`
l: []       # comment '"!&@%`
m:      # comment '"!&@%`
  n: o
END_YAML
	[
		{
			a => 'b#content',
			c => 'd',
			e => [
				'f',
				'g# content',
			],
			h => 'single',
			h2 => 'single # content',
			i => 'double',
			i2 => 'double # content',
			j => "literal # content\nblock   # content\n",
			k => {},
			l => [],
			m => {
				n => 'o',
			},
		},
	],
	'Properly ignore comments (with otherwise illegal characters)',
	noyamlpm => 1,
);
