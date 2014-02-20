// Linux userspace driver for IMT i2c-driven reset. 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>

#define I2CDEV "/dev/i2c-1"
#define I2CADDR 0x4D

int main(int argc, char **argv, char **envp) {
   int c = 0; /*Return code*/
   int fd; /*File descriptor*/
   int data; /*TODO: remove it*/
   float volts; /*TODO: remove it*/
   char buf[10] = {0}; /*Data buffer*/

   /* Open the Device. */
   if ((fd = open(I2CDEV, O_RDWR)) < 0) {
      printf("open() failed, errno=%d.", errno);
      exit(1);
   }
   if (ioctl(fd, I2C_PEC, 1) < 0) { /* Enable checksum. */
      printf("ioctl(I2C_PEC) failed, errno=%d\n", errno);
      c = errno;
      goto main_fail_exit;
   }
   if (ioctl(fd, I2C_SLAVE, I2CADDR) < 0) { /* Setup I2C device address. */
      printf("ioctl(I2C_SLAVE) failed, errno=%d\n", errno);
      c = errno;
      goto main_fail_exit;
   }
   /* Send the reset command. */
   while(1) {
      buf[0] = 0x8c; //TODO:
      if (write(fd, buf, 1) != 1) {
         printf("write() failed, errno=%d\n", errno);
      }
      do {
         if (read(fd, buf, 3) != 3) { //TODO:
            printf("read() failed, errno=%d\n", errno);
         }
      } while (buf[2] & (1<<7));

      data = (buf[0] << 8) | buf[1];
      if (data > 32767) {
         data = -(65535-data);
      }
      volts = ((float)data/32767)*2.048;
      printf("data: %d volts: %.4f\n",data, volts);
   }
  /* Failure handler and exit. */
main_fail_exit:
   close(fd);
   return c;
}

/* TODO:
 * 1. Communication sequence.
 * 2. Write/read buffer content.
 */


