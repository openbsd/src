#!./perl

chdir 't' if -d 't';
@INC = ('../lib', '.');

require 'thread_it.pl';
thread_it(qw(op regexp_unicode_prop.t));
