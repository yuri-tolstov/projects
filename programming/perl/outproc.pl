#!/usr/bin/perl
#===============================================================================
# Test file processor
# perl outproc.pl filename
#===============================================================================
use strict;
use warnings;

my ($fname) = @ARGV;
open my $file, $fname or die "Could not open $fname: $!";

system("clear");

my $n = 0;
my @words = ();

while(my $line = <$file>)  {   
   if ($line =~ /Cycle/) {
      chomp($line);
      @words = split(/ /, $line);
      $n = $words[2];
   }
   elsif ($line =~ /sha1/) {
      print "$n : $line";    
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
