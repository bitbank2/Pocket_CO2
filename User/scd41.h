//
// SCD4x CO2 sensor library
// written by Larry Bank
// bitbank@pobox.com
// Copyright (c) 2023 BitBank Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef SCD41_H_
#define SCD41_H_

extern int _iPowerMode, _iTemperature, _iHumidity;
extern uint16_t _iCO2;

#define SCD_SUCCESS 0
#define SCD_ERROR 1
#define SCD_NOT_READY 2

enum {
	SCD_POWERMODE_NORMAL=0,
	SCD_POWERMODE_LOW,
	SCD_POWERMODE_ONESHOT
};

// 16-bit I2C commands
#define SCD41_CMD_START_PERIODIC_MEASUREMENT              0x21b1
#define SCD41_CMD_START_LP_PERIODIC_MEASUREMENT           0x21ac
#define SCD41_CMD_SINGLE_SHOT_MEASUREMENT                 0x219d
#define SCD41_CMD_READ_MEASUREMENT                        0xec05 // execution time: 1ms
#define SCD41_CMD_STOP_PERIODIC_MEASUREMENT               0x3f86 // execution time: 500ms
#define SCD41_CMD_SET_AUTOMATIC_SELF_CALIBRATION_ENABLED  0x2416 // execution time 1ms
#define SCD41_CMD_GET_DATA_READY_STATUS                   0xe4b8 // execution time: 1ms
#define SCD41_CMD_POWERDOWN                               0x36e0 // execution time: 1ms
#define SCD41_CMD_WAKEUP                                  0x36f6 // execution time: 20ms
#define SCD41_CMD_FORCE_RECALIBRATE                       0x362f // execution time: 400ms
int scd41_readRegister(uint16_t u16Register, uint16_t *pOut);
void scd41_wakeup(void); // SCD41 only
int scd41_sendCMD(uint16_t u16Cmd);
int scd41_sendCMD2(uint16_t u16Cmd, uint16_t u16Parameter);
uint8_t scd41_computeCRC8(uint8_t *data, uint8_t len);
int scd41_start(int iPowerMode);
int scd41_getSample(void);
int scd41_shutdown(void); // SCD41 only
int scd41_stop(void);
int scd41_recalibrate(uint16_t u16CO2);

#endif /* SCD41_H_ */
