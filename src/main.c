/* Analyze Signal - main.c
 * Simple command-line signal processing
 * <https://github.com/whereswalden90/analyze-signal>
 *
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 * 0. You just DO WHAT THE FUCK YOU WANT TO.
 */

/* Modified from chromatic guitar tuner
 * <https://github.com/bejayoharen/>
 * Copyright (C) 2012 by Bjorn Roche
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
// #define SAMPLE_RATE (44100)
// //#define FFT_SIZE (8192)
// #define FFT_SIZE (1024)
// #define FFT_EXP_SIZE (10)     //13 for FFT_SIZE=8192. For each **2 you decrease FFT_SIZE, decrease this by 1
   
static const char HELP[] = "Analyze Signal - simple command-line signal processing\n"
   "\n"
   "\n"
   "USAGE: analyze-signal [options] < <signal>\n"
   "\n"
   "\n"
   "DESCRIPTION\n"
   "\n"
   "Perform various signal processing algorithms on a signal read from STDIN.\n"
   "The signal must be 1-channel, raw floating-point PCM data. Its sample rate\n"
   "must match analyze-signal's expected sample rate (-r).\n"
   "\n"
   "The default output is of the form:\n"
   "\n"
   "FREQUENCY AMPLITUDE RMS\n"
   "\n"
   "where AMPLITUDE is the power of FREQUENCY in the signal.\n"
   "\n"
   "I recommend <http://sox.sourceforge.net/> for doing conversion from standard \n"
   "audio formats. The relavent sox command then would be (assuming 2-channel input): \n"
   "\n"
   "sox <input> -r <rate> -t raw -e floating-point - rate remix 1,2 | analyze-signal -r <rate>\n"
   "\n"
   "Note that the choice of sample rate and fft bin size affects the accuracy of\n"
   "frequency measurements by this relationship:\n"
   "\n"
   "freq_accuracy = sample_rate/fft_bin_size\n"
   "\n"
   "For example, with a sample rate of 8000 and fft bin size of 8192, frequency\n"
   "measurements will be accurate within ~1 Hz.\n"
   "\n"
   "\n"
   "OPTIONS\n"
   "   -r, --sample-rate <rate>   Sample rate of incoming signal. (Default: 8000)\n"
   "   -s, --fft-size <bin-size>  FFT bin size. Should be a power of 2.\n"
   "      Lowering increases performance at the expense of accuracy. (Default: 8192)\n"
   "   -f, --frequency            Show the frequency in the results. (Default: false)\n"
   "   -a, --amplitude            Show the amplitude in the results. (Default: false)\n"
   "   -R, --rms                  Show the root-mean-square amplitude of the whole signal\n"
   "      (not just the power of a single frequency) in the results. (Default: false)\n"
   "   -h, --help                 Show this message and exit.\n"
   "   -V, --version              Show the version and exit.\n";


static const char VERSION[] = "analyze-signal 1.0\n"
   "https://github.com/whereswalden90/analyze-signal\n";

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

   // Default argument values
   int SAMPLE_RATE = 8000;
   int FFT_SIZE = 8192;
   bool RMS = false, FREQUENCY = false, AMPLITUDE = false;


   //Parse the arguments
   static struct option options[] = {
      {"sample-rate", required_argument, 0, 'r'},
      {"fft-size",    required_argument, 0, 's'},
      {"amplitude",   no_argument,       0, 'a'},
      {"frequency",   no_argument,       0, 'f'},
      {"rms",         no_argument,       0, 'R'},
      {"help",        no_argument,       0, 'h'},
      {"version",     no_argument,       0, 'V'},
      {0,             0,                 0,  0 }
   };
   int opts_index = 0, opt = 0;

   if (argc == 1) { 
      printf("%s", HELP);
      exit(2);
   }
   while ((opt = getopt_long(argc, argv, "r:s:afRhV", 
                   options, &opts_index )) != -1) {

      switch (opt) {
         case 'r': SAMPLE_RATE = atoi(optarg); break;
         case 's': FFT_SIZE    = atoi(optarg); break;
         case 'f': FREQUENCY   = true;         break;
         case 'a': AMPLITUDE   = true;         break;
         case 'R': RMS         = true;         break;
         case 'h': printf("%s", HELP);         exit(0);
         case 'V': printf("%s", VERSION);      exit(0);
         default: printf("%s", HELP);          exit(2);
      }
   }

   if (AMPLITUDE && !FREQUENCY) {
      printf("ERROR: --amplitude cannot be used without --frequency\n");
      exit(2);
   }

   int FFT_EXP_SIZE = (int) ( log((float) FFT_SIZE) / log(2.0) );


   //Declarations
   float freq = -1;
   float maxVal = -1;
   float rms = -1;
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

      if (RMS) {
         // get the RMS amplitude
         rms = 0;
         for (int i=0; i<FFT_SIZE; i++) {
            rms += data[i]*data[i];
         }
         rms = rms/FFT_SIZE;
         rms = sqrt(rms);
      }


      if (FREQUENCY) {
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

         freq = freqTable[maxIndex];
      }

      // now output the results:
      if (FREQUENCY) { printf("%f ", freq); }
      if (AMPLITUDE) { printf("%f ", maxVal); }
      if (RMS) { printf( "%f ", rms ); }
      printf("\n");

      //printf( "%f %f\n", freq, maxVal*1000 );
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
