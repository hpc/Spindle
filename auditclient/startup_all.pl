#!/usr/bin/perl
use strict;

my $param=$ARGV[0];
my $pwd=`pwd`;
chomp($pwd);

print "param=$param\n";

my $numnodes=&create_hostlist("../auditserver/hostlist.dat");

if($param) {
    
    my $servercallstr="(cd $pwd/../auditserver;";
    
    open(OUT, "> ../auditserver/cobo_param.dat");
    print OUT "LDCS_LOCATION=/tmp/myfifo\n";
    print OUT "LDCS_NUMBER=7777\n";
    print OUT "LDCS_NPORTS=10\n";
    print OUT "LDCS_EXIT_AFTER_SESSION=1\n";
    print OUT "SION_DEBUG=../auditserver/_debug_audit_server_md\n";
    print OUT "COBO_CLIENT_DEBUG=7\n";
    close(out);
    
    $servercallstr.="../tools/cobo/test/server_rsh_ldcs -np $numnodes ";
    $servercallstr.="-paramfile ./cobo_param.dat -hostfile ./hostlist.dat ";
    $servercallstr.="./ldcs_audit_server_par_pipe_md";
#    $servercallstr.="> ./auditserver.log  2> ./auditserver.errlog";
    $servercallstr.=")";
    
    print "call server:\n $servercallstr\n";
    open(OUT, "> ../auditserver/run_server.sh");
    print OUT "$servercallstr\n";
    close(out);
    system("chmod u+x ../auditserver/run_server.sh");
    
    print "run on cab: --> (cd $pwd/../auditserver;./run_server.sh)\n";

    exit;
}

my $clientscallstr="";
$clientscallstr.="LD_AUDIT=$pwd/ldcs_audit_client_pipe.so ";
$clientscallstr.="LDCS_LOCATION=/tmp/myfifo ";
$clientscallstr.="LDCS_NUMBER=7777 ";
$clientscallstr.="LD_LIBRARY_PATH=./lib:\$LD_LIBRARY_PATH ";
#$clientscallstr.="LD_DEBUG=all ";
$clientscallstr.="SION_DEBUG=_debug_audit_client_mpi ";

my $numtasks=$numnodes*2;
#$clientscallstr.="srun -N $numnodes -n $numtasks -o job-%2t.out -e job-%2t.err  ./helloworld3_mpi";
$clientscallstr.="srun -N $numnodes -n $numtasks ./helloworld3_mpi";

print "call clients:\n $clientscallstr\n";
system($clientscallstr);

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

    my @nodes = `scontrol show hostnames \$SLURM_NODELIST`;

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
