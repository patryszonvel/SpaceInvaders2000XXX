//*****************************************************************************
//
// synth.c - A single-octave synthesizer to demonstrate the use of the sound
//           driver.
//
// Copyright (c) 2013 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.0.1.11577 of the DK-TM4C129X Firmware Package.
//
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "grlib/grlib.h"
#include "grlib/widget.h"
#include "utils/sine.h"
#include "drivers/frame.h"
#include "drivers/kentec320x240x16_ssd2119.h"
#include "drivers/pinout.h"
#include "drivers/sound.h"
#include "drivers/touch.h"
#include "inc/hw_ints.h"

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>Synthesizer (synth)</h1>
//!
//! This application provides a single-octave synthesizer utilizing the touch
//! screen as a virtual piano keyboard.  The notes played on the virtual piano
//! are played out via the on-board speaker.
//
//*****************************************************************************

//*****************************************************************************
//
// The colors used to draw the white keys.
//
//*****************************************************************************
#define ClrWhiteKey             0xcfcfcf
#define ClrWhiteBright          0xffffff
#define ClrWhiteDim             0x9f9f9f

//*****************************************************************************
//
// The colors used to draw the black keys.
//
//*****************************************************************************
#define ClrBlackKey             0x000000
#define ClrBlackBright          0x606060
#define ClrBlackDim             0x303030

//*****************************************************************************
//
// The color used to draw a pressed key.
//
//*****************************************************************************
#define ClrPressed              0x3f3fbf

//*****************************************************************************
//
// The width and height of the white keys.  The width should be an even number.
//
//*****************************************************************************
#define WHITE_WIDTH             36
#define WHITE_HEIGHT            190

//*****************************************************************************
//
// The width and height of the black keys.  The width should be a multiple of
// four.
//
//*****************************************************************************
#define BLACK_WIDTH             26
#define BLACK_HEIGHT            110

//*****************************************************************************
//
// The screen offset of the upper left hand corner of the keyboard.
//
//*****************************************************************************
#define X_OFFSET                20
#define Y_OFFSET                32

//*****************************************************************************

//*****************************************************************************
//
// Values that can be passed to most of the timer APIs as the ui32Timer
// parameter.
//
//*****************************************************************************
#define TIMER_A                 0x000000ff  // Timer A

#define TIMER_TIMA_TIMEOUT      0x00000001  // TimerA time out interrupt

#define TIMER_CFG_PERIODIC       0x00000022  // Full-width periodic timer

#define TIMER_TIMA_TIMEOUT      0x00000001  // TimerA time out interrupt

//
// A structure that describes a key on the keyboard.
//
//*****************************************************************************
typedef struct
{
    //
    // The outline of the key.
    //
    tRectangle sOutline;

    //
    // The first/top fill for the key.
    //
    tRectangle sFill1;

    //
    // The second/bottom fill for the key (not used for black keys).
    //
    tRectangle sFill2;

}
tKey;

uint32_t g_ui32Flags;
tContext sContext;
int i=0;
tRectangle tab[] = {
    X_OFFSET + 2,
    Y_OFFSET + 2,
    X_OFFSET + 5,
    Y_OFFSET + 5
};
void Timer0IntHandler(void)
{
    //
    // Clear the timer interrupt.
    //
    ROM_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    //
    // Toggle the flag for the first timer.
    //
    HWREGBITW(&g_ui32Flags, 0) ^= 1;

    //
    // Update the interrupt status on the display.
    //
    ROM_IntMasterDisable();
    i++;
//    GrStringDraw(&g_sContext, (HWREGBITW(&g_ui32Flags, 0) ? "1" : "0"), -1,
//                 195, 108, 1);


    tab[0].i16YMax= i;
    GrRectFill(&sContext, &(tab[0]));
    ROM_IntMasterEnable();
}





static const tKey g_object[] =
{
    //
    // C4
    //
    {
        //
        // Outline
        //
        {
            X_OFFSET,
            Y_OFFSET,
            X_OFFSET + WHITE_WIDTH - 1,
            Y_OFFSET + WHITE_HEIGHT - 1
        },

        //
        // Top fill
        //
        {
            X_OFFSET + 2,
            Y_OFFSET + 2,
            X_OFFSET + WHITE_WIDTH - ((BLACK_WIDTH * 3) / 4) - 1,
            Y_OFFSET + BLACK_HEIGHT - 1
        },

        //
        // Bottom fill
        //
        {
            X_OFFSET + 2,
            Y_OFFSET + BLACK_HEIGHT,
            X_OFFSET + WHITE_WIDTH - 3,
            Y_OFFSET + WHITE_HEIGHT - 3
        }

    }
};


//*****************************************************************************
//
// The number of black keys.
//
//*****************************************************************************
#define NUM_BLACK_KEYS          (sizeof(g_psBlackKeys) /                      \
                                 sizeof(g_psBlackKeys[0]))

//*****************************************************************************
//
// The buffer used to store the synthesized waveform that is to be played.  The
// buffer size must be a power of 2 less than or equal to 2048.
//
//*****************************************************************************
#define AUDIO_SIZE              2048
static int16_t g_pi16AudioBuffer[AUDIO_SIZE];

//*****************************************************************************
//
// A set of flags that indicate the current state of the application.
//
//*****************************************************************************
static uint32_t g_ui32Flags;
#define FLAG_PING               0           // The "ping" half of the sound
                                            // buffer needs to be filled
#define FLAG_PONG               1           // The "pong" half of the sound
                                            // buffer needs to be filled

//*****************************************************************************
//
// The key that is currently pressed.
//
//*****************************************************************************
//uint32_t g_ui32Key = NUM_WHITE_KEYS + NUM_BLACK_KEYS;

//*****************************************************************************
//
// The position within the waveform of the currently playing key.
//
//*****************************************************************************
uint32_t g_ui32AudioPos;

//*****************************************************************************
//
// The step rate of the waveform for the currently playing key.
//
//*****************************************************************************
uint32_t g_ui32AudioStep;

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// Fills in one of the white keys with the given color.
//
//*****************************************************************************
//static inline void
//FillWhiteKey(tContext *pContext, uint32_t ui32Key, uint32_t ui32Color)
//{
//    //
//    // Select the specified color.
//    //
//    GrContextForegroundSet(pContext, ui32Color);
//
//    //
//    // Fill in the upper and lower portions of the white key.
//    //
///*    GrRectFill(pContext, &(g_psWhiteKeys[ui32Key].sFill1));
//    GrRectFill(pContext, &(g_psWhiteKeys[ui32Key].sFill2));*/
//}


////*****************************************************************************
////
//// Fills in one of the black keys with the given color.
////
////*****************************************************************************
//static inline void
//FillBlackKey(tContext *pContext, uint32_t ui32Key, uint32_t ui32Color)
//{
//    //
//    // Select the specified color.
//    //
//    GrContextForegroundSet(pContext, ui32Color);
//
//    //
//    // FIll in the black key.
//    //
//   // GrRectFill(pContext, &(g_psBlackKeys[ui32Key].sFill1));
//}

void
SoundCallback(uint32_t ui32Half)
{
    //
    // See which half of the sound buffer has been played.
    //
    if(ui32Half == 0)
    {
        //
        // The first half of the sound buffer needs to be filled.
        //
        HWREGBITW(&g_ui32Flags, FLAG_PING) = 1;
    }
    else
    {
        //
        // The second half of the sound buffer needs to be filled.
        //
        HWREGBITW(&g_ui32Flags, FLAG_PONG) = 1;
    }
}


//*****************************************************************************
//
// This application performs simple audio synthesis and playback based on the
// keys pressed on the touch screen virtual piano keyboard.
//
//*****************************************************************************
int
main(void)
{
    uint32_t ui32SysClock;

    //
    // Run from the PLL at 120 MHz.
    //
    ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                       SYSCTL_OSC_MAIN |
                                       SYSCTL_USE_PLL |
                                       SYSCTL_CFG_VCO_480), 120000000);

    //
    // Configure the device pins.
    //
    PinoutSet();

    //
    // Initialize the display driver.
    //
    Kentec320x240x16_SSD2119Init(ui32SysClock);

    //
    // Initialize the graphics context.
    //
    GrContextInit(&sContext, &g_sKentec320x240x16_SSD2119);

    //
    // Draw the application frame.
    //
    FrameDraw(&sContext, "");

    //
    // Draw the keys on the virtual piano keyboard.
    //
  //  DrawWhiteKeys(&sContext);
  //  DrawBlackKeys(&sContext);

    //
    // Initialize the touch screen driver.
    //
    TouchScreenInit(ui32SysClock);
    //TouchScreenCallbackSet(TouchCallback);

    //
    // Initialize the sound driver.
    //
    SoundInit(ui32SysClock);
    SoundVolumeSet(128);
    SoundStart(g_pi16AudioBuffer, AUDIO_SIZE, 64000, SoundCallback);

    //
    // Default the old and new key to not pressed so that the first key press
    // will be properly drawn on the keyboard.
    //
    //ui32OldKey = NUM_WHITE_KEYS + NUM_BLACK_KEYS;
    //ui32NewKey = NUM_WHITE_KEYS + NUM_BLACK_KEYS;

    //
    // Enable the peripherals used by this example.
    //
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);


    //
    // Configure the two 32-bit periodic timers.
    //
    ROM_TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    ROM_TimerLoadSet(TIMER0_BASE, TIMER_A, ui32SysClock/2);

    //
    // Setup the interrupts for the timer timeouts.
    //
    ROM_IntEnable(INT_TIMER0A);
    ROM_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    //
    // Enable the timers.
    //
    ROM_TimerEnable(TIMER0_BASE, TIMER_A);






    //
    // Loop forever.
    //
    while(1)
    {
//        //
//        // See if the first half of the sound buffer needs to be filled.
//        //
//        if(HWREGBITW(&g_ui32Flags, FLAG_PING) == 1)
//        {
//            //
//            // Synthesize new audio into the first half of the sound buffer.
//            //
//            ui32NewKey = GenerateAudio(g_pi16AudioBuffer, AUDIO_SIZE / 2);
//
//            //
//            // Clear the flag for the first half of the sound buffer.
//            //
//            HWREGBITW(&g_ui32Flags, FLAG_PING) = 0;
//        }
//
//        //
//        // See if the second half of the sound buffer needs to be filled.
//        //
//        if(HWREGBITW(&g_ui32Flags, FLAG_PONG) == 1)
//        {
//            //
//            // Synthesize new audio into the second half of the sound buffer.
//            //
//            ui32NewKey = GenerateAudio(g_pi16AudioBuffer + (AUDIO_SIZE / 2),
//                                       AUDIO_SIZE / 2);
//
//            //
//            // Clear the flag for the second half of the sound buffer.
//            //
//            HWREGBITW(&g_ui32Flags, FLAG_PONG) = 0;
//        }
//
//        //
//        // See if a different key has been pressed.
//        //
//        if(ui32OldKey != ui32NewKey)
//        {
//            //
//            // See if the old key was a white key.
//            //
//            if(ui32OldKey < NUM_WHITE_KEYS)
//            {
//                //
//                // Redraw the face of the white key so that it no longer shows
//                // as being pressed.
//                //
//                FillWhiteKey(&sContext, ui32OldKey, ClrWhiteKey);
//            }
//
//            //
//            // See if the old key was a black key.
//            //
//            else if(ui32OldKey < (NUM_WHITE_KEYS + NUM_BLACK_KEYS))
//            {
//                //
//                // Redraw the face of the black key so that it no longer shows
//                // as being pressed.
//                //
//                FillBlackKey(&sContext, ui32OldKey - NUM_WHITE_KEYS,
//                             ClrBlackKey);
//            }
//
//            //
//            // See if the new key is a white key.
//            //
//            if(ui32NewKey < NUM_WHITE_KEYS)
//            {
//                //
//                // Redraw the face of the white key so that it is shown as
//                // being pressed.
//                //
//                FillWhiteKey(&sContext, ui32NewKey, ClrPressed);
//            }
//
//            //
//            // See if the new key is a black key.
//            //
//            else if(ui32NewKey < (NUM_WHITE_KEYS + NUM_BLACK_KEYS))
//            {
//                //
//                // Redraw the face of the black key so that it is shown as
//                // being pressed.
//                //
//                FillBlackKey(&sContext, ui32NewKey - NUM_WHITE_KEYS,
//                             ClrPressed);
//            }
//
//            //
//            // Save the new key as the old key.
//            //
//            ui32OldKey = ui32NewKey;
//        }

    }

}
