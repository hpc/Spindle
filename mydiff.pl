#!/usr/bin/perl
use strict;

my $mydir=$ARGV[0];
my $remotedir=$ARGV[1];

my ($file,$otherfile);

print "WF: diff $mydir $remotedir\n";
foreach $file (`find $mydir -type f`) {
    chomp($file);
    next if($file=~/.svn\//);
    next if($file=~/_debug_/);
    next if($file=~/^\.\/samples\/tmp/);
    next if($file!~/(\.c|\.h|\.sh|\.pl|README|Makefile|Makefile_BGQ)$/);
#    print "$file\n";

    $otherfile="$remotedir/$file";
    my $result=`diff -q $file $otherfile`;
    if($result) {
	print "diff $file $otherfile\n";
    }
}
 
