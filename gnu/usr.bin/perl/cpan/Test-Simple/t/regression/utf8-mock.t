use Test2::Plugin::UTF8;
use Test2::Bundle::More;
use Test2::Mock;
use Test2::Require::Module 'ExtUtils::MakeMaker';
use ExtUtils::MakeMaker;

ok 1;

my $mock = Test2::Mock->new(
  class => 'ExtUtils::MakeMaker',
);

subtest 'user says yes' => sub {

  my($msg, $def);

  $mock->override(prompt => sub ($;$) { ($msg,$def) = @_; return 'y' });

  ok 1;

};

done_testing;
