// Shock collar driver. Provides routines to drive a Petrainer dog training
// (shock) collar.
//
// See README.txt for information, calling et c.
//
// (C) 2019-2023 Ruru, ruru67@yahoo.com
//
// Non-commercial personal use and modification of this work is permitted.
// Do not distribute this or derived work without permission.
//
#include <Arduino.h>
#include "ShockCollar.h"

#define COLLAR_KEEPALIVE 120000         // Frequency (ms) of keepalive messages

//-- Construct a command packet------------------------------------------------
// Returns 1 if packet buffer formats correctly, 0 otherwise
// pkt	= buffer to place packet in. 
// key	= transmitter ID key
// chan = channel (1 or 2)
// cmd  = command (1..4)
// pwr	= power level (0..100)
//
// The packet format, in bits, is as follows, fields sent high-order bit first 
// (left to right):
//
//	Octet	Field	  Bits	Value	   
//	0	Lead-in   1	1
//		Chan	  3	Ch1=000; Ch2=111
//		Mode	  4	LED=1000; BEEP=0100; VIB=0010; ZAP=0001
//	1-2	Key	  16	Identity field, specific to transmitter[2]
//	3	Power	  8	Power level[2]
//	4	ModeX	  4	LED=1110; BEEP=1101; VIB=1011; ZAP=0111
//		ChanX	  3	Ch1=111; Ch2=000
//	4/5	Trailer   2	00 (Note, 2nd trailer bit not strored in pkt)
//
// Example: Chan=1, Key=0xabcd, Mode=ZAP, Power=100 (0x64)
//	Octet:	     0	       | 1	 |2	  | 3	    | 4 	|5
//	Bit:	     7 654 3210| 76543210|76543210| 76543210| 7654 321 0|7
//			       |   111111|11112222| 22222233| 3333 333 3|4
//	Wire bit:    0 123 4567| 89012345|67890123| 45678901| 2345 678 9|0
//	Fields:      l ccc mmmm| kkkkkkkk|kkkkkkkk| pppppppp| MMMM CCC t|t
//	Field bits:  - 2-0 3--0|15------8|7------0| 7------0| 3--0 2-0 -|-
//	Packet bits: 1 000 0001| 10101011|11001101| 01100100| 0111 111 0|0
//	Hex	     8	   1   | a   b	 |c   d   | 6	4   | 7    e	|0
//	Fields: l=Lead-in c=Chan m=Mode k=Key p=Power M=ModeX C=ChanX t=Trailer
//
int ShockCollar::packet(collar_pkt &pkt,
			collar_key key, char chan, collar_cmd cmd, char pwr) {
	unsigned char h, t;	// pkt[0] and pkt[4] values

	// Set up the composite values for pkt[0] & pkt[4] (in h & t).
	// First, set the Lead-in, Channel, ChanX and Trailer values, based on
	// the channel number.
	//
	pkt[0] = 0;		// Force invalid packet
	switch(chan) {		//     lcccmmmm         MMMMCCCt
	case 1: 		h =  0b10000000;  t = 0b00001110;	break;
	case 2: 		h =  0b11110000;  t = 0b00000000;	break;
	default:		return 0;
	}

	// And then apply the Mode and ModeX values based on the command.
	//
	switch(cmd) {		//     lcccmmmm         MMMMCCCt
	case COLLAR_LED:	h |= 0b00001000; t |= 0b11100000;	break;
	case COLLAR_BEEP:	h |= 0b00000100; t |= 0b11010000;	break;
	case COLLAR_VIB:	h |= 0b00000010; t |= 0b10110000;	break;
	case COLLAR_ZAP:	h |= 0b00000001; t |= 0b01110000;	break;
	default:		return 0;
	}

	// Assemble the packet.
	//
	pkt[0]	= h;			// Lead-in, Channel, Mode
	pkt[1]	= key >> 8;		// Transmitter key, MSB
	pkt[2]	= key;			// Transmitter key, LSB
	pkt[3]	= pwr;			// Power
	pkt[4]	= t;			// ModeX, ChanX, trailer
	return 1;
}


//-- Send a packet to the collar ----------------------------------------------
//
// First, a routine to send a pulse of arbitrary length and spacing
// We use a context variable to lock-step with micros() to take into account
// code overhead (measured at 18-20 microseconds). Doing it this way means we
// don't need to guess (as long as we can assume that overhead is more or less
// constant per bit). Any slop is taken up in the off time, not the on time.
// Note that call returns after sending the on pulse; the off delay is
// imposed in the next call. That means the inter-packet gap must include the
// off delay for the trailer bit.
// Note that this code blocks during packet transmission.
//
void ShockCollar::sendpulse(long &clk, int on, int off) {
	delayMicroseconds(clk - micros());
	digitalWrite(collar_pin, HIGH);
	delayMicroseconds(on - 2);
	digitalWrite(collar_pin, LOW);
	clk += on + off;
}

// Timings
// Bits are on-LONG-off-SHORT for one, on-SHORT-off-LONG for zero.
//
#define BIT_START 1500,750	// Start bit, 1500 microseconds on, 750 off
#define BIT_ZERO   250,750	// Short (0),  250 microseconds on, 750 off
#define BIT_ONE    750,250	// Long  (1),  750 microseconds on, 250 off

// And the actual function.
// pkt	= pointer to packet buffer formatted by packet().
// 
void ShockCollar::send(collar_pkt &pkt) {
	long clk;	// Timer context
	char bit;	// Bit counter
	
	// Send the packet (with the indicator LED lit)
	//
	if(!pkt || !(pkt[0] & 0x80)) return;		// Ignore invalid
	if(collar_led >= 0) digitalWrite(collar_led, HIGH);	// Blinkenlight
	clk = micros(); 				// Start bit clock
	sendpulse(clk, BIT_START);			// Long start pulse
	for(bit = 0; bit < 40; bit++)			// 40 data bits
		if((pkt[bit >> 3] >> (7 - (bit & 7))) & 1)
			sendpulse(clk, BIT_ONE);	// High-order first
		else	sendpulse(clk, BIT_ZERO);
	sendpulse(clk, BIT_ZERO);			// Send 2nd trailer bit
	if(collar_led >= 0) digitalWrite(collar_led, LOW);
	delayMicroseconds(9000);			// 9ms inter-packet
}


//-- Execute a collar command -------------------------------------------------
//
// Transmit commands to collar for a period. 
// key	= transmitter ID key
// chan = channel (1, 2, or 3 for both channels)
// cmd  = command (1..4)
// pwr	= power level (0..100)
// durn = Duration in ms, or negative packet count
// intr = function to call to check if function should be interrupted
//	  Note that intr() is only called about once every 50ms, i.e.
//	  between transmitted packets.
// Return 0 on error, 1 on success, 2 if interrupted.
//
int ShockCollar::command(collar_cmd cmd, char chan, char pwr, long durn) {
	unsigned long t = millis();
	collar_pkt pkt1, pkt2;

	// Construct packet(s)
	//
	if(chan & 1) if(!packet(pkt1, key, 1, cmd, pwr)) return 0;
	if(chan & 2) if(!packet(pkt2, key, 2, cmd, pwr)) return 0;

	for(;;) {
		// Check for interruptions, or completion of time limit
		// or packet count
		//
		if(interrupt) if(interrupt()) return 2;
		if((durn >= 0 && millis() - t >= durn) ||
		   (durn  < 0 && ++durn >= 0))
			return 1;

		// Send packets
		//
		if(chan & 1) send(pkt1);
		if(chan & 2) send(pkt2);
	}
}


//-- Collar keep-alive --------------------------------------------------------
//
// Check if the keepalive period has expired. If it has, send three
// quick LED commands to keep the collar from going to sleep. If channel 
// specified, do for just that channel; if channel is 3, do both.
// key	= transmitter ID key
// chan = 0: do not keepalive; 1,2: keep <chan> alive; 3: keep both
//
void ShockCollar::keepalive() {
	if(!kchan || millis() - lastkeepalive < COLLAR_KEEPALIVE)
		return;
	command(COLLAR_LED, kchan, 50, -3);
	lastkeepalive = millis();
}


//-- Set up collar output pins ------------------------------------------------
//
// Sets up the radio output and LED pins
//
void ShockCollar::begin(char pin, char led) {
	collar_pin = pin;
	collar_led = led;
	key = 0x1234;			// Default
        kchan = 0;
	interrupt = 0;
	pinMode(pin, OUTPUT);		// Set transmitter pin as output
	digitalWrite(pin, LOW);		// Turn off radio
	if(led >= 0) {
		pinMode(led, OUTPUT);	// Set LED pin as output
		digitalWrite(led, LOW);	// Turn off LED
	}
}

//== Remote receiver code =====================================================
//
//-- Set up remote receiver ---------------------------------------------------
//
void ShockCollarRemote::begin(char pin) {
	remote_pin = pin;	// Input data pin
	pinMode(pin, INPUT);
	pt = et = st = 0;	// Timers
	state = 0;		// Idling
	bit = 99;		// Invalid
	expect_key = 0;		// Expect key;
}


//-- Monitor a pin to receive collar command packets --------------------------
//
char ShockCollarRemote::receive() {
	char b;
	char c, p, m, cx, mx;
	collar_cmd cmd;
	collar_key k;
	long ct, t;

	// Read input pin to see if it has changed.
	// If it's the start of a pulse, record the time.
	//
	b = digitalRead(remote_pin);	// Get pin state
	if(b == state) return 0;	// If no change, we're done
	state = b;			// Save state
	ct = micros();			// Get time
	if(state == 1) {		// Pulse start?
		pt = ct;		// Save start time
		return 0;		// And adios!
	}

	// If we've returned to 0, see how long that pulse was.
	// We allow lots of slop in this measurement because we're polling 
	// and may have been late reading one end or the other of the pulse.
	// Basically, we should be OK if we can get to this every 100us
	// or so.
	//
	t = ct - pt;			// Pulse length
	if(t > 100 && t < 400)		// ~250us = 0
		b = 0;
	else if(t > 600 && t < 900) 	// ~750us = 1
		b = 1;
	else if(t > 1300 && t < 1700) {	// ~1500us = start
		for(b = 0; b < 5; b++)	// Erase the packet
			pkt[b] = 0;
		bit = 0;		// Start bit counter
		st = ct;		// and record start time
		return 0;
	}
	else	return 0;		// Noise. Shrug.

	// We have a data bit. Put it in the packet (if there's room)
	// Done if we don't have 40 bits yet ... or if the timing of the
	// packet is off. (Should be just under 40ms).
	//
	if(bit >= 40) return 0;
	pkt[bit >> 3] |= b << (7 - (bit & 7));
	t = ct - st;			// Time since start bit
	if(++bit != 40 || t < 37000 || t > 42000) return 0;

	// Extract the bits from the packet
	// pkt[4] is pkt[0] complemented and reversed. So when we check
	// values in pkt[0] (c & m) for validity, we'll also check that
	// the pkt[4] fields (mx & cx) also hold appropriate values.
	//
	c =  pkt[0] >> 4;		// Channel & lead-in bit
	m =  pkt[0] & 0xf;		// Mode (command)
	k = (pkt[1] << 8) | pkt[2];	// Key
	p =  pkt[3];			// Power
	mx = pkt[4] >> 4;		// ModeX (see docs)
	cx = pkt[4] & 0x0f;		// ChanX & trailer bit

	// Check validity of key (if requested) 
	//
	if(expect_key && k != expect_key)	return 0;

	// Check channel, lead-in & trailer bits
	//
	if(     c == 0b1000 && cx == 0b1110)	c = 1;
	else if(c == 0b1111 && cx == 0b0000)	c = 2;
	else 					return 0;

	// and mode (command)
	//
	if     (m == 0b1000 && mx == 0b1110)	cmd = COLLAR_LED;
	else if(m == 0b0100 && mx == 0b1101)	cmd = COLLAR_BEEP;
	else if(m == 0b0010 && mx == 0b1011)	cmd = COLLAR_VIB;
	else if(m == 0b0001 && mx == 0b0111)	cmd = COLLAR_ZAP;
	else					return 0;

	// Get the inter-packet time
	// If it's less than 120ms (to allow for a couple of missed
	// packets) since the last packet, and the packet is basically
	// the same as last time, we can just return now with a return
	// code of 2.
	//
	t = st - et;			// et is end time of last packet
	et = ct;
	if(t < 120000 && k == key && c == chan && cmd == command
					       &&   p == power)
		return 2;

	// Copy the data into the object, and signal that we have a
	// shiny new packet.
	//
	key	= k;
	chan	= c;
	command	= cmd;
	power	= p;
	return 1;
}

//eof
