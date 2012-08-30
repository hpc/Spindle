#!/usr/bin/perl
use strict;

my $param=$ARGV[0];
my $pwd=`pwd`;
chomp($pwd);

my $TASKPERNODE=$ENV{SLURM_CPUS_ON_NODE};

print "param=$param\n";

my $numnodes=&create_hostlist("./hostlist.dat");

   
my $servercallstr="(cd ./;";

open(OUT, "> ./cobo_param.dat");
print OUT "LDCS_LOCATION=/tmp/myfifo\n";
print OUT "LDCS_NUMBER=7777\n";
print OUT "LDCS_NPORTS=10\n";
print OUT "LDCS_EXIT_AFTER_SESSION=1\n";
print OUT "SION_DEBUG=./_debug_audit_server_md\n";
print OUT "COBO_CLIENT_DEBUG=7\n";
close(out);

$servercallstr.="./../../LDCS/tools/cobo/test/server_rsh_ldcs -np $numnodes ";
$servercallstr.="-paramfile ./cobo_param.dat -hostfile ./hostlist.dat ";
$servercallstr.="./../../LDCS/auditserver/ldcs_audit_server_par_pipe_md";
#    $servercallstr.="> ./auditserver.log  2> ./auditserver.errlog";
$servercallstr.=")";
open(OUT, "> ./run_server.sh");
print OUT "$servercallstr\n";
close(out);
system("chmod u+x ./run_server.sh");

print "run on login node (standard):  --> (cd $pwd/;./run_server.sh)\n";


my $clientscallstr="";
$clientscallstr.="LD_AUDIT=$pwd/../../LDCS/auditclient/ldcs_audit_client_pipe.so ";
$clientscallstr.="LDCS_LOCATION=/tmp/myfifo ";
$clientscallstr.="LDCS_NUMBER=7777 ";
$clientscallstr.="LD_LIBRARY_PATH=./:\$LD_LIBRARY_PATH ";
#$clientscallstr.="LD_DEBUG=all ";
$clientscallstr.="SION_DEBUG=_debug_audit_client_mpi ";

my $numtasks=$numnodes*$TASKPERNODE;
#$clientscallstr.="srun -N $numnodes -n $numtasks -o job-%2t.out -e job-%2t.err  ./helloworld3_mpi";
$clientscallstr.="srun -N $numnodes -n $numtasks pynamic-pyMPI pynamic_driver.py";

open(OUT, "> ./run_client.sh");
print OUT "$clientscallstr\n";
close(OUT);
system("chmod u+x ./run_client.sh");


# ##############################################################
# ############ BENCHMARK #######################################
# ##############################################################

open(OUT, "> ./cobo_param_bm.dat");
print OUT "LDCS_LOCATION=/tmp/myfifo\n";
print OUT "LDCS_NUMBER=7777\n";
print OUT "LDCS_NPORTS=10\n";
print OUT "LDCS_EXIT_AFTER_SESSION=1\n";
close(OUT);

$servercallstr="(cd ./;";
$servercallstr.="./../../LDCS/tools/cobo/test/server_rsh_ldcs -np $numnodes ";
$servercallstr.="-paramfile ./cobo_param_bm.dat -hostfile ./hostlist.dat ";
$servercallstr.="./../../LDCS/auditserver/ldcs_audit_server_par_pipe_md";
$servercallstr.=")";
open(OUT, "> ./run_server_bm.sh");
print OUT "$servercallstr\n";
close(OUT);
system("chmod u+x ./run_server_bm.sh");

print "run on login node (BENCHMARK): --> (cd $pwd/;./run_server_bm.sh)\n";

$clientscallstr=" ";
$clientscallstr.="LD_AUDIT=$pwd/../../LDCS/auditclient/ldcs_audit_client_pipe.so ";
$clientscallstr.="LDCS_LOCATION=/tmp/myfifo ";
$clientscallstr.="LDCS_NUMBER=7777 ";
$clientscallstr.="LD_LIBRARY_PATH=./:\$LD_LIBRARY_PATH ";
my $numtasks=$numnodes*$TASKPERNODE;
$clientscallstr.="srun -v -N $numnodes -n $numtasks pynamic-pyMPI pynamic_driver.py";
open(OUT, "> ./run_client_bm_with_ldcs.sh");
print OUT "$clientscallstr\n";
close(OUT);
system("chmod u+x ./run_client_bm_with_ldcs.sh");

my $clientscallstr_wo=" ";
$clientscallstr_wo.="LD_LIBRARY_PATH=./:\$LD_LIBRARY_PATH ";
$clientscallstr_wo.="srun -v -N $numnodes -n $numtasks pynamic-pyMPI pynamic_driver.py";
open(OUT, "> ./run_client_bm_without_ldcs.sh");
print OUT "$clientscallstr_wo\n";
close(OUT);
system("chmod u+x ./run_client_bm_without_ldcs.sh");

my $cleanupcallstr="srun -N $numnodes -n $numnodes hostname\n";
$cleanupcallstr.="srun  -N $numnodes -n $numnodes --drop-caches rm -f /tmp/myfifo-tmp/*.so";
open(OUT, "> ./run_cleanup.sh");
print OUT "$cleanupcallstr\n";
close(OUT);
system("chmod u+x ./run_cleanup.sh");

open(OUT, "> ./run_benchmark.sh");
print OUT "echo \"Running without LDCS\"\n";
print OUT "./run_cleanup.sh 2>&1 > cleanup.out\n";
print OUT "/usr/bin/time -o timings_without_LDCS.dat -v ./run_client_bm_without_ldcs.sh > benchmark_wo.out 2> benchmark_wo.err\n";
print OUT "grep 'Elapsed (wall clock) time' timings_without_LDCS.dat\n";
print OUT "echo \"Running with LDCS\"\n";
print OUT "./run_cleanup.sh 2>&1 >> cleanup.out\n";
print OUT "/usr/bin/time -o timings_with_LDCS.dat -v ./run_client_bm_with_ldcs.sh > benchmark_w.out 2> benchmark_w.err\n";
print OUT "grep 'Elapsed (wall clock) time' timings_with_LDCS.dat\n";
print OUT "./run_cleanup.sh 2>&1 >> cleanup.out\n";
close(out);

open(OUT, "> ./run_benchmark_ldcs_only.sh");
print OUT "echo \"Running with LDCS\"\n";
print OUT "/usr/bin/time -o timings_with_LDCS.dat -v ./run_client_bm_with_ldcs.sh > benchmark_w.out 2> benchmark_w.err\n";
print OUT "grep 'Elapsed (wall clock) time' timings_with_LDCS.dat\n";
close(out);


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
