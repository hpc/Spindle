#!/usr/bin/perl
use strict;

my $fn=$ARGV[0];
my $ofn=$ARGV[1];

my($line,@data,$c,$item);
open(IN,$fn);
while($line=<IN>) {
    if($line=~/SERVER\[(\d+)\] STAT:\s+([^\s]+)\s*, #cnt=\s*(\d+),\s+bytes=\s*([\d\.]+) MB,\s+time=\s*([\d\.]+) sec/) {
#	print "$1 '$2' $3 $4 $5\n";
	$data[$1]->{$2}->{cnt}=$3;
	$data[$1]->{$2}->{bytes}=$4;
	$data[$1]->{$2}->{time}=$5;
    }
}
close(IN);

open(OUT,"> $ofn");
for($c=0;$c<=$#data;$c++) {
    printf(OUT  "%3d ", $c);
    foreach $item ("libread","libstore","libdist", "procdir", "distdir", "client_cb", "server_cb", "md_cb", "cl_msg_avg") {
	printf(OUT  "%10.4f ", $data[$c]->{$item}->{time});
    }
    printf(OUT  "\n");
}
close(OUT);

