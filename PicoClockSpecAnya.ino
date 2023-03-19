/* ESP32 Audio Spectrum Analyser on an SSD1306/SH1106 Display, 8-bands 125, 250, 500, 1k, 2k, 4k, 8k, 16k
 * Improved noise performance and speed and resolution.
  *####################################################################################################################################
 This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.
*/
#define _HOME_   // this is not needed only for My Personal Secret.h File

#include <Wire.h>
#include <arduinoFFT.h> // Standard Arduino FFT library https://github.com/kosme/arduinoFFT
#include <WiFi.h>
#include <Adafruit_Protomatter.h>
#include "Matrix_Config_GM.h"

// Ensure this line Secret.h is in " " not < > as i have My Secrets in a library Not Working Directory
#include <Secret.h>    

#include <ezTime.h>
#include "atawi8b.h"
#include "atawi10b.h"
arduinoFFT FFT = arduinoFFT();
/////////////////////////////////////////////////////////////////////////
Adafruit_Protomatter display(
    M_WIDTH,                   // Width of matrix (or matrix chain) in pixels
    4,                         // Bit depth, 1-6
    1, rgbPins,                // # of matrix chains, array of 6 RGB pins for each
    4, addrPins,               // # of address pins (height is inferred), array of pins
    clockPin, latchPin, oePin, // Other matrix control pins
    true,                     // No double-buffering here (see "doublebuffer" example)
    1);
#define BANDS                   // Comment out for 16 bands 
#define TZ 0
#define SAMPLES 1024            // Must be a power of 2 
#define SAMPLING_FREQUENCY 30000 // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define amplitude 600            // Depending on your audio source level, you may need to increase this value
unsigned int sampling_period_us;
unsigned long microseconds;
byte peak[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime, oldTime;
int dominant_value;
bool flasher = false;

uint8_t r = 0, g = 0, b = 0;
int hue1 = 32768;
int h, m, s;
unsigned int NewRTCh = 24;
unsigned int NewRTCm = 60;
unsigned int NewRTCs = 10;
time_t t;
uint32_t lastTime = 0, lastTim = 0; // millis() memory
static time_t last_t;
#define BAR_WIDTH 3

// standard colors
uint16_t myRED = display.Color333(2, 0, 0);
uint16_t myGREEN = display.Color333(0, 2, 0);
uint16_t myBLUE = display.Color333(0, 0, 2);
uint16_t myWHITE = display.Color333(2, 2, 2);
uint16_t myYELLOW = display.Color333(2, 2, 0);
uint16_t myCYAN = display.Color333(0, 2, 2);
uint16_t myMAGENTA = display.Color333(2, 0, 2);
uint16_t myShadow = display.Color333(1, 0, 2);
uint16_t myROSE = display.Color333(2, 0, 1);
uint16_t myBLACK = display.Color333(0, 0, 0);
uint16_t myCOLORS[9] = {myROSE, myWHITE, myMAGENTA, myBLUE, myYELLOW, myCYAN, myShadow, myGREEN, myRED};
/////////////////////////////////////////////////////////////////////////
void setup()
{
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("");
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);
    configTime(TZ * 3600, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp"); // enable NTP

    // Initialize matrix...
    ProtomatterStatus status = display.begin();
    Serial.print("Protomatter begin() status: ");
    Serial.println((int)status);
    if (status != PROTOMATTER_OK)
    {
        // DO NOT CONTINUE if matrix setup encountered an error.
        for (;;)
            ;
    }
    display.setTextWrap(false); // Don't wrap at end of line - will do ourselves
    display.fillScreen(0);
    display.show();
    sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));
}

void loop()
{
    //display.drawString(0, 0, "125 250 500 1K  2K 4K 8K 16K");
    for (int i = 0; i < SAMPLES; i++)
    {
        newTime = micros();
        vReal[i] = analogRead(26); // Using Arduino ADC nomenclature. A conversion takes about 1uS on an ESP32
                                   // vReal[i] = analogRead(VP); // Using logical name fo ADC port
        // vReal[i] = analogRead(36); // Using pin number for ADC port
        vImag[i] = 0;
        while ((micros() - newTime) < sampling_period_us)
        { /* do nothing to wait */
        }
    }
    FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);
    display.fillScreen(0);
    for (int i = 2; i < (SAMPLES / 2); i++)
    { // Don't use sample 0 and only the first SAMPLES/2 are usable.
        // Each array element represents a frequency and its value, is the amplitude. Note the frequencies are not discrete.
        if (vReal[i] > 1500)
        { // Add a crude noise filter
            #if defined(BANDS)
            /*8 bands, 12kHz top band*/
              if (i<=3 )            displayBand(0, (int)vReal[i]);
              if (i>3   && i<=6  )  displayBand(1, (int)vReal[i]);
              if (i>6   && i<=13 )  displayBand(2, (int)vReal[i]);
              if (i>13  && i<=27 )  displayBand(3, (int)vReal[i]);
              if (i>27  && i<=55 )  displayBand(4, (int)vReal[i]);
              if (i>55  && i<=112)  displayBand(5, (int)vReal[i]);
              if (i>112 && i<=229)  displayBand(6, (int)vReal[i]);
              if (i>229          )  displayBand(7, (int)vReal[i]);
            
            #else
            // 16 bands, 12kHz top band
            if (i <= 2)             displayBand(0, (int)vReal[i]);
            if (i > 2 && i <= 3)    displayBand(1, (int)vReal[i]);
            if (i > 3 && i <= 5)    displayBand(2, (int)vReal[i]);
            if (i > 5 && i <= 7)    displayBand(3, (int)vReal[i]);
            if (i > 7 && i <= 9)    displayBand(4, (int)vReal[i]);
            if (i > 9 && i <= 13)   displayBand(5, (int)vReal[i]);
            if (i > 13 && i <= 18)  displayBand(6, (int)vReal[i]);
            if (i > 18 && i <= 25)  displayBand(7, (int)vReal[i]);
            if (i > 25 && i <= 36)  displayBand(8, (int)vReal[i]);
            if (i > 36 && i <= 50)  displayBand(9, (int)vReal[i]);
            if (i > 50 && i <= 69)  displayBand(10, (int)vReal[i]);
            if (i > 69 && i <= 97)  displayBand(11, (int)vReal[i]);
            if (i > 97 && i <= 135) displayBand(12, (int)vReal[i]);
            if (i > 135 && i <= 189)displayBand(13, (int)vReal[i]);
            if (i > 189 && i <= 264)displayBand(14, (int)vReal[i]);
            if (i > 264)            displayBand(15, (int)vReal[i]);

        #endif
        }
        for (byte band = 0; band <= 15; band++)
            display.drawLine((4) * band, 32 - peak[band], ((4) * band) + BAR_WIDTH-1, 32 - peak[band], display.color565(50,0,0));
    }
    if (millis() % 2 == 0)
    {
        for (byte band = 0; band <= 15; band++)
        {
            if (peak[band] > 0)
                peak[band] -= 1;
        }
    } // Decay the peak
    t = time(NULL);
    if (last_t != t)
    {
        flasher = !flasher;
        last_t = t;
    }
        updateTime();
        getTim();
    display.show();
}

void displayBand(int band, int dsize)
{

    int dmax = 16;
    dsize /= amplitude;
    int V = map(dsize, 0, 31, 0, 255);
    int S = map(dsize, 0, 31, 0, 255);
    int H = map(band, 0, 16, 0, 65535);
    if (dsize > dmax)
        dsize = dmax;
    int colour = display.colorHSV(H, 255, 50);
    for (int s = 0; s <= dsize; s = s + 1)
    {
        // if (s > 20 && s <= 28)
        //     colour = display.color565(50, 50, 0);
        // if (s > 28)
        //     colour = display.color565(50, 0, 0);
        display.drawLine((4) * band, 32 - s, ((4) * band) + BAR_WIDTH-1, 32 - s, colour);
    }
    if (dsize > peak[band])
    {
        peak[band] = dsize;
    }
    Serial.printf("dsize :%d\tH :%d\tS :%d\tV :%d\n",dsize, H,S,V);
}
void updateTime()
{
    struct tm *tm;
    tm = localtime(&t);
    h = tm->tm_hour;
    m = tm->tm_min;
    s = tm->tm_sec;
    // Serial.println("Time Updated");
}
void getTim()
{
    if (flasher)
    {
        display.fillRect(52, 0, 7, 6, myBLACK);
        display.setFont(&atawi10b);
        display.setCursor(22, 0);
        display.setTextColor(myCOLORS[g]);
        display.print(":");
        b = random(0, 8);
        hue1+=16;
        if(hue1 >65535)
            hue1 = 0;
    }
    else
    {
        display.fillRect(22, 0, 4, 12, myBLACK);
        display.setFont(&atawi10b);
        display.setCursor(52, 0);
        display.setTextColor(myCOLORS[b]);
        display.print("*");
        display.setFont();
        g = random(0, 8);
    }
    if (NewRTCs != s / 10)
    {
        display.setFont(&atawi8b);
        display.setCursor(49, 6);
        display.setTextColor(myCOLORS[r]);
        display.fillRect(49, 6, 13, 6, myBLACK);
        display.printf("%02d", s);
        display.setFont();
        r = random(0, 8);
        NewRTCs = s / 10;
    }
    else
    {
        display.setFont(&atawi8b);
        display.setCursor(49, 6);
        display.setTextColor(myCOLORS[r]);
        display.fillRect(56, 6, 6, 6, myBLACK);
        display.printf("%02d", s);
        display.setFont();
    }
    // if (NewRTCm != m)
    // {
        display.setFont(&atawi10b);
        display.setCursor(26, 0);
        display.setTextColor(display.colorHSV(hue1, 255, 50));
        display.fillRect(26, 0, 22, 12, myBLACK);
        display.printf("%02d", m);
        display.setFont();
        NewRTCm = m;
    // }
    // if (NewRTCh != h)
    // {
        display.setFont(&atawi10b);
        display.setCursor(1, 0);
        display.setTextColor(display.colorHSV(hue1, 255, 50));
        display.fillRect(0, 0, 22, 12, myBLACK);
        display.printf("%02d", h);
        display.setFont();
        NewRTCh = h;
    // }
    // Serial.println("Time updated");
}
