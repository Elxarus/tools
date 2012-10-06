#include <stdio.h>
#include "valib/filters/agc.h"
#include "valib/filters/convert.h"
#include "valib/filters/gain.h"
#include "valib/filters/filter_graph.h"
#include "valib/filters/resample.h"
#include "valib/filters/dither.h"
#include "valib/filters/slice.h"
#include "valib/source/wav_source.h"
#include "valib/sink/sink_wav.h"
#include "valib/vargs.h"
#include "valib/vtime.h"

#include "wavconv_usage.txt.h"

const size_t block_size = 8192;

const struct
{
  const char *format_name;
  int format;
} format_tbl[] =
{
  { "pcm16", FORMAT_PCM16 },
  { "pcm24", FORMAT_PCM24 },
  { "pcm32", FORMAT_PCM32 },
  { "pcm_float",  FORMAT_PCMFLOAT  },
  { "pcm_double", FORMAT_PCMDOUBLE },
};

int text2format(const char *text)
{
  for (int i = 0; i < array_size(format_tbl); i++)
    if (strcmp(format_tbl[i].format_name, text) == 0)
      return format_tbl[i].format;
  return FORMAT_UNKNOWN;
}


int main(int argc, const char **argv)
{
  if (argc < 3)
  {
    printf(usage);
    return -1;
  }

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];
  int rate = 0;          // output sample rate
  double a = 100;        // src attenuation
  double q = 0.99;       // src quality
  sample_t g = 1.0;      // gain
  double cut_start = -1; // cut start point in secs
  double cut_end = -1;   // cut end point in secs
  int format = FORMAT_UNKNOWN; // output format

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (int iarg = 3; iarg < argc; iarg++)
  {
    if (is_arg(argv[iarg], "rate", argt_num) ||
        is_arg(argv[iarg], "r", argt_num))
    {
      rate = (int)arg_num(argv[iarg]);
      continue;
    }

    if (is_arg(argv[iarg], "attenuation", argt_num) ||
        is_arg(argv[iarg], "a", argt_num))
    {
      a = arg_num(argv[iarg]);
      continue;
    }

    if (is_arg(argv[iarg], "quality", argt_num) ||
        is_arg(argv[iarg], "q", argt_num))
    {
      q = arg_num(argv[iarg]);
      continue;
    }

    if (is_arg(argv[iarg], "gain", argt_num) ||
        is_arg(argv[iarg], "g", argt_num))
    {
      g = arg_num(argv[iarg]);
      continue;
    }

    if (is_arg(argv[iarg], "cut_start", argt_num))
    {
      cut_start = arg_num(argv[iarg]);
      continue;
    }

    if (is_arg(argv[iarg], "cut_end", argt_num))
    {
      cut_end = arg_num(argv[iarg]);
      continue;
    }

    if (is_arg(argv[iarg], "format", argt_text))
    {
      const char *format_name = arg_text(argv[iarg]);
      format = text2format(format_name);
      if (format == FORMAT_UNKNOWN)
      {
        printf("Error: unknown format %s\n", format_name);
        return -1;
      }
      continue;
    }

    printf("Error: unknown option: %s\n", argv[iarg]);
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource source(input_filename, block_size);
  if (!source.is_open())
  {
    printf("Error: cannot open file: %s\n", input_filename);
    return -1;
  }

  WAVSink sink;
  if (!sink.open_file(output_filename))
  {
    printf("Error: cannot open file: %s\n", output_filename);
    return -1;
  }

  Speakers spk = source.get_output();
  Speakers out_spk = spk;
  if (format != FORMAT_UNKNOWN)
    out_spk = Speakers(format, spk.mask, spk.sample_rate);
  if (rate > 0)
    out_spk.sample_rate = rate;

  /////////////////////////////////////////////////////////////////////////////
  // Build the processing chain

  FilterChain chain;

  Converter in_conv(block_size);
  in_conv.set_format(FORMAT_LINEAR);
  chain.add_back(&in_conv);

  SliceFilter slice;
  size_t cut_start_sample = SliceFilter::not_specified;
  size_t cut_end_sample = SliceFilter::not_specified;
  if (cut_start > 0)
    cut_start_sample = static_cast<size_t>(cut_start * spk.sample_rate);
  if (cut_end > 0)
    cut_end_sample = static_cast<size_t>(cut_end * spk.sample_rate);
  slice.init(cut_start_sample, cut_end_sample);
  chain.add_back(&slice);

  Gain gain(g * out_spk.level / spk.level);
  chain.add_back(&gain);

  Resample src;
  Dither dither(0.5 / out_spk.level);
  if (rate > 0)
  {
    src.set(rate, a, q);
    out_spk.sample_rate = rate;
    chain.add_back(&src);
    if (!out_spk.is_floating_point())
      chain.add_back(&dither);
  }

  AGC agc;
  chain.add_back(&agc);

  Converter out_conv(block_size);
  if (format != FORMAT_UNKNOWN)
    out_conv.set_format(format);
  else
    out_conv.set_format(spk.format);
  chain.add_back(&out_conv);

  /////////////////////////////////////////////////////////////////////////////
  // Process

  if (!chain.open(spk))
  {
    printf("Cannot start processing input: %s", spk.print().c_str());
    return -1;
  }

  if (!sink.open(out_spk))
  {
    printf("Cannot open output WAV file with format %s", out_spk.print().c_str());
    return -1;
  }

  printf("0%%\r");

  Chunk in_chunk, out_chunk;
  vtime_t t = local_time() + 0.1;
  while (source.get_chunk(in_chunk))
  {
    while (chain.process(in_chunk, out_chunk))
      sink.process(out_chunk);

    ///////////////////////////////////////////////////////
    // Statistics

    if (local_time() > t)
    {
      t += 0.1;
      double pos = double(source.pos()) * 100 / source.size();
      printf("%i%%\r", (int)pos);
    }
  }

  while (chain.flush(out_chunk))
    sink.process(out_chunk);

  sink.flush();

  printf("100%%\n");
  return 0;
}
