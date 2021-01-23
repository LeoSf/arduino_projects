/*
 *
 * Esp32_oscilloscope.ino
 *
 *  This file is part of Esp32_oscilloscope project: https://github.com/BojanJurca/Esp32_oscilloscope
 *  Esp32 oscilloscope is also fully included in Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 *
 *  Esp32 oscilloscope is built upon Esp32_web_ftp_telnet_server_template. As a stand-alone project it uses only those
 *  parts of Esp32_web_ftp_telnet_server_template that are necessary to run Esp32 oscilloscope.
 *
 *  Copy all files in the package into Esp32_oscilloscope directory, compile them with Arduino and run on ESP32.
 *
 * History:
 *          - first release as a stand-alone project,
 *            August 24, 2019, Bojan Jurca
 *
 */

#include <WiFi.h>

/* including network credentials */
#include "network_credentials.h"
extern const char* staSSID;
extern const char* staPassword;

// simplify the use of FTP server in this project by avoiding user management - anyone is alowed to use it
#define USER_MANAGEMENT NO_USER_MANAGEMENT
#include "user_management.h"

// we'll need SPIFFS file system for storing uploaded oscilloscope.html
#include "file_system.h"

// include FTP server - we'll need it for uploading oscilloscope.html to ESP32
#include "ftpServer.hpp"
ftpServer *ftpSrv;

// include Web server - we'll need it to handle oscilloscope WS requests
#include "webServer.hpp"
httpServer *webSrv;

// HTTP and WS request handler function
void runOscilloscope (WebSocket *webSocket);

String httpRequestHandler (String& httpRequest) {
    // handle HTTP protocol requests, return HTML or JSON text
    // note that webServer handles HTTP requests to .html files by itself (in our case oscilloscope.html if uploaded into /var/www/html/ with FTP previously)
    // so you don't have to handle this kind of requests yourself

    // example:
    // if (httpRequest.substring (0, 12) == "GET /upTime ") {  // return up-time in JSON format
    //                                                        unsigned long l = micros ();
    //                                                        return "{\"id\":\"esp32\",\"upTime\":\"" + String (l) + " sec\"}\r\n";
    //                                                      }
    // else

    return ""; // HTTP request has not been handled by httpRequestHandler - let the webServer handle it itself
}
void wsRequestHandler (String& wsRequest, WebSocket *webSocket) {
    // handle HTTP protocol requests, return HTML or JSON text
    // note that webServer handles HTTP requests to .html files by itself (in our case oscilloscope.html if uploaded into /var/www/html/ with FTP previously)
    // so you don't have to handle this kind of requests yourself

    Serial.printf ("[%10lu] [oscilloscope] got WS request\n%s", millis (), wsRequest.c_str ());

    // handle WS (WebSockets) protocol requests
    if (wsRequest.substring (0, 21) == "GET /runOscilloscope " ) { // called from oscilloscope.html
        runOscilloscope (webSocket);
    }
}

// Oscilloscope ---------------------------------------------------------------------------------------------------

struct oscilloscopeSample {
    int16_t value;       // sample value read by analogRead or digialRead
    int16_t timeOffset;  // sampe time offset drom previous sample in ms or us
};

struct oscilloscopeSamples {
    oscilloscopeSample samples [64]; // sample buffer will never exceed 41 samples, make it 64 for simplicity reasons
    int count;                       // number of samples in the buffer
    bool ready;                      // is the buffer ready for sending
};

struct oscilloscopeSharedMemoryType { // data structure to be shared with oscilloscope tasks
    // basic objects for webSocket communication
    WebSocket *webSocket;               // open webSocket for communication with javascript client
    bool clientIsBigEndian;             // true if javascript client is big endian machine
    // sampling parameterss
    char readType [8];                  // analog or digital
    bool analog;                        // true if readType is analog, false if digital
    int gpio;                           // gpio in which ESP32 is taking samples from
    int samplingTime;                   // time between samples in ms or us
    char samplingTimeUnit [3];          // ms or us
    bool microSeconds;                  // true if samplingTimeunit is us, false if ms
    byte microSecondCorrection;         // delay correction for micro second tiing
    int screenWidthTime;                // oscilloscope screen width in ms or us
    char screenWidthTimeUnit [3];       // ms or us
    int screenRefreshTime;              // time before screen is refreshed with new samples in ms or us
    long screenRefreshTimeCommonUnit;   // screen refresh time translated into same time unit as screen width time
    char screenRefreshTimeUnit [3];     // ms or us
    int screenRefreshModulus;           // used to reduce refresh frequency down to sustainable 20 Hz
    bool positiveTrigger;               // true if posotive slope trigger is set
    int positiveTriggerTreshold;        // positive slope trigger treshold value
    bool negativeTrigger;               // true if negative slope trigger is set
    int negativeTriggerTreshold;        // negative slope trigger treshold value
    // buffers holding samples
    oscilloscopeSamples readBuffer;     // we'll read samples into this buffer
    oscilloscopeSamples sendBuffer;     // we'll copy red buffer into this buffer before sending samples to the client
    portMUX_TYPE csSendBuffer;          // MUX for handling critical section while copying
    // status of oscilloscope threads
    bool readerIsRunning;
    bool senderIsRunning;
};

void oscilloscopeReader (void *parameters) {
    unsigned char gpio =                ((oscilloscopeSharedMemoryType *) parameters)->gpio;
    bool analog =                       !strcmp (((oscilloscopeSharedMemoryType *) parameters)->readType, "analog");
    int16_t samplingTime =              ((oscilloscopeSharedMemoryType *) parameters)->samplingTime;
    bool microSeconds =                 ((oscilloscopeSharedMemoryType *) parameters)->microSeconds;
    int screenWidthTime =               ((oscilloscopeSharedMemoryType *) parameters)->screenWidthTime;
    // int screenRefreshTime =             ((oscilloscopeSharedMemoryType *) parameters)->screenRefreshTime;
    long screenRefreshTimeCommonUnit =  ((oscilloscopeSharedMemoryType *) parameters)->screenRefreshTimeCommonUnit;
    int screenRefreshModulus =          ((oscilloscopeSharedMemoryType *) parameters)->screenRefreshModulus;
    oscilloscopeSamples *readBuffer =   &((oscilloscopeSharedMemoryType *) parameters)->readBuffer;
    oscilloscopeSamples *sendBuffer =   &((oscilloscopeSharedMemoryType *) parameters)->sendBuffer;
    bool positiveTrigger =              ((oscilloscopeSharedMemoryType *) parameters)->positiveTrigger;
    int16_t positiveTriggerTreshold =   ((oscilloscopeSharedMemoryType *) parameters)->positiveTriggerTreshold;
    bool negativeTrigger =              ((oscilloscopeSharedMemoryType *) parameters)->negativeTrigger;
    int16_t negativeTriggerTreshold =   ((oscilloscopeSharedMemoryType *) parameters)->negativeTriggerTreshold;
    portMUX_TYPE *csSendBuffer =        &((oscilloscopeSharedMemoryType *) parameters)->csSendBuffer;

    int screenTimeOffset = 0;
    int16_t sampleTimeOffset = 0;
    int screenRefreshCounter = 0;
    while (true) {

        // insert first dummy sample that tells javascript client to start drawing from the left
        readBuffer->samples [0] = {-1, -1};
        readBuffer->count = 1;

        unsigned int lastSampleTime = microSeconds ? micros () : millis ();
        screenTimeOffset = 0;
        bool triggeredMode = positiveTrigger || negativeTrigger;

        if (triggeredMode) { // if no trigger is set then skip this part and start sampling immediatelly
            // Serial.printf ("[oscilloscope] waiting to be triggered ...\n");
            int16_t lastSample = analog ? analogRead (gpio) : digitalRead (gpio);
            lastSampleTime = microSeconds ? micros () : millis ();

            while (true) {
                if (microSeconds) SPIFFSsafeDelayMicroseconds (samplingTime);
                else              SPIFFSsafeDelay (samplingTime);
                int16_t newSample = analog ? analogRead (gpio) : digitalRead (gpio);
                unsigned int newSampleTime = microSeconds ? micros () : millis ();
                if ((positiveTrigger && lastSample < positiveTriggerTreshold && newSample >= positiveTriggerTreshold) ||
                (negativeTrigger && lastSample > negativeTriggerTreshold && newSample <= negativeTriggerTreshold)) {
                    // insert both samples into the buffer
                    readBuffer->samples [1] = {lastSample, 0};
                    readBuffer->samples [2] = {newSample, (int16_t) (screenTimeOffset = newSampleTime - lastSampleTime)};
                    readBuffer->count = 3;
                    break;
                }
                lastSample = newSample;
                lastSampleTime = newSampleTime;
                // stop reading if sender is not running any more
                if (!((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning) {
                    ((oscilloscopeSharedMemoryType *) parameters)->readerIsRunning = false;
                    vTaskDelete (NULL); // instead of return; - stop this thread
                }
            } // while (true)
        } // if (positiveTrigger || negativeTrigger)

        // take (the rest of the) samples that fit into one screen
        do {

            unsigned int newSampleTime = microSeconds ? micros () : millis ();
            int16_t sampleTimeOffset = newSampleTime - lastSampleTime;
            screenTimeOffset += sampleTimeOffset;
            lastSampleTime = newSampleTime;

            // if we are past screen refresh time copy readBuffer into sendBuffer then empty readBuffer
            if (screenTimeOffset >= screenRefreshTimeCommonUnit) {
                // but only if modulus == 0 to reduce refresh frequency to sustainable 20 Hz
                if (triggeredMode || !(screenRefreshCounter = (screenRefreshCounter + 1) % screenRefreshModulus)) {
                    portENTER_CRITICAL (csSendBuffer);
                    *sendBuffer = *readBuffer; // this also copies 'ready' flag which is 'true'
                    portEXIT_CRITICAL (csSendBuffer);
                }
                // empty readBuffer
                readBuffer->count = 0;
            } // if (screenTimeOffset >= screenRefreshTimeCommonUnit)
            // take the next sample
            readBuffer->samples [readBuffer->count] = {(int16_t) (analog ? analogRead (gpio) : digitalRead (gpio)), sampleTimeOffset};
            readBuffer->count = (readBuffer->count + 1) & 0b00111111; // 0 .. 63 max (which is inside buffer size) - just in case, the number of samples will never exceed 41
            // stop reading if sender is not running any more
            if (!((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning) {
                ((oscilloscopeSharedMemoryType *) parameters)->readerIsRunning = false;
                vTaskDelete (NULL); // instead of return; - stop this thread
            }

            if (microSeconds) SPIFFSsafeDelayMicroseconds (samplingTime);
            else              SPIFFSsafeDelay (samplingTime);

        } while (screenTimeOffset < screenWidthTime);

        // after the screen frame is processed we have to handle the preparations for the next screen frame differently for triggered and non triggered sampling
        if (triggeredMode) {
            // in triggered mode we have to wait for refresh time to pass before trying again
            // one screen frame has already been sent, we have to wait yet screenRefreshModulus - 1 screen frames
            // for the whole screen refresh time to pass
            for (int i = 1; i < screenRefreshModulus; i++) {
                if (microSeconds) SPIFFSsafeDelayMicroseconds (screenRefreshTimeCommonUnit);
                else              SPIFFSsafeDelay (screenRefreshTimeCommonUnit);
            }
        } else {
            // correct sampleTimeOffset for the first sample int the next screen frame in non triggered mode
            sampleTimeOffset = screenTimeOffset - screenWidthTime;
        }
    } // while (true)
}

void oscilloscopeSender (void *parameters) {
    WebSocket *webSocket =            ((oscilloscopeSharedMemoryType *) parameters)->webSocket;
    bool clientIsBigEndian =          ((oscilloscopeSharedMemoryType *) parameters)->clientIsBigEndian;
    oscilloscopeSamples *sendBuffer = &((oscilloscopeSharedMemoryType *) parameters)->sendBuffer;
    portMUX_TYPE *csSendBuffer =      &((oscilloscopeSharedMemoryType *) parameters)->csSendBuffer;

    while (true) {
        SPIFFSsafeDelay (1);
        // send samples to javascript client if they are ready
        if (sendBuffer->ready) {
            oscilloscopeSamples samples;
            portENTER_CRITICAL (csSendBuffer);
            samples = *sendBuffer;
            sendBuffer->ready = false;
            portEXIT_CRITICAL (csSendBuffer);
            // swap bytes if javascript calient is big endian
            int size8_t = samples.count << 2;   // number of bytes = number of samles * 4
            int size16_t = samples.count << 1;  // number of 16 bit words = number of samles * 2
            if (clientIsBigEndian) {
                uint16_t *a = (uint16_t *) &samples;
                for (size_t i = 0; i < size16_t; i ++) a [i] = htons (a [i]);
            }
            if (!webSocket->sendBinary ((byte *) &samples,  size8_t)) {
                ((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning = false; // notify oscilloscope functions that session is finished
                vTaskDelete (NULL); // instead of return; - stop this task
            }
            // Serial.printf ("[oscilloscope] send %i samples, %i bytes to the client\n", samples.count, size8_t);
        }
        // read command form javscrip client if it arrives
        if (webSocket->available () == WebSocket::STRING) { // according to oscilloscope protocol the string could only be 'stop' - so there is no need checking it
        String s = webSocket->readString ();
        Serial.printf ("[%10lu] [oscilloscope] %s\n", millis (), s.c_str ());
        ((oscilloscopeSharedMemoryType *) parameters)->senderIsRunning = false; // notify oscilloscope functions that session is finished
        vTaskDelete (NULL); // instead of return; - stop this task
    }
}
}

void runOscilloscope (WebSocket *webSocket) {

    Serial.printf ("[%10lu] [oscilloscope] starting oscilloscope\n", millis ());

    // set up oscilloscope memory that will be shared among oscilloscope threads
    oscilloscopeSharedMemoryType oscilloscopeSharedMemory = {}; // get some memory that will be shared among all oscilloscope threads and initialize it with zerros
    oscilloscopeSharedMemory.webSocket = webSocket;             // put webSocket rference into shared memory
    oscilloscopeSharedMemory.readBuffer.ready = true;           // this value will be copied into sendBuffer later
    oscilloscopeSharedMemory.csSendBuffer = portMUX_INITIALIZER_UNLOCKED;   // initialize critical section mutex in shared memory

    // oscilloscope protocol starts with binary endian identification from the client
    uint16_t endianIdentification = 0;

    if (webSocket->readBinary ((byte *) &endianIdentification, sizeof (endianIdentification)) == sizeof (endianIdentification))
    {
        oscilloscopeSharedMemory.clientIsBigEndian = (endianIdentification == 0xBBAA); // cient has sent 0xAABB
    }

    if (endianIdentification == 0xAABB || endianIdentification == 0xBBAA)
    {
        Serial.printf ("[%10lu] [oscilloscope] clientIsBigEndian = %i\n", millis (), oscilloscopeSharedMemory.clientIsBigEndian);
    } else
    {
        Serial.printf ("[%10lu] [oscilloscope] communication does not follow oscilloscope protocol - expected endian identification\n", millis ());
        webSocket->sendString ("Oscilloscope error. Expected endian identification bytes but got something else."); // send error also to javascript client
        return;
    }

    Serial.printf ("[%10lu] [oscilloscope] client endiad identification complete: %s\n", millis (), oscilloscopeSharedMemory.clientIsBigEndian ? "big endian" : "little endian");

    // oscilloscope protocol continues with text start command in the following form:
    // "start digital sampling on GPIO 22 every 100 ms refresh screen of width 400 ms every 100 ms set positive slope trigger to 512 set negative slope trigger to 0"
    String s = webSocket->readString ();

    Serial.printf ("[%10lu] [oscilloscope] got start command %s\n", millis (), s.c_str ());

    // try to parse what we have got from client
    char posNeg1 [9] = "";
    char posNeg2 [9] = "";
    int treshold1;
    int treshold2;

    switch (sscanf(
        s.c_str (), "start %7s sampling on GPIO %i every %i %2s refresh screen of width %i %2s every %i %2s set %8s slope trigger to %i set %8s slope trigger to %i",
        oscilloscopeSharedMemory.readType,
        &oscilloscopeSharedMemory.gpio,
        &oscilloscopeSharedMemory.samplingTime,
        oscilloscopeSharedMemory.samplingTimeUnit,
        &oscilloscopeSharedMemory.screenWidthTime,
        oscilloscopeSharedMemory.screenWidthTimeUnit,
        &oscilloscopeSharedMemory.screenRefreshTime,
        oscilloscopeSharedMemory.screenRefreshTimeUnit,
        posNeg1,
        &treshold1,
        posNeg2,
        &treshold2))
    {

        case 8:   // no trigger
            // Serial.printf ("No trigger\n");
            break;

        case 12:  // two triggers
            // Serial.printf ("Two triggers\n");
            if (!strcmp (posNeg2, "positive")) {
                oscilloscopeSharedMemory.positiveTrigger = true;
                oscilloscopeSharedMemory.positiveTriggerTreshold = treshold2;
            }
            if (!strcmp (posNeg2, "negative")) {
                oscilloscopeSharedMemory.negativeTrigger = true;
                oscilloscopeSharedMemory.negativeTriggerTreshold = treshold2;
            }
            // break; // no break;

        case 10:  // one trigger
            // Serial.printf ("One trigger\n");
            if (!strcmp (posNeg1, "positive")) {
                oscilloscopeSharedMemory.positiveTrigger = true;
                oscilloscopeSharedMemory.positiveTriggerTreshold = treshold1;
            }
            if (!strcmp (posNeg1, "negative")) {
                oscilloscopeSharedMemory.negativeTrigger = true;
                oscilloscopeSharedMemory.negativeTriggerTreshold = treshold1;
            }
            break;

        default:
            Serial.printf ("[%10lu] [oscilloscope] communication does not follow oscilloscope protocol\n", millis ());
            return;
    }

    // check the values of oscilloscope parameters and calculate derived parameter values
    if (!strcmp (oscilloscopeSharedMemory.readType, "analog") || !strcmp (oscilloscopeSharedMemory.readType, "digital"))
    {
        Serial.printf ("[%10lu] [oscilloscope] readType = %s\n", millis (), oscilloscopeSharedMemory.readType);
        oscilloscopeSharedMemory.analog = !strcmp (oscilloscopeSharedMemory.readType, "analog");
        Serial.printf ("[%10lu] [oscilloscope] analog = %i\n", millis (), oscilloscopeSharedMemory.analog);
    } else
    {
        Serial.printf ("[%10lu] [oscilloscope] wrong readType\n", millis ());
        webSocket->sendString ("Oscilloscope error. Read type can only be analog or digital."); // send error also to javascript client
        return;
    }

    if (oscilloscopeSharedMemory.gpio >= 0 && oscilloscopeSharedMemory.gpio <= 255)
    {
        Serial.printf ("[%10lu] [oscilloscope] GPIO = %i\n", millis (), oscilloscopeSharedMemory.gpio);
    } else
    {
        Serial.printf ("[%10lu] [oscilloscope] wrong GPIO\n", millis ());
        webSocket->sendString ("Oscilloscope error. GPIO must be between 0 and 255."); // send error also to javascript client
        return;
    }

    if (oscilloscopeSharedMemory.samplingTime >= 1 && oscilloscopeSharedMemory.samplingTime <= 25000) {
        Serial.printf ("[%10lu] [oscilloscope] samplingTime = %i\n", millis (), oscilloscopeSharedMemory.samplingTime);
    } else
    {
        Serial.printf ("[%10lu] [oscilloscope] invalid sampling time\n", millis ());
        webSocket->sendString ("Oscilloscope error. Sampling time must be between 1 and 25000."); // send error also to javascript client
        return;
    }

    if (!strcmp (oscilloscopeSharedMemory.samplingTimeUnit, "ms") || !strcmp (oscilloscopeSharedMemory.samplingTimeUnit, "us"))
    {
        Serial.printf ("[%10lu] [oscilloscope] samplingTimeUnit = %s\n", millis (), oscilloscopeSharedMemory.samplingTimeUnit);
        oscilloscopeSharedMemory.microSeconds = !strcmp (oscilloscopeSharedMemory.samplingTimeUnit, "us");
        Serial.printf ("[%10lu] [oscilloscope] microSeconds = %i\n", millis (), oscilloscopeSharedMemory.microSeconds);
        if (oscilloscopeSharedMemory.microSeconds) oscilloscopeSharedMemory.microSecondCorrection = oscilloscopeSharedMemory.analog ? 12 : 2;
        Serial.printf ("[%10lu] [oscilloscope] microSecondCorrection = %i\n", millis (), oscilloscopeSharedMemory.microSecondCorrection);

    } else {
        Serial.printf ("[%10lu] [oscilloscope] wrong samplingTimeUnit\n", millis ());
        webSocket->sendString ("Oscilloscope error. Sampling time unit can only be ms or us."); // send error also to javascript client
        return;
    }

    if (oscilloscopeSharedMemory.screenWidthTime >= 4 * oscilloscopeSharedMemory.samplingTime  && oscilloscopeSharedMemory.screenWidthTime <= 1250000)
    {
        Serial.printf ("[%10lu] [oscilloscope] screenWidthTime = %i\n", millis (), oscilloscopeSharedMemory.screenWidthTime);
    } else {
        Serial.printf ("[%10lu] [oscilloscope] invalid screen width time\n", millis ());
        webSocket->sendString ("Oscilloscope error. Screen width time must be between 4 * sampling time and 125000."); // send error also to javascript client
        return;
    }

    if (!strcmp (oscilloscopeSharedMemory.screenWidthTimeUnit, oscilloscopeSharedMemory.samplingTimeUnit))
    {
        Serial.printf ("[%10lu] [oscilloscope] screenWidthTimeUnit = %s\n", millis (), oscilloscopeSharedMemory.screenWidthTimeUnit);
    } else {
        Serial.printf ("[%10lu] [oscilloscope] screenWidthTimeUnit must be the same as samplingTimeUnit\n", millis ());
        webSocket->sendString ("Oscilloscope error. Screen width time unit must be the same as sampling time unit."); // send error also to javascript client
        return;
    }

    if (oscilloscopeSharedMemory.screenRefreshTime >= 40 && oscilloscopeSharedMemory.screenRefreshTime <= 1000)
    {
        Serial.printf ("[%10lu] [oscilloscope] screenRefreshTime = %i\n", millis (), oscilloscopeSharedMemory.screenRefreshTime);
    } else
    {
        Serial.printf ("[%10lu] [oscilloscope] invalid screen refresh time\n", millis ());
        webSocket->sendString ("Oscilloscope error. Screen refresh time must be between 40 and 1000."); // send error also to javascript client
        return;
    }

    if (!strcmp (oscilloscopeSharedMemory.screenRefreshTimeUnit, "ms"))
    {
        Serial.printf ("[%10lu] [oscilloscope] screenRefreshTimeUnit = %s\n", millis (), oscilloscopeSharedMemory.screenRefreshTimeUnit);
    } else
    {
        Serial.printf ("[%10lu] [oscilloscope] screenRefreshTimeUnit must be ms\n", millis ());
        webSocket->sendString ("Oscilloscope error. Screen refresh time unit must be ms."); // send error also to javascript client
        return;
    }

    if (!strcmp (oscilloscopeSharedMemory.screenWidthTimeUnit, "ms"))
        oscilloscopeSharedMemory.screenRefreshTimeCommonUnit =  oscilloscopeSharedMemory.screenRefreshTime;
    else
        oscilloscopeSharedMemory.screenRefreshTimeCommonUnit =  oscilloscopeSharedMemory.screenRefreshTime * 1000;

    oscilloscopeSharedMemory.screenRefreshModulus = oscilloscopeSharedMemory.screenRefreshTimeCommonUnit / oscilloscopeSharedMemory.screenWidthTime;

    if (oscilloscopeSharedMemory.screenRefreshModulus < 1) // if requested refresh time is less then screen width time then leave it as it is
        oscilloscopeSharedMemory.screenRefreshModulus = 1;
    else
        // if requested refresh time is equal or greater then screen width time then make it equal to screen width time and increase modulus
        // for actual refresh if needed (this approach somehow simplyfies sampling code)
        oscilloscopeSharedMemory.screenRefreshTimeCommonUnit = oscilloscopeSharedMemory.screenWidthTime;

    Serial.printf ("[%10lu] [oscilloscope] screenRefreshModulus = %i\n", millis (), oscilloscopeSharedMemory.screenRefreshModulus);

    if (oscilloscopeSharedMemory.screenRefreshTimeCommonUnit > oscilloscopeSharedMemory.screenWidthTime &&
        oscilloscopeSharedMemory.screenRefreshTimeCommonUnit != (long) oscilloscopeSharedMemory.screenRefreshModulus * (long) oscilloscopeSharedMemory.screenWidthTime)
            Serial.printf ("[%10lu] [oscilloscope] it would be better if screen refresh time is multiple value of screen width time or equal to sampling time\n", millis ()); // just a suggestion

    Serial.printf ("[%10lu] [oscilloscope] screenRefreshTimeCommonUnit = %ld (same time unit as screen width time)\n", millis (), oscilloscopeSharedMemory.screenRefreshTimeCommonUnit);

    oscilloscopeSharedMemory.samplingTime -= oscilloscopeSharedMemory.microSecondCorrection;

    if (oscilloscopeSharedMemory.samplingTime >= 1)
    {
        Serial.printf ("[%10lu] [oscilloscope] samplingTime (after correction) = %i\n", millis (), oscilloscopeSharedMemory.samplingTime);
    } else
    {
        Serial.printf ("[%10lu] [oscilloscope] invalid sampling time after correction\n", millis ());
        webSocket->sendString ("Oscilloscope error. Sampling time is too low according to other settings."); // send error also to javascript client
        return;
    }
    if (oscilloscopeSharedMemory.positiveTrigger)
    {
        if (oscilloscopeSharedMemory.positiveTriggerTreshold > 0 && oscilloscopeSharedMemory.positiveTriggerTreshold <= (strcmp (oscilloscopeSharedMemory.readType, "analog") ? 1 : 4095)) {
            Serial.printf ("[%10lu] [oscilloscope] positive slope trigger treshold = %i\n", millis (), oscilloscopeSharedMemory.positiveTriggerTreshold);
        } else
        {
            Serial.printf ("[%10lu] [oscilloscope] invalid positive slope trigger treshold\n", millis ());
            webSocket->sendString ("Oscilloscope error. Positive slope trigger treshold is not valid (according to other settings)."); // send error also to javascript client
            return;
        }
    }

    if (oscilloscopeSharedMemory.negativeTrigger)
    {
        if (oscilloscopeSharedMemory.negativeTriggerTreshold >= 0 && oscilloscopeSharedMemory.negativeTriggerTreshold < (strcmp (oscilloscopeSharedMemory.readType, "analog") ? 1 : 4095))
        {
            Serial.printf ("[%10lu] [oscilloscope] negative slope trigger treshold = %i\n", millis (), oscilloscopeSharedMemory.negativeTriggerTreshold);
        } else
        {
            Serial.printf ("[%10lu] [oscilloscope] invalid negative slope trigger treshold\n", millis ());
            webSocket->sendString ("Oscilloscope error. Negative slope trigger treshold is not valid (according to other settings)."); // send error also to javascript client
            return;
        }
    }

    #define tskNORMAL_PRIORITY 1
    if (pdPASS != xTaskCreate ( oscilloscopeSender,
                                "oscilloscopeSender",
                                4096,
                                (void *) &oscilloscopeSharedMemory, // pass parameters to oscilloscopeSender
                                tskNORMAL_PRIORITY,
                                NULL))
    {
        Serial.printf ("[%10lu] [oscilloscope] could not start oscilloscopeSender\n", millis ());
    } else
    {
        oscilloscopeSharedMemory.senderIsRunning = true;
    }

    #define tskNORMAL_PRIORITY 1

    if (pdPASS != xTaskCreate ( oscilloscopeReader,
                                "oscilloscopeReader",
                                4096,
                                (void *) &oscilloscopeSharedMemory, // pass parameters to oscilloscopeReader
                                tskNORMAL_PRIORITY,
                                NULL))
    {
        Serial.printf ("[%10lu] [oscilloscope] could not start oscilloscopeReader\n", millis ());
    } else
    {
        oscilloscopeSharedMemory.readerIsRunning = true;
    }

    Serial.printf ("[%10lu] [oscilloscope] oscilloscopeSender and oscilloscopeReader started\n", millis ());

    while (oscilloscopeSharedMemory.senderIsRunning || oscilloscopeSharedMemory.readerIsRunning)
    SPIFFSsafeDelay (100); // check every 1/10 of secod

    return;
}

// setup (), loop () --------------------------------------------------------

void setup () {
    Serial.begin (115200);

    // connect ESP STAtion to WiFi

    // #define staSSID "YOUR-STA-SSID"
    // #define staPassword "YOUR-STA-PASSWORD"

    WiFi.disconnect (true);
    WiFi.mode (WIFI_OFF);
    WiFi.onEvent ([] (WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case SYSTEM_EVENT_WIFI_READY:
                Serial.printf ("[%10lu] [network] WiFi interface ready.\n", millis ());
                break;
            case SYSTEM_EVENT_STA_START:
                Serial.printf ("[%10lu] [network] [STA] WiFi client started.\n", millis ());
                break;
            case SYSTEM_EVENT_STA_CONNECTED:
                Serial.printf ("[%10lu] [network] [STA] connected to WiFi %s.\n", millis (), WiFi.SSID ().c_str());
                break;
            case SYSTEM_EVENT_STA_GOT_IP:
                Serial.printf ("[%10lu] [network] [STA] obtained IP address %s.\n", millis (), WiFi.localIP ().toString ().c_str ());
                break;
            default:
                break;
        }
    });
    Serial.printf ("[%10lu] [network] [STA] connecting STAtion to router using DHCP ...\n", millis ());
    WiFi.begin (staSSID, staPassword);
    WiFi.mode (WIFI_STA); // STA mode only

    // mount file system, WEB server will search tor files in /var/www/html directory

    mountSPIFFS (true);

    // start FTP server so we can upload .html and .png files into /var/www/html directory

    ftpSrv = new ftpServer ("0.0.0.0", 21, NULL);
    if (ftpSrv) { // did ESP create TcpServer instance?
        if (ftpSrv->started ()) { // did FTP server start correctly?
            Serial.printf ("[%10lu] FTP server has started.\n", millis ());
        } else {
            Serial.printf ("[%10lu] FTP server did not start. Compile it in Verbose Debug level to find the error.\n", millis ());
        }
    } else {
        Serial.printf ("[%10lu] not enough free memory to start FTP server\n", millis ()); // shouldn't happen
    }

    // start WEB server
    webSrv = new httpServer (httpRequestHandler, wsRequestHandler, 8192, (char *) "0.0.0.0", 80, NULL);
    if (webSrv) { // did ESP create TcpServer instance?
        if (webSrv->started ()) { // did WEB server start correctly?
            Serial.printf ("[%10lu] WEB server has started.\n", millis ());
        } else {
            Serial.printf ("[%10lu] WEB server did not start. Compile it in Verbose Debug level to find the error.\n", millis ());
        }
    } else {
        Serial.printf ("[%10lu] not enough free memory to start WEB server\n", millis ()); // shouldn't happen
    }
}

void loop () {
    SPIFFSsafeDelay (1);  // instead of delay ()
    // don't use delay () in multi-threaded environment together with SPIFSS - see:
    // https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template/issues/1
}
