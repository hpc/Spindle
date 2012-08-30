#!/usr/bin/perl
my @tasks = split(',', $ARGV[0]);
my @nodes = `scontrol show hostnames $SLURM_NODELIST`;
my $node_cnt = $#nodes + 1;
my $task_cnt = $#tasks + 1;

if ($node_cnt < $task_cnt) {
	print STDERR "ERROR: You only have $node_cnt nodes, but requested layout on $task_cnt nodes.\n";
	$task_cnt = $node_cnt;
}

my $cnt = 0;
my $layout;
foreach my $task (@tasks) {
	my $node = $nodes[$cnt];
	last if !$node;
	chomp($node);
	for(my $i=0; $i < $task; $i++) {
		$layout .= "," if $layout;
		$layout .= "$node";
	}
	$cnt++;
}
print $layout;
