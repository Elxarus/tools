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

const size_t chunk_size = 8192;

const enum_opt format_tbl[] = 
{
  { "pcm16",   FORMAT_PCM16 },
  { "pcm24",   FORMAT_PCM24 },
  { "pcm32",   FORMAT_PCM32 },
  { "pcm_float",  FORMAT_PCMFLOAT },
  { "pcm_double", FORMAT_PCMDOUBLE },
};

int wavconv_proc(const arg_list_t &args)
{
  if (args.size() < 3)
  {
    fprintf(stderr, usage);
    return -1;
  }

  const char *input_filename = args[1].raw.c_str();
  const char *output_filename = args[2].raw.c_str();
  int rate = 0;          // output sample rate
  double a = 100;        // src attenuation
  double q = 0.99;       // src quality
  sample_t g = 1.0;      // gain
  double cut_start = -1; // cut start point in secs
  double cut_end = -1;   // cut end point in secs
  int format = FORMAT_UNKNOWN; // output format

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (size_t iarg = 3; iarg < args.size(); iarg++)
  {
    const arg_t &arg = args[iarg];

    if (arg.is_option("rate", argt_int) ||
        arg.is_option("r", argt_int))
    {
      rate = arg.as_int();
      continue;
    }

    if (arg.is_option("attenuation", argt_double) ||
        arg.is_option("a", argt_double))
    {
      a = arg.as_double();
      continue;
    }

    if (arg.is_option("quality", argt_double) ||
        arg.is_option("q", argt_double))
    {
      q = arg.as_double();
      continue;
    }

    if (arg.is_option("gain", argt_double) ||
        arg.is_option("g", argt_double))
    {
      g = arg.as_double();
      continue;
    }

    if (arg.is_option("cut_start", argt_double))
    {
      cut_start = arg.as_double();
      continue;
    }

    if (arg.is_option("cut_end", argt_double))
    {
      cut_end = arg.as_double();
      continue;
    }

    if (arg.is_option("format", argt_enum))
    {
      format = arg.choose(format_tbl, array_size(format_tbl));
      continue;
    }

    fprintf(stderr, "Error: unknown option: %s\n", arg.raw.c_str());
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource source(input_filename, chunk_size);
  if (!source.is_open())
  {
    fprintf(stderr, "Error: cannot open file: %s\n", input_filename);
    return -1;
  }

  WAVSink sink;
  if (!sink.open_file(output_filename))
  {
    fprintf(stderr, "Error: cannot open file: %s\n", output_filename);
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

  Converter in_conv(chunk_size);
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

  Converter out_conv(chunk_size);
  if (format != FORMAT_UNKNOWN)
    out_conv.set_format(format);
  else
    out_conv.set_format(spk.format);
  chain.add_back(&out_conv);

  /////////////////////////////////////////////////////////////////////////////
  // Process

  if (!chain.open(spk))
  {
    fprintf(stderr, "Cannot start processing input: %s", spk.print().c_str());
    return -1;
  }

  if (!sink.open(out_spk))
  {
    fprintf(stderr, "Cannot open output WAV file with format %s", out_spk.print().c_str());
    return -1;
  }

  fprintf(stderr, "0%%\r");

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
      fprintf(stderr, "%i%%\r", (int)pos);
    }
  }

  while (chain.flush(out_chunk))
    sink.process(out_chunk);

  sink.flush();

  fprintf(stderr, "100%%\n");
  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return wavconv_proc(args_utf8(argc, argv));
  }
  catch (ValibException &e)
  {
    fprintf(stderr, "Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  catch (arg_t::bad_value_e &e)
  {
    fprintf(stderr, "Bad argument value: %s", e.arg.c_str());
    return -1;
  }
  return 0;
}
