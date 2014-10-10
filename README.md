analyze-signal
===========

Simple command-line signal processing

Perform various signal processing algorithms on a signal read from STDIN. At the moment, the only analyses included are frequency and RMS amplitude, but that may change in the future. 


The signal must be 1-channel, raw floating-point PCM data. Its sample rate must match analyze-signal's expected sample rate (-r). I recommend [sox](http://sox.sourceforge.net/) for doing conversion from standard audio formats. Assuming 2-channel input, the relavent command would be:

    sox <input> -r <rate> -t raw -e floating-point - rate remix 1,2 | analyze-signal -r <rate>

Usage:
-------
    analyze-signal [options] < <signal>

####OPTIONS
    -r, --sample-rate <rate>   Sample rate of incoming signal. (Default: 8000)
    -s, --fft-size <bin-size>  FFT bin size. Should be a power of 2. 
        Lowering increases performance at the expense of accuracy. (Default: 8192)
    -f, --frequency            Show the frequency in the results. (Default: false)
    -p, --power                Show the frequency's power in the results. (Default: false)
    -a, --amplitude            Show the root-mean-square amplitude of the whole signal
    (not just the power of a single frequency) in the results. (Default: false)
    -h, --help                 Show this message and exit.
    -V, --version              Show the version and exit.

The default output is of the form:

    FREQUENCY POWER AMPLITUDE

where POWER is the power of FREQUENCY in the signal.

Note that the choice of sample rate and fft bin size affects the accuracy of frequency measurements by this relationship:

    freq_accuracy = sample_rate/fft_bin_size

For example, with a sample rate of 8000 and fft bin size of 8192, frequency measurements will be accurate within ~1 Hz.

To Compile:
-----------
`analyze-audio` has no external dependencies so it should compile anywhere, though I've only tested it on OS X.

2. run "make"
3. the output is ./analyze-audio

Copyright/License
---------

DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE

TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION

0. You just DO WHAT THE FUCK YOU WANT TO.

Tuner Copyright (C) 2012 by Bjorn Roche

FFT Copyright (C) 1989 by Jef Poskanzer

More Info
---------

The frequency analysis works by calculating the magnitude of the FFT, which is not the ideal method, but
it works well enough.

You can find more info about this code on Bjorn Roche's blog:
http://blog.bjornroche.com/2012/07/frequency-detection-using-fft-aka-pitch.html
