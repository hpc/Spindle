#!/usr/bin/env perl

# ~/LLNL/LDCS/tools/slurm/scan_for_preload.pl tmp/_debug_auditclient_00_00.log  reloadfiles

use strict;

my $patint="([\\+\\-\\d]+)";   # Pattern for Integer number
my $patfp ="([\\+\\-\\d.E]+)"; # Pattern for Floating Point number
my $patwrd="([\^\\s]+)";       # Pattern for Work (all noblank characters)
my $patbl ="\\s+";             # Pattern for blank space (variable length)


my $infile=$ARGV[0];
my $outprefix=$ARGV[1];
open(IN,$infile);

my $lastlib="<?>";
my $lastfile="<?>";
my $cwd=""; 

my $libfoundc=0;
my $libnotfoundc=0;
my $filefoundc=0;
my $filenotfoundc=0;

my ($line, $localpath, %outfound, %outnotfound, %dirs, $dir, $file);
while($line=<IN>) {

    if($line=~/LDCS_MSG_CWD len=$patint data=$patwrd\s+/) {
	$cwd=$2;
    }

    if($line=~/AUDITSEND: L:$patwrd/) {
	$lastlib=$1;
	$lastlib=~s/\.\//$cwd\//gs;
	if($lastlib!~/^\//) {
	    $lastlib=$cwd."/".$lastlib;
	}
	$lastlib=~/^(.*)\/[^\/]+$/;
	$dirs{$1}++;
    }
    if($line=~/AUDITSEND: E:$patwrd/) {
	$lastfile=$1;
	$lastfile=~s/\.\//$cwd\//gs;
	if($lastfile!~/^\//) {
	    $lastfile=$cwd."/".$lastfile;
	}
	$lastfile=~/^(.*)\/[^\/]+$/;
	$dirs{$1}++;
    }
    if($line=~/AUDITRECV: L:$patwrd/) {
	$localpath=$1;
#	print "$localpath\n";
	if($localpath !~ /\(null\)/) {
	    $outfound{$lastlib}++;
	    $libfoundc++;
	} else {
	    $outnotfound{$lastlib}++;
	    $libnotfoundc++;
	}
    }
    if($line=~/AUDITRECV: E:$patwrd/) {
	$localpath=$1;
#	print "$localpath\n";
	if($localpath !~ /\(null\)/) {
	    $outfound{$lastfile}++;
	    $filefoundc++;
	} else {
	    $outnotfound{$lastfile}++;
	    $filenotfoundc++;
	}
    }
}
close(IN);


open(OUTFOUND,"> ${outprefix}_found.dat");
foreach $file (sort(keys(%outfound))) {
    print OUTFOUND "$file\n";
}
close(OUTFOUND);

open(OUTNOTFOUND,"> ${outprefix}_notfound.dat");
foreach $file (sort(keys(%outnotfound))) {
    print OUTNOTFOUND "$file\n";
}
close(OUTNOTFOUND);

open(OUTDIRS,"> ${outprefix}_dirs.dat");
foreach $dir (sort(keys(%dirs))) {
    print OUTDIRS "$dir/__dummy_ldcs_preload_dir__\n";
}
close(OUTDIRS);


printf("found libs  %4d requests\n",$libfoundc+$libnotfoundc);
printf("            %4d found\n",$libfoundc);
printf("            %4d not found\n",$libnotfoundc);

printf("found files %4d requests\n",$filefoundc+$filenotfoundc);
printf("            %4d found\n",$filefoundc);
printf("            %4d not found\n",$filenotfoundc);

printf("total found %4d requests\n",$filefoundc+$filenotfoundc+$libfoundc+$libnotfoundc);
printf("            %4d found\n",$filefoundc+$libfoundc);
printf("            %4d not found\n",$filenotfoundc+$libnotfoundc);

