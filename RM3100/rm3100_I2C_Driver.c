/* Driver for RM3100 Geomagnetic Sensor
 *
 * my_ioctl() function reads the sensor registers,
 * and returns the X and Y readings in a structure.
 * This function can be called by a userspace application
 * for displaying or storing the sensor readings.
 *
 * Assuming the Sensor is already set up.
 *
 */

#include <linux/init.h>   // contains macros for module initialisation
#include <linux/module.h> // includes the module related data structures
#include <linux/version.h>// for version compatibility of the module
#include <linux/kernel.h> // kernelspace header
#include <linux/fs.h>     // for character driver related support
#include <linux/device.h> // for struct device
#include <linux/cdev.h>   // for cdev functions
#include <linux/kdev_t.h> // for dev_t and related macros
#include <linux/uaccess.h>// contains functions for kernel to access userspace memory safely
#include <linux/errno.h>  // for int errno, which is set by system calls
#include <linux/i2c.h>    // contains definitions for the Linux I2C bus interface

#include "rm3100_Header.h" // Header file with type definitions

/* For device identification */

#define DRIVER_NAME "rm3100_Driver"
#define DRIVER_CLASS "rm3100_Class"

#define I2C_BUS_NUMBER 1 // Assuming the The I2C Bus number of the master device
#define SLAVE_DEVICE_NAME "rm3100"

#define SA1 1
#define SA0 0
#define RM3100_SLAVE_ADDRESS (0x20+(SA1<<1)+SA0)	

/* RM3100 I2C slave address is defined by a 7-bit binary number, with
 * the first five bits as 01000 and the next two bits being defined by
 * the user-configurable pins 3 and 28 (SA1 and SA0).
 * Thus the slave address is 01000<SA1><SA0>, which is 0x20 + (SA1 << 1) + SA0
 * 
 * Assuming the pins are already set to SA1 = SA0 = 1
 *
 */

/* Global Variables */

static struct i2c_adapter *rm3100_i2c_adapter = NULL;
// i2c_adapter is a structure used to identify a physical i2c bus.
 
static struct i2c_client *rm3100_i2c_client = NULL;
// identifies the slave device connected to the i2c bus.

static const struct i2c_device_id rm3100_id[] = {
		{ SLAVE_DEVICE_NAME, 0 }, 
		{ }
// Structure which holds device-specific information
};

static struct i2c_driver rm3100_driver = {
	.driver = {
		.name = SLAVE_DEVICE_NAME,
		.owner = THIS_MODULE
	}
// Structure which contains general access routines for the slave device
};

static struct i2c_board_info rm3100_i2c_board_info = {
	I2C_BOARD_INFO(SLAVE_DEVICE_NAME, RM3100_SLAVE_ADDRESS)
// Structure which is used to build tables of information about I2C devices.
};

static dev_t deviceNumber; // The device number <major,minor>
static struct cdev c_dev;  // Stores the character device attributes
static struct class *cl;   // Device Class

/* Device File Operations */

static int my_open(struct inode *i, struct file *f)
{
  printk(KERN_INFO "Called Driver Function: open()\n");
  return 0;
}
static int my_close(struct inode *i, struct file *f)
{
  printk(KERN_INFO "Called Driver Function: close()\n");
  return 0;
}

static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
  coord_data currentData; // Structure for reading sensor data

  /* To make a single measurement (polling) write to the POLL address register 0x00.
   * The POLL register byte establishes which axes are to be measured.
   * The value 0b00110000 (0x30) specifies that X and Y axes are to be measured.
   * Datasheet Section 5.3
   */
  i2c_smbus_write_byte_data(rm3100_i2c_client, 0x00, 0x30);

  /* To determine whether a measurement has been completed, read the Status Register 0xB4.
   * The return byte provides the contents of the Status Register:
   * Bit 7 (MSB) will be HIGH if data is available and LOW if it is unavailable.
   * Hence, a return value of 0b10000000 or 0x80 means the measurement was successful.
   * Datasheet Section 5.4.1
   */
  s32 ready = i2c_smbus_write_read_byte_data(rm3100_i2c_client, 0xb4);
  if (ready!=0x80) // Measurement failed
  {
    return -1;
  }

  /* If Measurement was successful, we can take the readings from the measurement result registers.
   * Each sensor reading consists of 3 bytes of data which are stored in 2’s complement format.
   * The read addresses for the measurement registers are given in datasheet table 5.5
   */
  s32 x2,x1,x0,y2,y1,y0;
  x2 = i2c_smbus_read_byte_data(rm3100_i2c_client, 0xa4); // X-axis Byte 2
  x1 = i2c_smbus_read_byte_data(rm3100_i2c_client, 0xa5); // X-axis Byte 1
  x0 = i2c_smbus_read_byte_data(rm3100_i2c_client, 0xa6); // X-axis Byte 0
  y2 = i2c_smbus_read_byte_data(rm3100_i2c_client, 0xa7); // Y-axis Byte 2
  y1 = i2c_smbus_read_byte_data(rm3100_i2c_client, 0xa8); // Y-axis Byte 1
  y0 = i2c_smbus_read_byte_data(rm3100_i2c_client, 0xa9); // Y-axis Byte 0

  /* To combine 3 bytes into a single number (for X and Y) and store in the structure currentData */
  currentData.X_reading = (x2<<16)+(x1<<8)+(x0);
  currentData.Y_reading = (y2<<16)+(y1<<8)+(y0);

  if (copy_to_user((coord_data *)arg, &currentData, sizeof(coord_data)))
  // copies into userspace (coord_data structure passed as arg)
        return -EACCES; // Error: Permission denied
  return 0;
}

// file_operations is a structure (defined in <linux/fs.h>)
// that contains the file operations allowed for the device.

static struct file_operations rm3100_FileOps =
{
  .owner = THIS_MODULE, //macro from <linux/module.h>
  .open = my_open,
  .release = my_close,
  .unlocked_ioctl = my_ioctl
};

static int __init rm3100_init(void) /* Constructor */
{
  int ret; // return value for errors
  struct device *dev_ret;// structure for device attributes

  printk(KERN_ALERT "Hello. Inside the %s function\n",__FUNCTION__);

  if ((ret = alloc_chrdev_region(&deviceNumber,0,1,"rm3100_Driver")) < 0)
  // Above function registers the character device file with the kernel,
  // and allocates an unused major number to the new device file. 
    return ret; //if failed to register

  if (IS_ERR(cl = class_create(THIS_MODULE, DRIVER_CLASS)))
  //creates device class in /sys directory.

  {//if failed
    unregister_chrdev_region(deviceNumber,1);
    return PTR_ERR(cl);
  }

  if (IS_ERR(dev_ret = device_create(cl, NULL, deviceNumber, NULL, DRIVER_NAME)))
  //populates device class with device info, and registers it in /sys directory.
  //automatically creates device file
  {//if failed
    class_destroy(cl);
    unregister_chrdev_region(deviceNumber, 1);
    return PTR_ERR(dev_ret);
  }

  cdev_init(&c_dev, &rm3100_FileOps); // initialise character device with the file operations

  if ((ret = cdev_add(&c_dev, deviceNumber, 1)) < 0)// add character device to the system
  {//if failed
    device_destroy(cl,deviceNumber);
    class_destroy(cl);
    unregister_chrdev_region(deviceNumber, 1);
    return ret;
  }

  rm3100_i2c_adapter = i2c_get_adapter(I2C_BUS_NUMBER);
  // API which returns the i2c_adapter structure

  rm3100_i2c_client = i2c_new_device(rm3100_i2c_adapter, &rm3100_i2c_board_info);
  // Instantiates a new i2c slave device
  
  i2c_add_driver(&rm3100_driver);
  // Registers the I2C device driver

  return 0; // Success
}

static void __exit rm3100_exit(void) /* Destructor */
{
  printk(KERN_ALERT "Goodbye. Inside the %s function\n",__FUNCTION__);

  i2c_unregister_device(rm3100_i2c_client); // Unregister I2C slave device
  i2c_del_driver(&rm3100_driver);           // Unregister I2C device driver
  cdev_del(&c_dev);
  device_destroy(cl, deviceNumber);
  class_destroy(cl);
  unregister_chrdev_region(deviceNumber,1);
}

module_init(rm3100_init);
module_exit(rm3100_exit);
//Macros to specify the init and exit functions

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Akshay Aserkar");
MODULE_DESCRIPTION("A driver for reading data from RM3100 geomagnetic sensor");
//Macros for module related information
