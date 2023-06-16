// Shock Collar Remote Receiver
//
// This little ditty demonstrates the ShockCollarRemote object to dump
// the contents of any valid received packet from a Petrainer collar remote
// controller.
//
// See the ShockCollar code for packet formats / timing.
//
// (C) 2019-2023 Ruru, ruru67@yahoo.com
//
// Non-commercial personal use and modification of this work is permitted.
// Do not distribute this or derived work without permission.
//
#include <ShockCollar.h>
#define PIN 14			// Data pin of receiver

// Some display shortcuts
//
#define P(x) Serial.print(x)	// General "print"
void H(unsigned int n) {	// Print a n integer in hex
	char i;
	for(i = 12; i >= 0; i -= 4)
		Serial.write("0123456789abcdef"[(n >> i) & 0xf]);
}

ShockCollarRemote remote;

// Set things up
void setup() {
	Serial.begin(9600);	// Start the serial port
	remote.begin(PIN);	// Set up the receiver
}

// Loop de loop
void loop() {
	int ret = remote.receive();
	if(ret) {
		P("Key: ");  	H(remote.key);
		P("  Chan: "); 	P((int) remote.chan);
		P("  Cmd: ");  	P((int) remote.command);
		switch(remote.command) {
		case COLLAR_LED:	P(" (LED)");	break;
		case COLLAR_BEEP:	P(" (Beep)");	break;
		case COLLAR_VIB:	P(" (Vib)");	break;
		case COLLAR_ZAP:	P(" (Zap)");	break;
		}
		P("  Power: ");	P((int) remote.power);
		P("  Ret: ");	P(ret);
		if(ret == 1)	P(" (First)");
		P("\r\n");
	}
}
