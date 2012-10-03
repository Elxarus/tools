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

int equalizer_proc(int argc, const char **argv)
{
  int i;

  if (argc < 3)
  {
    printf(usage);
    return 0;
  }

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];
  EqBand bands[max_bands];
  bool do_dither = false;

  for (i = 0; i < max_bands; i++) bands[i].freq = 0, bands[i].gain = 0;

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (int iarg = 3; iarg < argc; iarg++)
  {
    // -dither
    if (is_arg(argv[iarg], "dither", argt_bool))
    {
      do_dither = arg_bool(argv[iarg]);
      continue;
    }
    else
    {
      // -fx -gx
      for (i = 0; i < max_bands; i++)
      {
        char buf[10];
        sprintf(buf, "f%i", i);
        if (is_arg(argv[iarg], buf, argt_num))
        {
          bands[i].freq = (int)arg_num(argv[iarg]);
          break;
        }

        sprintf(buf, "g%i", i);
        if (is_arg(argv[iarg], buf, argt_num))
        {
          if (bands[i].freq == 0)
          {
            printf("Unknown freqency for band %i (define the band's frequency before the gain)\n", i);
            return -1;
          }
          bands[i].gain = arg_num(argv[iarg]);
          break;
        }
      }

      if (i < max_bands)
        continue;
    }

    printf("Error: unknown option: %s\n", argv[iarg]);
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
    printf("No bands set\n");
    return -1;
  }

  printf("%i bands:\n", nbands);
  for (i = 0; i < nbands; i++)
    printf("%i Hz => %g dB\n", bands[i].freq, bands[i].gain);

  for (i = 0; i < nbands; i++)
    bands[i].gain = db2value(bands[i].gain);

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource src(input_filename, block_size);
  if (!src.is_open())
  {
    printf("Error: cannot open file: %s\n", input_filename);
    return -1;
  }
  Speakers spk = src.get_output();

  WAVSink sink(output_filename);
  if (!sink.is_file_open() || !sink.open(spk))
  {
    printf("Error: cannot open file %s with format %s\n", output_filename, spk.print());
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
    printf("Bad band parameters\n");
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
    printf("Error: cannot start processing\n");
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Do the job

  printf("0%%\r");

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
      printf("%i%%\r", (int)pos);
    }
  }

  while (chain.flush(out_chunk))
    sink.process(out_chunk);

  printf("100%%\n");
  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return equalizer_proc(argc, argv);
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  return 0;
}
