// Shock collar driver. Takes serial/USB input, sends commands to a shock collar.
// By Ruru, ruru67@yahoo.com, 29/4/2019
//
// Uses ShockCollar library. See ShockCollar documentation for hardware and 
// on-the-air protocol details.
//
//
// -- Serial Interface --
//
// For devices such as the Arduino Uno, Nano or Mega2560 that use an external
// USB-to-serial interface, the serial interface runs at 9600 bps. For devices
// using native USB (such as those based on the ATmega32u4, like the Beetle or
// Pro Micro), the data is transferred at USB rates and the speed is ignored.
//
//
// -- Command line interface --
//
// By default it comes up in non-interactive mode (suitable for programatic
// control). No characters are echoed, nor are error messages issued. Only
// query commands produce output. Sending a CR (0x0d) enables interactive mode
// where prompts are displayed, input characters are echoed, and errors are 
// reported. A line terminated with LF (0x0a) or CR/LF will switch back to 
// non-interactive mode. 
//
// In non-interactive mode, responses are not normally sent. If you need to
// know when a command completed (or if it failed), start the command with a '+', 
// to indicate a response is needed when the command completes (or aborts).
// The response will be either "OK" if it completed successfully, or an error
// message starting with "ERROR: ". Note that commands producing output (e.g. 
// Q, S. ?) will still produce a response in addition to their normal output
// if so requested. 
//
// Output lines from query commands et c. are terminated with CR/LF in
// interactive mode, or if the input line terminator was CR/LF. Otherwise, lines
// are terminated with a bare LF.
//
// Backspace and DEL erase the last character on the line. Ctrl/U will erase
// the whole line. 
//
// If data is received while executing a collar command, the current
// command will abort; multiple commands should be "stacked" on one line for
// sequential execution, not sent as multiple command lines.
//
//
// -- Commands --
//
// These are the commands recognised by the CLI
//
//	?		Show help
//	S		Show current parameters
//	Q <parameter>	Query parameter (I, K, C, P, or D)
//	I		Show identity (equivalent to "QI")
//			displays COLLAR <version>
//	P <power>	Set power level (0..100)
//	D <duration>	Set default duration in ms
//	C <channel>	Set channel 1 or 2, or 3 for both
//	A <channel>	Set channel for keepalives, 1, 2, 3 for both, 
//			or 0 to disable keepalives.
//	K <key>		Set 16 bit key value (specify key in hex)
//	L [<duration>]	Light
//	B [<duration>]	Beep
//	V [<duration>]	Vibrate
//	Z [<duration>]	Zap
//	W <duration>	Wait <duration> ms
//	X		Save key and channel and keepalive channel to 
//			non-volatile storage
//	R		Reset to defaults with saved key/channel
//	!		Override safety limit (!! to reset safety counter)
//	+		Force a response (OK or error message)
//
// Multiple commands can be stacked on one line, e.g.
//
//	K ABCD C 1 P 100 D 200 Z
//
// will set the key to 0xabcd, channel to 1, and zap at 100% power for 0.2
// seconds. Spaces are for clarity and not required, except where a command
// after the key could be confused with a hex digit (i.e. A,B,C, or D).
//
// Key, channel, duration and power settings are persistent. The safety 
// override lasts only for the duration ofthe command line. 
//
// The key is an arbitrary 16 bit value used to differentiate transmitters. 
// Collars use that plus the channel for addressing. To program a collar's
// address, press the reset/program button (it beeps), then send a command
// to the desired key and channel (it will beep again when the ID is
// programmed, or if it times out waiting). The first packet received wins
// and the collar will respond to that key/channel combination from then on
// (and no other). It will need to be re-synced if the the collar's battery
// runs down.
//
// Zaps are limited for safety to 6 seconds (treating short zaps as 1 sec)
// in any given minute. This may be overridden by using the ! command in 
// the same command line (but you probably shouldn't).
//
// The key is defaulted by the ShockCollar object to 0x1234. This can be
// overridden with the K command and saved in NVR with the X command. 
//
//
// -- Communicating with interface --
//
// On Linux systems, you can communicate using standard I/O, e.g.:
//
//	COLLAR=/dev/ttyUSB0		# or /dev/ttyACM0 or ...
//	stty -F $COLLAR 9600 raw	# Set up port, 9600 bps, raw output
//	{ echo "I" ; read -t 5 c v ; } < $COLLAR > $COLLAR
//	test "$c" = "COLLAR" || exit	# Exit if unexpected response
//
//	{ echo "+P 100 Z" ; read r ; } < $COLLAR > $COLLAR
//
// Note that the some devices using the ACM driver shut down the USB connection on
// disconnect, potentially leading to the loss of transmitted characters, and the
// device needs to be kept open for a short time after sending, otherwise it'll
// stop transmitting before the final LF. This is not a problem if you wait for
// a response, but if not, a short delay before the command drops the connection
// should avoid the problem.
//
//
// -- Author --
//
// (C) 2019-2023 Ruru, ruru67@yahoo.com
//
// Non-commercial personal use and modification of this work is permitted.
// Do not distribute this or derived work without permission.
//
#include <ShockCollar.h>
#include <EEPROM.h>

// Parameters
//
#define VERSION		"1.4"		// Program version
#define MAXZAP		6000		// Max 6 secs of zap per minute
#define NVRKEY		1556596129	// Used to determine validity of NVR
#define COLLAR_PIN	14		// Pin 2 for the transmitter data line
#define COLLAR_IND	LED_BUILTIN	// Pin to indicate collar activity
					// Use LED_BUILTIN or LED_BUILTIN_TX
					// if no built-in LED. -1 for no LED.

// Global variables
//
char chan;				// Channel, 1 or 2
char powr;				// Power 0..100
long durn;				// Duration (ms)
long zaptime;				// Used to limit zappiness
long zap;				// Count of zap (ms) since zaptime
char inter = 0;				// Interactive mode (enabled by CR)
char crlf = 0;				// CRLF mode when non-interactive
ShockCollar collar;			// Collar object


// Little function to be given to the ShockCollar object to interrupt a
// command in progress if data is received.
//
int serialcheck() {
	return Serial.available();
}


// Setup. Also called from the (R)eset command
// 
void setup() {
	// Set up output pins and init serial
	//
	Serial.begin(9600);
	collar.begin(COLLAR_PIN, COLLAR_IND);

	// Set application defaults
	//
	collar.kchan	 = 0;
	collar.interrupt = serialcheck;
	chan		 = 1;
	powr		 = 50;
	durn		 = 100;
	zaptime		 = 0;
	zap		 = 0;
	inter		 = 0;

	// Get settings from NVRAM, if it's valid. If not, we'll
	// initialise it on the first command. 
	//
	if(((long)EEPROM[0] << 24 | (long)EEPROM[1] << 16
	  | (long)EEPROM[2] << 8  | (long)EEPROM[3]) == NVRKEY) {
		collar.key   = EEPROM[4] << 8 | EEPROM[5];
		chan	     = EEPROM[6];
		collar.kchan = EEPROM[7];
	}
}


// Parse a positive integer of any base up to 36 (default 10).
// ptr points to the number, and advances to the next character on return.
// -1 indicates error.
// (This is basically strtol() but lighter and more C++-ish)
// 
long parsenum(char * &ptr, int base = 10) {
	long r = -1;	// Return value defaults to -1 if no valid digits
	char c;

	while(*ptr == ' ') ptr++;		// Skip leading space
	while(*ptr) {				// While still in buffer
		c = *ptr;
		if(c >= '0' && c <= '9' && c < '0' + base)
			c -= '0';		// Digits 0-9
		else if(c >= 'A' && c < 'A' + base - 10)
			c = c + 10 - 'A';	// Digits A-Z
		else break;			// Invalid char? done
		ptr++;				// Advance over valid
		if(r == -1) r = 0;		// Number is valid
		r = r * base + c;		// Insert digit
	}
	return r;				// Done
}


// Some display stuff. First some convenient macros
//
#define IDENT		"COLLAR " VERSION
#define PV(s, v) { Serial.print(F(s)); printnum(v); }
#define PH(s, v) { Serial.print(F(s)); printhex(v); }
#define PS(s)    { Serial.print(F(s)); newline(); }
#define OOPS(msg) { if(inter || sayok) PS("ERROR: " msg); return; }

// Print a newline. If interactive, or the crlf flag is set, output CR/LF.
// Otherwise, just a LF. 
//
void newline() {
	if(inter || crlf)
		Serial.write("\r\n");
	else	Serial.write('\n');
}

// Print a numeric value, either decimal or hex, followed by a newline.
//
void printhex(unsigned int n) {
	int i;
	for(i = 12; i >= 0; i -= 4)
		Serial.write("0123456789ABCDEF"[(n >> i) & 0x000f]);	
	newline();
}
void printnum(long n) {
	int i;
	for(i = 1000000000; i >= 1; i /= 10)
		if(n / i > 0)
			Serial.write('0' + (n / i) % 10);
	//Serial.print(n);
	newline();
}


// Save key and channel to NVR
//
void nvrsave() {
	EEPROM[0] = NVRKEY >> 24;
	EEPROM[1] = NVRKEY >> 16;
	EEPROM[2] = NVRKEY >> 8;
	EEPROM[3] = NVRKEY;
	EEPROM[4] = collar.key >> 8;
	EEPROM[5] = collar.key;
	EEPROM[6] = chan;
	EEPROM[7] = collar.kchan;
}


// A little help ...
//
void help() {
  PS("Shock collar controller V" VERSION)
  PS("L,B,V,Z[<n>]\tAction: LED, Beep, Vibrate, Zap (duration optional)")
  PS("P<n>\t\tSet power (0-100%)")
  PS("D<n>\t\tSet default duration (milliseconds)")
  PS("C<n>\t\tSet channel (1, 2, 3=both)")
  PS("A<n>\t\tSet keepalive channel (0=none, 1, 2, 3=both)")
  PS("W<n>\t\tWait n ms")
  PS("K<key>\t\tSet key (4 hex digits)")
  PS("X\t\tSave key / channel to NVR")
  PS("R\t\tReset to defaults")
  PS("!\t\tOverride safety")
  PS("+\t\tForce a response (OK or error message)")
  PS("S\t\tShow settings")
  PS("Q<p>\t\tShow parameter I,K,C,P,D")
  PS("I\t\tShow identity (equivalent to QI)")
}


// Run a command. ptr points to nul-terminated command string
//
void docommand(char *ptr) {
	int unsafe = 0;
	int sayok = 0;
	collar_cmd cmd;
	int i;
	char c;
	long n, d, t;

	while((c = *ptr++)) {
		cmd = COLLAR_NONE;

		// Commands
		//
		switch(c) {		// Actions
		case 'L': cmd = COLLAR_LED;			break;
		case 'B': cmd = COLLAR_BEEP;			break;
		case 'V': cmd = COLLAR_VIB;			break;
		case 'Z': cmd = COLLAR_ZAP;			break;
			
		case 'P':		// Commands that take a numeric parameter:
		case 'D':		//	power, duration, channel. wait
		case 'C':
		case 'W':
		case 'A':
			n = parsenum(ptr);
			if(n < 0) OOPS("Invalid number");
			switch(c) {
			case 'P': powr  = (n < 100) ? n : 100;		break;
			case 'D': durn  = n;				break;
			case 'C': chan  = (n <= 3 ? n : 0);		break;
			case 'A': collar.kchan = (n <= 3 ? n : 0);	break;
			case 'W':
				// Loop until <n> ms has passed. Keep collar
				// alive in case this is really long.
				t = millis();
				while(millis() - t < n && !Serial.available())
					collar.keepalive();
				break;
			}
			break;

		case 'K':		// Change key
			n = parsenum(ptr, 16);
			if(n < 0 || n > 0xffff)
				OOPS("Invalid hex key")
			collar.key = n;
			break;

		case 'S':		// Status
			PS("Identity:\tI " 	 IDENT)
			PH("Key:\t\tK ",	 collar.key)
			PV("Channel:\tC ",	 chan)
			PV("Keepalive:\tA ", 	 collar.kchan)
			PV("Power (%):\tP ",	 powr)
			PV("Duration (ms):\tD ", durn)
			break;

		case 'I':		// Identity
			ptr--;		// Fall through


		case 'Q':		// Query
			while(*ptr == ' ') ptr++;
			switch(*ptr++) {
			case 'I': PS(IDENT)			break;
			case 'K': printhex(collar.key);		break;
			case 'C': printnum(chan);		break;
			case 'A': printnum(collar.kchan);	break;
			case 'P': printnum(powr);		break;
			case 'D': printnum(durn);		break;
			default:  OOPS("Unknown parameter");
			}
			break;

		case '!':		// Safety override. Two to clear count
			if(unsafe) zap = 0;
			unsafe = 1;
			break;

		case '+':		// Report status
			sayok = 1;
			break;

		case 'R':		// Reset to saved configuration
			if(sayok) PS("OK")
			setup();
			return;

		case 'X':		// Save key and channel to NVR
			nvrsave();
			break;

		case '?':		// Help
			help();
			break;

		case ' ': continue;	// Ignore spaces
		default:  OOPS("Unrecognised command")
		}

		// Execute command if requested
		// If there is a number after the command, use that as the duration,
		// otherwise use the duration set by the 'D' command.
		// Limit zap to max of six seconds of zap per minute. Treat short
		// zaps as one second.
		// Override prevents safety limit being applied for this zap but
		// zaps are still counted toward future zap commands.
		// Stop transmitting if new input received
		//
		if(cmd != 0) {
			while(*ptr == ' ') ptr++;
			if(*ptr >= '0' && *ptr <= '9')
				d = parsenum(ptr);
			else	d = durn;
			if(cmd == COLLAR_ZAP) {
				t = millis();
				if(t - zaptime > 60000) {
					zaptime = t;
					zap = 0;
				}
				if(zap >= MAXZAP & !unsafe)
					OOPS("Too much zap!")
				if(d > 1000) {
					if(d > MAXZAP - zap && !unsafe)
						d = MAXZAP - zap;
					zap += d;
				}
				else	zap += 1000;
			}
			collar.command(cmd, chan, powr, d);
		}
	}
	if(sayok) PS("OK")
}


// Loop de loop
//
void loop() {
	static char buf[78];
	static int ptr = 0;
	static long t;
	char c;	

	// Keep the collar from going sleepy-byes
	//
	collar.keepalive();

	// Read and process input character if available
	// If Serial has shut down, force back into non-interactive mode
	// Only check once a second (Calling Serial like that is slow, so 
	// we don't do it a lot)
	//
	if((long)millis() - t > 1000) {
		t = millis();
		if(!Serial) {
			inter = 0;
			ptr = 0;
		}
	}
	else if(Serial.available()) {
		c = Serial.read();

		// If CR or LF...
		//
		if(c == '\r' || c == '\n') {
			// If the terminator is a CR, there are two
			// possibilities: If it's a naked CR, then we need to
			// switch into interactive mode. 
			// If it's the start of a CR/LF pair, we need to
			// "eat" the subsequent LF, else it'll leave us with
			// a stray LF waiting in the input buffer, which could
			// cause the issued command to abort. We also don't
			// want to switch to interactive mode.
			// So, wait a short time (10 ms) for a LF to turn up.
			// Note that if we receive some other character in that
			// time (unlikely), we'll fall through and process it
			// after running the current command buffer.
			// If we do get a LF after the CR, we enable the CR/LF 
			// output mode.
			//
			crlf = 0;
			if(c == '\r') {
				t = millis();
				while(millis() - t < 10) {
					if(Serial.available()) {
						c = Serial.read();
						if(c == '\n')
							crlf = 1;
						break;
					}
				}
			}

			// If we (still) have a naked CR, switch into
			// interactive mode (if we aren't there already). If
			// there's a command stacked up that we haven't
			// echoed, echo it now.
			//
			if(c == '\r') {
				if(!inter) {
					Serial.print(F(IDENT));
					if(ptr) {
						Serial.print("\r\n> ");
						buf[ptr] = 0;
						Serial.print(buf);
					}
				}
				inter = 1;
			}
			else	inter = 0;
			if(inter) newline();

			// Run the command (if there is one)
			//
			if(ptr) {
				buf[ptr] = 0;
				docommand(buf);
			}

			// Command done. Prompt if required.
			//
			ptr = 0;
			if(inter) Serial.write("> ");
		}

		// Eat escape/CSI sequences. Nom!
		//
		if(c == 27 || c == 155) {
			t = millis();
			while(millis() - t < 10)
				if(Serial.available())
					Serial.read();
		}

		// Backspace - delete last character
		//
		if((c == 8 || c == 127) && ptr > 0) {
			ptr--;
			if(inter) Serial.write("\b \b");	
		}
		// Ctrl/U - erase entire line
		//
		if(c == 21) {
			while(ptr > 0) {
				ptr--;
				if(inter) Serial.write("\b \b");
			}
		}

		// Input characters. Smash to uppercase, echo if interactive
		//
		if(c >= ' ' && c < 127 && ptr < sizeof(buf) - 1) {
			if(c >= 'a' && c <= 'z') c = c - 'a' + 'A';
			if(inter) Serial.write(c);
			buf[ptr++] = c;
		}
	}
}
