#!/usr/bin/perl -wT

use strict;
use lib 't/lib';

use Test::More tests => 78;

BEGIN {

    # TAP::Parser must come first
    my @classes = qw(
      TAP::Parser
      App::Prove
      App::Prove::State
      App::Prove::State::Result
      App::Prove::State::Result::Test
      TAP::Base
      TAP::Formatter::Color
      TAP::Formatter::Console::ParallelSession
      TAP::Formatter::Console::Session
      TAP::Formatter::Console
      TAP::Harness
      TAP::Parser::Aggregator
      TAP::Parser::Grammar
      TAP::Parser::Iterator
      TAP::Parser::Iterator::Array
      TAP::Parser::Iterator::Process
      TAP::Parser::Iterator::Stream
      TAP::Parser::IteratorFactory
      TAP::Parser::Multiplexer
      TAP::Parser::Result
      TAP::Parser::ResultFactory
      TAP::Parser::Result::Bailout
      TAP::Parser::Result::Comment
      TAP::Parser::Result::Plan
      TAP::Parser::Result::Pragma
      TAP::Parser::Result::Test
      TAP::Parser::Result::Unknown
      TAP::Parser::Result::Version
      TAP::Parser::Result::YAML
      TAP::Parser::Result
      TAP::Parser::Scheduler
      TAP::Parser::Scheduler::Job
      TAP::Parser::Scheduler::Spinner
      TAP::Parser::Source::Perl
      TAP::Parser::Source
      TAP::Parser::YAMLish::Reader
      TAP::Parser::YAMLish::Writer
      TAP::Parser::Utils
      Test::Harness
    );

    foreach my $class (@classes) {
        use_ok $class or BAIL_OUT("Could not load $class");
        is $class->VERSION, TAP::Parser->VERSION,
          "... and $class should have the correct version";
    }

    diag("Testing Test::Harness $Test::Harness::VERSION, Perl $], $^X")
      unless $ENV{PERL_CORE};
}
