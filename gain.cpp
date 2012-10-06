#include <math.h>
#include "source/wav_source.h"
#include "sink/sink_wav.h"
#include "filters/agc.h"
#include "filters/convert.h"
#include "filters/filter_graph.h"
#include "filters/gain.h"
#include "vtime.h"
#include "vargs.h"
#include "gain_usage.txt.h"

const int block_size = 65536;

int gain_proc(int argc, const char **argv)
{
  if (argc < 3)
  {
    printf(usage);
    return 0;
  }

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];
  double gain = 1.0;

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (int iarg = 3; iarg < argc; iarg++)
  {
    // -gain
    if (is_arg(argv[iarg], "g", argt_num) ||
        is_arg(argv[iarg], "gain", argt_num))
    {
      gain = db2value(arg_num(argv[iarg]));
      continue;
    }

    printf("Error: unknown option: %s\n", argv[iarg]);
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource src(input_filename, block_size);
  if (!src.is_open())
  {
    printf("Error: cannot open file '%s'\n", input_filename);
    return -1;
  }

  WAVSink sink(output_filename);
  if (!sink.is_file_open())
  {
    printf("Error: cannot open file for writing '%s'\n", output_filename);
    return -1;
  }

  Speakers spk = src.get_output();
  if (!sink.open(spk))
  {
    printf("Error: cannot write wav of format %s\n", spk.print().c_str());
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Build the chain

  Converter iconv(block_size);
  Converter oconv(block_size);
  iconv.set_format(FORMAT_LINEAR);
  oconv.set_format(src.get_output().format);
  Gain gain_filter;
  AGC agc;

  gain_filter.gain = gain;

  FilterChain chain;
  chain.add_back(&iconv);
  chain.add_back(&gain_filter);
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

  sink.flush();

  printf("100%%\n");
  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return gain_proc(argc, argv);
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  return 0;
}
