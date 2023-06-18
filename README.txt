Shock collar driver. Provides routines to drive a Petrainer dog training
(shock) collar, and also to interpret commands receibed from its
handheld remote control.


-- Introduction --

This code is based on work done by DeviantDesigns:
  https://www.youtube.com/watch?v=kmvZVxP5Nvc

and code by Smouldery and mikey_dk:
  https://github.com/smouldery/shock-collar-control/

Note that while I used Smouldery's code and underlying work to determine
packet formats, encoding et c, and I thank them for doing that ground
work, this code is a complete re-write, designed to be used as a
callable library, and with a much smaller code/data footprint.


-- Hardware --

This code was tested on Arduino-style ATmega32U4 boards (a SparkFun Pro
Micro clone, and a an SS Micro, a similar board with less pins brough
out to make a smaller form factor. These are coupled with a 433 MHz ASK 
transmitter (to test the conllar control) and receiver (to test receiving
commands from the handheld controller); these typically come as a pair.

For the transmitter, onnect the radio's DATA pin to data pin 14 of the
SS Micro Vcc to 5V, and GND to GND. USB power is sufficient. This
arrangement allows the two small boards to be mounted piggyback, with
the antenna opposite the USB plug. (I have only tested the receiver using
a breadboarded Pro Micro.) You'll need an antenna as well.

AliExpress links for hardware:

SS Micro board:            https://www.aliexpress.com/item/32775220354.html
433 MHz xmitter/receiver:  https://www.aliexpress.com/item/32608684824.html
433 MHz antenna:           https://www.aliexpress.com/item/4000960125591.html
Petrainer collar/remote:   https://www.aliexpress.com/item/32947131768.html
Petrainer collar only:     https://www.aliexpress.com/item/32606845436.html

The collar links are for the rechargeable version; there are cheaper
versions of the same collar/remote that use removable batteries. The
transmitter module comes with a receiver, which can be used with the
ShockCollarRemote object to receive commands from the handheld remote.

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
	void setup() {
		collar.begin(14);
		collar.interrupt = keypress;
		...

The following high-level methods are provided. As long as you're not
trying to do something weird, these are likely to be the only calls you
need.

void begin(char pin, char led)
void begin(char pin)

	Call this method to set up the data and LED outputs on the
	device.

	pin	Defines the output data pin 
	led	Define the LED pin to flash on activity. If not
		specified, defaults to none (-1).

int command(collar_cmd cmd, char chan, char powr, long durn)
int led(  chan, durn)
int beep( chan, durn)
int vib(  chan, durn, pwr)
int zap(  chan, durn, pwr)

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

	Note that the collar key should be set up (it defauilts to 0x1234).

	The four "shortcut" methods, led(), beep(), vib() & zap() just
	call command() with appropriate parameters.

void keepalive()

	Call this regularly to ensure the collar doesn't go to sleep
	(done by sending a COLAR_LED command). The function has an
	internal timer to limit transmission to approx once every two
	minutes. The variable kchan must be set to 1, 2 or 3 (3 = both 
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

-------------------------------------------------------------------------------
#include <ShockCollar.h>
ShockCollar collar;	// Declare object

void setup() {		// Initial set-up
	collar.begin(2);		// Initialise collar, pin 2 is data
	collar.kchan = 1;		// Set keepalive channel
	collar.key = 0xbeef;		// Choose a different key
	pinMode(3, INPUT_PULLUP);	// Pin 3 is the activate button
}

loop() {		// Loop de loop
	collar_keepalive();		// Keep the collar alive
	if(dgitalRead(3))		// If button pressed ...
		collar.zap(1, 100, 500);// Activate the collar at full
					// power for 500ms.
}
-------------------------------------------------------------------------------

The code is intended to operate up to two collars on the same collar key,
on channels 1 & 2. command() and keepalive() can take 3 as the channel
value to alternately transmit to channels 1 & 2. Note that multiple
ShockCollar objects can be set up on the same data pin but with
different keys.

If using multiple objects, the keeplive method should be called
regularly on all objects. Two objects can not be be commanded
"simultaenously" (really on alternate packets) however; for that you
would need to use the packet() method to construct packets for each
object, then repeatedly call the send() method for each constructed
packet in turn. (This is what the command() method does internally.)

Note that a command takes bout 50ms to be transmitted, so cycling
through too many may mean the packets for a given collar get spaced too
far apart.


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
	and power values will be valid for that packet. (They are only
	updated when the method returns 1.)

	Polls should not be more than about 100 microseconds apart, as
	the protocol relies on timing the length of pulses.


Here's a simple example that re-purposes the controller to act as a remote
for two relays:

-------------------------------------------------------------------------------
#include <ShockCollar.h>
ShockCollarRemote remote;	// Remote object
char onoff[2];			// Relay status relay[0] = channel 1
				//		relay[1] = channel 2

void setup() {			// Initial set-up
	int relay;
	for(relay = 0; relay < 2; relay++) {	// Initialise two relays
		onoff[relay] = 0;		// Off
		pinMode(10 + relay, OUTPUT);	// Set up control pins
		digitalWrite(10 + relay, 0);	// Pins 10 & 11.
	}
	remote.begin(15);			// Use pin 15 for the receiver
}

void loop() {			// Loop de loop
	int relay;
	if(remote.receive() != 1) return; 	// Command received?
	if(remote.command != COLLAR_LED) return;// Need LED command
	remote.expect_key = remote.key;		// Lock to 1st remote used
	relay = collar.chan - 1;   		// Pick relay 0 or 1
	onoff[relay] = ! onoff[relay]; 		// Toggle status
	digitalWrite(10 + relay, onoff[relay]);	// And set the relay.
}
-------------------------------------------------------------------------------


-- Shock Collar Protocol --

The collar is driven by a simple 433 MHz ASK transmitter. The
transmitter has just three pins - Vcc, GND and DATA. The DATA pin is a
simple digital input (driven by a digital output pin on the Arduino),
and transmits a signal when the pin is high.

The on-air encoding is 1000 bps (1 ms per bit), pulse-width modulated.
Timings (in microseconds) [1]:
	Start flag:	mark 1500, space 750
	0 bit:		mark  250, space 750
	1 bit:		mark  750, space 250
	inter-packet:	~9ms

Example (each char represents 250 microseconds, _=low, X=high):
    Idle  Flag     0   1   2   3   4   5   6   7   8   9   10  11  12  13   ...
... ______XXXXXX___XXX_X___X___X___XXX_X___X___X___XXX_X___XXX_X___XXX_X___ ...
		   1   0   0   0   1   0   0   0   1   0   1   0   1   0    ...
		   <li><---chan---><------mode----><----------key---------- ...
		    1      000	          1000		      10101...

The packet format, in bits, is as follows, fields sent high-order bit first 
(left to right):

	Octet	Field	  Bits	Value	   
	0	Lead-in   1	1
		Chan	  3	Channel, Ch1=000; Ch2=111
		Mode	  4	LED=1000; BEEP=0100; VIB=0010; ZAP=0001
	1-2	Key	  16	Identity field, specific to transmitter[2]
	3	Power	  8	Power level[2]
	4	ModeX	  4	LED=1110; BEEP=1101; VIB=1011; ZAP=0111
		ChanX	  3	Ch1=111; Ch2=000
	4/5	Trailer   2	00

Example: Chan=1, Key=0xabcd, Mode=ZAP, Power=100 (0x64)
	Octet:	     0	       | 1	 |2	  | 3	    | 4 	|5
	Bit:	     7 654 3210| 76543210|76543210| 76543210| 7654 321 0|7
			       |   111111|11112222| 22222233| 3333 333 3|4
	Wire bit:    0 123 4567| 89012345|67890123| 45678901| 2345 678 9|0
	Fields:      l ccc mmmm| kkkkkkkk|kkkkkkkk| pppppppp| MMMM CCC t|t
	Field bits:  - 2-0 3--0|15------8|7------0| 7------0| 3--0 2-0 -|-
	Packet bits: 1 000 0001| 10101011|11001101| 01100100| 0111 111 0|0
	Hex	     8	   1   | a   b	 |c   d   | 6	4   | 7    e	|0
	Fields: l=Lead-in c=Chan m=Mode k=Key p=Power M=ModeX C=ChanX t=Trailer

Packets are typically transmitted with ~10ms idle between packets, giving
a total transmission time of about 50ms per packet.

There is an optional LEADIN #define to force the transmitter to send
two extra start flags before the actual start flag, but only if there
has been a significant gap between packets. (Consecutive paskets are
sent without the intervening extra flags.) Without this, a single
packet would not be received by either a collar or another arduino
running as a receiver; if two packets are sent, the second one gets
received but not the first. With the extra flags, a single packet
transmission is reliably received.

Some experimentation suggests this is to do with receivers not "locking"
onto the incoming signal until they have had signal for some time to
lock to and this period of positive signal needs to run for longer than
the normal start flag. This trait appears to be shared by both the actual
collars' receivers and the receiver modules that come with the
transmitter modules. Thus sending these extra flags makes this code more
reliable than a handheld remote.

Notes
[1] In Smouldery's code, the delays were 741 and 247, presumably to
    allow for code overhead. We clock using micros() to keep the bit
    timing aligned. The off delay is actually processed at the start of
    each bit, so we don't have to worry about how long the inter-bit
    processing takes - we just wait the remaining time before starting
    the on pulse of the next bit. The actual device doesn't seem too
    fussy. (The receiver code allows for a lot of slop in the timings,
    mainly to allow for delays between samples.)
[2] Smouldery's code had the key and power at 17 and 7 bits
    respectively. Experimentation showed that a 17th "key" bit is not
    used as part of the identity (i.e setting it to 1 or 0 did not
    affect whether a collar responded), and that the fields are as above
    with a 16 bit key and 8 bit power field. Power values above 101-255
    are accepted, but appear to simply be treated as 100.


-- Example code --

The code comes with two example / utility sketches. See the comments
within them for finer details.

examples/shock-collar-serial.ino

This sketch provides a simple interface that can be used by scripts
running on a host computer to control a collar.

examples/shock-collar-remote.ino

This sketch provides a demonstration of the ShockCollarRemote object,
which can be used to identify what a collar remote is sending, e.g. to
allow an Arduino controller to be given the same key as a handheld
controller, allowing both to control the same collar(s).


-- Author --

(C) 2019-2023 Ruru, ruru67@yahoo.com

Non-commercial personal use and modification of this work is permitted.
Do not distribute this or derived work without permission.
