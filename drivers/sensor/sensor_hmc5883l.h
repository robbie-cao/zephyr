/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SENSOR_HMC5883L_H__
#define __SENSOR_HMC5883L_H__

#include <device.h>
#include <misc/util.h>
#include <stdint.h>
#include <gpio.h>
#include <misc/nano_work.h>

#define SYS_LOG_DOMAIN "HMC5883L"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_SENSOR_LEVEL
#include <misc/sys_log.h>

#define HMC5883L_I2C_ADDR		0x1E

#define HMC5883L_REG_CONFIG_A		0x00
#define HMC5883L_ODR_SHIFT		2

#define HMC5883L_REG_CONFIG_B		0x01
#define HMC5883L_GAIN_SHIFT		5

#define HMC5883L_REG_MODE		0x02
#define HMC5883L_MODE_CONTINUOUS	0

#define HMC5883L_REG_DATA_START		0x03

#define HMC5883L_REG_CHIP_ID		0x0A
#define HMC5883L_CHIP_ID_A		'H'
#define HMC5883L_CHIP_ID_B		'4'
#define HMC5883L_CHIP_ID_C		'3'

static const char * const hmc5883l_odr_strings[] = {
	"0.75", "1.5", "3", "7.5", "15", "30", "75"
};

static const char * const hmc5883l_fs_strings[] = {
	"0.88", "1.3", "1.9", "2.5", "4", "4.7", "5.6", "8.1"
};

static const uint16_t hmc5883l_gain[] = {
	1370, 1090, 820, 660, 440, 390, 330, 230
};

struct hmc5883l_data {
	struct device *i2c;
	int16_t x_sample;
	int16_t y_sample;
	int16_t z_sample;
	uint8_t gain_idx;

#ifdef CONFIG_HMC5883L_TRIGGER
	struct device *gpio;
	struct gpio_callback gpio_cb;

	struct sensor_trigger data_ready_trigger;
	sensor_trigger_handler_t data_ready_handler;

#if defined(CONFIG_HMC5883L_TRIGGER_OWN_FIBER)
	char __stack fiber_stack[CONFIG_HMC5883L_FIBER_STACK_SIZE];
	struct nano_sem gpio_sem;
#elif defined(CONFIG_HMC5883L_TRIGGER_GLOBAL_FIBER)
	struct nano_work work;
	struct device *dev;
#endif

#endif /* CONFIG_HMC5883L_TRIGGER */
};

#ifdef CONFIG_HMC5883L_TRIGGER
int hmc5883l_trigger_set(struct device *dev,
			const struct sensor_trigger *trig,
			sensor_trigger_handler_t handler);

int hmc5883l_init_interrupt(struct device *dev);
#endif

#endif /* __SENSOR_HMC5883L__ */
