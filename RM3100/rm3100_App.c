/* Userspace Application rm3100_App.c to access the sensor readings
 * from the Device Driver rm3100_I2C_Driver.c (kernelspace to userspace)
 * 
 * Displays readings to terminal every 10 seconds.
 */

#include <stdio.h>     // Standard Input and Output
#include <sys/types.h> // Contains data types such as off_t and ssize_t
#include <fcntl.h>     // for open() function
#include <unistd.h>    // for close() function
#include <sys/ioctl.h> // contains ioctl() function prototype and related macros
#include <unistd.h>    // for 10 seconds delay

#include "rm3100_Header.h" // Header file with type definitions

/* Function to get the data from the driver using ioctl() */

void get_data(int fd)
{// Parameter: file descriptor of device file

  coord_data currentData; // Structure for the sensor readings X and Y

  if (ioctl(fd, READ_FROM_SENSOR, &currentData) == -1) // Call _IOR()
    perror("Error"); // Display error if failed to query
  else // Print the queried variables
  {
    printf("\tX: %l\tY: %l\n",currentData.X_reading,currentData.Y_reading);
    // 2's complement format
  }
  return 0;
}

/* Main Function */

int main(int argc, char *argv[])
{
  char *file_name = DEVICE_FILE; // Macro defined in header
  int fd; // File descriptor of device file
  int i=0; // counter variable

  printf("RM3100 Geomagnetic Sensor\n");
  printf("X and Y readings will be taken every 10 seconds.\nPress enter to start: \n\n");
  getchar();
  while(i<100) // Max. 100 readings
  {
    fd = open(file_name, O_RDONLY); // Opens device file, and returns file descriptor
    // O_RDONLY is a flag passed to open(), for read-only operations.
    if (fd == -1){
      perror("Opening device file\nError");
      return 2;
    }
    get_data(fd);
    close (fd); // Close the device file
    sleep(10);
    i++;
  }
  return 0;
}
