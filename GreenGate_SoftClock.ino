// GreenGate (software clock version - no persistence)
//
// Ver. 1.1
//
// by Buck McGibbony, GNU General Public License, 2016-2019

#include <math.h>
#define HOUR 60
#define MINUTE 60


#define DELAY_HOURS					4	// Length of gated timespan
#define ANNUNCIATION_BLOCK_MINUTES	30	// Size of annunciation blocks in minutes (how many minutes does each annunciation blink represent)

const int g_nledPin = 10;
const int g_nButtonPin = 3;

volatile unsigned long g_nStartTime = 0;
unsigned long nBlinkTime = 0;
bool g_bNotify = false;
int g_nNotifyStage = 0;
unsigned long g_nNotifyKeyframe = 0;

// time to wait in seconds
unsigned long g_nDelay = DELAY_HOURS * HOUR * MINUTE; // delay seconds

// notify consts
const int g_NotifyEvery = ANNUNCIATION_BLOCK_MINUTES * 2; // Notify every n MINUTES
//const int g_NotifyEvery = HOUR / 2;				// in MINUTES
const int g_nNotifyBlinkPreambleLength = 1000;		// milliseconds
const int g_nNotifyBlinkCountLength = 250;			// milliseconds


void setup () {
		
	//setup I/O
	pinMode( g_nledPin, OUTPUT);
	pinMode( g_nButtonPin, INPUT_PULLUP);
	attachInterrupt( digitalPinToInterrupt( g_nButtonPin), ButtonPress, FALLING);

	// turn on LED
	digitalWrite(g_nledPin, HIGH);

	// init vars
	g_nStartTime = 0;
	nBlinkTime = 0;
}

void ButtonPress() {

	g_bNotify = true;

	if (g_nStartTime == 0) {
		// start gate timer
		g_nStartTime = millis();		// record start time
		g_nNotifyStage = 1;
		g_nNotifyKeyframe = 0;
		//digitalWrite(g_nledPin, LOW); // turn off LED
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
	if( g_nStartTime != 0) {
		unsigned long nMillis = millis();
		unsigned long nTimeSpan = (nMillis - g_nStartTime) / 1000;  // seconds

		// should we announce time left?
		if( ( nTimeSpan % ( g_NotifyEvery * MINUTE) == 0) && !g_bNotify) {
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
					// how many ANNUNCIATION_BLOCK_MINUTES blocks remain?
					nNotifyCount = ceil( 1 + ( g_nDelay - nTimeSpan) / ( ANNUNCIATION_BLOCK_MINUTES * MINUTE));
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
		if( nTimeSpan >= g_nDelay) {
			g_nStartTime = 0;  // clear timer
			digitalWrite( g_nledPin, HIGH);  // turn on LED
		}
	}

	//delay( 10);
}

unsigned long Nearest(unsigned long nVal, unsigned long nBlock) {
	unsigned long nRetVal = 0;
	if (nVal % nBlock == 0) {
		nRetVal = nVal;
	}
	else {
		nRetVal = int(nVal/nBlock) * nBlock + 1;
	}
}
