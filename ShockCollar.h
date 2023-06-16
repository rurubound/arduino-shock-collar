// Shock collar driver header file
//
// See README.txt for details.
//
// (C) 2019-2023 Ruru, ruru67@yahoo.com
//
// Non-commercial personal use and modification of this work is permitted.
// Do not distribute this or derived work without permission.
//
#ifndef ShockCollar_h
#define ShockCollar_h

// Some data types
//
enum collar_cmd {
 	COLLAR_NONE = 0,		// Placeholder
	COLLAR_LED,			// Light LED
	COLLAR_BEEP,			// Beep the piezzo
	COLLAR_VIB,			// Spin the vibrator
	COLLAR_ZAP			// Zap!
};
typedef unsigned int  collar_key;	// Key is 16 bit unsigned integer
typedef unsigned char collar_pkt[5];	// 5-byte packet buffer

// Shock collar transmitter
//
class ShockCollar {
private:
	char collar_pin;		// Data pin to send via
	char collar_led;		// Data pin to flash activity LED
	unsigned long lastkeepalive = 0;		// Last KA packet time
	void sendpulse(long &clk, int on, int off);	// Send a pulse

public:
	// Parameters
	char kchan;			// Keepalive channel(s)
	collar_key key;			// Key to send to (default 0x1234)
	int (*interrupt)(void);		// Interrupt poll function

	// Methods
	void begin(char pin, char led);
	void begin(char pin) { begin(pin, -1); }
	int  command(collar_cmd cmd, char chan, char pwr, long durn);
	void keepalive();
	int  packet(collar_pkt &pkt, collar_key key, char chan,
					collar_cmd cmd, char pwr);
	void send(collar_pkt &pkt);

	// Shortcut methods
	int  led(char chan, long durn) {
		return command(COLLAR_LED, chan,  1, durn);
	}
	int  beep(char chan, long durn) {
		return command(COLLAR_BEEP, chan, 1, durn);
	}
	int  vib(char chan, long durn, char pwr = 100) {
		return command(COLLAR_VIB, chan, pwr, durn);
	}
	int  led(char chan, long durn, char pwr = 100) {
		return command(COLLAR_ZAP, chan,  pwr, durn);
	}
};

// Shock collar remote receiver
//
class ShockCollarRemote {
private:
	collar_pkt pkt;			// Packet buffer
	char bit;			// Bit counter
	unsigned long pt, st, et;	// Pulse start, pkt start & end times
	char state;			// Last pin state
	char remote_pin;		// Data pin to listen to

public:
	collar_key expect_key;		// If non-0, only this key
	collar_key key;			// Returns: Key
	char chan;			//	    Channel, 1, 2
	char command;			//	    Command (COLLAR_xxx)
	char power;			//	    Power 0..100

	void begin(char pin);		// Initialise
	char receive();			// Returns 1 for new packet,
};					//	   2 for repeat, 0 meh.

#endif // ShockCollar_h
//-- End of header file -------------------------------------------------------
