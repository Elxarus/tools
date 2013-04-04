const char *usage =
"Noise generator\n"
"This utility is part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2007-2013 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  nosie ms [options]\n"
"\n"
"Options:\n"
"  ms - length in milliseconds\n"
"\n"
"  output mode:\n"
"    -p[lay] - playback (*)\n"
"    -w[av] file.wav - write output to a WAV file\n"
"    -r[aw] file.raw - write output to a RAW file\n"
"\n"
"  format:\n"
"    -spk:n - set number of output channels:\n"
"        1 - 1/0 (mono)     4 - 2/2 (quadro) \n"
"    (*) 2 - 2/0 (stereo)   5 - 3/2 (5 ch)\n"
"        3 - 3/0 (surround) 6 - 3/2+SW (5.1)\n"
"    -fmt:n - set sample format:\n"
"    (*) 0 - PCM 16         3 - PCM 16 (big endian)\n"
"        1 - PCM 24         4 - PCM 24 (big endian)\n"
"        2 - PCM 32         5 - PCM 32 (big endian)\n"
"                 6 - PCM Float\n"
"                 7 - PCM Double\n"
"  -rate:n - sample rate (default is 48000)\n"
"\n"
"  -seed:n - seed for the random numbers generator\n"
"\n"
"Example:\n"
"  > noise 1000 -w noise.wav -fmt:2 -seed:666 -gain:-6\n"
"  Make 1sec stereo PCM Float noise file with level -6db\n"
;
