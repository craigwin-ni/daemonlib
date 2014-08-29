/*
 * daemonlib
 * Copyright (C) 2014 Ishraq Ibne Ashraf <ishraq@tinkerforge.com>
 * Copyright (C) 2014 Matthias Bolte <matthias@tinkerforge.com>
 *
 * i2c_eeprom.c: I2C EEPROM specific functions
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

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>

#include "i2c_eeprom.h"
#include "red_gpio.h"
#include "log.h"
#include "utils.h"

#define LOG_CATEGORY LOG_CATEGORY_RED_BRICK

static int _i2c_init_file;
static GPIOPin _i2c_enable_pin;
static GPIOPin _i2c_eeprom_address_pin;

int _i2c_init() {
    // enable I2C bus with GPIO
    _i2c_enable_pin.port_index = GPIO_PORT_B;
    _i2c_enable_pin.pin_index = GPIO_PIN_6;
    _i2c_eeprom_address_pin.port_index = GPIO_PORT_G;
    _i2c_eeprom_address_pin.pin_index = GPIO_PIN_9;
    
    gpio_mux_configure(_i2c_enable_pin, GPIO_MUX_OUTPUT);
    gpio_mux_configure(_i2c_eeprom_address_pin, GPIO_MUX_OUTPUT);
    
    // address pin high
    gpio_output_set(_i2c_eeprom_address_pin);
    // i2c enable pin low (pullups)
    gpio_output_clear(_i2c_enable_pin);

    _i2c_init_file = open(I2C_EEPROM_BUS, O_RDWR);

    if (_i2c_init_file < 0) {
        log_error("Unable to open I2C bus: %s (%d)",
                  get_errno_name(errno), errno);
        // disable I2C bus with GPIO
        gpio_output_set(_i2c_enable_pin);
        gpio_output_clear(_i2c_eeprom_address_pin);
        return -1;
    }
    
    if (ioctl(_i2c_init_file, I2C_SLAVE, I2C_EEPROM_DEVICE_ADDRESS) < 0) {
        log_error("Unable to access I2C device on the bus: %s (%d)",
                  get_errno_name(errno), errno);
        // disable I2C bus with GPIO
        gpio_output_set(_i2c_enable_pin);
        gpio_output_clear(_i2c_eeprom_address_pin);
        close(_i2c_init_file);
        return -1;
    }
    return _i2c_init_file;
}

int _i2c_eeprom_set_pointer(int device_file, uint8_t* eeprom_memory_address) {
    int bytes_written = 0;
    bytes_written = write(device_file, eeprom_memory_address, 2);
    if ( bytes_written != 2) {
        log_error("Error setting EEPROM address pointer: %s (%d)",
                  get_errno_name(errno), errno);
        // disable I2C bus with GPIO
        gpio_output_set(_i2c_enable_pin);
        gpio_output_clear(_i2c_eeprom_address_pin);
        close(_i2c_init_file);
        return -1;
    }
    return bytes_written;
}

int i2c_eeprom_read(uint16_t eeprom_memory_address,
                    uint8_t* buffer_to_store, int bytes_to_read) {
    int bytes_read = 0;
    uint8_t mem_address[2] = {eeprom_memory_address >> 8,
                              eeprom_memory_address & 0x00FF};
    int device_file = _i2c_init();
    
    if(device_file < 0) {
        return -1;
    }
    if((_i2c_eeprom_set_pointer(device_file, mem_address)) < 0) {
        return -1;
    }
    bytes_read = read(device_file, buffer_to_store, bytes_to_read);
    if(bytes_read != bytes_to_read) {
        log_error("EEPROM read failed: %s (%d)", get_errno_name(errno), errno);
        // disable I2C bus with GPIO
        gpio_output_set(_i2c_enable_pin);
        gpio_output_clear(_i2c_eeprom_address_pin);
        close(_i2c_init_file);
        return -1;
    }
    // disable I2C bus with GPIO
    gpio_output_set(_i2c_enable_pin);
    gpio_output_clear(_i2c_eeprom_address_pin);
    close(_i2c_init_file);
    return bytes_read;
}

int i2c_eeprom_write(uint16_t eeprom_memory_address,
                     uint8_t* buffer_to_write, int bytes_to_write) {
    int i = 0;
    char _bytes_written = 0;
    char bytes_written = 0;
    uint8_t write_byte[3] = {0};
    int device_file = _i2c_init();
    
    if(device_file < 0) {
        return -1;
    }
    write_byte[0] = eeprom_memory_address >> 8;
    write_byte[1] = eeprom_memory_address & 0xFF;
    
    for(i = 0; i < bytes_to_write; i++) {
        write_byte[2] = buffer_to_write[i];
        _bytes_written = write(device_file, write_byte, 3);
        if(_bytes_written != 3) {
            log_error("EEPROM write failed: %s (%d)",
                      get_errno_name(errno), errno);
            // disable I2C bus with GPIO
            gpio_output_set(_i2c_enable_pin);
            gpio_output_clear(_i2c_eeprom_address_pin);
            close(_i2c_init_file);
            return -1;
        }
        bytes_written ++;
    }
    // disable I2C bus with GPIO
    gpio_output_set(_i2c_enable_pin);
    gpio_output_clear(_i2c_eeprom_address_pin);
    close(_i2c_init_file);
    return bytes_written;
}
