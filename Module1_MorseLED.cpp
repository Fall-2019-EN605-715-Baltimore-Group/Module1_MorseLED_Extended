#include <Arduino.h>
#include <string.h>

#define DOT__MSEC   150
#define DASH_MSEC   500
#define DOWN_MSEC   250
#define CHAR_MSEC   500

#define ACTIVE_RX_LANES 3
#define ACTIVE_TX_LANES 3

#define DOT_STR  '*'
#define DASH_STR '-'
#define END_LINE_DELIMITER "."

//#define DEBUG

enum MORSE_UNIT { DOT = 0, DASH = 1 };

const char txPinVal[] = {13, 12, 8, 7};

const char morseUnitVal[] = {0x1,0x8,0xA,0x4,0x0,0x2,0x6,0x0,0x0,0x7,0x5,0x4,0x3,0x2,0x7,0x6,0xD,0x2,0x0,0x1,0x1,0x1,0x3,0x9,0x7,0xC};
const char morseUnitMax[] = {0x2,0x4,0x4,0x3,0x1,0x4,0x3,0x4,0x2,0x4,0x3,0x4,0x2,0x2,0x3,0x4,0x4,0x3,0x3,0x1,0x3,0x4,0x3,0x4,0x4,0x4};
/*                          a,  b,  c,  d,  e,  f,  g,  h,  i,  j,  k,  l,  m,  n,  o,  p,  q,  r,  s,  t,  u,  v,  w,  x,  y,  z */


unsigned long txTimers[ACTIVE_TX_LANES] = {0, };
unsigned int  txLane[ACTIVE_TX_LANES] = {0, };
unsigned char txChar[ACTIVE_TX_LANES] = {0, };
unsigned char txUnit[ACTIVE_TX_LANES] = {0, };

unsigned rxStrIdx(0);

int txStateMask(0);

String strs[ACTIVE_RX_LANES] = {"", };

#ifdef DEBUG
String debugStr;
#endif

void setup()
{
    for (int i = 0; i < ACTIVE_TX_LANES; ++i)
        {pinMode(i, OUTPUT); digitalWrite(i, LOW);}

    // open the serial port:
    Serial.begin(115200);

    Serial.println("<Arduino is ready>");
}

void loop()
{
    // check for incoming serial data:
    while (Serial.available() > 0)
    {
        // read incoming serial data:
        char ch = Serial.read();

        // Check for exit condition.
        if (ch == 0x1A)
        {
            for (unsigned i = 0u; i < ACTIVE_TX_LANES; ++i)
                digitalWrite(txPinVal[i], LOW);
            Serial.println("<Program Exiting>");
            return;
        }

        // Add the character to the current string.
        if (ch == 0xD && strs[rxStrIdx].length())
            strs[rxStrIdx]  += END_LINE_DELIMITER;
        else if (0x61 <= ch && ch <= 0x7A)
            strs[rxStrIdx]  += ch;

        if (strs[rxStrIdx].endsWith(END_LINE_DELIMITER))
        {
            // Check to see if any of the TX lanes are open
            for (unsigned i = 0u; i < ACTIVE_TX_LANES; ++i)
            {
                if (!txTimers[i])
                {
                    String str = String(i);
                    str += " | " ;
                    str += strs[rxStrIdx];
                    Serial.println(str);

                    txLane[i] = rxStrIdx;
                    txChar[i] = 0;
                    char currChar = strs[txLane[i]].charAt(0);
                    txUnit[i] = morseUnitMax[currChar - 0x61] - 1;

                    bool unit = bitRead(morseUnitVal[currChar - 0x61], txUnit[i]);

                    bitSet(txStateMask, i); digitalWrite(txPinVal[i],HIGH);

#ifdef DEBUG
                    debugStr = String(i);
                    debugStr += " | " ;
                    debugStr += (unit == DOT)? DOT_STR : DASH_STR;
                    Serial.println(debugStr);
#endif

                    txTimers[i] = (unit == DOT)? DOT__MSEC : DASH_MSEC; txTimers[i] += millis();

                    rxStrIdx = (rxStrIdx + 1) % ACTIVE_RX_LANES;
                    break;
                }
            }   // END for (i < ACTIVE_TX_LANES)
        }
    }

    for (unsigned i = 0u; i < ACTIVE_TX_LANES; ++i)
    {
        // If the timer is valid and has elapsed.
        if (txTimers[i] && (txTimers[i] < millis()))
        {
            // If the timer is transitioning to the dead state.
            bool txState = bitRead(txStateMask, i);
            if (txState)
            {
                bitClear(txStateMask, i);   digitalWrite(txPinVal[i], LOW);
                txTimers[i] = (txUnit[i])? DOWN_MSEC : CHAR_MSEC;
                txTimers[i] += millis(); // Update Timer
            }
            // Otherwise the timer is transitioning to the active state.
            else
            {
                --txUnit[i];

                // Transition to the next unit in the character.
                if (txUnit[i] < 255)
                {
                    char currChar = strs[txLane[i]].charAt(txChar[i]);

                    bool unit = bitRead(morseUnitVal[currChar - 'a'], txUnit[i]);

#ifdef DEBUG
                    debugStr = String(i);
                    debugStr += " | " ;
                    debugStr += (unit == DOT)? DOT_STR : DASH_STR;
                    Serial.println(debugStr);
#endif

                    bitSet(txStateMask, i); digitalWrite(txPinVal[i], HIGH);
                    txTimers[i] = (unit == DOT)? DOT__MSEC : DASH_MSEC; txTimers[i] += millis();

                }
                // Transition to the next character
                else
                {
                    if (++txChar[i] < strs[txLane[i]].length()-1)
                    {
                        char nextChar = strs[txLane[i]].charAt(txChar[i]);
                        txUnit[i] = morseUnitMax[nextChar - 0x61] - 1;
                        bool unit = bitRead(morseUnitVal[nextChar - 0x61], txUnit[i]);

#ifdef DEBUG
                        debugStr = String(i);
                        debugStr += " | " ;
                        debugStr += (unit == DOT)? DOT_STR : DASH_STR;
                        Serial.println(debugStr);
#endif

                        bitSet(txStateMask, i); digitalWrite(txPinVal[i], HIGH);

                        txTimers[i] = (unit == DOT)? DOT__MSEC : DASH_MSEC;
                        txTimers[i] += millis();
                    }
                    else
                    {
#ifdef DEBUG
                        debugStr = String(i);
                        debugStr += " - String Complete";
                        Serial.println(debugStr);
#endif
                        bitClear(txStateMask, i); digitalWrite(txPinVal[i], LOW);

                        strs[txLane[i]] = "";
                        txTimers[i] = 0;    txChar[i] = 0;  txUnit[i] = 0; txLane[i] = 0;
                    }
                }
            }
        }   // END if (txTimers[i] && (txTimers[i] < millis()))
    }   // END for (i < ACTIVE_TX_LANES)
}


//    // check for incoming serial data:
//    if (Serial.available() > 0)
//    {
//        // read incoming serial data:
//        String str = Serial.readStringUntil(0x0D);
//
//        Serial.print("This just in ... ");
//        Serial.println(str);
//
//        for (int i = 0; i < str.length(); ++i)
//        {
//            printMorseChar(str.charAt(i));
//        }
//
//        while (Serial.available() > 0) { Serial.read();}
//    }


