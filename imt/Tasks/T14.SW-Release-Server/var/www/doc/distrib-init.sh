#!/bin/bash
case $1 in
#*******************************************************************************
   setup)
#*******************************************************************************
centos/5/centos5-dist-init.sh setup
centos/6/centos6-dist-init.sh setup
intmas/touchstone-iqp/1.0.0/touchstone-iqp-1.0.0-init.sh setup
echo "Done."
;;
#*******************************************************************************
   cleanup)
#*******************************************************************************
centos/5/centos5-dist-init.sh cleanup
centos/6/centos6-dist-init.sh cleanup
intmas/touchstone-iqp/1.0.0/touchstone-iqp-1.0.0-init.sh cleanup
echo "Done."
;;
#*******************************************************************************
   *)
#*******************************************************************************
echo "Usage:"
echo "  ./distrib-init.sh setup|cleanup"
;;
esac



