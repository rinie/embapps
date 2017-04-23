/*
 * General OOK decoding without knowing the protocol before hand.
 *
 * A lot of opensource software/hardware OOK decoding solutions
 * disregard the fact that commercial solutions do work with cheap receivers
 * and without lowpass filters or such.
 *
 * IMHO this stems from the fact that the protocols are designed to overcome
 * the limitations of these receivers:
 * - Add a preamble or Sync to give the AGC time to adjust.
 * - Repeat the signal at least 3 times, so that the 2nd and 3rd are received
 *   correctly with a tuned AGC and compared for identical packages.
 * - Use a weak checksum/CRC as the computing power is limited (optimized for low power,
 *   sending from a battery operated sensor).
 * - Use a simple encoding so typically 1 Pulse(On)/Space(Off) time combination for '1'
 *   and 1 Pulse/Space time combination for '0' is used.
 *	 Preamble, Gap or Sync use clearly distinctive timings or standard timings repeated often.
 *
 * Using these properties instead of a CRC checksum on the individual packages,
 * I try to guess from the relative timings of the signal what encoding is used,
 * and where the gap between the packages occur:
 * - GAP should be clear enough to capture package 2 and 3 reliable,
 *   but should be small enough to keep AGC correct.
 * - Few time variations can be stored as index instead of exact timepulse.
 *
 * Change sort to sort merge?
 *
 * Test with
 *	ORSV2
 *		200..1200 split on 700
 *		Manchester encoding, 160 bits, Short pulse within first 32 bits, start bit
 *
 *		http://wmrx00.sourceforge.net/Arduino/OregonScientific-RF-Protocols.pdf
 *
 *		For version 2.1 sensors only, each data bit is actually sent four times. This is
 *		accomplished by first sending each data bit twice (inverted the first time),
 *		doubling the size of the original message. A one bit is sent as a 01 sequence
 *		and a zero bit is sent as 10. Secondly, the entire message is repeated once.
 *		 Oregon Scientific RF Protocols
 *		 Page 2 of 23
 *		Some sensors will insert a gap (about 10.9 msec for the THGR122NX) between
 *		the two repetitions, while others (e.g. UVR128) have no gap at all.
 *
 *		Both 2.1 and 3.0 protocols have a similar message structure containing four
 *		parts.
 *		1. The preamble consists of 1 bits, 24 bits (6 nibbles) for v3.0 sensors
 *		and 16 bits (4 nibbles) for v2.1 sensors (since a v2.1 sensor bit stream
 *		contains an inverted and interleaved copy of the data bits, there is in
 *		fact a 32 bit sequence of alternating 0 and 1 bits in the preamble).
 *		2. A sync nibble (4-bits) which is 0101 in the order of transmission. With
 *		v2.1 sensors this actually appears as 10011001. Since nibbles are sent
 *		LSB first, the preamble nibble is actually 1010 or a hexadecimal A.
 *		3. The sensor data payload, which is described in the Message Formats
 *		section below.
 *		4. A post-amble (usually) containing two nibbles, the purpose or format of
 *		which is not understood at this time. At least one sensor (THR238NF)
 *		sends a 5-nibble post-amble where the last four nibbles are all zero.
 *		The number of bits in each message is sensor-dependent. The duration of most
 *		v3.0 messages is about 100msec. Since each bit is doubled in v2.1 messages,
 *		and each v2.1 message is repeated once in its entirety, these messages last
 *		about four times as long, or 400msec.
 *
 *  	KAKU
 *  		NodeDue: Encoding volgens Princeton PT2262 / MOSDESIGN M3EB / Domia Lite spec.
 *  		Pulse (T) is 350us PWDM
 *  		0 = T,3T,T,3T, 1 = T,3T,3T,T, short 0 = T,3T,T,T
 * 		Timings theoretical: 350, 1050
 *
 *		Jeelabs: Split 700, Gap 2500
 *
 *	KAKUA/KAKUNEW:
 *		NodeDue: Encoding volgens Arduino Home Easy pagina
 *		Pulse (T) is 275us PDM
 *		0 = T,T,T,4T, 1 = T,4T,T,T, dim = T,T,T,T op bit 27
 *		8T // us, Tijd van de space na de startbit
 *		Timings theoretical: 275, 1100, 2200
 *
 *		Jeelabs: Split 700, 1900, Max 3000
 *
 *	XRF / X10:
 *		http://davehouston.net/rf.htm
 *
 * 	WS249 plant humidity sensor.
 *		SevenWatt: Normal signals between 170 and 2600. Sync (space after high( between 5400 and 6100))
 *		Split 1600, 64..66 pulse/spaces
 *
 * RcSwitch timing definitions:
 *  Format for protocol definitions:
 *  {pulselength, Sync bit, "0" bit, "1" bit}
 *
 *  pulselength: pulse length in microseconds, e.g. 350
 *  Sync bit: {1, 31} means 1 high pulse and 31 low pulses
 *      (perceived as a 31*pulselength long pulse, total length of sync bit is
 *      32*pulselength microseconds), i.e:
 *       _
 *      | |_______________________________ (don't count the vertical bars)
 *  "0" bit: waveform for a data bit of value "0", {1, 3} means 1 high pulse
 *      and 3 low pulses, total length (1+3)*pulselength, i.e:
 *       _
 *      | |___
 *  "1" bit: waveform for a data bit of value "1", e.g. {3,1}:
 *       ___
 *      |   |_
 *
 *  These are combined to form Tri-State bits when sending or receiving codes.
 *
 *   { 350, {  1, 31 }, {  1,  3 }, {  3,  1 }, false },    // protocol 1
 *   { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false },    // protocol 2
 *   { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false },    // protocol 3
 *   { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false },    // protocol 4
 *   { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false },    // protocol 5
 *   { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true }      // protocol 6 (HT6P20B)
 *   RKR: OR 450, {1, 23}, {2, 1}, {1, 2} not inverted?
 *   Gap after message > space time  of sync
 *};
 */

bool fIsRf = true;
// RKR modifications
typedef unsigned long ulong; //RKR U N S I G N E D is so verbose
typedef unsigned int uint;

uint psiCount;
byte psMinMaxCount = 0;

#define PSI_OVERFLOW 0x0F
#define PS_MICRO_ELEMENTS 8
uint psMicroMin[PS_MICRO_ELEMENTS]; // nibble index, 0x0F is overflow so max 15
uint psMicroMax[PS_MICRO_ELEMENTS]; // nibble index, 0x0F is overflow so max 15

typedef enum {psixPulse, psixSpace, psixPulseSpace, PSIXNRELEMENTS} psiIx; //
uint psixCount[PS_MICRO_ELEMENTS][PSIXNRELEMENTS]; // index frequency, makes sense to split Pulse/Space to detect signal type...

byte psiNibbles[512]; // psiCount pulseIndex << 4 | spaceIndex
#define NRELEMENTS(a) (sizeof(a) / sizeof(*(a)))

#define psiNibblePulse(psiNibbles, i) (((psiNibbles)[(i)] >> 4) & 0x0F)
#define psiNibbleSpace(psiNibbles, i) (((psiNibbles)[(i)]) & 0x0F)
#define psPulseSpaceNibble(pulse, space) ((((pulse) & 0x0F) << 4) | ((space) & 0x0F))
#define psiNibblePS(psiNibbles, j) ((j) & 1) ? psiNibbleSpace(psiNibbles, (uint)((j) / 2)) : psiNibblePuls(psiNibbles, (uint)((j) / 2))

uint jDataMax = 0;
uint jDataCount = 0;
uint jDataStart[8];
uint jDataEnd[8];

static void PrintChar(byte S) {
	Serial.write(S);
}

static void PrintDash(void) {
	PrintChar('-');
}

static void PrintComma(void) {
	Serial.print(F(", "));
}

static void PrintNum(uint x, char c, int digits) {
	// Rinie add space for small digits
	if(c) {
		PrintChar(c);
	}
	for (uint i=1, val=10; i < digits; i++, val *= 10) {
		if (x < val) {
			PrintChar(' ');
		}
	}

	Serial.print(x,DEC);
}

static void PrintNumHex(uint x, char c, uint digits) {
	// Rinie add space for small digits
	if(c) {
		PrintChar(c);
	}
	for (uint i=1, val=16; i < digits; i++, val *= 16) {
		if (x < val) {
			PrintChar('0');
		}
	}
	Serial.print(x,HEX);
}

static void psiSortMergeMicroMinMax() {
	byte psNewIndex[PS_MICRO_ELEMENTS];
	uint prevMinVal = 0;
	uint prevMaxVal = 0;
	byte jPrev = 0;

	// psMicroMin/psMicroMax have actual timings
	// store sort in psNewIndex
	for (byte i = 0; i < psMinMaxCount;) {
  	    byte j2=0;
  	    uint minVal = EDGE_TIMEOUT;
		for (byte j = 0; j < psMinMaxCount; j++) {
			uint curMinVal = psMicroMin[j];
			if ((curMinVal < minVal) && (curMinVal > prevMinVal)) {
				minVal = curMinVal;
				j2 = j;
			}
		}
		psNewIndex[j2] = i;
		prevMaxVal = psMicroMax[j2];
		prevMinVal = psMicroMin[j2];
		i++;
	}

	// swap sort psMicroMin, psMicroMax and psiCount gaat fout op merge!
	bool fDoneSome;
	do {
		fDoneSome = false;
		for (byte j = 0; j < psMinMaxCount-1; j++) {
			if (psMicroMin[j] > psMicroMin[j+1]) {
				uint psMicroMinTemp = psMicroMin[j];
				psMicroMin[j] = psMicroMin[j+1];
				psMicroMin[j+1] = psMicroMinTemp;

				uint psMicroMaxTemp = psMicroMax[j];
				psMicroMax[j] = psMicroMax[j+1];
				psMicroMax[j+1] = psMicroMaxTemp;
				// enum {psiPulse, psiSpace, psiPulseSpace, PSINRELEMENTS} psiIx; //
				for (uint ix = 0; ix < PSIXNRELEMENTS; ix++) {
					uint psixCountTemp = psixCount[j][ix];
					psixCount[j][ix] = psixCount[j+1][ix];
					psixCount[j+1][ix] = psixCountTemp;
				}
				fDoneSome = true;
			}
		}
	}
	while (fDoneSome);

	// replace index values
	for (uint i=0; i < psiCount; i++) {
		byte pulse = psiNibblePulse(psiNibbles, i);
		byte space = psiNibbleSpace(psiNibbles, i);
		pulse = (pulse < psMinMaxCount) ? psNewIndex[pulse] : pulse;
		space = (space < psMinMaxCount) ? psNewIndex[space] : space;
		psiNibbles[i] = psPulseSpaceNibble(pulse, space);
	}
}

void psiPrint() {
	uint psCount;
	bool fPrintHex = false;
	//digitalWrite(MonitorLedPin,HIGH);
	//uint psniStart = 0;
	//uint psniSize = psiNibbles[psniStart];
	//uint psniSize = psiCount -1;
	//psCount = (psniSize * 2) - (((psiNibbleSpace(psiNibbles, psniSize)) == PSI_OVERFLOW) ? 1 : 0);
	psCount = psiCount * 2;
	Serial.print((fIsRf)? F("RF receive "): F("IR receive "));
	PrintChar('*');
	Serial.print(psCount);
	uint psxCount[PSIXNRELEMENTS];
	for (uint ix = 0; ix < PSIXNRELEMENTS; ix++) {
		psxCount[ix] = 0;
		for (uint i=0; i < psMinMaxCount; i++) {
			psxCount[ix] += psixCount[i][ix];
		}
	}
	PrintNum(psxCount[psixPulseSpace], '?', 1);
	PrintNum(psxCount[psixPulse], '(', 1);
	PrintNum(psxCount[psixSpace], ',', 1);
	Serial.print(F(")("));
	// data/syncGap seperation detection
	// RF receive *392(196,196,0,1)[0*7(7,0):300/304 1*190(155,35):308/708 2*3(3,0):756/812 3*185(31,154):1012/1104 4*7(0,7):9900/10076 ]
	//typedef enum {psixPulse, psixSpace, psixPulseSpace, PSIXNRELEMENTS} psiIx; //
	uint psiDataShort[PSIXNRELEMENTS];
	uint psiCountDataShort[PSIXNRELEMENTS];
	uint psiDataLong[PSIXNRELEMENTS];
	uint psiCountDataLong[PSIXNRELEMENTS];
	uint psiCountGapMax;
	for (uint ix = 0; ix < PSIXNRELEMENTS; ix++) {
		psiDataShort[ix] = (ix < psixPulseSpace) ? 0 : max(psiDataShort[psixPulse], psiDataShort[psixSpace]);
		psiCountDataShort[ix] = psixCount[psiDataShort[ix]][ix];
		psiDataLong[ix] = (ix < psixPulseSpace) ? 1 : max(psiDataLong[psixPulse], psiDataLong[psixSpace]);
		psiCountDataLong[ix] = psixCount[psiDataLong[ix]][ix];
		psiCountGapMax = 0;
		for (uint i=psiDataLong[ix] + 1; i < psMinMaxCount; i++) {
			uint psiCount = psixCount[i][ix];
			if (psiCount > psiCountDataLong[ix]) {
				if (psiCountDataLong[ix] > psiCountDataShort[ix]) {
					psiDataShort[ix] = psiDataLong[ix];
					psiCountDataShort[ix] = psiCountDataLong[ix];
				}
				psiDataLong[ix] = i;
				psiCountDataLong[ix] = psiCount;
				psiCountGapMax = 0;
			}
			else if (psiCount > psiCountDataShort[ix]) {
				psiDataShort[ix] = psiDataLong[ix];
				psiCountDataShort[ix] = psiCountDataLong[ix];
				psiDataLong[ix] = i;
				psiCountDataLong[ix] = psiCount;
				psiCountGapMax = 0;
			}
			else if (psiCount > psiCountGapMax) {
				psiCountGapMax = psiCount;
			}
		}
		PrintNum(psiDataShort[ix], ',', 1);
		PrintNum(psiDataLong[ix], ',', 1);
		PrintNum(psiCountGapMax, '*', 1);
	}
	for (uint ix = 0; ix < PSIXNRELEMENTS-1; ix++) {
		if (psiCountDataLong[ix] < psiCountGapMax) {
				psiDataShort[ix] = psiDataLong[ix];
				psiCountDataLong[ix] += psiCountDataShort[ix];
				psiCountDataShort[ix] = 0;
		}
	}
	Serial.print(F(")["));
	for (uint i=0; i < psMinMaxCount; i++) {
		PrintNum(i, 0, 1);
		PrintNum(psixCount[i][psixPulseSpace], '*', 1);
		PrintNum(psixCount[i][psixPulse], '(', 1);
		PrintNum(psixCount[i][psixSpace], ',', 1);
		PrintChar(')');
		PrintNum(psMicroMin[i], ':', 3);
		PrintNum(psMicroMax[i], '/', 3);
		PrintChar(' ');
	}
	Serial.print(F("]"));
	Serial.println();
	uint j = 0;
#if 1
	// preamble/sync/gap lengths
	Serial.print(F("Gap Intervals "));
	if (psiDataShort[psixPulse] == psiDataLong[psixPulse]) {
		Serial.print(F("Single Pulse "));
	}
	else if (psiDataShort[psixSpace] == psiDataLong[psixSpace]) {
		Serial.print(F("Single Space "));
	}
	uint jMax = 32; // min package length
	uint jMaxCount = 0; // likely number of packages
	uint jPreStart, jStart, jEnd;
	uint jMatchCount = 0; // likely number of packages
	for (uint i=0; i < psiCount; i++, j++) {
		byte pulse = psiNibblePulse(psiNibbles, i);
		byte space = psiNibbleSpace(psiNibbles, i);
		if ((pulse > psiDataLong[psixPulse]) /* && j > 16*/) { // sync pulse
			uint jj = j * 2;
			if (jj > jMax) {
				if (jj > jMax + 4) {
					jMaxCount = 0;
				}
				jMax = jj;
				jDataMax = jMax;
			}
			if (jj >= jMax - 4) {
				uint ii = i * 2;
				if (jMaxCount < NRELEMENTS(jDataEnd)) {
					jDataEnd[jMaxCount] = ii;
					jDataStart[jMaxCount] = ii - jj;
					jDataCount = jMaxCount + 1;
				}
				jMaxCount++;
			}
			j = 0;
		}
		if (space > psiDataLong[psixSpace]) {
			uint jj = j * 2 + 1;
			if (jj > jMax) {
				if (jj > jMax + 4) {
					jMaxCount = 0;
				}
				jMax = jj;
				jDataMax = jMax;
			}
			if (jj >= jMax - 4) {
				uint ii = i * 2 + 1;
				if (jMaxCount < NRELEMENTS(jDataEnd)) {
					jDataEnd[jMaxCount] = ii;
					jDataStart[jMaxCount] = ii - jj;
					jDataCount = jMaxCount + 1;
				}
				jMaxCount++;
			}
			j = 0;
		}
	}
	PrintNum(jMaxCount, '#', 1);
	PrintNum(jMax, '*', 2);
	PrintNum(jPreStart, '[', 2);
	PrintNum(jStart, '*', 2);
	PrintNum(jEnd, ':', 2);
	PrintChar(']');
	PrintChar(':');
	if ((jMax <= 64) && (jMaxCount < 2)) {
		Serial.println(F(" Assume Noise"));
		return;
	}
	if (jMaxCount > 1) { //assume repeated packages
		j = 0;
	}
	j = 0;
	for (uint i=0; i < psiCount; i++, j++) {
		byte pulse = psiNibblePulse(psiNibbles, i);
		byte space = psiNibbleSpace(psiNibbles, i);
		if ((pulse > psiDataLong[psixPulse]) /* && j > 16*/) { // sync pulse
			uint jj = j * 2;
			//PrintNum(i*2, 'P', 1);
			PrintNum(jj, 'P', 1);
			PrintNum(pulse, '*', 1);
			PrintChar(' ');
			j = 0;
		}
		if ((space > psiDataLong[psixSpace]) /* && ((j > 16) )*/ || (space > psiDataLong[psixSpace] + 1)) { // long gap
			uint jj = j * 2 + 1;
			//PrintNum(i*2+1, 'S', 1);
			PrintNum(jj, 'S', 1);
			PrintNum(space, '*', 1);
			PrintChar(' ');
			j = 0;
		}
	}
	Serial.println();
	j = 0;
#endif
	for (uint i=0; i < psiCount; i++, j++) {
		byte pulse = psiNibblePulse(psiNibbles, i);
		byte space = psiNibbleSpace(psiNibbles, i);

		pulse = (pulse <= psiDataShort[psixPulse]) ? 0 : ((pulse <= psiDataLong[psixPulse]) ? 1 : pulse);
		space = (space <= psiDataShort[psixSpace]) ? 0 : ((space <= psiDataLong[psixSpace]) ? 1 : space);

		if ((pulse > psiDataLong[psixPulse]) && ((j > 16) || (psiDataShort[psixPulse] == psiDataLong[psixPulse]))) { // sync pulse
			Serial.println();
			j = 0;
		}

		if ((psiDataShort[psixPulse] != psiDataLong[psixPulse]) || (pulse > psiDataLong[psixPulse])) {
			Serial.print(pulse,HEX);
		}
		if ((psiDataShort[psixSpace] != psiDataLong[psixSpace]) ||  (space > psiDataLong[psixSpace])) {
			Serial.print(space,HEX);
		}

		if ((space > psiDataLong[psixSpace]) && ((j > 16)) || ((psiDataShort[psixSpace] == psiDataLong[psixSpace]))) { // long gap
			Serial.println();
			j = 0;
		}
	}
	Serial.println();
}

void psInit(void) {
	psMinMaxCount = 0;
	psiCount = 0;
}

/*
 *	psNibbleIndex
 *
 * Lookup/Store timing of pulse and space in psMicroMin/psMicroMax/psiCount array
 * Could use seperate arrays for pulses and spaces but 15 (0x0F for overflow) seems enough
 * Store Header as last entry, footer will come last of normal data...
 */
static byte psNibbleIndex(uint pulse, uint space) {
	byte psNibble = 0;
	uint value = pulse; // pulse, then space...
	for (int j = 0; j < 2; j++) {
		int i = 0;
		if (value > 0) {
			// existing match first
			for (i = 0; i < psMinMaxCount; i++) {
				if ((psMicroMin[i] <= value) && (value <= psMicroMax[i])) {
					psixCount[i][psixPulseSpace]++;
					if (j == 0) {
						psixCount[i][psixPulse]++;
					}
					else {
						psixCount[i][psixSpace]++;
					}
					break;
				}
			}
			if (i >= psMinMaxCount) { // no existing match check within tolerance
				// Either a new length or just outside the current boundaries of a current value
				uint k;
				uint offBy = value;
				//i = 0;
				uint tolerance = (value < 500) ? 400 : (value < 1000) ? 200 : ((value < 2000) ? 400 : ((value < 5000) ? 600 : 1000));
				//uint tolerance = 100;
				//uint tolerance = (value < 1000) ? 100 : ((value < 2000) ? 200 : ((value < 5000) ? 300 : 500));
				for (k = 0; k < psMinMaxCount; k++) { // determine closest interval
					uint offByi = value;
					if ((value < psMicroMin[k]) && (value + tolerance >= psMicroMax[k])) { // new min?
						offByi = psMicroMin[k] - value;
						if (offByi < offBy) {
							i = k;
							offBy = offByi;
						}
					}
					else if ((value > psMicroMax[k]) && (value <= psMicroMin[k] + tolerance)) { // new max
						offByi = value - psMicroMax[k];
						if (offByi < offBy) {
							i = k;
							offBy = offByi;
						}
					}
				}
				if (i < psMinMaxCount) { // existing match
					if (value < psMicroMin[i]) { // new min
						psMicroMin[i] = value;
					}
					else if (value > psMicroMax[i]) { // new max
						psMicroMax[i] = value;
					}
					if (j == 0) {
						psixCount[i][psixPulse]++;
					}
					else {
						psixCount[i][psixSpace]++;
					}
					psixCount[i][psixPulseSpace]++;
				}
				else {
					i = psMinMaxCount; // no match
				}
			}
			if (i >= psMinMaxCount && i < PSI_OVERFLOW) { // new value
				if (i < PS_MICRO_ELEMENTS) {
					psMinMaxCount++;
					psMicroMin[i] = value;
					psMicroMax[i] = value;
					if (j == 0) {
						psixCount[i][psixPulse] = 1;
						psixCount[i][psixSpace] = 0;
					}
					else {
						psixCount[i][psixPulse] = 0;
						psixCount[i][psixSpace] = 1;
					}
					psixCount[i][psixPulseSpace] = 1;
				}
				else {
					i = PSI_OVERFLOW; // overflow
				}
			}
		}
		else {
			i = PSI_OVERFLOW; //invalid data
		}
		//psNibble = psPulseSpaceNibble(psNibble, i);
		psNibble = ((psNibble & 0x0F) << 4) | (i & 0x0F);
		value = space;
	}
	return psNibble;
}

#if 1 // Rinie get to know code
	static uint16_t psCount = 0;
	static uint32_t startSignal;
	static uint32_t startSignalm;
	static uint32_t lastSignal = 0;
	static uint lastPulseDur = 0;
	static bool fCheckClear = false;
#endif

//#include "analysepacket.h"
static void processReady() {
	if (psCount > 64) {
		uint32_t now = millis();
		uint32_t nowm = micros();
		// terminate recording
		PrintNum(psiCount, '#', 3);
		PrintNum(psCount, '$', 3);
		printRSSI();
		/* if (printRSSI()) */ {
			Serial.println();

			psiSortMergeMicroMinMax();
			psiPrint();
			//analysepacket(0);
			lastSignal = millis();
		}
		//else {
		//	Serial.println(F(" Assume Noise"));
		//}
	}
	psCount = 0;
	psInit();
}

bool processBitRkr(uint16_t pulse_dur, uint8_t signal, uint8_t rssi) {
	if (pulse_dur > 1) {
		if ((pulse_dur > 75) && (pulse_dur < EDGE_TIMEOUT)){
			if (psCount == 0) {
				startSignal = millis();
				startSignalm = micros();
				if (!lastSignal) {
					lastSignal = startSignal;
				}
				psInit();
			}
			if (psCount & 1) {	// Odd means pulse and space, so pulse_dur is space
				if (psiCount < NRELEMENTS(psiNibbles)) {
					psiNibbles[psiCount++] = psNibbleIndex(lastPulseDur, pulse_dur);
					if (psiCount >=  NRELEMENTS(psiNibbles)) {
						processReady();
						return false;
					}
				}
				else {
					processReady();
					return false;
				}
			}
			else {
				lastPulseDur = pulse_dur;
			}
			psCount++;
		}
	}
	if ((!rssi) && (pulse_dur == 1)) { // footer, fake pulse, print and reset
		processReady();
	}
	return false; // return true to skip decoders...
}
