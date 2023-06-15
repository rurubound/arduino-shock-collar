Shock collar driver. Provides routines to drive a Petrainer dog training
(shock) collar, and also to interpret commands receibed from its
handheld remote control.


-- Introduction --

This code is based on work done by DeviantDesigns:
  http://deviant-designs.co.uk/2019/03/29/arduino-controlled-shock-collar/

and code by Smouldery and mikey_dk:
  https://github.com/smouldery/shock-collar-control/blob/master/Arduino%20Modules/transmitter_vars.ino

Note that while I used Smouldery's code and underlying work to determine
packet formats, encoding et c, and I thank them for doing that ground
work, this code is a complete re-write (with a much smaller code/data
footprint).


-- Hardware --

This code was tested on a mini Arduino "Beetle", a Pro Micro clone in a
smaller form factor, connected to a simple 433 MHz ASK transmitter (see
the Deviant Designs link). Connect the radio's DATA pin to data pin 14 of
the Beetle, VCC to 5V, and GND to GND. USB power is sufficient. This
arrangement allows the two small boards to be mounted piggyback, with the
antenna opposite the USB plug. It has also been tested on other small 
Aurduino lookalikes such as the Nano (although pin 2 is more convenient on
that board). You'll need an antenna as well.

AliExpress links for hardware:

"Beetle" microcontroller:  https://www.aliexpress.com/item/32775220354.html
Transmitter module:        https://www.aliexpress.com/item/32608684824.html
433 MHz antenna:           https://www.aliexpress.com/item/4000960125591.html
Petrainer collar/remote:   https://www.aliexpress.com/item/32947131768.html
Petrainer collar only:     https://www.aliexpress.com/item/32606845436.html

The collar links are for the rechargeable version; there are cheaper
versions of the same collar/remote that use removable batteries. The
transmitter module also comes with a receiver, which can be used with
the ShockCollarRemote object to receive commands from the handheld
remote.

Note that the collars need to be paired with the controller. This is
done by pressing the reset button on the collar and sending the collar a
command; thereafter (until it's battery goes flat or it is reset again),
the collar will respond to any controler using the same 16 bit collar
key and channel number. 


-- ShockCollar object --

Synopsis:
	#include <ShockCollar.h>
	ShockCollar collar;

The following methods and variables are provided in the ShockCollar
object

	char kchan;
	collar_key key;
	int (*interrupt)(void);

The kchan variable is used by the keepalive() method (see below) to
specify which channels should be kept alive.

The key defaults to 0x1234. This key is a 16-bit value assigned to a
controller and is used to "pair" the collars with it, by pressing the
reset button on the collar, then transmitting a command. From then on,
only commands using the same key and channel will be processed by that
collar.

The interrupt parameter, if set, will be called during the a command to
allow the command to check for interruptions. 

For example, to check for pending data on the serial, use:

	int keypress() {
		return Serial.available();
	}
	...
	ShockCollar collar;
	...
		collar.begin(14);
		collar.interrupt = keypress;

The following high-level methods are provided. As long as you're not
trying to do something weird, these are likely to be the only calls you
need.

void begin(char pin, char led)

	Call this method to set up the data and LED outputs on the
	device.

	pin	Defines the output data pin 
	led	Define the LED pin to flash on activity. Defaults to
		none (-1).

int command(collar_cmd cmd, char chan, char powr, long durn)

	Format and send collar commands for the specified duration.
	Returns 1 if successful, 0 if error, or 2 if interrupted.
	cmd	Command, can be:	COLLAR_LED
					COLLAR_BEEP
					COLLAR_VIB
					COLLAR_ZAP
	chan	Specify channel 1 or 2, or 3 to transmit to both
		alternately.
	pwr	Power, 0-100. 
	durn	Duration of command, in milliseconds, or if negative,
		the (negated) count of packets to send (i.e. -3 will
		send three packets). (Packets take about 47ms each.)

	Note that he collar key should be set up (it defauilts to 0x1234).

	There are four "shortcut" methods that just call command():

		led(  chan, durn)		Light the LED
		beep( chan, durn)	Beep the buzzer
		vib(  chan, durn, pwr)	Buzz the vibrator	
		zap(  chan, durn, pwr)	Zap the brat

void keepalive()

	Call this regularly to ensure the collar doesn't go to sleep
	(done by sending a COLAR_LED command). The function has an
	internal timer to limit transmission to approx once every two
	minutes. The variable kchan must be set to 1, 2 or 3 (3 - both 
	channels).

And these are the low-level methods (which the above functions call).
You can use these if you need to operate more than two collars (using
different keys) or otherwise feel the need to have finer control, such
as vibrating and zapping the collars at the same time.

int packet(collar_pkt pkt, collar_key key, char chan, char cmd, char powr)

	Call this to obtain a formatted packet ready for transmission.
	Returns 1 if packet successfully fomatted, 0 if packet
	parameters out of range.

	pkt	Packet buffer to write packet to
	key	Specify the transmitter key
	chan	Specify the channel (1 or 2)
	cmd	Command, can be:	COLLAR_LED  (1)
					COLLAR_BEEP (2)
					COLLAR_VIB  (3)
					COLLAR_ZAP  (4)
	pwr	Power, 0-100.

void send(collar_pkt pkt)

	Transmit one packet.

	pkt	Packet prepared using packet()

Note that the collar requires a constant stream of packets to keep a command
running. command() will do this for you.

Here is a simple example, using a button wired between pin 3 and ground
to activate a shock on collar channel 1, key 0xbeef:

#include <ShockCollar.h>
ShockCollar collar;			// Declare object

void setup() {
	collar.begin(2);		// Initialise collar, pin 2 is data
	collar.kchan = 1;		// Set keepalive channel
	collar.key = 0xbeef;		// Choose a different key
	ponMode(3, INPUT_PULLUP);	// Pin 3 is the activate button
}

loop() {
	collar_keepalive();		// Keep the collar alive
	if(dgitalRead(3))		// If button pressed ...
		collar.zap(1, 100, 500);// Activate the collar at full
					// power for 500ms.
}

The code is intended to operate up to two collars on the same collar key,
on channels 1 & 2. command() and keepalive() can take 3 as the channel
value to alternately transmit to channels 1 & 2. Note that multiple
ShockCollar objects can be set up on the same data pin but with
different keys.

If using multiple objects, the keeplive method should be called
regularly on all objects. Two objects can not be be commanded
simultaenously however; for that you would need to use the packet()
method to construct packets for each object, then repeatedly call the
send() method for each constructed packet in turn. (This is what the
command() method does internally for each channel).

Note that a command takes bout 48ms to be transmitted, so commanding too
many may mean the packjets for a given coller get spaced too far apart,


-- ShockCollarRemote object --

Synopsis:
	#include <ShockCollar.h>
	ShockCollarRemote remote;

In addition to sending to a collar, the ShockCollarRemote object allows
an Arduino to receive commands from a shock collar handheld remote
control.

The following methods and variables are provided in the ShockCollar
object.

        collar_key expect_key;
        collar_key key;
        char chan;
        char command;
        char power;

If set to a value other than 0, expect_key contains the only key
(identifying a particular controller) that will be passed to the
application. (Simply copying the key from a received packet will "lock"
the receiver to the first controller that sends it a command.)

The key, chan, command, and power values are used to return the 
interpreted contents of an incoming packet:

	key	16 bit controller identifier (hard encoded into each
		controller)
	chan	Channel number, 1 or 2.
	command	One of:	COLLAR_LED  (1)
			COLLAR_BEEP (2)
			COLLAR_VIB  (3)
			COLLAR_ZAP  (4)
	power	Power level, 0-100 for COLLAR_VIB or COLLAR_ZAP;
		0 otherwise.

void begin(char pin, collar_key key)

	Initialises the object, and sets up a data pin for receiving
	data. Resets all status.

char receive()
	This should be called repeatedly to poll the data line for
	incoming commands. The call will return 0 until a complete,
	valid command packet is received.

	When a command is received, it returns either 1 or 2, depending
	on whether the command is a repeat (within 120 ms) of the
	previous one. This if you hold down the remote button, the first
	packet received will return a 1, and subsequent packets a 2; if
	you release the button and press it again, the next packet will
	be returned with 1.

	When a non-zero return code is received, the key, chan, command,
	and power values will be valid for that packet.

	Polls should not be more than about 100 microseconds apart, as
	the protocol relies on timing the length of pulses.


// A simple example, that re-purposes the controller to act as a remote
// for two relays:
//
#include <ShockCollar.h>
ShockCollarRemote remote;

char onoff[2];		// Relay status

void setup() {
	int relay;
	for(relay = 0; relay < 2; relay++) {	// Initialise two relays
		onoff[relay] = 0;		// Off
		pinMode(10 + relay, OUTPUT);	// Set up control pimns
		digitalWrite(10 + relay, 0);
	}
	remote.begin(13);			// Use pin 13 for the receiver
}

void loop() {
	int relay;
	if(remote.receive() != 1) return; 	// Command received
	if(remote.command != COLLAR_LED) return;// Need LED command
	remote.expect_key = remote.key;		// Lock to 1st remote  heard
	relay = collar.chan - 1;   		// Pick relay	
	onoff[relay] = ! onoff[relay]; 		// Toggle status
	digitalWrite(10 + relay, onoff[relay]);	// And set the relay.
}


-- Shock Collar Protocol --

The collar is driven by a simple 433 MHz ASK transmitter. The
transmitter has just three pins - Vcc, GND and DATA. The DATA pin is a
simple digital input (driven by a digital output pin on the Arduino),
and transmits a signal when the pin is high.

The on-air encoding is 1000 bps (1 ms per bit), pulse-width modulated.
Timings (in microseconds) [1]:
	Start bit:	mark 1500, space 750
	0 bit:		mark  250, space 750
	1 bit:		mark  750, space 250
	inter-packet:	~9ms

Example (each char represents 250 microseconds, _=low, X=high):
    Idle  Start    0   1   2   3   4   5   6   7   8   9   10  11  12  13   ...
... ______XXXXXX___XXX_X___X___X___XXX_X___X___X___XXX_X___XXX_X___XXX_X___ ...
		   1   0   0   0   1   0   0   0   1   0   1   0   1   0    ...
		   <li><---chan---><------mode----><----------key---------- ...
		    1      000	          1000		      10101...

The packet format, in bits, as as follows, fields sent high-order bit first 
(left to right):

	Octet	Field	  Bits	Value	   
	0	Lead-in   1	1
		Channel   3	Ch1=000; Ch2=111
		Mode	  4	LED=1000; BEEP=0100; VIB=0010; ZAP=0001
	1-2	Key	  16	Identity field, specific to transmitter[2]
	3	Power	  8	Power level[2]
	4	ModeX	  4	LED=1110; BEEP=1101; VIB=1011; ZAP=0111
		ChannelX  3	Ch1=111; Ch2=000
	4/5	Trailer   2	00

Example: Chan=1, Key=0xabcd, Mode=ZAP, Power=100 (0x64)
	Octet:	     0	       | 1	 |2	  | 3	    | 4 	|5
	Bit:	     7 654 3210| 76543210|76543210| 76543210| 7654 321 0|7
	Wire bit:    0 123 4567| 89111111|11112222| 22222233| 3333 333 3|4
			       |   012345|67890123| 45678901| 2345 678 9|0
	Fields:      1 ccc mmmm| kkkkkkkk|kkkkkkkk| pppppppp| MMMM CCC 0|0
	Field bits:  - 2-0 3--0|15------8|7------0| 7------0| 3--0 2-0 -|-
	Packet bits: 1 000 0001| 10101011|11001101| 01100100| 0111 111 0|0
	Hex	     8	   1   | a   b	 |c   d   | 6	4   | 7    e	|0
	Fields: c=Channel; m=Mode; k=Key; p=Power, M=ModeX, C=ChannelX

Notes
[1] In Smouldery's code, the delays were 741 and 247, presumably to
    allow for code overhead. We clock using micros() to keep the bit
    timing aligned, and any variation due to overhead appears when the
    data is off (0). The actual device doesn't seem too fussy.
[2] Smouldery's code had the key and power at 17 and 7 bits
    respectively. Experimentation showed that a 17th "key" bit is not
    used as part of the identity (i.e setting it to 1 or 0 did not
    affect whether a collar responded), and that the fields are as above
    with a 16 bit key and 8 bit power field. Power values above 100 are
    accepted but appear to do the same thing as power 100.


-- Author --

(C) 2019-2023 Ruru, ruru67@yahoo.com

Non-commercial personal use and modification of this work is permitted.
Do not distribute this or derived work without permission.