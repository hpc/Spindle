#!/usr/bin/perl
use strict;

my $filespec=$ARGV[0];
my $numnodes=$ARGV[1];
my $taskpernode=$ARGV[2];

my $pwd=`pwd`;
chomp($pwd);

my $TASKPERNODE=$ENV{SLURM_CPUS_ON_NODE};

print "filespec=$filespec\n";

my $numnodes=&create_hostlist("./tmp/hostlist_${filespec}.dat", "./tmp/slurm_hostlist_${filespec}.dat", $numnodes, $taskpernode);

   
my $servercallstr="(cd ./;";

exit;

sub create_hostlist {
    my($filename,$slurmfilename,$numnodes,$taskpernode)=@_;
    my($item,$t);
    my $nodelist  = "";
    
    $nodelist = $ENV{SLURM_NODELIST};
    
    if(!defined($nodelist) || length($nodelist) <= 0) {
	print "Not in a slurm allocation. Please run 'salloc' before this command\n";
	exit;
    }

    my @nodes = `/usr/bin/scontrol show hostnames \$SLURM_NODELIST`;

    print "found $#nodes nodes\n";

    print "create $filename\n";
    open(OUT,"> $filename");
    my $nodecount=0;
    foreach $item (@nodes) {
	chomp($item);
	next if ($item=~/sierra176[0123]/);
	print " found nodes: $item\n";
	print OUT "$item\n";
	$nodecount++;
	last if($nodecount>=$numnodes);
    }
    close(OUT);
    print "total number of nodes: $nodecount\n";

    print "create $slurmfilename\n";
    open(OUT,"> $slurmfilename");
    my $nodecount=0;
    foreach $item (@nodes) {
	chomp($item);
	next if ($item=~/sierra176[0123]/);
	print " found nodes: $item\n";
	for($t=0;$t<$taskpernode;$t++) {
	    print OUT "$item\n";
	}
	$nodecount++;
	last if($nodecount>=$numnodes);
    }
    close(OUT);
    print "total number of nodes: $nodecount\n";

    return($nodecount);
}
