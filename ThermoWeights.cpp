// Work in progress
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include <HX711.h>
#include <Wire.h>
#include <WString.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 1. HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 6;
const int LOADCELL_SCK_PIN = 5;
const int tareButtonPin = 2;
const int calibrateScaleButtonPin = 4;

const int numOfCalibrationIterations = 10;
const int numOfWeightingIterations = 10;
const int readyms = 15;

const int CHAN_A = 1;
const int CHAN_B = 2;

long loadcell_offset_a = 0;
long loadcell_offset_b = 0;
float loadcell_scale_a = 1.0f;
float loadcell_scale_b = 1.0f;

HX711 loadcell;

void loadSettingsFromEEPROM();
void printSettings();
long getCalibrationOffsetForSingleChannel(int chan);
float getCalibrationScaleForSingleChannel(int chan, float realWeightInGrams);
void calibrateTare();
void calibrateScales(int realWeightInGrams);
void printBoth(String text);
void printlnBoth(String text);
void printBoth(long number);
void printlnBoth(long number);
void printBoth(double number);
void printlnBoth(double number);
void printCurrentWeight(int repeat);
void switchToChannel(int chan);

void setup() {
	Serial.begin(9600);

	// SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
	if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
		Serial.println(F("SSD1306 allocation failed"));
		for (;;)
			; // Don't proceed, loop forever
	}

	// Show initial display buffer contents on the screen --
	// the library initializes this with an Adafruit splash screen.
	display.display();

	delay(100);

	// Clear the buffer
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(WHITE);

	loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
	//loadcell.set_scale(LOADCELL_DIVIDER);
	//loadcell.set_offset(LOADCELL_OFFSET);
	//loadcell.tare();

	// initialize the buttons:
	pinMode(tareButtonPin, INPUT_PULLUP);
	pinMode(calibrateScaleButtonPin, INPUT_PULLUP);

	//clearEEPROM();
	loadSettingsFromEEPROM();
	printSettings();
	delay(4000);
}

void loop() {
	if (!digitalRead(tareButtonPin)) {
		calibrateTare();
		delay(3000);
	}
	if (!digitalRead(calibrateScaleButtonPin)) {
		calibrateScales(5000.0f);
		delay(3000);
	}
	printCurrentWeight(numOfWeightingIterations);
	//delay(500);
}

void printBStuffBefore() {
	switchToChannel(CHAN_B);
	loadcell.wait_ready(readyms);
	long raw_b4 = loadcell.read_average(numOfWeightingIterations);
	loadcell.wait_ready(readyms);
	long value_b4 = loadcell.get_value(numOfWeightingIterations);
	loadcell.wait_ready(readyms);
	float units_b4 = loadcell.get_units(numOfWeightingIterations);
	Serial.println();
	Serial.print("raw_b4 ");
	Serial.println(raw_b4);
	Serial.print("value_b4 ");
	Serial.println(value_b4);
	Serial.print("units_b4 ");
	Serial.println(units_b4);
}

void printBStuffAfter() {
	switchToChannel(CHAN_B);
	loadcell.wait_ready(readyms);
	long raw_af = loadcell.read_average(numOfWeightingIterations);
	loadcell.wait_ready(readyms);
	long value_af = loadcell.get_value(numOfWeightingIterations);
	loadcell.wait_ready(readyms);
	float units_af = loadcell.get_units(numOfWeightingIterations);
	Serial.print("raw_af ");
	Serial.println(raw_af);
	Serial.print("value_af ");
	Serial.println(value_af);
	Serial.print("units_af ");
	Serial.println(units_af);
	Serial.println();
}

void printCurrentWeight(int repeat) {
	display.clearDisplay();
	display.setCursor(0, 0); // Start at top-left corner
	switchToChannel(CHAN_A);
	loadcell.wait_ready(readyms);
	float current_weight_a = loadcell.get_units(repeat);

	switchToChannel(CHAN_B);
	printBStuffBefore();
	loadcell.wait_ready(readyms);
	float current_weight_b = loadcell.get_units(repeat);
	printBStuffAfter();
	long average = (current_weight_a + current_weight_b) / 2;
	display.setTextSize(4);
	printlnBoth(average);
	display.setTextSize(1);
	printBoth("A: ");
	printlnBoth(current_weight_a);
	printBoth("B: ");
	printlnBoth(current_weight_b);
}

void clearEEPROM() {
	for (int i = 0; i < EEPROM.length(); i++) {
		EEPROM.write(i, 0);
	}
}

void loadSettingsFromEEPROM() {
	int addr = 0;
	int longSize = sizeof(long);
	int floatSize = sizeof(float);
	EEPROM.get(addr, loadcell_offset_a);
	addr += longSize;
	EEPROM.get(addr, loadcell_offset_b);
	addr += longSize;
	EEPROM.get(addr, loadcell_scale_a);
	addr += floatSize;
	EEPROM.get(addr, loadcell_scale_b);
	if (loadcell_scale_a == 0) {
		loadcell_scale_a = 1.0f;
	}
	if (loadcell_scale_b == 0) {
		loadcell_scale_b = 1.0f;
	}
	// Switching to channels applies settings to loadcell
	switchToChannel(CHAN_A);
	switchToChannel(CHAN_B);
}

void saveSettingsToEEPROM() {
	int addr = 0;
	int longSize = sizeof(long);
	int floatSize = sizeof(float);
	if (loadcell_scale_a == 0) {
		loadcell_scale_a = 1.0f;
	}
	if (loadcell_scale_b == 0) {
		loadcell_scale_b = 1.0f;
	}
	EEPROM.put(addr, loadcell_offset_a);
	addr += longSize;
	EEPROM.put(addr, loadcell_offset_b);
	addr += longSize;
	EEPROM.put(addr, loadcell_scale_a);
	addr += floatSize;
	EEPROM.put(addr, loadcell_scale_b);
	// Switching to channels applies settings to loadcell
	switchToChannel(CHAN_A);
	switchToChannel(CHAN_B);
}

void printSettings() {
	display.clearDisplay();
	display.setCursor(0, 0); // Start at top-left corner
	Serial.println();
	printlnBoth("Current settings: ");
	printBoth("A. Offset: ");
	printlnBoth(loadcell_offset_a);
	printBoth("B. Offset: ");
	printlnBoth(loadcell_offset_b);
	printBoth("A. Scale: ");
	printlnBoth(loadcell_scale_a);
	printBoth("B. Scale: ");
	printlnBoth(loadcell_scale_b);
	Serial.println();
}

void printBoth(String text) {
	Serial.print(text);
	//display.setTextSize(1);             // Normal 1:1 pixel scale
	//display.setTextColor(WHITE);        // Draw white text
	//display.print(F(text));
	display.print(text);
	display.display();
}

void printlnBoth(String text) {
	Serial.println(text);
	//display.setTextSize(1);             // Normal 1:1 pixel scale
	//display.setTextColor(WHITE);        // Draw white text
	//display.println(F(text));
	display.println(text);
	display.display();
}

void printBoth(long number) {
	Serial.print(number);
	//display.setTextSize(1);             // Normal 1:1 pixel scale
	//display.setTextColor(WHITE);        // Draw white text
	display.print(number);
	display.display();
}

void printlnBoth(long number) {
	Serial.println(number);
	//display.setTextSize(1);             // Normal 1:1 pixel scale
	//display.setTextColor(WHITE);        // Draw white text
	display.println(number);
	display.display();
}

void printBoth(double number) {
	Serial.print(number);
	//display.setTextSize(1);             // Normal 1:1 pixel scale
	//display.setTextColor(WHITE);        // Draw white text
	display.print(number);
	display.display();
}

void printlnBoth(double number) {
	Serial.println(number);
	//display.setTextSize(1);             // Normal 1:1 pixel scale
	//display.setTextColor(WHITE);        // Draw white text
	display.println(number);
	display.display();
}

void calibrateTare() {
	display.clearDisplay();
	display.setCursor(0, 0); // Start at top-left corner
	printlnBoth("Tare...");
	long old_offset_a = loadcell_offset_a;
	long old_offset_b = loadcell_offset_b;
	loadcell_offset_a = getCalibrationOffsetForSingleChannel(CHAN_A);
	//printBoth("50%...");

	printBStuffBefore();
	loadcell_offset_b = getCalibrationOffsetForSingleChannel(CHAN_B);
	printBStuffAfter();

	//printlnBoth("100%");
	delay(500);
	display.clearDisplay();
	display.setCursor(0, 0);
	float differenceA = old_offset_a - loadcell_offset_a;
	float differenceB = old_offset_b - loadcell_offset_b;
	display.setTextSize(1);
	printBoth("A diff: ");
	printlnBoth(differenceA);
	printBoth("B diff: ");
	printlnBoth(differenceB);
	display.setTextSize(1);
	delay(2000);
	saveSettingsToEEPROM();
	printSettings();
}

void calibrateScales(int realWeightInGrams) {
	display.clearDisplay();
	display.setCursor(0, 0); // Start at top-left corner
	printlnBoth("Calibrating scales...");
	loadcell_scale_a = getCalibrationScaleForSingleChannel(CHAN_A,
			realWeightInGrams);
	//printlnBoth("50%...");

	printBStuffBefore();
	loadcell_scale_b = getCalibrationScaleForSingleChannel(CHAN_B,
			realWeightInGrams);
	printBStuffAfter();

	//printlnBoth("100%");
	delay(500);
	saveSettingsToEEPROM();
	printSettings();
}

void switchToChannel(int chan) {
	int gain = 0;
	long offset = 0;
	float scale = 1.0f;
	//set gain and channel, 32 = B, 64 = A
	if (chan == CHAN_A) {
		gain = 64;
		offset = loadcell_offset_a;
		scale = loadcell_scale_a;
	} else if (chan == CHAN_B) {
		gain = 32;
		offset = loadcell_offset_b;
		scale = loadcell_scale_b;
	}
	if (gain == 0) {
		printlnBoth("Error: unknown channel");
	} else {
		//loadcell.read(); // raw reading
		loadcell.set_gain(gain);
		//loadcell.read();
	}
	loadcell.set_offset(offset);
	loadcell.set_scale(scale);
	loadcell.wait_ready(readyms);
	if (loadcell.is_ready()) {
		//Serial.println("ready");
	} else {
		Serial.println("LoadCell is NOT ready");
	}
}

long getCalibrationOffsetForSingleChannel(int chan) {
	long offset = 0;
	switchToChannel(chan);
	loadcell.wait_ready(readyms);
	offset = loadcell.read_average(numOfCalibrationIterations);
	return offset;
}

float getCalibrationScaleForSingleChannel(int chan, float realWeightInGrams) {
	float scale_factor = 0;
	switchToChannel(chan);
	loadcell.wait_ready(readyms);
	double current_weight_removed_offset = loadcell.get_value(
			numOfCalibrationIterations);
	scale_factor = ((float) current_weight_removed_offset) / realWeightInGrams; // divide the result by a known weight
	return scale_factor;
}
