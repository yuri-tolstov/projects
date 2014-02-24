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
#define I2CADDR 0x54

int main(int argc, char **argv, char **envp) {
   int c = 0; /*Return code*/
   int fd; /*File descriptor*/
   char buf[8] = {0}; /*Data buffer*/

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
   buf[0] = 0x00; //TODO:
   if (write(fd, buf, 1) != 1) {
      printf("write(a=0x%x, d=0x%x) failed, errno=%d\n", I2CADDR, buf[0], errno);
      goto main_fail_exit;
   }
   if (read(fd, buf, 1) != 1) { //TODO:
      printf("read(a=0x%x) failed, errno=%d\n", I2CADDR, errno);
      goto main_fail_exit;
   }
   printf("data = 0x%x\n",buf[0]);

  /* Failure handler and exit. */
main_fail_exit:
   close(fd);
   return c;
}

/* TODO:
 * 1. Communication sequence.
 * 2. Write/read buffer content.
 */


