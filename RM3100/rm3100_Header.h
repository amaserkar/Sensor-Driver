#ifndef RM3100_HEADER
#define RM3100_HEADER

#include <linux/ioctl.h>
// contains ioctl() function prototype and related macros

typedef struct { // Structure for the sensor readings X and Y
  s32 X_reading;
  s32 Y_reading;
} coord_data;

#define READ_FROM_SENSOR _IOR('currentData', 1, coord_data *)
// Macro _IOR acts as the command parameter to ioctl(), specifying read operation.
// Defined in <linux/ioctl.h>

#define DEVICE_FILE "rm3100_Driver-0"
// Name of device file, which will have minor number 0

#endif
