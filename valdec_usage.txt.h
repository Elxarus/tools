const char *usage =
"Valib decoder\n"
"=============\n"
"Audio decoder/processor/player utility.\n"
"\n"
"This utility is a part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2006-2013 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  valdec some_file [options]\n"
"\n"
"Options:\n"
"  (*)  - default value\n"
"  {ch} - channel name (l, c, r, sl, sr)\n"
"\n"
"  output mode:\n"
"    -d[ecode]  - just decode (used for testing and performance measurements)\n"
"    -p[lay]    - play file (*)\n"
"    -r[aw] file.raw - decode to RAW file\n"
"    -w[av] file.wav - decode to WAV file\n"
"    -n[othing] - do nothing (to be used with -i option)\n"
"\n"
"  output options:\n"
"    -spk:{layout} - define output channel layout\n"
"      You may choose a predefined layout and/or specify each channel\n"
"      individually with several -spk options. When layout is not set, output\n"
"      layout will match the input layout.\n"
"\n"
"      Predefined channel layouts:\n"
"          mono    c\n"
"          stereo  l, c\n"
"          quadro  l, r, sl, sr\n"
"          2.1     c, lfe\n"
"          4.1     l, r, sl, sr, lfe\n"
"          5.1     l, c, r, sl, sr, lfe\n"
"          6.1     l, c, r, sl, sr, bc, lfe\n"
"          7.1     l, c, r, sl, sr, bl, br, lfe\n"
"\n"
"      Predefined channel layouts for backwards compatibility:\n"
"          0 - not set       4 - 2/2 (quadro)\n"
"          1 - 1/0 (mono)    5 - 3/2 (5 ch)\n"
"          2 - 2/0 (stereo)  6 - 3/2+SW (5.1)\n"
"          3 - 3/0 (surround)\n"
"\n"
"      Examples:\n"
"          -spk:5.1\n"
"          Instructs valdec to output 5.1 channels\n"
"\n"
"          -spk:5.1 -spk:bc\n"
"          Add back center to 5.1 layout.\n"
"\n"
"          -spk:l -spk:r -spk:lfe\n"
"          2.1 layout constructed from individual channels\n"
"\n"
"    -fmt:{format} - set output sample format:\n"
"      List of supported sample formats:\n"
"      (*) pcm16             pcm16be\n"
"          pcm24             pcm24be\n"
"          pcm32             pcm32be\n"
"          pcm_float         pcm_double\n"
"\n"
"      List of sample formats for backwards compatibility:\n"
"          0 - PCM 16        3 - PCM 16 (big endian)\n"
"          1 - PCM 24        4 - PCM 24 (big endian) \n"
"          2 - PCM 32        5 - PCM 32 (big endian)\n"
"          6 - PCM Float     7 - PCM Double\n"
"\n"
"    -rate:N - set output sample rate.\n"
"      If set, valdec will do sample rate conversion to the rate specified\n"
"      (if nessesary).\n"
"\n"
"  format selection:\n"
"    -ac3 - force ac3 (do not autodetect format)\n"
"    -dts - force dts (do not autodetect format)\n"
"    -mpa - force mpa (do not autodetect format)\n"
"\n"
"  info:\n"
"    -i     - print bitstream info\n"
"    -hist  - print levels histogram\n"
"    -log logfile - dump processing log to a file\n"
"    -log_level:level - maximum level for log events:\n"
"      critical  - critical error, so program cannot continue\n"
"      exception - exceptions\n"
"      error     - 'normal' errors (file open, resource allocation, etc)\n"
"      warning   - event that require special attention.\n"
"      event     - important event happen during execution\n"
"      trace     - informational event\n"
"      all (*)   - log everything\n"
"\n"
"  mixer options:\n"
"    -auto_matrix[+|-] - automatic matrix calculation on(*)/off\n"
"    -normalize_matrix[+|-] - normalize matrix on(*)/off\n"
"    -voice_control[+|-] - voice control on(*)/off\n"
"    -expand_stereo[+|-] - expand stereo on(*)/off\n"
"    -clev:N - center mix level (dB)\n"
"    -slev:N - surround mix level (dB)\n"
"    -lfelev:N - lfe mix level (dB)\n"
"    -gain:N - master gain (dB)\n"
"    -gain_{ch}:N - output channel gain (dB)\n"
"    -{in}_{out}:N - factor (not dB!) for a matrix cell at input channel {in}\n"
"          and output channels {out}. Effective only when auto_matrix is off.\n"
"\n"
"  automatic gain control options:\n"
"    -agc[+|-] - auto gain control on(*)/off\n"
"    -normalize[+|-] - one-pass normalize on/off(*)\n"
"    -drc[+|-] - dynamic range compression on/off(*)\n"
"    -drc_power:N - dynamic range compression level (dB)\n"
"    -attack:N - attack speed (dB/s)\n"
"    -release:N - release speed (dB/s)\n"
"\n"
"  delay options:\n"
"    -delay_units:n - units in wich delay values are given:\n"
"      (*) s  - samples       m  - meters       ft - feet  \n"
"          ms - milliseconds  cm - centimeters  in - inches\n"
"\n"
"      Backwards compatibility:\n"
"          0 - samples        2 - meters        4 - feet   \n"
"          1 - milliseconds   3 - centimeters   5 - inches \n"
"\n"
"    -delay_{ch}:N - delay for channel {ch} (in samples by default) \n"
"\n"
"  Channel names used at some parameters:\n"
"    l  - front left       bl - back left\n"
"    c  - front center     bc - back center\n"
"    r  - front right      sr - back right\n"
"    sl - surround left    cl - left of center\n"
"    sr - surround right   cr - right of center\n"
"    lfe - lfe or subwoofer\n"
;
