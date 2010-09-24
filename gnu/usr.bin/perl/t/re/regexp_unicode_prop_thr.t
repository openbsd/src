#!./perl

chdir 't' if -d 't';
@INC = ('../lib', '.');

require 'thread_it.pl';
thread_it(qw(re regexp_unicode_prop.t));
