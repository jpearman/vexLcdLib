/*-----------------------------------------------------------------------------*/
/*                                                                             */
/*                        Copyright (c) James Pearman                          */
/*                                   2013                                      */
/*                            All Rights Reserved                              */
/*                                                                             */
/*-----------------------------------------------------------------------------*/
/*                                                                             */
/*    Module:     lcdLib.c                                                     */
/*    Author:     James Pearman                                                */
/*    Created:    17 October 2013                                              */
/*                                                                             */
/*    Revisions:                                                               */
/*                V1.00  17 Oct 2013 - Initial release                         */
/*-----------------------------------------------------------------------------*/
/*                                                                             */
/*    The author is supplying this software for use with the VEX cortex        */
/*    control system. This file can be freely distributed and teams are        */
/*    authorized to freely use this program , however, it is requested that    */
/*    improvements or additions be shared with the Vex community via the vex   */
/*    forum.  Please acknowledge the work of the authors when appropriate.     */
/*    Thanks.                                                                  */
/*                                                                             */
/*    Licensed under the Apache License, Version 2.0 (the "License");          */
/*    you may not use this file except in compliance with the License.         */
/*    You may obtain a copy of the License at                                  */
/*                                                                             */
/*      http://www.apache.org/licenses/LICENSE-2.0                             */
/*                                                                             */
/*    Unless required by applicable law or agreed to in writing, software      */
/*    distributed under the License is distributed on an "AS IS" BASIS,        */
/*    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*    See the License for the specific language governing permissions and      */
/*    limitations under the License.                                           */
/*                                                                             */
/*    The author can be contacted on the vex forums as jpearman                */
/*    or electronic mail using jbpearman_at_mac_dot_com                        */
/*    Mentor for team 8888 RoboLancers, Pasadena CA.                           */
/*                                                                             */
/*-----------------------------------------------------------------------------*/
/*    A port back of some of the ConVEX LCD code which was based on the        */
/*    example lcd code I wrote for ROBOTC back in 2011                         */
/*-----------------------------------------------------------------------------*/

#ifndef _LCD_LIB__
#define _LCD_LIB__

// hide globals
#pragma systemFile

#define VEX_LCD_BACKLIGHT   0x01    ///< bit for turning backlight on

/*-----------------------------------------------------------------------------*/
/** @brief      Holds all data for a single LCD display                        */
/*-----------------------------------------------------------------------------*/

typedef struct _LcdData {
    short         port;       ///< The uart port
    unsigned char flags;      ///< flags, currently backlight on/off
    short         buttons;    ///< button data read from lcd
    char          line1[20];  ///< stirage for line 1 lcd data
    char          line2[20];  ///< stirage for line 2 lcd data
    char          txbuf[32];  ///< the serial transmit buffer
    char          rxbuf[16];  ///< the serial receive buffer
    } LcdData;

// Only one LCD available to this code
static  LcdData vexLcdData;

// A global into which we update the buttons to make this work like
// Standard ROBOTC code
TControllerButtons  nLCDButtons2;

// prototypes for the task
void    vexLcdSendMessage( LcdData *lcd, short line );
void    vexLcdCheckReceiveMessage( LcdData *lcd );

/*-----------------------------------------------------------------------------*/
/** @brief      task sending data constantly to the LCD                        */
/*-----------------------------------------------------------------------------*/

task
vexLcdPollTask()
{
    while(1)
        {
        vexLcdSendMessage( &vexLcdData, 0 );
        wait1Msec(25);
        vexLcdCheckReceiveMessage( &vexLcdData );
        wait1Msec(10);
        vexLcdSendMessage( &vexLcdData, 1 );
        wait1Msec(25);
        vexLcdCheckReceiveMessage( &vexLcdData );
        wait1Msec(10);
        }
}

/*-----------------------------------------------------------------------------*/
/** @brief      Initializa the second LCD                                      */
/** @param[in]  port The uart on which this LCD is installed                   */
/*-----------------------------------------------------------------------------*/

void
vexLcdInit( int port )
{
    // init the lcd structure
    vexLcdData.port    = port;
    vexLcdData.flags   = 0;
    vexLcdData.buttons = 0;

    // Init the baud rate
    setBaudRate( port, baudRate19200 );

    // start task slightly higher priority then default
    StartTask( vexLcdPollTask, kDefaultTaskPriority + 1 );
}

/*-----------------------------------------------------------------------------*/
/** @brief      Show text from beginning of an LCD line                        */
/** @param[in]  line The line to display the text on, 0 or 1                   */
/** @param[in]  buf Pointer to char buffer with test to display                */
/*-----------------------------------------------------------------------------*/

void
vexLcdSet( short line, char *buf )
{
    if( !line  )
        strncpy( vexLcdData.line1, buf, 16 );
    else
        strncpy( vexLcdData.line2, buf, 16 );
}

/*-----------------------------------------------------------------------------*/
/** @brief      Show text on LCD line and column                               */
/** @param[in]  line The line to display the text on, 0 or 1                   */
/** @param[in]  col The position at which to start displaying text             */
/** @param[in]  buf Pointer to char buffer with test to display                */
/*-----------------------------------------------------------------------------*/

void
vexLcdSetAt( short line, short col, char *buf )
{
    if( (col < 0) || (col > 15 ) )
        return;

    if( !line  )
        strncpy( &vexLcdData.line1[col], buf, 16-col );
    else
        strncpy( &vexLcdData.line2[col], buf, 16-col );
}

/*-----------------------------------------------------------------------------*/
/** @brief      Clear a given lcd display line                                 */
/** @param[in]  line The line to display the text on, 0 or 1                   */
/*-----------------------------------------------------------------------------*/

void
vexLcdClearLine( short line )
{
    vexLcdSet( line, "                " );
}

/*-----------------------------------------------------------------------------*/
/** @brief      Turn on or off the backlight                                   */
/** @param[in]  value 1 for backlight on, 0 for off                            */
/*-----------------------------------------------------------------------------*/

void
vexLcdBacklight( short value )
{
    vexLcdData.flags = (value == 1) ?  vexLcdData.flags | VEX_LCD_BACKLIGHT : vexLcdData.flags & ~VEX_LCD_BACKLIGHT;
}

/*-----------------------------------------------------------------------------*/
/** @brief      Get the current button status                                  */
/** @returns    The button (or buttons) pressed                                */
/*-----------------------------------------------------------------------------*/

TControllerButtons
vexLcdButtonGet( )
{
    return( (TControllerButtons)vexLcdData.buttons );
}

/*-----------------------------------------------------------------------------*/
/** @brief      Form and send message to LCD                                   */
/** @param[in]  lcd Pointer to LcdData structure                               */
/** @param[in]  line The line to display the text on, 0 or 1                   */
/** @note       Internal driver use only                                       */
/*-----------------------------------------------------------------------------*/

void
vexLcdSendMessage( LcdData *lcd, short line )
{
    // create message
    short  i, cs;
    short  timeout;

    // bounds check line variable
    if( ( line < 0 ) || (line > 1) )
        return;

    // Header for LCD communication
    lcd->txbuf[0] = 0xAA;
    lcd->txbuf[1] = 0x55;
    lcd->txbuf[2] = 0x1e;
    lcd->txbuf[3] = 0x12;
    lcd->txbuf[4] = (lcd->flags & VEX_LCD_BACKLIGHT) ? 0x02 + line : line;

    // fill with spaces
    for(i=0;i<16;i++)
        lcd->txbuf[ 5+i ] = 0x20;

    // Copy data transmit buffer
    if(!line) {
        for(i=0;(i<16)&&(lcd->line1[i]!=0);i++)
            lcd->txbuf[ 5+i ] = lcd->line1[i];
        }
    else {
        for(i=0;(i<16)&&(lcd->line2[i]!=0);i++)
            lcd->txbuf[ 5+i ] = lcd->line2[i];
        }

    // calculate checksum
    cs = 0;
    for(i=4;i<21;i++)
         cs = cs + lcd->txbuf[i];

    lcd->txbuf[21] = 0x100 - cs;

    // send to port
    for(i=0;i<22;i++)
        {
        // wait for transmit complete or timeout
        // Under normal circumstances this should not be needed, there is
        // a transmit buffer larger than most messages
        // timeout here is set at about 200uS (from experimentation)
        timeout = 0;
        while( !bXmitComplete(lcd->port) && (timeout < 20) )
            timeout++;

        // send char
        sendChar( lcd->port, lcd->txbuf[i] );
        }
}

/*-----------------------------------------------------------------------------*/
/** @brief      Check for receive data and store                               */
/** @param[in]  lcd Pointer to LcdData structure                               */
/** @note       Internal driver use only                                       */
/*-----------------------------------------------------------------------------*/

void
vexLcdCheckReceiveMessage( LcdData *lcd )
{
    short i;
    short c;

    // any characters ?
    c = getChar(lcd->port);
    if( c < 0 )
        return;

    // read up to 16 bytes from serial port
    lcd->rxbuf[0] = c;
    for(i=1;i<16;i++)
        {
        c = getChar(lcd->port);
        if( c >= 0 )
            lcd->rxbuf[i] = c;
        else
            break;
        }

    // 6 chars ?
    if( i == 6 )
        {
        // lcd message ?
        if( (lcd->rxbuf[0] == 0xAA) && (lcd->rxbuf[1] == 0x55) && (lcd->rxbuf[2] == 0x16))
            {
            // verify checksum
            if( !((lcd->rxbuf[4] + lcd->rxbuf[5]) & 0xFF) )
                {
                lcd->buttons = lcd->rxbuf[4];
                nLCDButtons2 = (TControllerButtons)lcd->buttons;
                }
            }
        }

    // Flush any remaining characters
    while(getChar(lcd->port) >= 0)
        i++; // to remove warning
}

#endif /* _LCD_LIB_ */
