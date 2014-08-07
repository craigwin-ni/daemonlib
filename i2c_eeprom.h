#ifndef DAEMONLIB_I2C_EEPROM_H
#define DAEMONLIB_I2C_EEPROM_H

#define I2C_EEPROM_BUS "/dev/i2c-2"
#define I2C_EEPROM_DEVICE_ADDRESS 0x54

#include <stdint.h>

int i2c_eeprom_read(uint16_t eeprom_memory_address,
uint8_t* buffer_to_store, int bytes_to_read);

int i2c_eeprom_write(uint16_t eeprom_memory_address,
uint8_t* buffer_to_write, int bytes_to_write);

#endif // DAEMONLIB_I2C_EEPROM_H
