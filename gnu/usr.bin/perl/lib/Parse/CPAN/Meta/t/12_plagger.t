#!/usr/bin/perl

# Testing Plagger config samples from Miyagawa-san's YAPC::NA 2006 talk

BEGIN {
	if( $ENV{PERL_CORE} ) {
		chdir 't';
		@INC = ('../lib', 'lib');
	}
	else {
		unshift @INC, 't/lib/';
	}
}

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use File::Spec::Functions ':ALL';
use Parse::CPAN::Meta::Test;
use Test::More tests(2);





#####################################################################
# Example Plagger Configuration 1

yaml_ok(
	<<'END_YAML',
plugins:
  - module: Subscription::Bloglines
    config:
      username: you@example.pl
      password: foobar
      mark_read: 1

  - module: Publish::Gmail
    config:
      mailto: example@gmail.com
      mailfrom: miyagawa@example.com
      mailroute:
        via: smtp
        host: smtp.example.com
END_YAML
	[ { plugins => [
		{
			module => 'Subscription::Bloglines',
			config => {
				username  => 'you@example.pl',
				password  => 'foobar',
				mark_read => 1,
			},
		},
		{
			module => 'Publish::Gmail',
			config => {
				mailto    => 'example@gmail.com',
				mailfrom  => 'miyagawa@example.com',
				mailroute => {
					via  => 'smtp',
					host => 'smtp.example.com',
				},
			},
		},
	] } ],
	'Plagger',
);





#####################################################################
# Example Plagger Configuration 2

yaml_ok(
	<<'END_YAML',
plugins:
 - module: Subscription::Config
   config:
     feed:
        # Trac's feed for changesets
        - http://plagger.org/.../rss

 # I don't like to be notified of the same items
 # more than once
 - module: Filter::Rule
   rule:
     module: Fresh
     mtime:
       path: /tmp/rssbot.time
       autoupdate: 1

 - module: Notify::IRC
   config:
     daemon_port: 9999
     nickname: plaggerbot
     server_host: chat.freenode.net
     server_channels:
       - '#plagger-ja'
       - '#plagger'

   
END_YAML
	[ { plugins => [ {
		module => 'Subscription::Config',
		config => {
			feed => [ 'http://plagger.org/.../rss' ],
		},
	}, {
		module => 'Filter::Rule',
		rule   => {
			module => 'Fresh',
			mtime  => {
				path => '/tmp/rssbot.time',
				autoupdate => 1,
			},
		},
	}, {
		module => 'Notify::IRC',
		config => {
			daemon_port     => 9999,
			nickname        => 'plaggerbot',
			server_host     => 'chat.freenode.net',
			server_channels => [
				'#plagger-ja',
				'#plagger',
			],
		},
	} ] } ],
	'plagger2',
);			
