// GreenGate (software clock version - no persistence)
//
#define VERSION "1.2"
//
// by Buck McGibbony, GNU General Public License, 2016-2019

//#include <avr/eeprom.h>
#include <EEPROM.h>
#include <math.h>
#define HOUR 60
#define MINUTE 60

// serial communication settings
#define USB_BAUD_RATE 115200

const int g_nledPin = 10;
const int g_nButtonPin = 3;

bool g_bNotify = false;
int g_nNotifyStage = 0;
unsigned long g_nNotifyKeyframe = 0;

// notify consts
const int g_nNotifyBlinkPreambleLength = 1000;		// milliseconds
const int g_nNotifyBlinkCountLength = 250;			// milliseconds


// Settings functions
//
// increment this every time you make ANY change to the Settings or Calibrations structure
// this way we can know when to revert to defaults after a code update
byte SettingsVersion = 1;
struct Settings_t {
	unsigned long nStartTime;		// start time millis
	unsigned long nDelay;			// length of gate time in seconds
	int nNotificationBlockMinutes;	// length of time blocks
	int nNotifyEvery;				// notify every n MINUTES
} Settings;


// LoadDefaults
//
void LoadDefaults() {

	Settings.nStartTime = 0;
	Settings.nDelay = 4 * HOUR * MINUTE;
	Settings.nNotificationBlockMinutes = 30;
	Settings.nNotifyEvery = Settings.nNotificationBlockMinutes;

	// save to EEPROM
	SaveSettings();
}


// LoadSettings
//
void LoadSettings() {

	byte nCurrentSettingsVersion;
	int SettingsBlockStart = sizeof(SettingsVersion);

#if defined(__MKL26Z64__)
	LoadDefaults();
#else
	// first load the settings version and check for mis-match
	eeprom_read_block((void*)&nCurrentSettingsVersion, (void*)0, sizeof(nCurrentSettingsVersion));
	if (nCurrentSettingsVersion != SettingsVersion) {
		LoadDefaults();
	}
	else {
		// load settings
		eeprom_read_block((void*)&Settings, (void*)SettingsBlockStart, sizeof(Settings));
	}
#endif
}


// SaveSettings
//
void SaveSettings() {
	int SettingsBlockStart = sizeof(SettingsVersion);

	// save settings version
	eeprom_write_block((void*)&SettingsVersion, (void*)0, sizeof(SettingsVersion));
	// save settings
	eeprom_write_block((void*)&Settings, (void*)SettingsBlockStart, sizeof(Settings));
}


//	setup()
//
void setup () {
		
	// open serial ports
	Serial.begin(USB_BAUD_RATE);

	//setup I/O
	pinMode( g_nledPin, OUTPUT);
	pinMode( g_nButtonPin, INPUT_PULLUP);
	attachInterrupt( digitalPinToInterrupt( g_nButtonPin), ButtonPress, FALLING);

	// turn on LED
	digitalWrite(g_nledPin, HIGH);

	// load settings
	LoadSettings();
	
	// init vars
	Settings.nStartTime = 0;
}


void ButtonPress() {

	g_bNotify = true;

	if (Settings.nStartTime == 0) {
		// start gate timer
		Settings.nStartTime = millis();		// record start time
		SaveSettings();
		g_nNotifyStage = 1;
		g_nNotifyKeyframe = 0;
		//digitalWrite(g_nledPin, LOW);		// turn off LED
	}
	else {
		// annunciate
		g_nNotifyStage = 0;
		g_nNotifyKeyframe = 0;
	}
}


void loop () {

	static int nNotifyCount = 0;
	static int nNotifyKeyframeState = 0;

	// are we waiting?
	if( Settings.nStartTime != 0) {
		unsigned long nMillis = millis();
		unsigned long nTimeSpan = (nMillis - Settings.nStartTime) / 1000;  // seconds

		// should we announce time left?
		if( ( nTimeSpan % ( Settings.nNotifyEvery * MINUTE) == 0) && !g_bNotify) {
			g_bNotify = true;
			g_nNotifyStage = 0;
			g_nNotifyKeyframe = 0;
		}

		if(g_bNotify) {
			switch(g_nNotifyStage) {
			case 0:
				// 1 second blink
				// setup or execute?
				if(g_nNotifyKeyframe == 0) {
					digitalWrite( g_nledPin, HIGH);  // turn on LED
					g_nNotifyKeyframe = nMillis  + g_nNotifyBlinkPreambleLength;
				}
				else if( nMillis >= g_nNotifyKeyframe) {
					digitalWrite( g_nledPin, LOW); // turn off LED
					g_nNotifyStage = 1;		// goto next stage
					g_nNotifyKeyframe = nMillis  + g_nNotifyBlinkPreambleLength;
				}
				break;
			
			// delay
			case 1:
				// setup or execute?
				if (g_nNotifyKeyframe == 0) {
					digitalWrite(g_nledPin, LOW);	// turn off LED
					g_nNotifyKeyframe = nMillis + g_nNotifyBlinkPreambleLength;
				}
				else if( nMillis >= g_nNotifyKeyframe) {
					g_nNotifyStage = 2;
					// how many Settings.nNotificationBlockMinutes blocks remain?
					nNotifyCount = ceil( 1 + ( Settings.nDelay - nTimeSpan) / ( Settings.nNotificationBlockMinutes * MINUTE));
					nNotifyKeyframeState = HIGH;
					digitalWrite( g_nledPin, nNotifyKeyframeState);
					// setup next keyframe
					g_nNotifyKeyframe = nMillis + g_nNotifyBlinkCountLength;
					nNotifyKeyframeState = LOW;
				}
				break;

			case 2:
				// now do the count
				if( nNotifyCount > 0) {
					if( nMillis >= g_nNotifyKeyframe) {
						digitalWrite( g_nledPin, nNotifyKeyframeState);
						if( !nNotifyKeyframeState) --nNotifyCount;
						
						// setup next keyframe
						g_nNotifyKeyframe = nMillis + g_nNotifyBlinkCountLength;
						nNotifyKeyframeState = !nNotifyKeyframeState;
					}
					
				}
				else {
					// all done
					g_bNotify = false;
				}
				break;
			}
			
		}


		// are we timed out?
		if( nTimeSpan >= Settings.nDelay) {
			Settings.nStartTime = 0;  // clear timer
			digitalWrite( g_nledPin, HIGH);  // turn on LED
		}
	}

	//service comm port
	while (Serial.available()) SerialEvent();

	//delay( 10);
}


// SerialEvent()
//
void SerialEvent() {

	static char g_sCommandBuffer[40] = { 0 };
	static uint8_t g_nCommandLength = 0;

	char sInputChar = 0;   // for incoming serial data

						   // read the incoming char
						   //  sBuffer[Serial.readBytes(sBuffer, Serial.available())] = 0;
	sInputChar = Serial.read();

	switch (sInputChar) {
	case '\r':  // Carriage return /Enter 0x0d
		if (g_nCommandLength > 0) {
			Serial.println();
			ProcessCommand(g_sCommandBuffer);
			Serial.flush();
			g_nCommandLength = 0;
		}

		//show prompt
		Serial.println();
		Serial.print(F("GreenGate>"));
		break;

	case 127: // Backspace
		if (g_nCommandLength > 0) {
			Serial.print(sInputChar);
			//Serial.print( " "); Serial.print( sInputChar);
			g_sCommandBuffer[--g_nCommandLength] = '\0';
		}
		break;

	default:
		// if sInputChar is a visible (non-escape code) character then we append it to our command line buffer
		if ((int)sInputChar > 31 && (int)sInputChar < 127) {
			if (g_nCommandLength< sizeof(g_sCommandBuffer)) {
				// echo char
				Serial.print(sInputChar);
				g_sCommandBuffer[g_nCommandLength] = sInputChar;
				g_sCommandBuffer[++g_nCommandLength] = '\0';
			}
		}
		break;
	}
}


// ProcessCommand
//
void ProcessCommand(char *sCommand) {

	int n;
	int n1;

	switch (sCommand[0]) {
	case 'A':
	case 'a':
		break;

	case 'B':
	case 'b':
		break;

	case 'c':
	case 'C':
		break;

	case 'd':
	case 'D':
		// Load Defaults
		LoadDefaults();
		Serial.println(F("Loaded default settings."));
		break;

	case 'f':
	case 'F':
		break;

	case 'G':
	case 'g':
		break;

	case 'i':
	case 'I':
		break;

	case 'l':
	case 'L':
		Serial.println(F("Current Settings:"));
		Serial.println();

		Serial.print(F("Gate Length (hours)       = ")); Serial.println(Settings.nDelay / HOUR / MINUTE);
		Serial.print(F("Notification Block (mins) = ")); Serial.println(Settings.nNotificationBlockMinutes);
		Serial.print(F("Notify Every (mins)       = ")); Serial.println(Settings.nNotifyEvery);
		break;

	case 'M':
	case 'm':
		break;

	case 'N':
	case 'n':
		break;

	case 'p':
	case 'P':
		break;

	case 'r':
	case 'R':
		LoadSettings();
		Serial.println(F("Settings reloaded."));
		break;

	case 's':
	case 'S':
		SaveSettings();
		Serial.println(F("Settings saved."));
		break;

	case 't':
	case 'T':
		break;

	case 'V':
	case 'v':
		break;

	case 'z':
	case 'Z':
		break;

	case 'h':
	case '?':
		Serial.print(F("GreenGate - By Buck McGibbony - Version ")); Serial.println(VERSION);
		Serial.println();
		Serial.println(F("   L      - List settings"));
		Serial.println(F("   D      - Load default settings"));
		Serial.println(F("   R      - Reload settings"));
		Serial.println(F("   S      - Save settings"));
		break;

	default:
		Serial.println(F("Command unrecognized.  Type '?' or 'h' for help."));
		break;
	}
}
