const char *usage =
"Filter\n"
"======\n"
"Simple parametric audio filter\n"
"This utility is a part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2008-2013 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  > filter input.wav output.wav -<type> -f:n [-f2:n] -df:n [-a:n] [-norm]\n"
"\n"
"Options:\n"
"  input.wav  - file to process\n"
"  output.wav - file to write the result to\n"
"  -<type> - filter type:\n"
"    -lowpass  - low pass filter\n"
"    -highpass - high pass filter\n"
"    -bandpass - band pass filter\n"
"    -bandstop - band stop filter\n"
"  -f    - center frequency of the filter (all filters)\n"
"  -f2   - second bound frequency (bandpass and bandstop filters only)\n"
"  -df   - transition band width\n"
"  -a    - attenuation factor in dB (100dB by default)\n"
"  -norm - if this switch is present all frequecies are specified in\n"
"          normalized form instead of Hz.\n"
"  -dither - dither the result\n"
"\n"
"Examples:\n"
"  Band-pass filter from 100Hz to 8kHz with transition width of 100Hz.\n"
"  Attenuate all other frequencies (0-50Hz and 8050 to nyquist) by 100dB.\n"
"  > filter in.wav out.wav -bandpass -f:100 -f2:8000 -df:100 -a:100\n"
"\n"
"  Low-pass half of the total bandwidth with 1%% transition bandwidth.\n"
"  Note, that in normalized form nyquist frequency is 0.5 (not 1.0!), so half\n"
"  of the bandwidth is 0.25 and 1% of the bandwidth is 0.005.\n"
"  For 48kHz input, this means low-pass filter with 12kHz cutoff frequency\n"
"  and 240Hz transition band width.\n"
"  > filter in.wav out.wav -lowpass -f:0.25 -df:0.005\n"
;
