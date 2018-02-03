//------------------------------------------------------------------------------
/// @addtogroup weather_station Mini Backyard Weather Station
///
/// @file WeatherStation.ino
///
/// @author     Joshua R. Talbot
///
/// @date       02-FEB-18
///
/// @version    1.0 - initial release
///
/// @license
/// Copyright (c) 2018 J. Talbot
/// 
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.
/// 
/// @brief      The start of a backyard weather station using the Sparkfun
///             weather board.
///
///

/** 
 * Information that Wunderground wants to see...
 * 
 * winddir - [0-360 instantaneous wind direction]
 * windspeedmph - [mph instantaneous wind speed]
 * windgustmph - [mph current wind gust, using software specific time period]
 * windgustdir - [0-360 using software specific time period]
 * windspdmph_avg2m  - [mph 2 minute average wind speed mph]
 * winddir_avg2m - [0-360 2 minute average wind direction]
 * windgustmph_10m - [mph past 10 minutes wind gust mph ]
 * windgustdir_10m - [0-360 past 10 minutes wind gust direction]
 * 
 * humidity - [% outdoor humidity 0-100%]
 * dewptf- [F outdoor dewpoint F]
 * tempf - [F outdoor temperature] 
 * --- supported ---
 * rainin - [rain inches over the past hour)] -- the accumulated rainfall in the past 60 min
 * dailyrainin - [rain inches so far today in local time]
 * 
 * baromin - [barometric pressure inches]
 * --- note supported --- 
 * weather - [text] -- metar style (+RA)
 * clouds - [text] -- SKC, FEW, SCT, BKN, OVC
 * --- could be supported using light sensor. --- 
 * solarradiation - [W/m^2]
 * --- could be supported using UV sensor - would like to support for the pool location. ---
 * UV - [index]
 * softwaretype - [text] ie: WeatherLink, VWS, WeatherDisplay
 *
 * https://weatherstation.wunderground.com/weatherstation/updateweatherstation.php?ID=KCASANFR5&PASSWORD=XXXXXX&dateutc=2000-01-01+10%3A32%3A35&winddir=230&windspeedmph=12&windgustmph=12&tempf=70&rainin=0&baromin=29.1&dewptf=68.2&humidity=90&weather=&clouds=&softwaretype=vws%20versionxx&action=updateraw
 * 
* }
 */

#include <Wire.h>       // required for I2C

#include "drv_htu21d.h" // need the hut21 driver we are testing.
#include "MPL3115A2.h"
#include "WSA80422.h"

/*-------------------------------------------------*/
// Hardware pin definitions

// digital I/O pins
#define WSPEED_PIN 		3
#define RAIN_PIN 		2
#define STAT1_PIN		7
#define STAT2_PIN 		8

// analog I/O pins
#define REF_3V3_PIN 	A3
#define LIGHT_PIN 		A1
#define BATT_PIN 		A2
#define WDIR_PIN 		A0

DRV_HTU21D hum_sensor = DRV_HTU21D();
MPL3115A2 baro = MPL3115A2();
WSA80422 wStation = WSA80422();

/* define some timers required to handle the weather station
function calls in a "timely" fashion */

long timer_1s_millis;
long timer_5s_millis;
long timer_60s_millis;

const long timer_1s_preset = 1000;
const long timer_5s_preset = 5000;
const long timer_60s_preset = 60000;


const char *wind_name_ary[] =
	{
		"N", /* WDIR_N,   Offset:  0 */
		"NNW", /* WDIR_NNW, Offset:  3 */
		"NW", /* WDIR_NW,  Offset:  6 */
		"WNW", /* WDIR_WNW, Offset:  9 */
		"W", /* WDIR_W,   Offset: 12 */
		"WSW", /* WDIR_WSW, Offset: 15 */
		"SW", /* WDIR_SW,  Offset: 18 */
		"SSW", /* WDIR_SSW, Offset: 21 */
		"S", /* WDIR_S,   Offset: 24 */
		"SSE", /* WDIR_SSE, Offset: 27 */
		"SE", /* WDIR_SE,  Offset: 30 */
		"ESE", /* WDIR_ESE, Offset: 33 */
		"E", /* WDIR_E,   Offset: 36 */
		"ENE", /* WDIR_ENE, Offset: 39 */
		"NE", /* WDIR_NE,  Offset: 42 */
		"NNE", /* WDIR_NNE, Offset: 45 */
		"ERR", /* WDIR_ERR, Offset: 48 */
	};

void rainIRQ( void ) {
	wStation.rainIRQ_CB();
}

void windIRQ( void ) {
	wStation.windIRQ_CB();
}


void timer_reset( long *timer_value ) {
	*timer_value = millis();
}

bool is_timer_done( long *timer_value, long preset) {
	long current_time = millis();
	if ( (current_time - *timer_value) > preset) {
		*timer_value = current_time;
		return true;
	}
	return false;
}

void setup() {
    Serial.begin(9600);

    if ( hum_sensor.init() ) {
        Serial.println("\n\nHumidity Sensor Init'd!");
    }
    else {
        Serial.println("\n\nERR: Hum Sensor FAILED Init!");
        while(1);
    }

    Serial.println("MPL3115A2.c test!");
    if ( baro.init( true ) ){
        Serial.println("MPL3115A2 init'd!");
        baro.setPressure_Mode();
    }
    else {
    	Serial.println("\n\nERR: MPL3115A2 Sensor FAILED Init!");
        while(1);
    }

    Serial.println("WSA80422.c test!");
    if ( wStation.init( 2, 3, A0 ) ){
        Serial.println("WSA80422 init'd!");
    }
    else {
    	Serial.println("\n\nERR: WSA80422 Sensor FAILED Init!");
        while(1);
    }

    pinMode(STAT1_PIN, OUTPUT); //Status LED Blue
	pinMode(STAT2_PIN, OUTPUT); //Status LED Green

	wStation.init_light_sensor(LIGHT_PIN, REF_3V3_PIN);
	//pinMode(REF_3V3_PIN, INPUT);
	//pinMode(LIGHT_PIN, INPUT);

	timer_reset(&timer_1s_millis);
	timer_reset(&timer_5s_millis);
	timer_reset(&timer_60s_millis);

	wStation.wind_reset_arrays();
}



//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float get_light_level()
{
	return wStation.get_light_level();
}

void test_MPL3115A2( void ) {
	Serial.println("--------  MPL3115A2  -----------");
    Serial.print(baro.getPressure_Pa());Serial.println(" pascals.");
    Serial.print(baro.getPressure_InHg()); Serial.println(" Inches (Hg)");
    Serial.print(baro.getTemperature()); Serial.println("*C");
}

void test_HTU21D( void ) {
	Serial.println("-----------   HTU21D    --------");
    Serial.print(hum_sensor.getTemp_C()); Serial.print(" *C\t");
    Serial.print(hum_sensor.getTemp_F()); Serial.println(" *F");
    Serial.print(hum_sensor.getHumidity());Serial.println("%");
}

void test_WSA80422( void ) {
	Serial.println("-------   BOARD LEVEL   --------");
    Serial.print("Light: ");
    Serial.print(get_light_level()); Serial.println(" (lumens ?)\t");

    Serial.print("Wind: ");
    uint16_t wind_counts = wStation.getWindAcc();
    Serial.print(wind_counts); Serial.print(" counts,\t");
    
    Serial.print(", dir: ");
    Serial.print(wStation.getWindDirRaw()); Serial.println(" raw dir\t");
    
}

void print_temperatures( void ) {
	float c1, c2, c_avg, f_avg, h;
	c1 = baro.getTemperature();
	c2 = hum_sensor.getTemp_C();
	c_avg = (c1 + c2)/2;
	f_avg = (9.0/5.0)*c_avg + 32.0;
	Serial.println("Temperatures:");
	Serial.print(c1); Serial.print("*C, ");
	Serial.print(c2); Serial.println("*C");
	Serial.print("  Averages:");
	Serial.print(c_avg); Serial.print("*C / ");Serial.print(f_avg); Serial.println("*F");
}

void print_wind_data( void ) {
	int16_t x, y;
	uint32_t spd;
	wStation.get_last_a5s_wind( &x, &y, &spd);
	Serial.println("Wind x, y, speed:");
	Serial.print(x);Serial.print(", ");
	Serial.print(y);Serial.print(", ");
	Serial.println(spd);
}

void print_rain_data( void ) {
	uint16_t r_min, r_hr, r_day;
	wStation.get_last_a1m_rain( &r_min );
	Serial.println("Rain last minute:");
	Serial.println(r_min);
	Serial.println("Rain last hour, Daily total:");
	wStation.get_last_a1hr_24hr_rain( r_hr, r_day );
	Serial.print(r_min);Serial.print(", ");Serial.print(r_day);
}

void loop() {
	
	if ( is_timer_done( &timer_5s_millis, timer_5s_preset ) ) {
		Serial.println("\n---------------\n");
		print_wind_data();
		print_temperatures();
		Serial.print("Light Level: ");Serial.println(get_light_level());
	}

	if ( is_timer_done( &timer_1s_millis, timer_1s_preset ) ) {
		wStation.wind_calcs_per_second();
	}

	if ( is_timer_done( &timer_60s_millis, timer_60s_preset ) ) {
		wStation.rain_calcs_per_minute();
	}

}
