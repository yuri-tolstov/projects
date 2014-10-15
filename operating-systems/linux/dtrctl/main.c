/******************************************************************************/
/* Linux userspace driver for Serial:DTR control.                             */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>


/******************************************************************************/
int main(int argc, char **argv, char **envp)
/******************************************************************************/
{
   int fd, serial;
   int i;

   fd = open("/dev/ttyS0", O_RDWR);

   for (i = 0; i < 16; i++) {
      ioctl(fd, TIOCMGET, &serial);
      if (serial & TIOCM_DTR) {
         printf("%d: TIOCM_DTR is not set\n", i);
         serial &= ~TIOCM_DTR;
      }
      else {
         printf("%d: TIOCM_DTR is set\n", i);
         serial |= TIOCM_DTR;
      }
      ioctl(fd, TIOCMSET, &serial);
      
   }
   close(fd);
   return 0;
}

