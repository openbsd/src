use Test2::Bundle::Extended -target => 'Test2::Workflow::Task::Group';

skip_all "Tests not yet written";

can_ok($CLASS, qw/before after primary rand variant/);

done_testing;
