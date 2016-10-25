/**
* \file
*
* \brief User board configuration template
*
* Copyright (C) 2012-2015 Atmel Corporation. All rights reserved.
*
* \asf_license_start
*
* \page License
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* 3. The name of Atmel may not be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
* 4. This software may only be redistributed and used in connection with an
*    Atmel micro-controller product.
*
* THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
* EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
* ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
* \asf_license_stop
*
*/
/*
* Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
*/
#include "MAX31856/MAX31856.h"
#include <asf.h>
#ifndef CONF_BOARD_H
#define CONF_BOARD_H



#define SDI12_MAX_SUPPORTED_SENSORS 25


struct LoggerConfig{
	char SDI12_SensorAddresses[SDI12_MAX_SUPPORTED_SENSORS + 1];	//extra char on the end for ease of f_gets function.
	uint8_t SDI12_SensorNumValues[SDI12_MAX_SUPPORTED_SENSORS];
	uint8_t numSdiSensors;
	uint16_t loggingInterval;
	bool logImmediately;
};


#define SD_VOLUME_NUMBER 0
#define SD_TIME_FILENAME "0:time.txt"
#define SD_DATALOG_FILENAME "0:data.csv"
#define SD_READING_IN_PROGRESS_FILENAME "0:a"
#define SD_CONFIG_FILENAME "0:lgr.cfg"



#define SDI_PIN PIN_PA09
#define SDI_PIN_PORT PORTA
#define SDI_PIN_PINMASK 0x200 // 1<<9

#define DEND_ANALOG_PIN ADC_POSITIVE_INPUT_PIN0		//PA02
#define DENDROMETER_TRAVEL_DISTANCE_MICROMETERS 12700.0	//12.7 millimeters

#define MAX31856_THERMOCOUPLE_TYPE_USED MAX31856_THERMOCOUPLE_TYPE_T

#define DS1302_DATA_PIN		PIN_PA03
#define DS1302_CLOCK_PIN	PIN_PA04
#define DS1302_ENABLE_PIN	PIN_PA05

#endif // CONF_BOARD_H
