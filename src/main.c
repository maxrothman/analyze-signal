/* Modified from... */

/* main.c - chromatic guitar tuner
 *
 * Copyright (C) 2012 by Bjorn Roche
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  This software is provided "as is" without express or
 * implied warranty.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include "libfft.h"

/* -- some basic parameters -- */
#define SAMPLE_RATE (44100)
//#define FFT_SIZE (8192)
#define FFT_SIZE (1024)
#define FFT_EXP_SIZE (10)     //13 for FFT_SIZE=8192. For each **2 you decrease FFT_SIZE, decrease this by 1

/* -- functions declared and used here -- */
void buildHammingWindow( float *window, int size );
void buildHanWindow( float *window, int size );
void applyWindow( float *window, float *data, int size );
//a must be of length 2, and b must be of length 3
void computeSecondOrderLowPassParameters( float srate, float f, float *a, float *b );
//mem must be of length 4.
float processSecondOrderFilter( float x, float *mem, float *a, float *b );
void signalHandler( int signum ) ;

static bool running = true;


/* -- main function -- */
int main( int argc, char **argv ) {
   float a[2], b[3], mem1[4], mem2[4];
   float data[FFT_SIZE];
   float datai[FFT_SIZE];
   float window[FFT_SIZE];
   float freqTable[FFT_SIZE];
   void * fft = NULL;
   struct sigaction action;

   // add signal listen so we know when to exit:
   action.sa_handler = signalHandler;
   sigemptyset (&action.sa_mask);
   action.sa_flags = 0;

   sigaction (SIGINT, &action, NULL);
   sigaction (SIGHUP, &action, NULL);
   sigaction (SIGTERM, &action, NULL);

   // build the window, fft, etc
/*
   buildHanWindow( window, 30 );
   for( int i=0; i<30; ++i ) {
      for( int j=0; j<window[i]*50; ++j )
         printf( "*" );
      printf("\n");
   }
   exit(0);
*/
   buildHanWindow( window, FFT_SIZE );
   fft = initfft( FFT_EXP_SIZE );
   computeSecondOrderLowPassParameters( SAMPLE_RATE, 330, a, b );
   mem1[0] = 0; mem1[1] = 0; mem1[2] = 0; mem1[3] = 0;
   mem2[0] = 0; mem2[1] = 0; mem2[2] = 0; mem2[3] = 0;
   
   //freq/note tables
   for( int i=0; i<FFT_SIZE; ++i ) {
      freqTable[i] = ( SAMPLE_RATE * i ) / (float) ( FFT_SIZE );
   }

   for( int i=0; i<127; ++i ) {
      float pitch = ( 440.0 / 32.0 ) * pow( 2, (i-9.0)/12.0 ) ;
      if( pitch > SAMPLE_RATE / 2.0 )
         break;
      //find the closest frequency using brute force.
      float min = 1000000000.0;
      int index = -1;
      for( int j=0; j<FFT_SIZE; ++j ) {
         if( fabsf( freqTable[j]-pitch ) < min ) {
             min = fabsf( freqTable[j]-pitch );
             index = j;
         }
      }
   }


   // this is the main loop where we listen to and
   // process audio.
   while( running )
   {
      // read a chunk of data from STDIN
      fread(&data, sizeof(float), FFT_SIZE, stdin);

      //get the amplitude
      // float rms = 0;
      // for (int i=0; i<FFT_SIZE; i++) {
      //    rms += data[i]*data[i];
      // }
      // rms = rms/FFT_SIZE;
      // rms = sqrt(rms);

      // low-pass
      //for( int i=0; i<FFT_SIZE; ++i )
      //   printf( "in %f\n", data[i] );
      for( int j=0; j<FFT_SIZE; ++j ) {
         data[j] = processSecondOrderFilter( data[j], mem1, a, b );
         data[j] = processSecondOrderFilter( data[j], mem2, a, b );
      }
      // window
      applyWindow( window, data, FFT_SIZE );

      // do the fft
      for( int j=0; j<FFT_SIZE; ++j )
         datai[j] = 0;
      applyfft( fft, data, datai, false );

      //find the peak
      float maxVal = -1;
      int maxIndex = -1;
      for( int j=0; j<FFT_SIZE/2; ++j ) {
         float v = data[j] * data[j] + datai[j] * datai[j] ;
         /*
         printf( "%d: ", j*SAMPLE_RATE/(2*FFT_SIZE) );
         for( int i=0; i<sqrt(v)*100000000; ++i )
            printf( "*" );
         printf( "\n" );
         */
         if( v > maxVal ) {
            maxVal = v;
            maxIndex = j;
         }
      }

      float freq = freqTable[maxIndex];

      // now output the results:
      printf( "%f %f\n", freq, maxVal*1000 );
      //printf( "%f %f\n", freq, rms );
   }

   // cleanup
   destroyfft( fft );

   return 0;
}

void buildHammingWindow( float *window, int size )
{
   for( int i=0; i<size; ++i )
      window[i] = .54 - .46 * cos( 2 * M_PI * i / (float) size );
}
void buildHanWindow( float *window, int size )
{
   for( int i=0; i<size; ++i )
      window[i] = .5 * ( 1 - cos( 2 * M_PI * i / (size-1.0) ) );
}
void applyWindow( float *window, float *data, int size )
{
   for( int i=0; i<size; ++i )
      data[i] *= window[i] ;
}
void computeSecondOrderLowPassParameters( float srate, float f, float *a, float *b )
{
   float a0;
   float w0 = 2 * M_PI * f/srate;
   float cosw0 = cos(w0);
   float sinw0 = sin(w0);
   //float alpha = sinw0/2;
   float alpha = sinw0/2 * sqrt(2);

   a0   = 1 + alpha;
   a[0] = (-2*cosw0) / a0;
   a[1] = (1 - alpha) / a0;
   b[0] = ((1-cosw0)/2) / a0;
   b[1] = ( 1-cosw0) / a0;
   b[2] = b[0];
}
float processSecondOrderFilter( float x, float *mem, float *a, float *b )
{
    float ret = b[0] * x + b[1] * mem[0] + b[2] * mem[1]
                         - a[0] * mem[2] - a[1] * mem[3] ;

		mem[1] = mem[0];
		mem[0] = x;
		mem[3] = mem[2];
		mem[2] = ret;

		return ret;
}
void signalHandler( int signum ) { running = false; }
