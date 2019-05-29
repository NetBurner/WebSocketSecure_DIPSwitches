/* Revision: 0.0.0 */

/******************************************************************************
 * Copyright 1998-2019 NetBurner, Inc.  ALL RIGHTS RESERVED
 *
 *    Permission is hereby granted to purchasers of NetBurner Hardware to use or
 *    modify this computer program for any use as long as the resultant program
 *    is only executed on NetBurner provided hardware.
 *
 *    No other rights to use this program or its derivatives in part or in
 *    whole are granted.
 *
 *    It may be possible to license this or other NetBurner software for use on
 *    non-NetBurner Hardware. Contact sales@Netburner.com for more information.
 *
 *    NetBurner makes no representation or warranties with respect to the
 *    performance of this computer program, and specifically disclaims any
 *    responsibility for any damages, special or consequential, connected with
 *    the use of this program.
 *
 * NetBurner
 * 5405 Morehouse Dr.
 * San Diego, CA 92121
 * www.netburner.com
 ******************************************************************************/

/*******************************************************************************
 * Real-Time DIP Switch States via WebSocketSecure
 *
 * NOTE: This application was built to run on NNDK 3.x
 *
 * This program illustrates how to use the NetBurner WebSocket class to display
 * the state of the DIP swithes on the MOD-DEV-70CR in real-time on a webpage.
 * That means as soon as you flip the DIP switches on the development board,
 * the state will change on the webpage without having to refresh the page.
 * This program continously polls the DIP switches and sends the state of the DIP
 * switches via a websocket to the client. It's not the most effecient implementation
 * when considering CPU utilization but this app demonstrates the capabilities of
 * a WebSocket to allow the server (NetBurner device) to send the state of a variable
 * to the client (webpage) with low latency and minimal packet size. This app
 * also illustrates how to use the NetBurner JSON library to build and send JSON
 * objects from the NetBurner device to the client. In this case, JSON objects are
 * used to pass the state of the DIP switches to the webpage. You will need to access
 * the secure version of the webpage using the "https://" protocol URL to use the
 * WebSocketSecure protocol. You're web browser will likely require you to explicitly allow
 * the use of self-signed certificates which are used in this example for the purpose
 * of easily demonstrating HTTPS functionality. For production code, we highly
 * recommend obtaining a certificate signed by a trusted Certificate Authority and
 * implementing that into your webserver.
 ******************************************************************************/

#include <init.h>
#include <nbrtos.h>
#include <system.h>
#include <predef.h>
#include <stdio.h>
#include <ctype.h>
#include <startnet.h>
#include <dhcpclient.h>
#include <smarttrap.h>
#include <constants.h>
#include <tcp.h>
#include <websockets.h>
#include <string.h>
#include <json_lexer.h>
#include <pins.h>
#include "SimpleAD.h"
#ifdef SSL_TLS_SUPPORT
#include <crypto/ssl.h>
#endif


extern "C"
{
    void UserMain(void *pd);
}

const char *AppName = "Real-Time DIP Switch State via WebSocketSecure";

#define INCOMING_BUF_SIZE 8192
#define REPORT_BUF_SIZE 512
#define NUM_LEDS 8
#define NUM_SWITCHES 8
#define STATE_BUF_SIZE 8

extern http_wshandler *TheWSHandler;
int ws_fd = -1;
OS_SEM SockReadySem;
char ReportBuffer[REPORT_BUF_SIZE];
char dipStates[NUM_SWITCHES][STATE_BUF_SIZE];
char IncomingBuffer[INCOMING_BUF_SIZE];
bool bFirstRun = true;

#if (defined MCF5441X)
#define Pins J2
#define CONFIG_PIN_OUTPUT PIN_GPIO
#elif (defined MODM7AE70)
#define Pins P2
#define CONFIG_PIN_OUTPUT PinIO::PIN_FN_OUT
#else
#define Pins P2
#define CONFIG_PIN_OUTPUT PIN_GPIO
#endif

/*-------------------------------------------------------------------
 * On the MOD-DEV-70, the LEDs are on J2 connector pins:
 * 15, 16, 31, 23, 37, 19, 20, 24 (in that order)
 * -----------------------------------------------------------------*/
void WriteLeds(int ledNum, bool ledValue)
{
    static bool bLedGpioInit = FALSE;
    const uint8_t PinNumber[8] = {15, 16, 31, 23, 37, 19, 20, 24};
    static uint8_t ledMask = 0x00;   // LED mask stores the state of all 8 LEDs
    uint8_t BitMask = 0x01;

    if (!bLedGpioInit)
    {
        for (int i = 0; i < 8; i++)
        {
            Pins[PinNumber[i]].function(CONFIG_PIN_OUTPUT);
        }
        bLedGpioInit = TRUE;
    }

    if (ledValue)
    {
        // LED on
        ledMask |= (0x01 << (ledNum));
    }
    else
    {
        // LED off
        ledMask &= ~(0x01 << (ledNum));
    }

    for (int i = 0; i < 8; i++)
    {
        if ((ledMask & BitMask) == 0)
        {
            Pins[PinNumber[i]] = 1;   // LEDs tied to 3.3V, so 1 = off
        }
        else
        {
            Pins[PinNumber[i]] = 0;
        }

        BitMask <<= 1;
    }
}

static void ParseInputForLedMask(char *buf, int &ledNum, bool &ledValue)
{
    ParsedJsonDataSet JsonInObject(buf);
    const char *pJsonElementName;
    int tempLedValue = 0;

    /* Print the buffer received to serial  */
    JsonInObject.PrintObject(true);

    /* navigate to the first element name */
    JsonInObject.GetFirst();
    JsonInObject.GetNextNameInCurrentObject();

    /* Get a pointer to the first element's name */
    pJsonElementName = JsonInObject.CurrentName();

    if(pJsonElementName != nullptr)
    {
        /* Scan the element name for the LED number. Store the number value */
        sscanf(pJsonElementName, "ledcb%d\"", &ledNum);
        ledNum -= 1;   // offset the LED number to index in array

        /* Get the boolean value of the JSON element */
        ledValue = JsonInObject.FindFullNamePermissiveBoolean(pJsonElementName);
    }
}

static int ConsumeSocket(char c, bool &inStr, bool &strEscape)
{
    switch (c)
    {
        case '\\':
            if (!inStr)
            {
                return 0;   // no change to openCount
            }
            strEscape = !strEscape;
            break;
        case '"':
            if (!strEscape) { inStr = !inStr; }
            else
            {
                strEscape = false;
            }
            break;
        case '{':
            if (!strEscape) { return 1; }
            else
            {
                strEscape = false;
            }
            break;
        case '}':
            if (!strEscape) { return -1; }
            else
            {
                strEscape = false;
            }
            break;
        default:
            if (strEscape) { strEscape = false; }
            break;
    }

    return 0;
}

void InputTask(void *pd)
{
    SMPoolPtr pp;
    fd_set read_fds;
    fd_set error_fds;
    int index = 0, openCount = 0;
    bool inString = false, strEscape = false;

    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);

    while (1)
    {
        if (ws_fd > 0)
        {
            FD_SET(ws_fd, &read_fds);
            FD_SET(ws_fd, &error_fds);
            if (select(1, &read_fds, NULL, &error_fds, 0))
            {
                if (FD_ISSET(ws_fd, &error_fds))
                {
                    close(ws_fd);
                    ws_fd = -1;
                    iprintf("WebSocket Closed\r\n");
                }
                else
                {
                    if (FD_ISSET(ws_fd, &read_fds))
                    {
                        while (dataavail(ws_fd) && (index < INCOMING_BUF_SIZE))
                        {
                            read(ws_fd, IncomingBuffer + index, 1);
                            openCount += ConsumeSocket(IncomingBuffer[index], inString, strEscape);
                            index++;
                            if (openCount == 0) { break; }
                        }
                    }
                    if (openCount == 0)
                    {
                        int ledNum;
                        bool ledValue;
                        IncomingBuffer[index] = '\0';
                        OSTimeDly(4);
                        ParseInputForLedMask(IncomingBuffer, ledNum, ledValue);
                        WriteLeds(ledNum, ledValue);
                        index = 0;
                    }
                }
            }
        }
        else
        {
            OSSemPend(&SockReadySem, 0);
        }
    }
}

void SendJSONReport(int ws_fd)
{
    SMPoolPtr pq;
    ParsedJsonDataSet JsonOutObject;

    // Assemble JSON object
    JsonOutObject.StartBuilding();
    JsonOutObject.AddObjectStart("dipSwitches");
    JsonOutObject.Add("dip1", dipStates[0]);
    JsonOutObject.Add("dip2", dipStates[1]);
    JsonOutObject.Add("dip3", dipStates[2]);
    JsonOutObject.Add("dip4", dipStates[3]);
    JsonOutObject.Add("dip5", dipStates[4]);
    JsonOutObject.Add("dip6", dipStates[5]);
    JsonOutObject.Add("dip7", dipStates[6]);
    JsonOutObject.Add("dip8", dipStates[7]);
    JsonOutObject.EndObject();
    JsonOutObject.DoneBuilding();

    // If you would like to print the JSON object to serial to see the format, uncomment the next line
    // JsonOutObject.PrintObject(true);

    // Print JSON object to a buffer and write the buffer to the WebSocket file descriptor
    int dataLen = JsonOutObject.PrintObjectToBuffer(ReportBuffer, REPORT_BUF_SIZE);
    writeall(ws_fd, ReportBuffer, dataLen - 1);
}

int MyDoWSUpgrade(HTTP_Request *req, int sock, PSTR url, PSTR rxb)
{
    iprintf("[MyDoWSUpgrade]\r\n");

    if (httpstricmp(url, "/INDEX.HTML"))
    {
        if (ws_fd < 0)
        {
            // if(IsSSLfd(sock)) { SSL_clrsockoption(sock, SO_NOPUSH); }
            int rv = WSUpgrade(req, sock);
            if (rv >= 0)
            {
                bFirstRun = true;
                ws_fd = rv;
                iprintf("[MyDoWSUpgrade] ws_fd: %d \r\n", ws_fd);
                NB::WebSocket::ws_setoption(ws_fd, WS_SO_TEXT);
                OSSemPost(&SockReadySem);
                return 2;
            }
            else
            {
                return 0;
            }
        }
    }
    else
    {
        iprintf("Error parsing URL: %s\r\n", url);
    }

    NotFoundResponse(sock, url);
    return 0;
}

/*-------------------------------------------------------------------
 * On the MOD-DEV-70, the switches are on J2 connector pins:
 * 8, 6, 7, 10, 9, 11, 12, 13 (in that order). These signals are
 * Analog to Digital, not GPIO, so we read the analog value and
 * determine the switch position from it. This function is exclusive to
 * the MOD5441X and NANO54415.
 * ------------------------------------------------------------------*/
uint8_t ReadSwitch()
{
    static BOOL bReadSwitchInit = FALSE;
    const uint8_t PinNumber[8] = {7, 6, 5, 3, 4, 1, 0, 2};   // map J2 conn signals pins to A/D number 0-7

    uint8_t BitMask = 0;

    if (!bReadSwitchInit)
    {
        InitSingleEndAD();
        bReadSwitchInit = TRUE;
    }

    StartAD();
    while (!ADDone())
        asm("nop");

    for (int BitPos = 0x01, i = 0; BitPos < 256; BitPos <<= 1, i++)
    {
        // if greater than half the 16-bit range, consider it logic high
        if (GetADResult(PinNumber[i]) > (0x7FFF / 2)) BitMask |= (uint8_t)(0xFF & BitPos);
    }

    return BitMask;
}

/*-------------------------------------------------------------------
 This function gets the state of the DIP Switches on the MOD-DEV-70 carrier board.
 The state of each switch is represented by a bit in a 8-bit
 register. A bit value of 0 = on, and 1 = off.
 returns true if the state of the switches has changed, false otherwise.
  ------------------------------------------------------------------*/
bool DoSwitches()
{
    static uint8_t previousStates = 0;
    static uint8_t currentStates = 0;

    // Get the value of the switches
#if (defined MCF5441X)
    currentStates = ReadSwitch();
#else
    currentStates = getdipsw();
#endif

    if (previousStates == currentStates && !bFirstRun) { return false; }

    memset(dipStates, 0, NUM_SWITCHES * STATE_BUF_SIZE);

    // Write out each row of the table
    for (int i = 0; i < NUM_SWITCHES; i++)
    {
        if (currentStates & (0x01 << i))
        {
            // Switch is off
            strncpy(dipStates[i], "Off", STATE_BUF_SIZE);
        }
        else
        {
            // Switch is on
            strncpy(dipStates[i], "On", STATE_BUF_SIZE);
        }
    }

    bFirstRun = false;
    previousStates = currentStates;

    return true;
}

void UserMain(void *pd)
{
    init();
    OSSemInit(&SockReadySem, 0);
    StartHttps();
    TheWSHandler = MyDoWSUpgrade;
    OSSimpleTaskCreatewName(InputTask, MAIN_PRIO - 1, "Input Task");

#ifndef _DEBUG
    EnableSmartTraps();
#endif
    bool bValuesChanged = false;

    iprintf("Application started\n");

    while (1)
    {
        if (ws_fd > 0)
        {
            // Get the state of the DIP switches
            bValuesChanged = DoSwitches();

            if (bValuesChanged)
            {
                // Send the state of the DIP switches as a JSON blob via a WebSocket
                SendJSONReport(ws_fd);
            }
        }
        else
        {
            OSTimeDly(TICKS_PER_SECOND);
        }
    }
}
