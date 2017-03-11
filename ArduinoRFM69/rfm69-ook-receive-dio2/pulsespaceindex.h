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
		accomplished by first sending each data bit twice (inverted the first time),
		doubling the size of the original message. A one bit is sent as a “01” sequence
		and a zero bit is sent as “10”. Secondly, the entire message is repeated once.
		 Oregon Scientific RF Protocols
		 Page 2 of 23
		Some sensors will insert a gap (about 10.9 msec for the THGR122NX) between
		the two repetitions, while others (e.g. UVR128) have no gap at all.

		Both 2.1 and 3.0 protocols have a similar message structure containing four
		parts.
		1. The preamble consists of “1” bits, 24 bits (6 nibbles) for v3.0 sensors
		and 16 bits (4 nibbles) for v2.1 sensors (since a v2.1 sensor bit stream
		contains an inverted and interleaved copy of the data bits, there is in
		fact a 32 bit sequence of alternating “0” and “1” bits in the preamble).
		2. A sync nibble (4-bits) which is “0101” in the order of transmission. With
		v2.1 sensors this actually appears as “10011001”. Since nibbles are sent
		LSB first, the preamble nibble is actually “1010” or a hexadecimal “A”.
		3. The sensor data payload, which is described in the “Message Formats”
		section below.
		4. A post-amble (usually) containing two nibbles, the purpose or format of
		which is not understood at this time. At least one sensor (THR238NF)
		sends a 5-nibble post-amble where the last four nibbles are all zero.
		The number of bits in each message is sensor-dependent. The duration of most
		v3.0 messages is about 100msec. Since each bit is doubled in v2.1 messages,
		and each v2.1 message is repeated once in its entirety, these messages last
		about four times as long, or 400msec.
 *
 * 	KAKU
 * 		NodeDue: Encoding volgens Princeton PT2262 / MOSDESIGN M3EB / Domia Lite spec.
 * 		Pulse (T) is 350us PWDM
 * 		0 = T,3T,T,3T, 1 = T,3T,3T,T, short 0 = T,3T,T,T
 *		Timings theoretical: 350, 1050
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
  * Format for protocol definitions:
  * {pulselength, Sync bit, "0" bit, "1" bit}
  *
  * pulselength: pulse length in microseconds, e.g. 350
  * Sync bit: {1, 31} means 1 high pulse and 31 low pulses
  *     (perceived as a 31*pulselength long pulse, total length of sync bit is
  *     32*pulselength microseconds), i.e:
  *      _
  *     | |_______________________________ (don't count the vertical bars)
  * "0" bit: waveform for a data bit of value "0", {1, 3} means 1 high pulse
  *     and 3 low pulses, total length (1+3)*pulselength, i.e:
  *      _
  *     | |___
  * "1" bit: waveform for a data bit of value "1", e.g. {3,1}:
  *      ___
  *     |   |_
  *
  * These are combined to form Tri-State bits when sending or receiving codes.
  *
 #ifdef ESP8266
 static const RCSwitch::Protocol proto[] = {
 #else
 static const RCSwitch::Protocol PROGMEM proto[] = {
 #endif
   { 350, {  1, 31 }, {  1,  3 }, {  3,  1 }, false },    // protocol 1
   { 650, {  1, 10 }, {  1,  2 }, {  2,  1 }, false },    // protocol 2
   { 100, { 30, 71 }, {  4, 11 }, {  9,  6 }, false },    // protocol 3
   { 380, {  1,  6 }, {  1,  3 }, {  3,  1 }, false },    // protocol 4
   { 500, {  6, 14 }, {  1,  2 }, {  2,  1 }, false },    // protocol 5
   { 450, { 23,  1 }, {  1,  2 }, {  2,  1 }, true }      // protocol 6 (HT6P20B)
   RKR: OR 450, {1, 23}, {2, 1}, {1, 2} not inverted?
   Gap after message > space time  of sync
};
*/

bool fIsRf = true;
// RKR modifications
typedef unsigned long ulong; //RKR U N S I G N E D is so verbose
typedef unsigned int uint;
// typedef unsigned char byte;
bool fOnlyRepeatedPackages = false;
bool fSortNibbles = true;
int package = 0;
volatile ulong lastStart;

uint psniStart;
uint psniIndex;

typedef enum PSIX {	//
	psData, psHeader, psFooter
} PSIX;

#define PS_MICRO_ELEMENTS 8
uint psMicroMin[PS_MICRO_ELEMENTS]; // nibble index, 0xF is overflow so max 15
uint psMicroMax[PS_MICRO_ELEMENTS]; // nibble index, 0xF is overflow so max 15
byte psiCount[PS_MICRO_ELEMENTS]; // index frequency
byte psMinMaxCount = 0;

byte psiNibbles[512]; // byteCount + nBytes pulseIndex << 4 | spaceIndex
#define NRELEMENTS(a) (sizeof(a) / sizeof(*(a)))

static void PrintChar(byte S)
  {
  Serial.write(S);
  }
static void PrintDash(void)
  {
  PrintChar('-');
  }

static void PrintComma(void)
  {
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
#undef RKR_TEST_SORT


static void sortMicroMinMax() {
  if (psiNibbles[0] == 0) {
	  return;
  }
  byte psNewIndex[PS_MICRO_ELEMENTS]; // index frequency
	/*
		[ 1192  384 2120 6024]
		0 1
		1 0
		2 2
		3 3
		 + sort by swap
RF receive 218 (69/175) {P104/0/0/4/1/0} {S65/38/2/0/0/4} H[ 3840  566]
 [  0:524/ 608 1: 1664/1712 2:1704/1704 3:3692/3988 4:4012/4012 5:5020/5060]

 Merge 1 and 2
	 */
	uint prevMinVal = 0;
	uint prevMaxVal = 0;
	byte jPrev;
	byte merge = 0;
#if 0
	for (byte i = 0; i < PS_MICRO_ELEMENTS;) {
			psNewIndex[i] = i;
	}
#endif

	for (byte i = 0; i < psMinMaxCount;) {
  	    uint minVal = -1; // max range
  	    byte j2=0;
		for (byte j = 0; j < psMinMaxCount; j++) {
			uint curMinVal = psMicroMin[j];
			if ((curMinVal < minVal) && (curMinVal > prevMinVal)) {
				minVal = curMinVal;
				j2 = j;
			}
		}
		psNewIndex[j2] = i;
		if (prevMaxVal && ((minVal - prevMaxVal) < 100)) { // rkr was 100
			//merge
#if 0
			Serial.print(F("RKR Merge: "));
			Serial.print(minVal);
			Serial.print(F(" "));
			Serial.print(prevMaxVal);
			Serial.print(F(" "));
			Serial.print(i);
			Serial.print(F(" "));
			Serial.print(j2);
			Serial.println();
#endif
			merge++;
			//psNewIndex[j2] = i - 1;
			psNewIndex[j2] = psNewIndex[jPrev];
#if 0
			psMicroMax[jPrev] = psMicroMax[j2];
			psiCount[jPrev] += psiCount[j2];
#endif
			prevMaxVal = psMicroMax[j2];
#if 0
			// shift j2 values out
			for (byte j = j2+1; j < psMinMaxCount; j++) {
				psMicroMin[j-1] = psMicroMin[j];
				psMicroMax[j-1] = psMicroMax[j];
				psiCount[j-1] = psiCount[j];
			}
			psMinMaxCount--;
#endif
		}
		else {
			i++;
			jPrev = j2;
			prevMaxVal = psMicroMax[j2];
		}
		prevMinVal = minVal;
	}
#if 1
			//psMinMaxCount -= merge;
			Serial.print(F("RKR psNewIndex: "));
			for (byte i = 0; i < psMinMaxCount+merge;i++) {
				Serial.print(psNewIndex[i]);
				Serial.print(F(" "));
			}
			Serial.println();
#endif
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

				uint psiCountTemp = psiCount[j];
				psiCount[j] = psiCount[j+1];
				psiCount[j+1] = psiCountTemp;

				fDoneSome = true;
			}
		}
	}
	while (fDoneSome);

	// merge min/max/count
	merge = 0;
	for (byte j = 0; j < psMinMaxCount-1; j++) {
			if (psMicroMax[j] + 100 >= psMicroMin[j+1]) {
					psMicroMax[j] = psMicroMax[j + 1];
					psiCount[j] += psiCount[j+1];
				// shift j2 values out
				for (byte j2 = j+1; j2 < psMinMaxCount-1; j2++) {
					psMicroMin[j2] = psMicroMin[j2+1];
					psMicroMax[j2] = psMicroMax[j2+1];
					psiCount[j2] = psiCount[j2+1];
				}
				psMinMaxCount--;
				merge++;
			}
	}

	// replace
	uint psniStart1 = 0;
	byte psniSize1 = psiNibbles[psniStart1];
	while (psniSize1 > 0) {
		uint psniStart2 = psniStart1 + psniSize1 + 1;
		byte psniSize2 = psiNibbles[psniStart2];
		for (uint i= 1; i <= psniSize1; i++) {
			byte pulse = ((psiNibbles[i] >> 4) & 0x0F);
			byte space = ((psiNibbles[i]) & 0x0F);
//			pulse = (pulse < PS_MICRO_ELEMENTS) ? psNewIndex[pulse] : pulse;
//			space = (space < PS_MICRO_ELEMENTS) ? psNewIndex[space] : space;
			pulse = (pulse < psMinMaxCount + merge) ? psNewIndex[pulse] : pulse;
			space = (space < psMinMaxCount + merge) ? psNewIndex[space] : space;
			psiNibbles[i] = (pulse << 4) | (space);
		}
		// next
		psniStart1 = psniStart2;
		psniSize1 = psniSize2;
	}
}

void psInit(void) {
	psMinMaxCount = 0;
	psniIndex = 0;
	psniStart = psniIndex;
	if (psniIndex < NRELEMENTS(psiNibbles)) {
		psiNibbles[psniIndex++] = 0;
	}
}

/*
 *	psNibbleIndex
 *
 * Lookup/Store timing of pulse and space in psMicroMin/psMicroMax/psiCount array
 * Could use seperate arrays for pulses and spaces but 15 (0xF for overflow) seems enough
 * Store Header as last entry, footer will come last of normal data...
 */
static byte psNibbleIndex(uint pulse, uint space, byte psDataHeaderFooter) {
	byte psNibble = 0;
	uint value = pulse; // pulse, then space...
	for (int j = 0; j < 2; j++) {
		int i = 0;
		if (value > 0) {
			// existing match first
			for (i = 0; i < psMinMaxCount; i++) {
				if ((psMicroMin[i] <= value) && (value <= psMicroMax[i])) {
					psiCount[i]++;
					break;
				}
			}
			if (i >= psMinMaxCount) { // no existing match
				// Either a new length or just outside the current boundaries of a current value
#if 1
				uint k;
				uint offBy = value;
				//i = 0;
				uint tolerance = (value < 1000) ? 100 : ((value < 2000) ? 200 : ((value < 5000) ? 300 : 500));
				//uint tolerance = (value < 1000) ? 100 : ((value < 2000) ? 200 : ((value < 5000) ? 300 : 500));
				tolerance += tolerance;
				for (k = 0; k < psMinMaxCount; k++) { // determine closest interval
					uint offByi = value;
					if (value < psMicroMin[k]) { // new min?
						//offByi = psMicroMin[k] - value;
						offByi = psMicroMax[k] - value;
					}
					else if (value > psMicroMax[k]){ // new max
						//offByi = value - psMicroMax[k];
						offByi = value - psMicroMin[k];
					}
					if (k == 0 || offByi < offBy) {
						i = k;
						offBy = offByi;
#if 0
						Serial.print(F("RKR Min/Max: "));
						Serial.print(value);
						Serial.print(F(" "));
						Serial.print(offBy);
						Serial.print(F(" "));
						Serial.print(i);
						Serial.print(F(" "));
						Serial.print(psMicroMin[i]);
						Serial.print(F("-"));
						Serial.print(psMicroMax[i]);
						Serial.println();
#endif
					}
				}
				if ((i < psMinMaxCount) && (offBy <= tolerance)) { // use k for min/max
#if 0
						Serial.print(F("RKR new Min/Max: "));
						Serial.print(value);
						Serial.print(F(" "));
						Serial.print(offBy);
						Serial.print(F(" "));
						Serial.print(i);
						Serial.print(F(" "));
						Serial.print(psMicroMin[i]);
						Serial.print(F("-"));
						Serial.print(psMicroMax[i]);
						Serial.println();
#endif
					if (value < psMicroMin[i]) { // new min
						psMicroMin[i] = value;
					}
					else if (value > psMicroMax[i]) { // new max
						psMicroMax[i] = value;
					}
					psiCount[i]++;
					break;
				}
				else {
					i = psMinMaxCount; // no match
				}
#else // old code
				for (i = 0; i < psMinMaxCount; i++) {
					uint tolerance = /* (value < 1000) ? 100 : */ ((value < 2000) ? 200 : ((value < 5000) ? 300 : 500));
					// new max
					uint nextMin = (i >= psMinMaxCount - 1) ? (psMicroMax[i + 1] - tolerance) : value + 1;
					if (value > psMicroMax[i]
						&& value <= (psMicroMin[i] + tolerance)
						&& (value < nextMin)) {
						psMicroMax[i] = value;
						psiCount[i]++;
						break;
					}
					else {
						// new min
						// uint prevMax = (i > 0) ? psMicroMin[i - 1] + tolerance : 0;
						if (value < psMicroMin[i]
							&& value > (psMicroMax[i] - tolerance)
							/* && (value > prevMax) */) {
							psMicroMin[i] = value;
							psiCount[i]++;
							break;
						}
					}
				}
#endif
			}
			if (i >= psMinMaxCount && i < 0x0F) { // new value
				if (psDataHeaderFooter == psHeader) {
					i = PS_MICRO_ELEMENTS -1;
				}
				else if (i < PS_MICRO_ELEMENTS - 1) { // except header
					psMinMaxCount++;
				}
				else {
					i = 0x0F; // overflow
				}
				if (i < PS_MICRO_ELEMENTS) {
					psMicroMin[i] = value;
					psMicroMax[i] = value;
					psiCount[i] = 1;
				}
			}
		}
		else {
			i = 0x0F; //invalid data
		}
		psNibble = (psNibble << 4) | (i & 0x0F);
		value = space;
	}
	return psNibble;
}

void psiPrint() {
#if 0
	// terminate recording
	psiNibbles[psniStart] = psniIndex - psniStart - 1;
   	psniStart = psniIndex;
	if (psniIndex < NRELEMENTS(psiNibbles)) {
		psiNibbles[psniIndex++] = 0;
	}
#endif
	if (fSortNibbles) {
		sortMicroMinMax();
	}
	else {
		 Serial.print(F("NoSort:"));
	}
	if ((package > 0) || (!fOnlyRepeatedPackages)) {
		unsigned int psCount;
		unsigned int pulseCount[PS_MICRO_ELEMENTS];
		unsigned int pulseCountX;
		unsigned int spaceCount[PS_MICRO_ELEMENTS];
		unsigned int spaceCountX;
		bool fHeader = false;
		bool fFooter = false;
		bool fPrintHex = false;
		//digitalWrite(MonitorLedPin,HIGH);
		uint psniStart = 0;
		byte psniSize = psiNibbles[psniStart];
		uint psFewCount;
		do {
			  psCount = (psniSize * 2) - (((psiNibbles[psniStart + psniSize] & 0x0F) == 0x0F) ? 1 : 0);
			  Serial.print((fIsRf)? F("RF receive "): F("IR receive "));
			  Serial.print(psCount);
			  //Serial.print(F("#"));
			  //Serial.print(package);
			  printRSSI();
			  for (unsigned int i = 0; i < NRELEMENTS(pulseCount); i++) {
				  pulseCount[i] = 0;
				  spaceCount[i] = 0;
			  }
			  pulseCountX = 0;
			  spaceCountX = 0;

			  for(unsigned int i=1; i <= psniSize; i++) {
					byte pulse = ((psiNibbles[psniStart + i] >> 4) & 0x0F);
					byte space = ((psiNibbles[psniStart + i]) & 0x0F);
					if (i == 1) { // check header
						fHeader = (pulse > 1) || (space > 1);
						//continue;
					}
					else if (i == psniSize) { // check footer
						fFooter = (pulse > 1) || (space > 1);
						//continue;
					}

					if (pulse < NRELEMENTS(pulseCount)) {
						pulseCount[pulse]++;
					}
					else {
						pulseCountX++;
					}
					if (space < NRELEMENTS(spaceCount)) {
						spaceCount[space]++;
					}
					else {
						spaceCountX++;
					}
			  }
			  psFewCount = psniSize/16;
			  if ((pulseCountX > 0) || (spaceCountX > 0)) {
				  // don't bother

				  for(unsigned int i=1; i <= psniSize; i++ ) {
						byte pulse = ((psiNibbles[psniStart + i] >> 4) & 0x0F);
						byte space = ((psiNibbles[psniStart + i]) & 0x0F);
						if (i == 1) { // check header
							fHeader = (pulseCount[pulse] < psFewCount) || (spaceCount[space] < psFewCount);
							//continue;
							i = psniSize - 1;
						}
						else if (i == psniSize) { // check footer
							fFooter = (pulseCount[pulse] < psFewCount) || (spaceCount[space] < psFewCount);
							//continue;
						}
				  }
			  }
			  else {
				  fPrintHex = true;
			  }
			  Serial.print(F(" {P"));
			  Serial.print(pulseCount[0]);
			  for (byte i = 1; i < psMinMaxCount; i++) {
				  Serial.print(F("/"));
				  Serial.print(pulseCount[i]);
			  }
			  if (pulseCountX > 0) {
			    Serial.print(F("!"));
			  	Serial.print(pulseCountX);
			  }
			  Serial.print(F("}"));

			  Serial.print(F(" {S"));
			  Serial.print(spaceCount[0]);
			  for (byte i = 1; i < psMinMaxCount; i++) {
				  Serial.print(F("/"));
				  Serial.print(spaceCount[i]);
			  }
			  if (spaceCountX > 0) {
			    Serial.print(F("!"));
			  	Serial.print(spaceCountX);
			 }
		     Serial.print(F("}"));

			  if ( /*(package>0) && */ fPrintHex) {
				  if (fHeader) {
					unsigned int i=1;
					byte pulse = ((psiNibbles[psniStart + i] >> 4) & 0x0F);
					byte space = ((psiNibbles[psniStart + i]) & 0x0F);
					unsigned long bucket0 = (psMicroMax[pulse] + psMicroMin[pulse]) / 2;
					unsigned long bucket1 = (psMicroMax[space] + psMicroMin[space]) / 2;
					Serial.print(F(" H["));
					PrintNum(bucket0, ' ', 4);
					PrintNum(bucket1, ' ', 4);
					Serial.print(F("] "));
				  }
				  if (fFooter) {
					unsigned int i=psniSize;
					byte pulse = ((psiNibbles[psniStart + i] >> 4) & 0x0F);
					byte space = ((psiNibbles[psniStart + i]) & 0x0F);
					unsigned long bucket0 = (psMicroMax[pulse] + psMicroMin[pulse]) / 2;
					unsigned long bucket1 = (psMicroMax[space] + psMicroMin[space]) / 2;
					Serial.print(F(" F["));
					PrintNum(bucket0, ' ', 4);
					PrintNum(bucket1, ' ', 4);
					Serial.print(F("] "));
				  }
				unsigned int fewStart = 1;
				unsigned int fewCount = 0;
				bool fOldFew = false;
				for(unsigned int i=1; i <= psniSize; i++) {
					byte pulse = ((psiNibbles[psniStart + i] >> 4) & 0x0F);
					byte space = ((psiNibbles[psniStart + i]) & 0x0F);
					if ((i == 1) && fHeader) { // check header
						continue;
					}
					else if ((i == psniSize) && fFooter) { // check footer
						continue;
					}
					bool fFew = (pulseCount[pulse] < psFewCount) || (spaceCount[space] < psFewCount);
					if (fFew != fOldFew) {
						if (fFew) {
							if (!fewCount) {
								Serial.print(F(" Few:"));
								PrintNum(psFewCount, 0, 2);
								Serial.print(F("{"));
							}
							if ((pulseCount[pulse] < psFewCount)) {
								PrintNum(i*2, ' ', 2);
							}
							else {
								PrintNum(i*2 + 1, ' ', 2);
							}
							PrintNum((i - fewStart)*2, ':', 2);
							fewCount++;
						}
						fOldFew = fFew;
						fewStart = i;
					}
				}
				if (fewCount) {
					Serial.print(F(" }"));
			    }
				Serial.print(F(" ["));
				unsigned long bucket0 = (psMicroMax[0] + psMicroMin[0]) / 2;
				unsigned long bucket1 = (psMicroMax[1] + psMicroMin[1]) / 2;
#if 1
				PrintNum(psMicroMin[0], ' ', 4);
				PrintNum(psMicroMax[0], '/', 4);
				PrintNum(psMicroMin[1], ' ', 4);
				PrintNum(psMicroMax[1], '/', 4);
#else
				PrintNum(bucket0, ' ', 4);
				PrintNum(bucket1, ' ', 4);
#endif
				Serial.print(F("] 0x"));

				unsigned int j = 0;
				byte x = 0;
				for(unsigned int i=1; i <= psniSize; i++) {
					byte pulse = ((psiNibbles[psniStart + i] >> 4) & 0x0F);
					byte space = ((psiNibbles[psniStart + i]) & 0x0F);
					if ((i == 1) && fHeader) { // check header
						continue;
					}
					else if ((i == psniSize) && fFooter) { // check footer
						continue;
					}
					x = (x << 2) | (pulse << 1) | space;
					j += 2;
					if (j >= 8) {
						PrintNumHex(x, 0, 2);
						j = 0;
						x = 0;
					}
				}
				if (j > 0) {
						PrintNumHex(x, 0, 2);
				}
				Serial.println();
			  }
			  /* else */ {
				  Serial.print(F(" ["));
#if 1
				  for(unsigned int i=0; i < psMinMaxCount; i++) {
						PrintNum(psMicroMin[i], ' ', 4);
						PrintNum(psMicroMax[i], '/', 4);
				  }
#else
				  for(unsigned int i=0; i < psMinMaxCount; i++) {
					  unsigned long bucket = (psMicroMax[i] + psMicroMin[i]) / 2;
					  //Serial.print(psiCount[i]);
					  //Serial.write('/');
					  PrintNum(bucket, ' ', 4);
					  //Serial.write(' ');
				  }
#endif
					// detect sync out of band value
				  unsigned int i = ((psiNibbles[psniStart + 1] >> 4) & 0x0F);
				  if (i <= psMinMaxCount) {
					  i = ((psiNibbles[psniStart + 1]) & 0x0F);
				  }
				  if (i > psMinMaxCount) {
					  unsigned long bucket = (psMicroMax[i] + psMicroMin[i]) / 2;
					  PrintNum(bucket, ' ', 4);
					  //Serial.write(' ');
				   }
				  Serial.print(F("] "));

				  for(unsigned int i=1; i <= psniSize; i++) {
						Serial.print(((psiNibbles[psniStart + i] >> 4) & 0x0F),HEX);
						Serial.print(((psiNibbles[psniStart + i]) & 0x0F),HEX);
				  }
				}
			  Serial.println();
#if 1			// no package detection
				//psInit();
				return;
#endif
			  psniStart = psniStart + psniSize + 1;
			  psniSize = psiNibbles[psniStart];
		} while (package == 0 && psniSize > 16);
	}
}

#if 1 // Rinie get to know code
  static uint16_t psCount = 0;
  static uint16_t spikeCount = 0;
  static uint16_t spikeCountL = 0;
  static uint16_t longSpike = 0;
  static uint16_t psCountSpike1 = 0;
  static uint16_t psCountSpike2 = 0;
  static uint32_t startSignal;
  static uint32_t startSignalm;
  static uint32_t lastSignal = 0;
  static uint lastPulseDur = 0;
  static bool fCheckClear = false;
  static uint16_t lastPsCount = 0;
#endif

#include "analysepacket.h"

bool processBitRkr(uint16_t pulse_dur, uint8_t signal, uint8_t rssi) {
  if (pulse_dur > 1) {
	  if ((pulse_dur > 75) && (pulse_dur < EDGE_TIMEOUT)){
#if 1
			if (fCheckClear) {
				if ((millis() - lastSignal) >= 100) {
					psCount = 0;
					spikeCount = 0;
					spikeCountL = 0;
					longSpike = 0;
					psCountSpike1 = 0;
					psCountSpike2 = 0;
					psInit();
					Serial.println(F("fCheckClear lastSignal"));
				}
				else {
					Serial.print(F("Try Continue lastSignal "));
					Serial.println(millis() - lastSignal);
					lastPsCount = psCount;
					 startSignal = millis();
				}
				fCheckClear = false;
			}
#endif
		  if (psCount == 0) {
			  startSignal = millis();
			  startSignalm = micros();
			  if (!lastSignal) {
				  lastSignal = startSignal;
			  }
			  psInit();
		  }
		  if (psCount & 1) {
			if (psniIndex < NRELEMENTS(psiNibbles)) {
				psiNibbles[psniIndex++] = psNibbleIndex(lastPulseDur, pulse_dur, /* (psCount < 2) ? psHeader : */ psData);
			}
		  }
		  else {
			  lastPulseDur = pulse_dur;
		  }
  		  psCount++;
	   }
	   else if (psCount > 0) {
		   if (pulse_dur <= 75) {
		   spikeCount++;
	   }
	   else {
			spikeCountL++;
			if (psCountSpike1 == 0) {
				psCountSpike1 = psCount + 1;
			}
			else if (psCountSpike2 == 0) {
				psCountSpike2 = psCount + 1;
			}
			if (pulse_dur > longSpike) {
				longSpike = pulse_dur;
			}
	   }
   }
  }
  if ((!rssi) && (pulse_dur == 1)) { // footer, fake pulse, print and reset
	if ((psCount > 64) && ((lastPsCount == 0) || ((psCount - lastPsCount) > 8))) {
		uint32_t now = millis();
		uint32_t nowm = micros();
		if (lastPsCount) {
			Serial.print(F("*"));
		}
		if ((spikeCount > 0) || (spikeCountL > 1)) {
			Serial.print(F("RKR Count[Spike/SpikeL/Spike1/Spike2/LongSpike/]Length/Gap: "));
			Serial.print(psCount);
			Serial.print(F("["));
			Serial.print(spikeCount);
			Serial.print(F(" "));
			Serial.print(spikeCountL);
			Serial.print(F(" "));
			Serial.print(psCountSpike1);
			Serial.print(F(" "));
			Serial.print(psCountSpike2);
			Serial.print(F(" "));
			Serial.print(longSpike);
			Serial.print(F("]"));
			Serial.print(nowm - startSignalm);
			Serial.print(F(":"));
			Serial.print(startSignal - lastSignal);
			//Serial.print(F("?"));
			//Serial.print(startSignal);
			//Serial.print(F("!"));
			//Serial.print(lastSignal);
			  printRSSI();
			  Serial.println();
			fCheckClear = false;
		}
		else {
			fCheckClear = psCount < 400;
		}
//		else {
#if 1
			// terminate recording
			psiNibbles[psniStart] = psniIndex - psniStart - 1;
			psniStart = psniIndex;
			if (psniIndex < NRELEMENTS(psiNibbles)) {
				psiNibbles[psniIndex++] = 0;
			}
			fSortNibbles = false;
			psiPrint();
			fSortNibbles = true;
#endif
			psiPrint();
			analysepacket(0);
//		}
		lastSignal = millis();
	}
if (!fCheckClear) {
	psCount = 0;
	lastPsCount = 0;
	spikeCount = 0;
	spikeCountL = 0;
	longSpike = 0;
	psCountSpike1 = 0;
	psCountSpike2 = 0;
	psInit();
}
  }
	return false; // return true to skip decoders...
}
