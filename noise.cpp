#include <stdio.h>

#include "filters/convert.h"
#include "source/generator.h"
#include "source/source_filter.h"
#include "sink/sink_dsound.h"
#include "sink/sink_wav.h"
#include "sink/sink_raw.h"
#include "vargs.h"

const int mask_tbl[] =
{
  0,
  MODE_MONO,
  MODE_STEREO,
  MODE_3_0,
  MODE_2_2,
  MODE_3_2,
  MODE_5_1
};

const int format_tbl[] = 
{
  FORMAT_PCM16,
  FORMAT_PCM24,
  FORMAT_PCM32,
  FORMAT_PCM16_BE,
  FORMAT_PCM24_BE,
  FORMAT_PCM32_BE,
  FORMAT_PCMFLOAT,
  FORMAT_PCMDOUBLE,
};



int noise_proc(int argc, const char **argv)
{
  if (argc < 2)
  {
    printf(
"Noise generator\n"
"This utility is part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2007-2011 by Alexander Vigovsky\n\n"

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
"        1 - 1/0 (mono)     4 - 2/2 (quadro)\n" 
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
    );
    return -1;
  }

  DSoundSink  dsound;
  RAWSink     raw;
  WAVSink     wav;
  Sink *sink = 0;

  int imask = 2;
  int iformat = 0;
  int sample_rate = 48000;
  int seed = 0;
  double gain_db = 1.0;

  /////////////////////////////////////////////////////////
  // Parse arguments
  /////////////////////////////////////////////////////////

  int ms = atoi(argv[1]);
  for (int iarg = 2; iarg < argc; iarg++)
  {
    ///////////////////////////////////////////////////////
    // Output format
    ///////////////////////////////////////////////////////

    // -spk - number of speakers
    if (is_arg(argv[iarg], "spk", argt_num))
    {
      imask = int(arg_num(argv[iarg]));

      if (imask < 1 || imask > array_size(mask_tbl))
      {
        printf("-spk : incorrect speaker configuration\n");
        return -1;
      }
      continue;
    }

    // -fmt - sample format
    if (is_arg(argv[iarg], "fmt", argt_num))
    {
      iformat = int(arg_num(argv[iarg]));
      if (iformat < 0 || iformat > array_size(format_tbl))
      {
        printf("-fmt : incorrect sample format");
        return -1;
      }
      continue;
    }

    // -rate - sample rate
    if (is_arg(argv[iarg], "rate", argt_num))
    {
      sample_rate = int(arg_num(argv[iarg]));
      if (sample_rate < 0)
      {
        printf("-rate : incorrect sample rate");
        return -1;
      }
      continue;
    }

    ///////////////////////////////////////////////////////
    // Output mode
    ///////////////////////////////////////////////////////

    // -p[lay] - play
    if (is_arg(argv[iarg], "p", argt_exist) || 
        is_arg(argv[iarg], "play", argt_exist))
    {
      if (sink)
      {
        printf("-play : ambiguous output mode\n");
        return -1;
      }

      sink = &dsound;
      continue;
    }
    
    // -r[aw] - RAW output
    if (is_arg(argv[iarg], "r", argt_exist) ||
        is_arg(argv[iarg], "raw", argt_exist))
    {
      if (sink)
      {
        printf("-raw : ambiguous output mode\n");
        return -1;
      }
      if (argc - iarg < 1)
      {
        printf("-raw : specify a file name\n");
        return -1;
      }

      const char * filename = argv[++iarg];
      if (!raw.open_file(filename))
      {
        printf("-raw : cannot open file '%s'\n", filename);
        return -1;
      }

      sink = &raw;
      continue;
    }

    // -w[av] - WAV output
    if (is_arg(argv[iarg], "w", argt_exist) ||
        is_arg(argv[iarg], "wav", argt_exist))
    {
      if (sink)
      {
        printf("-wav : ambiguous output mode\n");
        return -1;
      }
      if (argc - iarg < 1)
      {
        printf("-wav : specify a file name\n");
        return -1;
      }

      const char * filename = argv[++iarg];
      if (!wav.open_file(filename))
      {
        printf("-wav : cannot open file '%s'\n", filename);
        return -1;
      }

      sink = &wav;
      continue;
    }

    // -seed - RNG seed
    if (is_arg(argv[iarg], "seed", argt_num))
    {
      seed = int(arg_num(argv[iarg]));
      if (sample_rate < 0)
      {
        printf("-rate : incorrect sample rate");
        return -1;
      }
      continue;
    }

    printf("Error: unknown option: %s\n", argv[iarg]);
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Setup output
  /////////////////////////////////////////////////////////

  if (!sink || sink == &dsound)
  {
    sink = &dsound;
    dsound.open_dsound(0);
  }

  Speakers spk(format_tbl[iformat], mask_tbl[imask], sample_rate);
  printf("Opening %s %s %iHz audio output...\n", spk.format_text(), spk.mode_text(), spk.sample_rate);

  if (!sink->open(spk))
  {
    printf("Error: Cannot open audio output!");
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Process
  /////////////////////////////////////////////////////////

  NoiseGen noise;
  Converter conv(1024);
  SourceFilter fp_noise;
  uint64_t out_size = uint64_t(double(spk.sample_size() * spk.nch() * spk.sample_rate) * double(ms) / 1000);

  Source *source = &noise;
  if (spk.is_floating_point())
  {
    Speakers noise_spk = spk;
    noise_spk.format = FORMAT_LINEAR;
    uint64_t samples = uint64_t(double(spk.sample_rate) * double(ms) / 1000);
    noise.init(noise_spk, seed, samples);
    conv.set_format(spk.format);

    fp_noise.set(&noise, &conv);
    source = &fp_noise;
  }
  else
    noise.init(spk, seed, out_size);

  Chunk chunk;
  uint64_t pos = 0;

  while (source->get_chunk(chunk))
  {
    sink->process(chunk);
    pos += chunk.size;
    fprintf(stderr, "%.0f%%\r", double(pos)*100/out_size);
  }

  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return noise_proc(argc, argv);
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  return 0;
}
