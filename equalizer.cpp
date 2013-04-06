#include "source/wav_source.h"
#include "sink/sink_wav.h"
#include "filters/filter_graph.h"
#include "filters/agc.h"
#include "filters/convert.h"
#include "filters/dither.h"
#include "filters/equalizer.h"
#include "vtime.h"
#include "vargs.h"
#include "equalizer_usage.txt.h"

const int block_size = 65536;
const int max_bands = 100;

int equalizer_proc(const arg_list_t &args)
{
  int i;

  if (args.size() < 3)
  {
    fprintf(stderr, usage);
    return 0;
  }

  const char *input_filename = args[1].raw.c_str();
  const char *output_filename = args[2].raw.c_str();
  EqBand bands[max_bands];
  bool do_dither = false;

  for (i = 0; i < max_bands; i++) bands[i].freq = 0, bands[i].gain = 0;

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (size_t iarg = 3; iarg < args.size(); iarg++)
  {
    const arg_t &arg = args[iarg];

    // -dither
    if (arg.is_option("dither", argt_bool))
    {
      do_dither = arg.as_bool();
      continue;
    }
    else
    {
      // -fx -gx
      for (i = 0; i < max_bands; i++)
      {
        char buf[10];
        sprintf(buf, "f%i", i);
        if (arg.is_option(buf, argt_int))
        {
          bands[i].freq = arg.as_int();
          break;
        }

        sprintf(buf, "g%i", i);
        if (arg.is_option(buf, argt_double))
        {
          if (bands[i].freq == 0)
          {
            fprintf(stderr, "Unknown freqency for band %i (define the band's frequency before the gain)\n", i);
            return -1;
          }
          bands[i].gain = arg.as_double();
          break;
        }
      }

      if (i < max_bands)
        continue;
    }

    fprintf(stderr, "Error: unknown option: %s\n", arg.raw.c_str());
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Compact bands and print band info

  int nbands = 0;
  for (i = 0; i < max_bands; i++)
    if (bands[i].freq != 0)
    {
      bands[nbands].freq = bands[i].freq;
      bands[nbands].gain = bands[i].gain;
      nbands++;
    }

  if (nbands < max_bands)
  {
    bands[nbands].freq = 0;
    bands[nbands].gain = 0;
  }

  if (nbands == 0)
  {
    fprintf(stderr, "No bands set\n");
    return -1;
  }

  fprintf(stderr, "%i bands:\n", nbands);
  for (i = 0; i < nbands; i++)
    fprintf(stderr, "%i Hz => %g dB\n", bands[i].freq, bands[i].gain);

  for (i = 0; i < nbands; i++)
    bands[i].gain = db2value(bands[i].gain);

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource src(input_filename, block_size);
  if (!src.is_open())
  {
    fprintf(stderr, "Error: cannot open file: %s\n", input_filename);
    return -1;
  }
  Speakers spk = src.get_output();

  WAVSink sink(output_filename);
  if (!sink.is_file_open() || !sink.open(spk))
  {
    fprintf(stderr, "Error: cannot open file %s with format %s\n", output_filename, spk.print());
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Build the chain

  Converter iconv(block_size);
  Converter oconv(block_size);
  iconv.set_format(FORMAT_LINEAR);
  oconv.set_format(spk.format);

  Equalizer eq;
  eq.set_enabled(true);
  if (!eq.set_bands(bands, nbands))
  {
    fprintf(stderr, "Bad band parameters\n");
    return -1;
  }

  AGC agc;
  Dither dither;
  FilterChain chain;

  chain.add_back(&iconv);
  chain.add_back(&eq);
  if (do_dither && !spk.is_floating_point())
  {
    chain.add_back(&dither);
    dither.level = 0.5 / spk.level;
  }
  chain.add_back(&agc);
  chain.add_back(&oconv);

  if (!chain.open(spk))
  {
    fprintf(stderr, "Error: cannot start processing\n");
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Do the job

  fprintf(stderr, "0%%\r");

  Chunk in_chunk, out_chunk;
  vtime_t t = local_time() + 0.1;
  while (src.get_chunk(in_chunk))
  {
    while (chain.process(in_chunk, out_chunk))
      sink.process(out_chunk);

    ///////////////////////////////////////////////////////
    // Statistics

    if (local_time() > t)
    {
      t += 0.1;
      double pos = double(src.pos()) * 100 / src.size();
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
    return equalizer_proc(args_utf8(argc, argv));
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
