#!/usr/bin/python
#===============================================================================
# Note:
#   1. Requires "apt-get install psutils python-pip"
#===============================================================================
import psutil 

style="margin:0px auto"
 
#===============================================================================
# Program
#===============================================================================
if __name__ == '__main__':
   #----------------------------------------------------------------------------
   # Current system-wide CPU utilization
   #----------------------------------------------------------------------------
   # Server as a whole:
   percs = psutil.cpu_percent(interval=1.0, percpu=False)
   print " CPU(total): ",percs,"%";
 
   # Individual CPUs
   percs = psutil.cpu_percent(interval=1.0, percpu=True)
   for cpu, perc in enumerate(percs):
      print " CPU%-2s %5s %% " % (cpu, perc);
 
   #----------------------------------------------------------------------------
   # Details on Current system-wide CPU utilziation
   #----------------------------------------------------------------------------
   # Server as a whole
   perc = psutil.cpu_times(percpu=False)
   print " CPU(total):";
   print "   user:    %5s  nice:  %5s" % (perc.user, perc.nice);
   print "   system:  %5s  idle:  %5s" % (perc.system, perc.idle);
   print "   iowait:  %5s  irq:   %5s" % (perc.iowait, perc.irq);
   print "   softirq: %5s" % perc.softirq;
 
   # Individual CPUs
   percs = psutil.cpu_times(percpu=True)
   for cpu, perc in enumerate(percs):
      print " CPU%-2s" % (cpu);
      print "   user:    %5s  nice:  %5s" % (perc.user, perc.nice);
      print "   system:  %5s  idle:  %5s" % (perc.system, perc.idle);
      print "   iowait:  %5s  irq:   %5s" % (perc.iowait, perc.irq);
      print "   softirq: %5s" % perc.softirq;
 
