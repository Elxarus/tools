#include <stdio.h>

#include "filters/convert.h"
#include "source/generator.h"
#include "source/source_filter.h"
#include "sink/sink_dsound.h"
#include "sink/sink_wav.h"
#include "sink/sink_raw.h"
#include "vargs.h"
#include "noise_usage.txt.h"

const enum_opt mask_tbl[] =
{
  { "mono",   MODE_MONO },
  { "stereo", MODE_STEREO },
  { "quadro", MODE_2_2 },
  { "2.1",    MODE_2_0_LFE },
  { "4.1",    MODE_2_2_LFE },
  { "5.1",    MODE_5_1 },
  { "6.1",    MODE_6_1 },
  { "7.1",    MODE_7_1 },
  { "l",      CH_MASK_L },
  { "c",      CH_MASK_C },
  { "r",      CH_MASK_R },
  { "sl",     CH_MASK_SL },
  { "sr",     CH_MASK_SR },
  { "cl",     CH_MASK_CL },
  { "cr",     CH_MASK_CR },
  { "lfe",    CH_MASK_LFE },
  // Backwards compatibility
  { "0",      0 },
  { "1",      MODE_MONO },
  { "2",      MODE_STEREO },
  { "3",      MODE_3_0 },
  { "4",      MODE_QUADRO },
  { "5",      MODE_3_2 },
  { "6",      MODE_5_1 },
};

const enum_opt format_tbl[] = 
{
  { "pcm16",   FORMAT_PCM16 },
  { "pcm24",   FORMAT_PCM24 },
  { "pcm32",   FORMAT_PCM32 },
  { "pcm16be", FORMAT_PCM16_BE },
  { "pcm24be", FORMAT_PCM24_BE },
  { "pcm32be", FORMAT_PCM32_BE },
  { "pcm_float",  FORMAT_PCMFLOAT },
  { "pcm_double", FORMAT_PCMDOUBLE },
  // Backwards compatibility
  { "0", FORMAT_PCM16 },
  { "1", FORMAT_PCM24 },
  { "2", FORMAT_PCM32 },
  { "3", FORMAT_PCM16_BE },
  { "4", FORMAT_PCM24_BE },
  { "5", FORMAT_PCM32_BE },
  { "6", FORMAT_PCMFLOAT },
  { "7", FORMAT_PCMDOUBLE },
};

int noise_proc(const arg_list_t &args)
{
  if (args.size() < 2)
  {
    printf(usage);
    return -1;
  }

  DSoundSink  dsound;
  RAWSink     raw;
  WAVSink     wav;
  Sink *sink = 0;

  int mask = 0;
  int format = 0;
  int sample_rate = 48000;
  int seed = 0;
  double gain_db = 1.0;

  /////////////////////////////////////////////////////////
  // Parse arguments
  /////////////////////////////////////////////////////////

  int ms = atoi(args[1].raw.c_str());
  for (size_t iarg = 2; iarg < args.size(); iarg++)
  {
    const arg_t &arg = args[iarg];

    ///////////////////////////////////////////////////////
    // Output format
    ///////////////////////////////////////////////////////

    // -spk - number of speakers
    if (arg.is_option("spk", argt_enum))
    {
      int new_mask = arg.choose(mask_tbl, array_size(mask_tbl));
      mask |= new_mask;
      continue;
    }

    // -fmt - sample format
    if (arg.is_option("fmt", argt_enum))
    {
      format = arg.choose(format_tbl, array_size(format_tbl));
      continue;
    }

    // -rate - sample rate
    if (arg.is_option("rate", argt_int))
    {
      sample_rate = arg.as_int();
      continue;
    }

    ///////////////////////////////////////////////////////
    // Output mode
    ///////////////////////////////////////////////////////

    // -p[lay] - play
    if (arg.is_option("p", argt_exist) || 
        arg.is_option("play", argt_exist))
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
    if (arg.is_option("r", argt_exist) ||
        arg.is_option("raw", argt_exist))
    {
      if (sink)
      {
        printf("-raw : ambiguous output mode\n");
        return -1;
      }
      if (args.size() - iarg < 1)
      {
        printf("-raw : specify a file name\n");
        return -1;
      }

      const char *filename = args[++iarg].raw.c_str();
      if (!raw.open_file(filename))
      {
        printf("-raw : cannot open file '%s'\n", filename);
        return -1;
      }

      sink = &raw;
      continue;
    }

    // -w[av] - WAV output
    if (arg.is_option("w", argt_exist) ||
        arg.is_option("wav", argt_exist))
    {
      if (sink)
      {
        printf("-wav : ambiguous output mode\n");
        return -1;
      }
      if (args.size() - iarg < 1)
      {
        printf("-wav : specify a file name\n");
        return -1;
      }

      const char *filename = args[++iarg].raw.c_str();
      if (!wav.open_file(filename))
      {
        printf("-wav : cannot open file '%s'\n", filename);
        return -1;
      }

      sink = &wav;
      continue;
    }

    // -seed - RNG seed
    if (arg.is_option("seed", argt_int))
    {
      seed = arg.as_int();
      continue;
    }

    printf("Error: unknown option: %s\n", arg.raw.c_str());
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

  Speakers spk(format, mask, sample_rate);
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
    return noise_proc(args_utf8(argc, argv));
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  catch (arg_t::bad_value_e &e)
  {
    printf("Bad argument value: %s", e.arg.c_str());
    return -1;
  }
  return 0;
}
