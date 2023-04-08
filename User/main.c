/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : Larry Bank
 * Version            : V1.0.0
 * Date               : 2023/01/15
 * Description        : Portable CO2 measurement unit
 * Copyright (c) 2023 BitBank Software, Inc. All rights reserved
 *******************************************************************************/

#include <stdint.h>
#include <string.h>
#include "debug.h"
#include "scd41.h"
#include "Arduino.h"
#include "oled.h"
#include "st7302.h"
#include "Roboto_Black_40.h"
#include "Roboto_Black_13.h"
#include "co2_emojis.h"

#define DC_PIN 0xd3
#define CS_PIN 0xd2
#define RST_PIN 0xd4
#define BUTTON0_PIN 0xd2
#define BUTTON1_PIN 0xd3
#define LED_GREEN 0xc3
#define LED_RED 0xc4
#define MOTOR_PIN 0xc5

#define DEBUG_MODE

enum
{
	MODE_CONTINUOUS=0,
	MODE_LOW_POWER,
	MODE_ON_DEMAND,
	MODE_TIMER,
	MODE_COUNT
};

enum
{
	ALERT_VIBRATION=0,
	ALERT_LED,
	ALERT_BOTH,
	ALERT_COUNT
};

int GetButtons(void);
void ShowAlert(void);

const char *szMode[] = {"Continuous", "Low Power ", "On Demand ", "Timer     "};
const char *szAlert[] = {"Vibration", "LEDs     ", "Vib+LEDs "};
static int iMode = MODE_CONTINUOUS; // default
static int iAlert = 0; // vibration only
static int iLevel = 1000; // warning level
static int iPeriod = 5; // wake up period in minutes

#define MAX_SAMPLES 540
static uint8_t ucLast32[32]; // holds top 8 bits of last 32 samples
static uint8_t ucSamples[MAX_SAMPLES]; // 24h worth of samples (80 seconds each)
static int iSample = 0; // number of CO2 samples captured
static int iHead = 0, iTail = 0; // circular list of samples
static int iMaxCO2 = 0, iMinCO2 = 5000;
static int iMaxTemp = 0, iMinTemp = 1000;
static uint8_t ucMaxHumid = 0, ucMinHumid = 100;

// Convert a number into a zero-terminated string
int i2str(char *pDest, int iVal)
{
	char *d = pDest;
	int i, iPlaceVal = 10000;
	int iDigits = 0;

	if (iVal < 0) {
		iDigits++;
		*d++ = '-';
		iVal = -iVal;
	}
	while (iPlaceVal) {
		if (iVal >= iPlaceVal) {
			i = iVal / iPlaceVal;
			*d++ = '0' + (char)i;
			iVal -= (i*iPlaceVal);
			iDigits++;
		} else if (iDigits != 0) {
			*d++ = '0'; // non-zeros were already displayed
		}
		iPlaceVal /= 10;
	}
	if (d == pDest) // must be zero
		*d++ = '0';
	*d++ = 0; // terminator
	return (int)(d - pDest - 1); // string length
} /* i2str() */

//
// Add a sample to the collected statistics
//
void AddSample(int i)
{
	if (_iCO2 > iMaxCO2) iMaxCO2 = _iCO2;
	if (_iCO2 < iMinCO2) iMinCO2 = _iCO2;
	ucLast32[i & 31] = (uint8_t)(_iCO2>>5); // keep top 8 bits
	if ((i & 31) == 0) {
		int iAvg = 0;
		// take average and store it
		for (int j=0; j<31; j++)
			iAvg += ucLast32[j];
		ucSamples[iHead++] = (uint8_t)(iAvg>>5);
		if (iHead >= MAX_SAMPLES) iHead -= MAX_SAMPLES; // wrap
	}
	if (_iTemperature > iMaxTemp)
		iMaxTemp = _iTemperature;
	else if (_iTemperature < iMinTemp)
		iMinTemp = _iTemperature;
	if ((_iHumidity/10) > ucMaxHumid)
		ucMaxHumid = (uint8_t)(_iHumidity/10);
	else if ((_iHumidity/10) < ucMinHumid)
		ucMinHumid = (uint8_t)(_iHumidity/10);
} /* AddSample() */

void EXTI7_0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      EXTI0_IRQHandler
 *
 * @brief   This function handles EXTI0 Handler.
 *
 * @return  none
 */
void EXTI7_0_IRQHandler(void)
{
  if(EXTI_GetITStatus(EXTI_Line0)!=RESET)
  {
//    printf("EXTI0 Wake_up\r\n");
	  oledFill(0);
    EXTI_ClearITPendingBit(EXTI_Line0);     /* Clear Flag */
  }
}

void Option_Byte_CFG(void)
{
    FLASH_Unlock();
    FLASH_EraseOptionBytes();
    FLASH_UserOptionByteConfig(OB_IWDG_SW, OB_STOP_NoRST, OB_STDBY_NoRST, OB_RST_NoEN);
    FLASH_Lock();
}


void ShowGraph(void)
{
	char szTemp[32];
	int i;

	I2CInit(400000);
	oledFill(0);

	i2str(szTemp, iHead*32);
    oledWriteString(0,0, szTemp, FONT_8x8, 0);
    oledWriteString(-1, 0, " Samples", FONT_8x8, 0);
    i = (iHead*32*5)/60; // number of minutes
    oledWriteString(0,8, "(", FONT_8x8, 0);
	i2str(szTemp, i);
    oledWriteString(-1,8, szTemp, FONT_8x8, 0);
    oledWriteString(-1,8, " minutes)", FONT_8x8, 0);
	oledWriteString(0,16,"CO2 level:",FONT_12x16, 0);
	oledWriteString(0,32,"Min:",FONT_8x8, 0);
	oledWriteString(0,40,"Max:",FONT_8x8, 0);
	oledWriteString(0,48,"Temp min/max: ",FONT_6x8, 0);
	oledWriteString(0,56,"Humi min/max: ", FONT_6x8, 0);

	i2str(szTemp, iMinCO2);
    oledWriteString(40, 32, szTemp, FONT_8x8, 0);
    i2str(szTemp, iMaxCO2);
    oledWriteString(40, 40, szTemp, FONT_8x8, 0);

	i2str(szTemp, iMinTemp/10); // whole part
    oledWriteString(84, 48, szTemp, FONT_6x8, 0);
    oledWriteString(-1, 48, "/", FONT_6x8, 0);
    i2str(szTemp, iMaxTemp/10);
    oledWriteString(-1, 48, szTemp, FONT_6x8, 0);
    oledWriteString(-1, 48, "C", FONT_6x8, 0);

    i2str(szTemp, ucMinHumid);
    oledWriteString(84, 56, szTemp, FONT_6x8, 0);
    oledWriteString(-1, 56, "/", FONT_6x8, 0);
    i2str(szTemp, ucMaxHumid);
    oledWriteString(-1, 56, szTemp, FONT_6x8, 0);
    oledWriteString(-1, 56, "%", FONT_6x8, 0);

    while (digitalRead(BUTTON0_PIN) == 0) {}; // wait for button to release
	while (digitalRead(BUTTON0_PIN) == 1) {}; // wait for button to press
	oledFill(0);
	while (digitalRead(BUTTON0_PIN) == 0) {}; // wait for button to release to exit
} /* ShowGraph() */

//
// Display the current conditions on the OLED
//
void ShowCurrent(void)
{
int x;
char szTemp[32];

	I2CSetSpeed(400000); // OLED can handle 400k
	i2str(szTemp, (int)_iCO2);
	oledClearLine(0); // make sure old pixels are gone
	oledClearLine(8);
	oledClearLine(16); // need to clear the lines since the prop font doesn't overwrite all pixels
	oledClearLine(24);
	oledWriteStringCustom(&Roboto_Black_40, 0, 32, szTemp, 1);
	x = oledGetCursorX();
	oledWriteString(x, 0, "CO2", FONT_8x8, 0);
	oledWriteString(x, 8, "ppm", FONT_8x8, 0);
	oledClearLine(32);
	oledClearLine(40);
	oledClearLine(48);
	oledClearLine(56);
    oledWriteStringCustom(&Roboto_Black_13, 0, 45, (char *)"Temp", 1);
    oledWriteStringCustom(&Roboto_Black_13, 0, 63, (char *)"Humidity", 1);
    i2str(szTemp, _iTemperature/10); // whole part
    oledWriteStringCustom(&Roboto_Black_13, 44, 45, szTemp, 1);
    oledWriteStringCustom(&Roboto_Black_13, -1, -1, ".", 1);
    i2str(szTemp, _iTemperature % 10); // fraction
    oledWriteStringCustom(&Roboto_Black_13, -1, -1, szTemp, 1);
    oledWriteStringCustom(&Roboto_Black_13, -1, -1, "C ", 1);
    i2str(szTemp, _iHumidity/10); // throw away fraction since it's not accurate
    oledWriteStringCustom(&Roboto_Black_13, 64, 63, szTemp, 1);
    oledWriteStringCustom(&Roboto_Black_13, -1, -1, "%", 1);
    // Display an emoji indicating the CO2 level
    // There are 5 which go from happy to angry, so divide the values into
    // 5 categories: 0-999, 1000-1499, 1500-1999, 2000-2499, 2500+
    x = (_iCO2 - 500)/500;
    if (x < 0) x = 0;
    else if (x > 4) x = 4;
    oledDrawSprite(96, 16, 31, 32, (uint8_t *)&co2_emojis[x * 4], 20, 1);
} /* ShowCurrent() */

void RunTimer(void)
{
  int i, iTicks = iPeriod * 731; // number of 83ms ticks per minute
//  oledPower(0); // turn off the display
  oledFill(0);
  oledContrast(20);
  oledWriteString(0,0, "Timer Mode", FONT_12x16, 0);
  while (iTicks > 0) {
	  Delay_Ms(820);
//	  Standby82ms(10); // sleep about 820ms
	  i = GetButtons();
	  if (i == 3) { // both buttons cancels timer mode
		  return;
	  }
//	  BlinkLED((j & 1) ? LED_GREEN : LED_RED, 50);
//	  j++;
	  iTicks -= 10;
  }
  ShowAlert();
} /* RunTimer() */

void RunMenu(void)
{
int iSelItem = 0;
int y;
char szTemp[16];

//       pinMode(BUTTON0_PIN, INPUT_PULLUP);
//       pinMode(BUTTON1_PIN, INPUT_PULLUP);
	   oledInit(0x3c, 400000);
	   oledFill(0);
	   oledContrast(150);
	   oledWriteString(4,0,"Pocket CO2", FONT_12x16, 0);
//	   oledWriteString(0,16,"================", FONT_8x8, 0);
	   while (1) {
		   // draw the menu items and highlight the currently selected one
		   y = 24;
		   oledWriteString(0,y, "Mode", FONT_8x8, (iSelItem == 0));
		   oledWriteString(40,y, szMode[iMode], FONT_8x8, 0);
		   y += 8;
		   oledWriteString(0,y,"Warn", FONT_8x8, (iSelItem == 1));
		   if (iLevel == 500) {// disabled
			   oledWriteString(40,y,"Disabled", FONT_8x8, 0);
		   } else {
     		   i2str(szTemp, iLevel);
	    	   oledWriteString(40,y, szTemp, FONT_8x8, 0);
		       oledWriteString(-1,y, " ppm ", FONT_8x8, 0); // erase old value
		   }
		   y += 8;
		   oledWriteString(0,y,"Alert", FONT_8x8, (iSelItem == 2));
		   oledWriteString(48,y,szAlert[iAlert], FONT_8x8, 0);
		   y += 8;
		   oledWriteString(0,y,"Time", FONT_8x8, (iSelItem == 3));
		   i2str(szTemp, iPeriod); // time in minutes
		   oledWriteString(40, y, szTemp, FONT_8x8, 0);
		   oledWriteString(-1,y, " Mins ", FONT_8x8, 0); // erase old value
		   y += 8;
		   oledWriteString(0,y,"Start", FONT_8x8, (iSelItem == 4));
		   // wait for button releases
		   while (GetButtons() != 0) {
			   Delay_Ms(20);
		   }
		   // wait for a button press
		   while (GetButtons() == 0) {
			   Delay_Ms(20);
		   }
		   y = GetButtons();
		   if (y & 1) { // button 0
		      iSelItem++;
		      if (iSelItem > 4) iSelItem = 0;
		      continue;
		   }
		   if (y & 2) { // button 1 - action on an item
			   switch (iSelItem) {
			   case 0: // mode
				   iMode++;
				   if (iMode >= MODE_COUNT) iMode = 0;
				   break;
			   case 1: // warning level
				   iLevel += 100;
				   if (iLevel > 2500) iLevel = 500;
				   break;
			   case 2: // alert type
				   iAlert++;
				   if (iAlert >= ALERT_COUNT) iAlert = 0;
				   break;
			   case 3: // time period
				   iPeriod += 5;
				   if (iPeriod > 60) iPeriod = 5;
				   break;
			   case 4: // exit
				   return;
			   }
			   continue;
		   }
		   return;
	   };
} /* RunMenu() */

void BlinkLED(uint8_t u8LED, int iDuration)
{
	pinMode(u8LED, OUTPUT);
    digitalWrite(u8LED, 1);
    Delay_Ms(iDuration);
    digitalWrite(u8LED, 0);
} /* BlinkLED() */

void Vibrate(int iDuration)
{
	pinMode(MOTOR_PIN, OUTPUT);
	digitalWrite(MOTOR_PIN, 1);
	Delay_Ms(iDuration);
	digitalWrite(MOTOR_PIN, 0);
} /* Vibrate() */

void ShowAlert(void)
{
int i;

	switch (iAlert)
	{
	case ALERT_VIBRATION:
		for (i=0; i<3; i++) {
		    Vibrate(150);
		//    Standby82ms(10);
		    Delay_Ms(820);
		  }
		break;
	case ALERT_LED:
		for (i=0; i<4; i++) {
		    BlinkLED(LED_GREEN, 300);
		    BlinkLED(LED_RED, 300);
		  }
		break;
	case ALERT_BOTH:
		for (i=0; i<3; i++) {
		    Vibrate(150);
		    BlinkLED(LED_GREEN, 400);
		    BlinkLED(LED_RED, 400);
		  }
		break;
	}
} /* ShowAlert() */

int GetButtons(void)
{
	int i = 0;
	pinMode(BUTTON0_PIN, INPUT_PULLUP); // re-enable gpio in case it got disabled by standby mode
	pinMode(BUTTON1_PIN, INPUT_PULLUP);
	if (digitalRead(BUTTON0_PIN) == 0) i|=1;
	if (digitalRead(BUTTON1_PIN) == 0) i|=2;
	return i;

} /* GetButtons() */

void RunLowPower(void)
{
	int i, iUITick = 0, iSampleTick = 0;

	oledFill(0);
	oledWriteString(0,0,"Low Power...", FONT_8x8, 0);
	Delay_Ms(1000);
//	oledPower(0); // turn off the OLED
	while (1) {
		// capture a sample every 5 minutes
		if (iSampleTick == 0) {
           I2CInit(50000);
	       scd41_start(SCD_POWERMODE_NORMAL);
	      // Delay_Ms(5000);
		}
		i = GetButtons();
		//pinMode(LED_GREEN, OUTPUT);
		//pinMode(LED_RED, OUTPUT);
		//digitalWrite(LED_GREEN, i & 1);
		//digitalWrite(LED_RED, i>>1);
		if (i == 3) { // both buttons pressed, return to menu
	//	    oledInit(0x3c, 400000);
			return;
		} else if (i && iUITick == 0) { // one button pressed, show the current data
//		   oledInit(0x3c, 400000);
		   oledPower(1);
		   ShowCurrent(); // display the current conditions on the OLED
		   iUITick = 20; // number of 250ms periods before turning off the display
		}
		Delay_Ms(250);
		//Standby82ms(3); // conserve power (1.8mA running, 10uA standby)
		iSampleTick++;
		if (iSampleTick == 21) { // 5 seconds have passed
		  // I2CInit(50000);
			I2CSetSpeed(50000);
	       scd41_getSample();
	       scd41_stop(); // power down the sensor
		}
		if (iSampleTick == 1200) { // 5 minutes have passed, get another sample
			iSampleTick = 0;
		}
		if (iUITick > 0) {
			iUITick--;
			if (iUITick == 0) { // shut off the display
				I2CInit(400000);
				oledPower(0);
			}
		}
	}
} /* RunLowPower() */

void RunOnDemand(void)
{

} /* RunOnDemand() */

int main(void)
{

    Delay_Init();
//    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
//    Option_Byte_CFG(); // allow PD7 to be used as GPIO
//    Delay_Ms(5000); //100); // give time for power to settle
//    USART_Printf_Init(460800);
//    printf("SystemClk:%d\r\n",SystemCoreClock);
    iAlert = ALERT_BOTH;
    ShowAlert(); // blink LEDs and vibration motor
menu_top:
   RunMenu();
   if (iMode == MODE_TIMER) {
	   RunTimer();
	   goto menu_top;
   } else if (iMode == MODE_LOW_POWER) {
	   RunLowPower();
	   goto menu_top;
   } else if (iMode == MODE_ON_DEMAND) {
	   RunOnDemand();
	   goto menu_top;
   } else { // continuous mode
	   I2CSetSpeed(50000);
	   scd41_start(SCD_POWERMODE_NORMAL);
	   Delay_Ms(5000); // allow time for first sample to capture
    while(1) {
    	int i, j;
get_sample:
    	I2CSetSpeed(50000); // SCD40 can't handle 400k
    	scd41_getSample();
    	iSample++;
    	if (iSample > 3) AddSample(iSample); // add it to collected stats
    	if (iSample == 16 && iMode != MODE_CONTINUOUS ) { // after 1 minute, turn off the display
    		oledPower(0); // turn off display
    	}
    	ShowCurrent(); // display the current conditions on the OLED
		for (i=0; i<61; i+= 3) { // 5 seconds total
#ifdef DEBUG_MODE
			Delay_Ms(3*82); // use a power wasting delay to allow SWDIO to work
#else
			Standby82ms(3); // conserve power (1.8mA running, 10uA standby)
#endif
			j = GetButtons();
			if (j == 3) { // both buttons pressed
				goto menu_top;
			}
//				if (iMode == 0) {
					// OLED is off, turn it back on
//					I2CInit(100000);
//					oledPower(1);
//					iMode = 1;
//					while (digitalRead(BUTTON0_PIN) == 0) {}; // wait for button to release
//					iSample = 4; // reset sample counter to reset display timeout (does not affect sample collection)
//				} else {
//					ShowGraph(); // show collected stats
//					goto get_sample; // enough time has passed, get the next sample
//				}
//			}
		} // for i
    } // while (1)
   } // not timer mode
} /* main() */
