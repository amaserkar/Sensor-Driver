/* Program to read sensor's date, time and temperature
 * every 10 seconds, using I2C bus.
 * Assume all data is in bcd.
 */

#include <stdio.h>         // Standard Input and Output
#include <fcntl.h>         // for open() function
#include <unistd.h>        // for close() function and sleep() function
#include <math.h>          // for pow() function
#include <sys/ioctl.h>     // contains ioctl() function prototype and related macros
#include <linux/i2c-dev.h> // Allows I2C bus communication using read() and write()

#define I2C_DEVICE_FILE "/dev/i2c-0"
#define I2C_SLAVE_ADDRESS 0x6e // 7-bit Slave address 1101110
uint8_t buf[1]; // for reading data
int date,month,year,hour,min,sec,temp;

int convert(uint8_t input)
{//converts one byte of bcd to decimal
  int ones=0,tens=0;
  for(int i=0;i<4;i++)
  {
    ones+=(input%2)*pow(2,i);
    input>>=1;
  }
  for(int i=0;i<4;i++)
  {
    tens+=(input%2)*pow(2,i);
    input>>=1;
  }
  return((tens*10)+ones);
}

int sensorDate(int file)
{
  write(file,0x04,1); // Date Register
  read(file,buf,1);
  date=convert(buf[0]);
  return 0;
}

int sensorTime(int file)
{
  write(file,0x01,1); // Minutes register
  read(file,buf,1);
  min=convert(buf[0]);
  write(file,0x00,1); // Seconds register
  read(file,buf,1);
  sec=convert(buf[0]);
  write(file,0x02,1); // Hours register
  read(file,buf,1);
  hour=convert(buf[0]);
  return 0;
}

int sensorTemp(int file) // Using only Temperature MSB register
{
  write(file,0x11,1);
  read(file,buf,1);
  uint8_t temporary=(buf[0]>>7)?(buf[0]-0x80):buf[0]; // ignore sign bit
  temp=(buf[0]>>7)?(-1*convert(temporary)):convert(temporary); // Check sign bit
  return 0;
}

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

  for (int i=0;i<100;i++) // Take 100 readings
  {
    sensorDate(file);
    sensorTime(file);
    sensorTemp(file);
    printf("Date:%d\tTime: %d:%d:%d\tTemp:%d\n",date,hour,min,sec,temp);
    sleep(10);
  }

  close(file); // Close the I2C Device File
  return 0;
}
