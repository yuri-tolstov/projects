#!/usr/bin/perl
#===============================================================================
# Test file processor
# perl outproc.pl filename
#===============================================================================
use strict;
use warnings;

my ($fname) = @ARGV;
open my $file, $fname or die "Could not open $fname: $!";

while(my $line = <$file>)  {   
   if ($line =~ /sha1/) {
      print $line;    
   }
   elsif ($line =~ /Average/) {
      print $line;    
   }
}

close $file;

#---------------------------------------------------------------
# Another version
#---------------------------------------------------------------
# open(tfile, 'test3.out')
# while (<tfile>) {
#    chomp;
#    print "$_\n";
# }
# close(tfile);
#---------------------------------------------------------------
