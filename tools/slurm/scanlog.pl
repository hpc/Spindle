#!/usr/bin/env perl

$patint="([\\+\\-\\d]+)";   # Pattern for Integer number
$patfp ="([\\+\\-\\d.E]+)"; # Pattern for Floating Point number
$patwrd="([\^\\s]+)";       # Pattern for Work (all noblank characters)
$patbl ="\\s+";             # Pattern for blank space (variable length)


my $infile=$ARGV[0];
open(IN,$infile);
while($line=<IN>) {
    if($line=~/process $patint done with computation/) {
	$count[$1]=1;
    }
}
close(IN);

for($i=0;$i<=$#count;$i++) {
    if(!$count[$i]) {
	printf("task %02d (node %d) missing\n",$i, int($i/12)); 
    }
}
