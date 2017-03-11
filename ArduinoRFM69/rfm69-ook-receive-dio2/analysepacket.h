// RF Sniffer V1.0.7
// Martinus van den Broek
// 01-01-2014
//
// Stel de baudrate van de Arduino IDE in op 115200 baud!
//
// Based on work by Paul Tonkes (www.nodo-domotica.nl)
//
// This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
/*********************************************************************************************\
 * Analyse unknown protocol
\*********************************************************************************************/
#define RKR_PRINT_HEX

void analysepacket(byte mode)
{
		uint psniStart = 0;
		byte psniSize = psiNibbles[psniStart];

      Serial.print(F("AnalysePacket"));
      Serial.print(F(", Pulses:"));
      Serial.print(psniSize*2);
      Serial.print(F(", "));

      int x;
      unsigned int y,z;

      unsigned int MarkShort=50000;
      unsigned int MarkLong=0;
      for(x=2;x<psniSize;x++)
        {
		byte pulse = ((psiNibbles[psniStart + x] >> 4) & 0x0F);
		y = (psMicroMax[pulse] + psMicroMin[pulse]) / 2;
        if(y<MarkShort)
          MarkShort=y;
        if(y>MarkLong)
          MarkLong=y;
        }
      z=true;
      while(z)
        {
        z=false;
      for(x=2;x<psniSize;x++)
        {
		byte pulse = ((psiNibbles[psniStart + x] >> 4) & 0x0F);
		y = (psMicroMax[pulse] + psMicroMin[pulse]) / 2;
          if(y>MarkShort && y<(MarkShort+MarkShort/2))
            {
            MarkShort=y;
            z=true;
            }
          if(y<MarkLong && y>(MarkLong-MarkLong/2))
            {
            MarkLong=y;
            z=true;
            }
          }
        }
      unsigned int MarkMid=((MarkLong-MarkShort)/2)+MarkShort;

      unsigned int SpaceShort=50000;
      unsigned int SpaceLong=0;
      for(x=2;x<psniSize;x++)
        {
		byte space = ((psiNibbles[psniStart + x]) & 0x0F);
		y = (psMicroMax[space] + psMicroMin[space]) / 2;
        if(y<SpaceShort)
          SpaceShort=y;
        if(y>SpaceLong)
          SpaceLong=y;
        }
      z=true;
      while(z)
        {
        z=false;
      for(x=2;x<psniSize;x++)
        {
		byte space = ((psiNibbles[psniStart + x]) & 0x0F);
		y = (psMicroMax[space] + psMicroMin[space]) / 2;
          if(y>SpaceShort && y<(SpaceShort+SpaceShort/2))
            {
            SpaceShort=y;
            z=true;
            }
          if(y<SpaceLong && y>(SpaceLong-SpaceLong/2))
            {
            SpaceLong=y;
            z=true;
            }
          }
        }
      int SpaceMid=((SpaceLong-SpaceShort)/2)+SpaceShort;

      // Bepaal soort signaal
      y=0;
      if(MarkLong  > (2*MarkShort  ))y=1; // PWM
      if(SpaceLong > (2*SpaceShort ))y+=2;// PDM

      Serial.print(F( "Bits="));
#ifdef RKR_PRINT_HEX
	unsigned int j = 0;
	byte hex = 0;
#endif
      if(y==0)Serial.print(F("?"));
      if(y==1)
        {
      for(x=1;x<psniSize;x++)
        {
		byte pulse = ((psiNibbles[psniStart + x] >> 4) & 0x0F);
		y = (psMicroMax[pulse] + psMicroMin[pulse]) / 2;
#ifndef RKR_PRINT_HEX
          if(y>MarkMid)
            Serial.write('1');
          else
            Serial.write('0');
          }
#else
		hex = (hex << 1) | ((y>MarkMid) ? 1 : 0);
		j ++;
		if (j >= 8) {
			PrintNumHex(hex, 0, 2);
			j = 0;
			hex = 0;
		}
	}
	if (j > 0) {
			PrintNumHex(hex, 0, 2);
	}
#endif
        Serial.print(F(", Type=PWM"));
        }
      if(y==2)
        {
      for(x=1;x<psniSize;x++)
        {
		byte space = ((psiNibbles[psniStart + x]) & 0x0F);
		y = (psMicroMax[space] + psMicroMin[space]) / 2;
#ifndef RKR_PRINT_HEX
          if(y>SpaceMid)
            Serial.write('1');
          else
            Serial.write('0');
          }
#else
		hex = (hex << 1) | ((y>SpaceMid) ? 1 : 0);
		j ++;
		if (j >= 8) {
			PrintNumHex(hex, 0, 2);
			j = 0;
			hex = 0;
		}
	}
	if (j > 0) {
			PrintNumHex(hex, 0, 2);
	}
#endif
        Serial.print(F(", Type=PDM"));
        }
      if(y==3)
        {
      for(x=1;x<psniSize;x++)
        {
		byte pulse = ((psiNibbles[psniStart + x] >> 4) & 0x0F);
		y = (psMicroMax[pulse] + psMicroMin[pulse]) / 2;
#ifndef RKR_PRINT_HEX
          if(y>MarkMid)
            Serial.write('1');
          else
            Serial.write('0');
#else
		hex = (hex << 1) | ((y>MarkMid) ? 1 : 0);
		j ++;
		if (j >= 8) {
			PrintNumHex(hex, 0, 2);
			j = 0;
			hex = 0;
		}
#endif
		byte space = ((psiNibbles[psniStart + x]) & 0x0F);
		y = (psMicroMax[space] + psMicroMin[space]) / 2;
#ifndef RKR_PRINT_HEX
          if(y>SpaceMid)
            Serial.write('1');
          else
            Serial.write('0');
          }
#else
		hex = (hex << 1) | ((y>SpaceMid) ? 1 : 0);
		j ++;
		if (j >= 8) {
			PrintNumHex(hex, 0, 2);
			j = 0;
			hex = 0;
		}
	}
	if (j > 0) {
			PrintNumHex(hex, 0, 2);
	}
#endif
        Serial.print(F( ", Type=Manchester"));
        }
#if 0
      if (mode == 2)
        {
          Serial.print(F(", Pulses(uSec)="));
          for(x=1;x<RawSignal.Number;x++)
            {
              Serial.print(RawSignal.Pulses[x]*RawSignal.Multiply);
              Serial.write(',');
            }
        }
      count_protocol[10]++;
#endif
      Serial.println();
}
