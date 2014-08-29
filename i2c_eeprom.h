/*
 * daemonlib
 * Copyright (C) 2014 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 *
 * i2c_eeprom.h: I2C EEPROM specific functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
