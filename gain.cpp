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

int gain_proc(const arg_list_t &args)
{
  if (args.size() < 3)
  {
    fprintf(stderr, usage);
    return 0;
  }

  const char *input_filename = args[1].raw.c_str();
  const char *output_filename = args[2].raw.c_str();
  double gain = 1.0;

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (size_t iarg = 3; iarg < args.size(); iarg++)
  {
    const arg_t &arg = args[iarg];

    // -gain
    if (arg.is_option("g", argt_double) ||
        arg.is_option("gain", argt_double))
    {
      gain = db2value(arg.as_double());
      continue;
    }

    fprintf(stderr, "Error: unknown option: %s\n", arg.raw.c_str());
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource src(input_filename, block_size);
  if (!src.is_open())
  {
    fprintf(stderr, "Error: cannot open file '%s'\n", input_filename);
    return -1;
  }

  WAVSink sink(output_filename);
  if (!sink.is_file_open())
  {
    fprintf(stderr, "Error: cannot open file for writing '%s'\n", output_filename);
    return -1;
  }

  Speakers spk = src.get_output();
  if (!sink.open(spk))
  {
    fprintf(stderr, "Error: cannot write wav of format %s\n", spk.print().c_str());
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
    return gain_proc(args_utf8(argc, argv));
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
