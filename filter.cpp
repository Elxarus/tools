#include "source/wav_source.h"
#include "sink/sink_wav.h"
#include "filter_graph.h"
#include "filters/convert.h"
#include "filters/convolver.h"
#include "fir/param_ir.h"
#include "vtime.h"

const int block_size = 65536;

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    printf(
"Filter\n"
"======\n"
"Sample audio filter\n"
"This utility is a part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2007 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  > resample input.wav output.wav\n");
    return 0;
  }

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];

  WAVSource src(input_filename, block_size);
  if (!src.is_open())
  {
    printf("Error: cannot open file: %s\n", input_filename);
    return -1;
  }

  WAVSink sink(output_filename);
  if (!sink.is_open())
  {
    printf("Error: cannot open file: %s\n", output_filename);
    return -1;
  }

  Converter iconv(block_size);
  Converter oconv(block_size);
  iconv.set_format(FORMAT_LINEAR);
  oconv.set_format(src.get_output().format);

  ParamIR ir(IR_LOW_PASS, 0.1, 0.0, 0.1, 100, true);
  Convolver filter(&ir);

  FilterChain chain;
  chain.add_back(&iconv, "Input converter");
  chain.add_back(&filter, "Convolver");
  chain.add_back(&oconv, "Output converter");

  Chunk chunk;
  printf("0%%\r");

  vtime_t t = local_time() + 0.1;
  while (!src.is_empty())
  {
    src.get_chunk(&chunk);
    if (!chain.process_to(&chunk, &sink))
    {
      char buf[1024];
      chain.chain_text(buf, array_size(buf));
      printf("Processing error. Chain dump:\n", buf);
      return -1;
    }

    if (local_time() > t)
    {
      t += 0.1;
      double pos = double(src.pos()) * 100 / src.size();
      printf("%i%%\r", (int)pos);
    }
  }
  printf("100%%\n");
  return 0;
}
