#!/usr/bin/env perl

# Copyright (C) 2005  Joshua Hoblitt
#
# $Id$

use strict;

use Test::More tests => 2;

use Pod::Find qw( contains_pod );

{
    ok(contains_pod('t/pod/contains_pod.xr'), "contains pod");
}

{
    ok(contains_pod('t/pod/contains_bad_pod.xr'), "contains bad pod");
}
