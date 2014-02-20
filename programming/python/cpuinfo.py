#!/usr/bin/python

try:
 import os 
except ImportError:
 print "Cannot import os module.";
 sys.exit();

try:
 import time 
except ImportError:
 print "Cannot import time module.";
 sys.exit();

#===============================================================================
# Program
#===============================================================================
if __name__ == '__main__':
 
 aveu = os.getloadavg()
 print "Average CPU load (1, 5, 15 min): %.2f, %.2f, %.2f" % \
       (aveu[0], aveu[1], aveu[2]);
 
