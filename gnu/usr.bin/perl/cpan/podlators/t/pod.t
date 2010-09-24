#!/usr/bin/perl -w
#
# Test POD formatting.
#
# Copyright 2009 Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.

use strict;
use Test::More;
eval 'use Test::Pod 1.00';
plan skip_all => "Test::Pod 1.00 required for testing POD" if $@;
all_pod_files_ok ();
