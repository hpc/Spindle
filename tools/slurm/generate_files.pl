#!/usr/bin/perl
use strict;

my $filespec=$ARGV[0];
my $pwd=`pwd`;
chomp($pwd);

my $TASKPERNODE=$ENV{SLURM_CPUS_ON_NODE};

print "filespec=$filespec\n";

my $numnodes=&create_hostlist("./tmp/hostlist_${filespec}.dat");

   
my $servercallstr="(cd ./;";

exit;

sub create_hostlist {
    my($filename)=@_;
    my($item);
    my $nodelist  = "";
    
    $nodelist = $ENV{SLURM_NODELIST};
    
    if(!defined($nodelist) || length($nodelist) <= 0) {
	print "Not in a slurm allocation. Please run 'salloc' before this command\n";
	exit;
    }

    print "create $filename\n";

    my @nodes = `/usr/bin/scontrol show hostnames \$SLURM_NODELIST`;

    open(OUT,"> $filename");
    my $nodecount=0;
    foreach $item (@nodes) {
	chomp($item);
	print " found nodes: $item\n";
	print OUT "$item\n";
	$nodecount++;
    }
    close(OUT);
    print "total number of nodes: $nodecount\n";
    return($nodecount);
}
