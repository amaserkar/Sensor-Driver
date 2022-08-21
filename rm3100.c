/* Program to take reading from RM3100 sensor
 * via I2C bus, using the I2C device file.
 */

#include <stdio.h>         // Standard Input and Output
#include <fcntl.h>         // for open() function
#include <unistd.h>        // for close() function
#include <sys/ioctl.h>     // contains ioctl() function prototype and related macros
#include <linux/i2c-dev.h> // Allows I2C bus communication using read() and write()

#define I2C_DEVICE_FILE "/dev/i2c-0"
#define I2C_SLAVE_ADDRESS 0x23	
/* RM3100 I2C slave address is a 7-bit binary number.
 * The first five bits are 01000, and the next two bits
 * are the user-configurable pins 3 and 28 (SA1 and SA0).
 * So the address is 01000<SA1><SA0>, ie. (0x20 + (SA1 << 1) + SA0)
 * Assuming SA1 = SA0 = 1, the address is 0100011 (or 0x23).
 */

int main()
{
  int file;

  // Opening the I2C Device File
  if ((file=open(I2C_DEVICE_FILE, O_RDWR))<0) // Allow read and write operations
  {
    printf("Failed to open the I2C bus.\n");
    return -1;
  }

  // Connecting to the Sensor (I2C Slave)
  if (ioctl(file, I2C_SLAVE, I2C_SLAVE_ADDRESS)<0)
  {
    printf("Failed to connect to the sensor.\n");
    return -1;
  }

  /* Using Polling Mode (Datasheet Section 5.3)
   * The POLL address register 0x00 contains a "Poll Register Byte"
   * which establishes the axes to be measured.
   * For X and Y axes, the value is 0b00110000 (0x30).
   */
  char wr[2];
  wr[0]=0x00; // Poll register address
  wr[1]=0x30; // Value to be written

  if (write(file,wr,2)!=2) // Request a single measurement
  {
    printf("Failed to write to the POLL register.\n");
    return -1;
  }

  /* To determine whether the measurement has been completed, read the Status Register 0xB4.
   * The MSB of the return byte will be 1 if data is available and 0 if unavailable.
   */
  if (write(file, 0xb4, 1)!=1) // To read a register, first write its address
  {
    printf("Failed to access the status register.\n");
    return -1;
  }
  char reply;
  if (read(file, reply, 1)!=1)
  {
    printf("Failed to read the status register.\n");
    return -1;
  }
  if (reply>>7==0) // MSB of status register byte
  {
    printf("Failed to take measurement.\n");
    return -1;
  }

  /* If measurement was successful, we can take the readings from the result registers.
   * Each axis reading consists of 3 bytes of data (in 2’s complement format).
   * The reading from one register address is followed by all the requested axis readings.
   * Read addresses begin with 0xa4.
   */
  char buf[6]; // To store x and y raw data
  write(file, 0xa4, 1); // Request readings
  read(file, buf, 6); // Read measurement registers
  int X_reading = (buf[0]<<16)+(buf[1]<<8)+(buf[2]); // X-axis measurement
  int Y_reading = (buf[3]<<16)+(buf[4]<<8)+(buf[5]); // Y-axis measurement

  printf("X-axis Reading: %d\n",X_reading);
  printf("Y-axis Reading: %d\n",Y_reading);

  close(file); // Close the I2C Device File
  return 0;
}
