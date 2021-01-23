/*
 *
 * webServer.hpp
 *
 *  This file is part of Esp32_web_ftp_telnet_server_template project: https://github.com/BojanJurca/Esp32_web_ftp_telnet_server_template
 *
 *  WebServer includes httpServer with connectionHandler that handles TcpConnection according to HTTP protocol.
 *
 *  A connectionHandler handles some HTTP requests by itself but the calling program can provide its own callback
 *  function. In this case connectionHandler will first ask callback function weather is it going to handle the HTTP
 *  request. If not the connectionHandler will try to resolve it by checking file system for a proper .html file
 *  or it will respond with reply 404 - page not found.
 *
 * History:
 *          - first release,
 *            November 19, 2018, Bojan Jurca
 *          - added SPIFFSsemaphore and SPIFFSsafeDelay () to assure safe muti-threading while using SPIFSS functions (see https://www.esp32.com/viewtopic.php?t=7876),
 *            April 13, 2019, Bojan Jurca
 *          - added webClient function,
 *            added basic support for web sockets
 *            May, 19, 2019, Bojan Jurca
 *          - minor structural changes,
 *            the use of dmesg
 *            September 14, 2019, Bojan Jurca
 *          - added webClientCallMAC function
 *            September, 27, Bojan Jurca
 *          - separation of httpHandler and wsHandler
 *            October 30, 2019, Bojan Jurca
 *          - webServer is now inherited from TcpServer and renamed to httpServer
 *            February 27.2.2020, Bojan Jurca
 *          - elimination of compiler warnings and some bugs
 *            Jun 11, 2020, Bojan Jurca
 *
 */

#ifndef __WEB_SERVER__
  #define __WEB_SERVER__

  #ifndef HOSTNAME
    #define HOSTNAME "MyESP32Server" // WiFi.getHostname() // use default if not defined
  #endif

  // ----- includes, definitions and supporting functions -----

  #include <WiFi.h>

  void __webDmesg__ (String message) {
    #ifdef __TELNET_SERVER__ // use dmesg from telnet server if possible
      dmesg (message);
    #else
          Serial.printf ("[%10lu] %s\n", millis (), message.c_str ());
    #endif
  }
  void (* webDmesg) (String) = __webDmesg__; // use this pointer to display / record system messages

  #include "TcpServer.hpp"        // webServer.hpp is built upon TcpServer.hpp
  #include "user_management.h"    // webServer.hpp needs user_management.h to get www home directory
  #include "file_system.h"        // webServer.hpp needs file_system.h to read files  from home directory
  #include "network.h"            // webServer.hpp needs network.h

  // missing C function in Arduino, but we are going to need it
  char *stristr (char *haystack, char *needle) {
    if (!haystack || !needle) return NULL; // nonsense
    int nCheckLimit = strlen (needle);
    int hCheckLimit = strlen (haystack) - nCheckLimit + 1;
    for (int i = 0; i < hCheckLimit; i++) {
      int j = i;
      int k = 0;
      while (*(needle + k)) {
          char nChar = *(needle + k ++); if (nChar >= 'a' && nChar <= 'z') nChar -= 32; // convert to upper case
          char hChar = *(haystack + j ++); if (hChar >= 'a' && hChar <= 'z') hChar -= 32; // convert to upper case
          if (nChar != hChar) break;
      }
      if (!*(needle + k)) return haystack + i; // match!
    }
    return NULL; // no match
  }


/*
 * Basic support for webSockets. A lot of webSocket things are missing. Out of 3 frame sizes only first 2 are
 * supported, the third is too large for ESP32 anyway. Continuation frames are not supported hence all the exchange
 * information should fit into one frame. Beside that only non-control frames are supported. But all this is just
 * information for programmers who want to further develop this modul. For those who are just going to use it I'll
 * try to hide this complexity making the use of webSockets as easy and usefull as possible under given limitations.
 * See the examples.
 *
 */

  #include <hwcrypto/sha.h>       // needed for websockets support
  #include <mbedtls/base64.h>     // needed for websockets support

  class WebSocket {

    public:

      WebSocket (TcpConnection *connection,     // TCP connection over which webSocket connection communicate with browser
                 String wsRequest               // ws request for later reference if needed
                )                               {
                                                  // make a copy of constructor parameters
                                                  __connection__ = connection;
                                                  __wsRequest__ = wsRequest;

                                                  // do the handshake with the browser so it would consider webSocket connection established
                                                  int i = wsRequest.indexOf ("Sec-WebSocket-Key: ");
                                                  if (i > -1) {
                                                    int j = wsRequest.indexOf ("\r\n", i + 19);
                                                    if (j > -1) {
                                                      String key = wsRequest.substring (i + 19, j);
                                                      if (key.length () <= 24) { // Sec-WebSocket-Key is not supposed to exceed 24 characters

                                                        // calculate Sec-WebSocket-Accept
                                                        char s1 [64]; strcpy (s1, key.c_str ()); strcat (s1, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
                                                        #define SHA1_RESULT_SIZE 20
                                                        unsigned char s2 [SHA1_RESULT_SIZE]; esp_sha (SHA1,(unsigned char*) s1, strlen (s1), s2);
                                                        #define WS_CLIENT_KEY_LENGTH  24
                                                        size_t olen = WS_CLIENT_KEY_LENGTH;
                                                        char s3 [32];
                                                        mbedtls_base64_encode ((unsigned char *) s3, 32, &olen, s2, SHA1_RESULT_SIZE);
                                                        // compose websocket accept reply and send it back to the client
                                                        char buffer  [255]; // this will do
                                                        sprintf (buffer, "HTTP/1.1 101 Switching Protocols \r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", s3);
                                                        if (connection->sendData (buffer)) {
                                                          // Serial.printf ("[webSocket] connection confirmed\n");
                                                        } else {
                                                          // log_e ("[webSocket] couldn't send accept key back to browser\n");
                                                        }
                                                      } else { // |key| > 24
                                                        // log_e ("[webSocket] key in wsRequest too long\n");
                                                      }
                                                    } else { // j == -1
                                                      // log_e ("[webSocket] key not found in webRequest\n");
                                                    }
                                                  } else { // i == -1
                                                    // log_e ("[webSocket] key not found in webRequest\n");
                                                  }

                                                  // we won't do the checking if everythingg was sucessfull in constructor,
                                                  // subsequent calls would run into an error enyway in this case so they
                                                  // can handle the errors themselves

                                                  // TO DO: make constructor return NULL in case of any error
                                                }

      ~WebSocket ()                             { // destructor
                                                  if (__payload__) {
                                                    free (__payload__);
                                                    // __payload__ = NULL;
                                                  }
                                                  // send closing frame if possible
                                                  __sendFrame__ (NULL, 0, WebSocket::CLOSE);
                                                }

      String getWsRequest ()                    {return __wsRequest__;}

      enum WEBSOCKET_DATA_TYPE {
        NOT_AVAILABLE = 0,          // no data is available to be read
        STRING = 1,                 // text data is available to be read
        BINARY = 2,                 // binary data is available to be read
        CLOSE = 8,
        ERROR = 3
      };

      WEBSOCKET_DATA_TYPE available ()          { // checks if data is ready to be read, returns the type of data that arrived.
                                                  // All the TCP connection reading is actually done within this function
                                                  // that buffers data that can later be read by using readString () or
                                                  // readBinary () or binarySize () functions. Call it repeatedly until
                                                  // data type is returned.

                                                  switch (__bufferState__) {
                                                    case EMPTY:                   {
                                                                                    // Serial.printf ("[webSocket] EMPTY, preparing data structure\n");
                                                                                    // prepare data structure for the first read operation
                                                                                    __bytesRead__ = 0;
                                                                                    __bufferState__ = READING_SHORT_HEADER;
                                                                                    // continue reading immediately
                                                                                  }
                                                    case READING_SHORT_HEADER:    {
                                                                                    // Serial.printf ("[webSocket] READING_SHORT_HEADER, reading 6 bytes of header\n");
                                                                                    switch (__connection__->available ()) {
                                                                                      case TcpConnection::NOT_AVAILABLE:  return WebSocket::NOT_AVAILABLE;
                                                                                      case TcpConnection::ERROR:          return WebSocket::ERROR;
                                                                                      default:                            break;
                                                                                    }

                                                                                    // read 6 bytes of short header
                                                                                    if (6 != (__bytesRead__ += __connection__->recvData ((char *) __header__ + __bytesRead__, 6 - __bytesRead__))) return WebSocket::NOT_AVAILABLE; // if we haven't got 6 bytes continue reading short header the next time available () is called

                                                                                    // check if this frame type is supported
                                                                                    if (!(__header__ [0] & 0b10000000)) { // check fin bit
                                                                                      Serial.printf ("[webSocket] browser send a frame that is not supported: fin bit is not set\n");
                                                                                      __connection__->closeConnection ();
                                                                                      return WebSocket::ERROR;
                                                                                    }
                                                                                    byte b  = __header__ [0] & 0b00001111; // check opcode, 1 = text, 2 = binary, 8 = close
                                                                                    if (b == WebSocket::CLOSE) {
                                                                                      __connection__->closeConnection ();
                                                                                      Serial.printf ("[webSocket] browser requested to close webSocket\n");
                                                                                      return WebSocket::NOT_AVAILABLE;
                                                                                    }
                                                                                    if (b != WebSocket::STRING && b != WebSocket::BINARY) {
                                                                                      Serial.printf ("[webSocket] browser send a frame that is not supported: opcode is not text or binary\n");
                                                                                      __connection__->closeConnection ();
                                                                                      return WebSocket::ERROR;
                                                                                    } // NOTE: after this point only TEXT and BINRY frames are processed!
                                                                                    // check payload length that also determines frame type
                                                                                    __payloadLength__ = __header__ [1] & 0b01111111; // byte 1: mask bit is always 1 for packets thet came from browsers, cut it off
                                                                                    if (__payloadLength__ <= 125) { // short payload
                                                                                          __mask__ = __header__ + 2; // bytes 2, 3, 4, 5
                                                                                          if (!(__payload__ = (byte *) malloc (__payloadLength__ + 1))) { // + 1: final byte to conclude C string if data type is text
                                                                                            __connection__->closeConnection ();
                                                                                            Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                                                            return WebSocket::ERROR;
                                                                                          }
                                                                                          // continue with reading payload immediatelly
                                                                                          __bufferState__ = READING_PAYLOAD;
                                                                                          __bytesRead__ = 0; // reset the counter, count only payload from now on
                                                                                          goto readingPayload;
                                                                                    } else if (__payloadLength__ == 126) { // 126 means medium payload, read additional 2 bytes of header
                                                                                          __bufferState__ = READING_MEDIUM_HEADER;
                                                                                          // continue reading immediately
                                                                                    } else { // 127 means large data block - not supported since ESP32 doesn't have enough memory
                                                                                          Serial.printf ("[webSocket] browser send a frame that is not supported: payload is larger then 65535 bytes\n");
                                                                                          __connection__->closeConnection ();
                                                                                          return WebSocket::ERROR;
                                                                                    }
                                                                                  }
                                                    case READING_MEDIUM_HEADER:   {
                                                                                    // Serial.printf ("[webSocket] READING_MEDIUM_HEADER, reading additional 2 bytes of header\n");
                                                                                    // we don't have to repeat the checking already done in short header case, just read additiona 2 bytes and correct data structure

                                                                                    // read additional 2 bytes (8 altogether) bytes of medium header
                                                                                    if (8 != (__bytesRead__ += __connection__->recvData ((char *) __header__ + __bytesRead__, 8 - __bytesRead__))) return WebSocket::NOT_AVAILABLE; // if we haven't got 8 bytes continue reading medium header the next time available () is called
                                                                                    // correct internal structure for reading into extended buffer and continue at FILLING_EXTENDED_BUFFER immediately
                                                                                    __payloadLength__ = __header__ [2] << 8 | __header__ [3];
                                                                                    __mask__ = __header__ + 4; // bytes 4, 5, 6, 7
                                                                                    if (!(__payload__ = (byte *) malloc (__payloadLength__ + 1))) { // + 1: final byte to conclude C string if data type is text
                                                                                      __connection__->closeConnection ();
                                                                                      Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                                                      return WebSocket::ERROR;
                                                                                    }
                                                                                    __bufferState__ = READING_PAYLOAD;
                                                                                    __bytesRead__ = 0; // reset the counter, count only payload from now on
                                                                                    // continue with reading payload immediatelly
                                                                                  }
                                                    case READING_PAYLOAD:
readingPayload:
                                                                                  // Serial.printf ("[webSocket] READING_PAYLOAD, reading %i bytes of payload\n", __payloadLength__);
                                                                                  {
                                                                                    // read all payload bytes
                                                                                    if (__payloadLength__ != (__bytesRead__ += __connection__->recvData ((char *) __payload__ + __bytesRead__, __payloadLength__ - __bytesRead__))) {
                                                                                      return WebSocket::NOT_AVAILABLE; // if we haven't got all payload bytes continue reading the next time available () is called
                                                                                    }
                                                                                    // all is read, decode (unmask) the data
                                                                                    for (int i = 0; i < __payloadLength__; i++) __payload__ [i] = (__payload__ [i] ^ __mask__ [i % 4]);
                                                                                    // conclude payload with 0 in case this is going to be interpreted as text - like C string
                                                                                    __payload__ [__payloadLength__] = 0;
                                                                                    __bufferState__ = FULL;     // stop reading until buffer is read by the calling program
                                                                                    return __header__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary
                                                                                  }

                                                    case FULL:                    // return immediately, there is no space left to read new incoming data
                                                                                  // Serial.printf ("[webSocket] FULL, waiting for calling program to fetch the data\n");
                                                                                  return __header__ [0] & 0b0000001 /* if text bit set */ ? STRING : BINARY; // notify calling program about the type of data waiting to be read, 1 = text, 2 = binary
                                                  }
                                                  // for every case that has not been handeled earlier return not available
                                                  return NOT_AVAILABLE;
                                                }

      String readString ()                      { // reads String that arrived from browser (it is a calling program responsibility to check if data type is text)
                                                  // returns "" in case of communication error
                                                  while (true) {
                                                    switch (available ()) {
                                                      case WebSocket::NOT_AVAILABLE:  SPIFFSsafeDelay (1);
                                                                                      break;
                                                      case WebSocket::STRING:         { // Serial.printf ("readString: binary size = %i, buffer state = %i, available = %i\n", binarySize (), __bufferState__, available ());
                                                                                        String s;
                                                                                        // if (__bufferState__ == FULL && __payload__) { // double check ...
                                                                                          s = String ((char *) __payload__);
                                                                                          free (__payload__);
                                                                                          __payload__ = NULL;
                                                                                          __bufferState__ = EMPTY;
                                                                                        // } else {
                                                                                        //   s = "";
                                                                                        // }
                                                                                        return s;
                                                                                      }
                                                      default:                        return ""; // WebSocket::BINARY or WebSocket::ERROR
                                                    }
                                                  }
                                                }

      size_t binarySize ()                      { // returns how many bytes has arrived from browser, 0 if data is not ready (yet) to be read
                                                  return __bufferState__ == FULL ? __payloadLength__ : 0;                                                    return 0;
                                                }

      size_t readBinary (byte *buffer, size_t bufferSize) { // returns number bytes copied into buffer
                                                            // returns 0 if there is not enough space in buffer or in case of communication error
                                                  while (true) {
                                                    switch (available ()) {
                                                      case WebSocket::NOT_AVAILABLE:  SPIFFSsafeDelay (1);
                                                                                      break;
                                                      case WebSocket::BINARY:         { // Serial.printf ("readBinary: binary size = %i, buffer state = %i, available = %i\n", binarySize (), __bufferState__, available ());
                                                                                        size_t l = binarySize ();
                                                                                        // if (__bufferState__ == FULL && __payload__) { // double check ...
                                                                                          if (bufferSize >= l)
                                                                                            memcpy (buffer, __payload__, l);
                                                                                          else
                                                                                            l = 0;
                                                                                          free (__payload__);
                                                                                          __payload__ = NULL;
                                                                                          __bufferState__ = EMPTY;
                                                                                        // } else {
                                                                                        //   l = 0;
                                                                                        // }
                                                                                        return l;
                                                                                      }
                                                      default:                        return 0; // WebSocket::STRING or WebSocket::ERROR
                                                    }
                                                  }
                                                }

      bool sendString (String text)             { // returns success
                                                  return __sendFrame__ ((byte *) text.c_str (), text.length (), WebSocket::STRING);
                                                }

      bool sendBinary (byte *buffer, size_t bufferSize) { // returns success
                                                  return __sendFrame__ (buffer, bufferSize, WebSocket::BINARY);
                                                }

    private:

      bool __sendFrame__ (byte *buffer, size_t bufferSize, WEBSOCKET_DATA_TYPE dataType) { // returns true if frame have been sent successfully
                                                if (bufferSize > 0xFFFF) { // this size fits in large frame size - not supported
                                                  Serial.printf ("[webSocket] trying to send a frame that is not supported: payload is larger then 65535 bytes\n");
                                                  __connection__->closeConnection ();
                                                  return false;
                                                }
                                                byte *frame = NULL;
                                                int frameSize;
                                                if (bufferSize > 125) { // medium frame size
                                                  if (!(frame = (byte *) malloc (frameSize = 4 + bufferSize))) { // 4 bytes for header (without mask) + payload
                                                    Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                    __connection__->closeConnection ();
                                                    return false;
                                                  }
                                                  // frame type
                                                  frame [0] = 0b10000000 | dataType; // set FIN bit and frame data type
                                                  frame [1] = 126; // medium frame size, without masking (we won't do the masking, won't set the MASK bit)
                                                  frame [2] = bufferSize >> 8; // / 256;
                                                  frame [3] = bufferSize; // % 256;
                                                  memcpy (frame + 4, buffer, bufferSize);
                                                } else { // small frame size
                                                  if (!(frame = (byte *) malloc (frameSize = 2 + bufferSize))) { // 2 bytes for header (without mask) + payload
                                                    Serial.printf ("[webSocket] malloc failed - out of memory\n");
                                                    __connection__->closeConnection ();
                                                    return false;
                                                  }
                                                  frame [0] = 0b10000000 | dataType; // set FIN bit and frame data type
                                                  frame [1] = bufferSize; // small frame size, without masking (we won't do the masking, won't set the MASK bit)
                                                  if (bufferSize) memcpy (frame + 2, buffer, bufferSize);
                                                }
                                                if (__connection__->sendData ((char *) frame, frameSize) != frameSize) {
                                                  free (frame);
                                                  __connection__->closeConnection ();
                                                  Serial.printf ("[webSocket] failed to send frame\n");
                                                  return false;
                                                }
                                                free (frame);
                                                return true;
                                              }

      TcpConnection *__connection__;
      String __wsRequest__;

      enum BUFFER_STATE {
        EMPTY = 0,                                // buffer is empty
        READING_SHORT_HEADER = 1,
        READING_MEDIUM_HEADER = 2,
        READING_PAYLOAD = 3,
        FULL = 4                                  // buffer is full and waiting to be read by calling program
      };
      BUFFER_STATE __bufferState__ = EMPTY;
      unsigned int __bytesRead__;                 // how many bytes of current frame have been read so far
      byte __header__ [8];                        // frame header: 6 bytes for short frames, 8 for medium (and 12 for large frames - not supported)
      unsigned int __payloadLength__;             // size of payload in current frame
      byte *__mask__;                             // pointer to 4 frame mask bytes
      byte *__payload__ = NULL;                   // pointer to buffer for frame payload
  };

/*
 * httpServer is inherited from TcpServer with connection handler that handles connections according
 * to HTTP protocol - HTTP 1.0 in this particular implementation, meaning that connection is closed
 * immediatelly after beeing served.
 *
 * Connection handler tries to resolve HTTP request in three ways:
 *  1. checks if the request is a WS request and starts WebSocket in this case
 *  2. asks httpRequestHandler provided by the calling program if it is going to provide the reply
 *  3. checks /var/www/html directry for .html file that suits the request
 *  4. replyes with 404 - not found
 */

  class httpServer: public TcpServer {

    public:

      httpServer (String (*httpRequestHandler) (String& httpRequest),                 // httpRequestHandler callback function provided by calling program
                  void (*wsRequestHandler) (String& wsRequest, WebSocket *webSocket), // httpRequestHandler callback function provided by calling program
                  unsigned int stackSize,                                             // stack size of httpRequestHandler thread, usually 4 KB will do
                  char *serverIP,                                                     // web server IP address, 0.0.0.0 for all available IP addresses - 15 characters at most!
                  int serverPort,                                                     // web server port
                  bool (*firewallCallback) (char *)                                   // a reference to callback function that will be celled when new connection arrives
                 ): TcpServer (__webConnectionHandler__, this, stackSize, 10000, serverIP, serverPort, firewallCallback)
                                {
                                  __httpRequestHandler__ = httpRequestHandler;
                                  __wsRequestHandler__ = wsRequestHandler;
                                  char homeDir [33];
                                  char *p = getUserHomeDirectory (homeDir, (char *) "webserver");
                                  if (p && strlen (p) < sizeof (__webHomeDirectory__)) strcpy (__webHomeDirectory__, p);
                                  if (!*__webHomeDirectory__) {
                                    webDmesg ("[httpServer] home directory for webserver system account is not set.");
                                    return;
                                  }
                                  __started__ = true; // we have initialized everything needed for TCP connection
                                  if (started ()) webDmesg ("[httpServer] started on " + String (serverIP) + ":" + String (serverPort) + (firewallCallback ? " with firewall." : "."));
                                }

      ~httpServer ()            { if (started ()) webDmesg ("[httpServer] stopped."); }

      bool started ()           { return TcpServer::started () && __started__; }

      char *getHomeDirectory () { return __webHomeDirectory__; }

    private:

      String (*__httpRequestHandler__) (String& httpRequest);                 // httpRequestHandler callback function provided by calling program
      void (*__wsRequestHandler__) (String& wsRequest, WebSocket *webSocket); // wsRequestHandler callback function provided by calling program
      char __webHomeDirectory__ [33] = {};                                    // webServer system account home directory

      bool __started__ = false;

      static void __webConnectionHandler__ (TcpConnection *connection, void *thisWebServer) {  // connectionHandler callback function
        httpServer *ths = (httpServer *) thisWebServer; // this is how you pass "this" pointer to static memeber function
        // log_v ("[Thread:%i][Core:%i] connection has started\n", xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
        char buffer [4096];  // make sure there is enough space for each type of use but be modest - this buffer uses thread stack
        int receivedTotal = buffer [0] = 0;
        while (int received = connection->recvData (buffer + receivedTotal, sizeof (buffer) - receivedTotal - 1)) { // this loop may not seem necessary but TCP protocol does not guarantee that a whole request arrives in a single data block althought it usually does
          buffer [receivedTotal += received] = 0; // mark the end of received request
          if (strstr (buffer, "\r\n\r\n")) { // is the end of HTTP request is reached?
            // log_v ("[Thread:%i][Core:%i] new request:\n%s", xTaskGetCurrentTaskHandle (), xPortGetCoreID (), buffer);

            // ----- make a String copy of the http request -----
            String httpRequest = String (buffer);

            // ----- first check if this is a websocket request -----

            if (stristr (buffer, (char *) "CONNECTION: UPGRADE")) {
              connection->setTimeOut (300000); // set time-out to 5 minutes fro WebSockets
              WebSocket webSocket (connection, httpRequest);
              if (ths->__wsRequestHandler__) ths->__wsRequestHandler__ (httpRequest, &webSocket);
              goto closeWebConnection;
            }

            // ----- then ask httpRequestHandler (if it is provided by the calling program) if it is going to handle this HTTP request -----

            // log_v ("[Thread:%i][Core:%i] trying to get a reply from calling program\n", xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
            String httpReply;
            unsigned long timeOutMillis = connection->getTimeOut (); connection->setTimeOut (TcpConnection::INFINITE); // disable time-out checking while proessing httpRequestHandler to allow longer processing times
            if (ths->__httpRequestHandler__ && (httpReply = ths->__httpRequestHandler__ (httpRequest)) != "") {
              httpReply = "HTTP/1.0 200 OK\r\nContent-Type:text/html;\r\nCache-control:no-cache\r\nContent-Length:" + String (httpReply.length ()) + "\r\n\r\n" + httpReply; // add HTTP header
              connection->sendData (httpReply); // send everything to the client
              connection->setTimeOut (timeOutMillis); // restore time-out checking before sending reply back to the client
              goto closeWebConnection;
            }
            connection->setTimeOut (timeOutMillis); // restore time-out checking

            // ----- check if request is of type GET filename - if yes then reply with filename content -----

            char htmlFile [33] = {};
            char fullHtmlFilePath [33];
            if (buffer == strstr (buffer, "GET ")) {
              char *p; if ((p = strstr (buffer + 4, " ")) && (p - buffer) < (sizeof (htmlFile) + 4)) memcpy (htmlFile, buffer + 4, p - buffer - 4);
              if (*htmlFile == '/') strcpy (htmlFile, htmlFile + 1); if (!*htmlFile) strcpy (htmlFile, "index.html");
              char homeDir [33];
              if ((p = getUserHomeDirectory (homeDir, (char *) "webserver"))) {
                if (strlen (p) + strlen (htmlFile) < sizeof (fullHtmlFilePath)) strcat (strcpy (fullHtmlFilePath, p), htmlFile);
                xSemaphoreTake (SPIFFSsemaphore, portMAX_DELAY);
                  File file;
                  if ((bool) (file = SPIFFS.open (fullHtmlFilePath, FILE_READ))) {
                    if (!file.isDirectory ()) {
                      char *buff = (char *) malloc (4096); // get 4 KB of memory from heap (not from the stack)
                      if (buff) {
                        sprintf (buff, "HTTP/1.0 200 OK\r\nContent-Type:text/html;\r\nCache-control:no-cache\r\nContent-Length:%i\r\n\r\n", file.size ());
                        int i = strlen (buff);
                        while (file.available ()) {
                          *(buff + i++) = file.read ();
                          if (i == 4096) { connection->sendData ((char *) buff, 4096); i = 0; }
                        }
                        if (i) { connection->sendData ((char *) buff, i); }
                        free (buff);
                      }
                      file.close ();
                      xSemaphoreGive (SPIFFSsemaphore);
                      goto closeWebConnection;
                    } // if file is a file, not a directory
                    file.close ();
                  } // if file is opened
                xSemaphoreGive (SPIFFSsemaphore);
              }
            }

            // ----- if request was GET / and index.html couldn't be found then send special reply -----

            if (!strcmp (htmlFile, "index.html")) {
              #define NO_INDEX_HTML_MESSAGE "Please use FTP, loggin as webadmin / webadminpassword and upload *.html and *.png files found in Esp32_web_ftp_telnet_server_template package into webserver home directory."
              sprintf (buffer, "HTTP/1.0 200 OK\r\nContent-Type:text/html;\r\nCache-control:no-cache\r\nContent-Length:%i\r\n\r\n%s", strlen (NO_INDEX_HTML_MESSAGE), NO_INDEX_HTML_MESSAGE);
              connection->sendData (buffer);
              goto closeWebConnection;
            }

// reply404:

            // ----- 404 page not found reply -----

            #define response404 "HTTP/1.0 404 Not found\r\nContent-Type:text/html;\r\nContent-Length:20\r\n\r\nPage does not exist." // HTTP header and content
            connection->sendData ((char *) response404); // send response
            webDmesg (String ("[httpServer] don't know how to handle http request from " + String (connection->getOtherSideIP ()) + "\r\n") + httpRequest);

          } else { // is the end of HTTP request is reached?
            webDmesg ("[httpServer] http request does not end properly, server is closing the connection.");
          }
        }

      closeWebConnection:
        ;
        // log_v ("[Thread:%i][Core:%i] connection has ended\n", xTaskGetCurrentTaskHandle (), xPortGetCoreID ());
      }

  };

/*
 * webClient function doesn't really belong to webServer, but it may came handy every now and then
 */

  String webClient (char *serverIP, int serverPort, unsigned int timeOutMillis, String httpRequest) {
    if (getWiFiMode () == WIFI_OFF) {
      webDmesg ("Could not start webClient since there is no network.");
      return "";
    }

    char buffer [256]; *buffer = 0; // reserve some space to hold the response
    String retVal = ""; // place for response
    // create non-threaded TCP client instance
    TcpClient myNonThreadedClient (serverIP, serverPort, timeOutMillis);
    // get reference to TCP connection. Before non-threaded constructor of TcpClient returns the connection is established if this is possible
    TcpConnection *myConnection = myNonThreadedClient.connection ();
    // test if connection is established
    if (myConnection) {
      // Serial.printf ("[%10lu] [webClient] connected.\n", millis ());
      httpRequest += " \r\n\r\n"; // make sure HTTP request ends properly
      /* int sentTotal = */ myConnection->sendData (httpRequest); // send HTTP request
      // Serial.printf ("[%10lu] [webClient] sent %i bytes.\n", millis (), sentTotal);
      // read response in a loop untill 0 bytes arrive - this is a sign that connection has ended
      // if the response is short enough it will normally arrive in one data block although
      // TCP does not guarantee that it would
      int receivedTotal = 0;
      while (int received = myConnection->recvData (buffer, sizeof (buffer) - 1)) {
        receivedTotal += received;
        buffer [received] = 0; // mark the end of the string we have just read
        retVal += String (buffer);
        // Serial.printf ("[%10lu] [webClient] received %i bytes.\n", millis (), received);
        // search buffer for content-lenght: to check if the whole reply has arrived against receivedTotal
        char *s = (char *) retVal.c_str ();
        char *c = stristr (s, (char *) "CONTENT-LENGTH:");
        if (c) {
          unsigned long ul;
          if (sscanf (c + 15, "%lu", &ul) == 1) {
            // Serial.printf ("[%10lu] [webClient] content-length %i, receivedTotal %i.\n", millis (), ul, receivedTotal);
            if ((c = strstr (c + 15, "\r\n\r\n")))
              if (receivedTotal == ul + c - s + 4) {
                // Serial.printf ("[%10lu] [webClient] whole HTTP response of content-length %i bytes and total length of %i bytes has arrived.\n", millis (), ul, retVal.length ());
                return retVal; // the response is complete
              }
          }
        }
      }
      if (receivedTotal) webDmesg ("[webClient] error in HTTP response regarding content-length.");
      else               webDmesg ("[webClient] time-out.");
    } else {
      webDmesg ("[webClient] unable to connect.");
    }
    return ""; // response arrived, it may evend be OK but it doesn't match content-length field
  }

  #define webClientCallIP webClient

  // webClientCallMAC works like webClient with exception that it calls station connected to AP network interface by its
  // MAC address instead of IP (please note that only stations connected to AP are addressed this way and not the
  // devices that could be addressed by their MAC addresses through router - meaning through STA interface)
  // since we do not have 100 % influence to which connecting MAC addresses which IP numbers are assigned
  // this may be more reliable way to adress connected stations in some cases (for example if other ESPs connect
  // to this ESP and you want to send a request to specitif ESP regardles which IP it has been assigned
  // Thanks to: https://techtutorialsx.com/2019/09/22/esp32-arduino-soft-ap-obtaining-ip-address-of-connected-stations/

  String MacAddressAsString (byte *MacAddress, byte addressLength);
  String inet_ntos (ip4_addr_t addr);

  #include <esp_wifi.h>

  String webClientCallMAC (char *serverMAC, int serverPort, unsigned int timeOutMillis, String httpRequest) {
    if (getWiFiMode () == WIFI_OFF) {
      webDmesg ("Could not start webClient since there is no network.");
      return "";
    }

    // scan through list of connected stations
    wifi_sta_list_t wifi_sta_list = {};
    tcpip_adapter_sta_list_t adapter_sta_list = {};
    esp_wifi_ap_get_sta_list (&wifi_sta_list);
    tcpip_adapter_get_sta_list (&wifi_sta_list, &adapter_sta_list);
    for (int i = 0; i < adapter_sta_list.num; i++) {
      tcpip_adapter_sta_info_t station = adapter_sta_list.sta [i];
      // call webClient if MAC address and its corresponding IP is found
      if (MacAddressAsString ((byte *) &station.mac, 6) == String (serverMAC)) {
        return webClientCallIP ((char *) inet_ntos (station.ip).c_str (), serverPort, timeOutMillis, httpRequest);
      }
    }
    webDmesg ("[webClient] device " + String (serverMAC) + " is not connected to AP.");
    return "";
  }

#endif
