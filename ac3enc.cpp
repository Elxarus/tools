#include <stdio.h>

#include "source/wav_source.h"
#include "sink/sink_raw.h"
#include "parsers/ac3/ac3_enc.h"
#include "filters/convert.h"
#include "filters/filter_graph.h"
#include "win32/cpu.h"
#include "vargs.h"
#include "ac3enc_usage.txt.h"

int ac3enc(int argc, const char *argv[])
{
  if (argc < 3)
  {
    printf(usage);
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Parse arguments
  /////////////////////////////////////////////////////////

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];
  int bitrate = 448;

  for (int iarg = 3; iarg < argc; iarg++)
  {
    if (is_arg(argv[iarg], "br", argt_num))
    {
       bitrate = (int)arg_num(argv[iarg]);
       continue;
    }

    printf("Error: unknown option: %s\n", argv[iarg]);
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Open files
  /////////////////////////////////////////////////////////

  WAVSource src;
  if (!src.open(input_filename, 65536))
  {
    printf("Error: Cannot open file (not a PCM file?) '%s'\n", input_filename);
    return -1;
  }

  RAWSink sink;
  if (!sink.open_file(output_filename))
  {
    printf("Error: Cannot open file for writing '%s'\n", argv[2]);
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Setup everything
  /////////////////////////////////////////////////////////

  Converter   conv(2048);
  AC3Enc      enc;
  FilterChain chain;

  chain.add_back(&conv);
  chain.add_back(&enc);

  conv.set_format(FORMAT_LINEAR);
  conv.set_order(win_order);

  if (!enc.set_bitrate(bitrate*1000))
  {
    printf("Error: Wrong bitrate %ikbps!\n", bitrate);
    return -1;
  }

  Speakers spk = src.get_output();
  if (!chain.open(spk))
  {
    printf("Error: Cannot encode file (%s)!\n", spk.print().c_str());
    return -1;
  }
  printf("Input format: %s\n", spk.print().c_str());
  printf("Output format: AC3 %ikbps\n", bitrate);

  /////////////////////////////////////////////////////////
  // Process
  /////////////////////////////////////////////////////////

  Chunk pcm_chunk;
  Chunk ac3_chunk;

  CPUMeter cpu_usage;
  CPUMeter cpu_total;
  
  double ms = 0;
  double old_ms = 0;

  cpu_usage.start();
  cpu_total.start();

  fprintf(stderr, "0.0%% Frs/err: 0/0\tTime: 0:00.000i\tFPS: 0 CPU: 0%%\r"); 
  int frames = 0;

  while (src.get_chunk(pcm_chunk))
    while (chain.process(pcm_chunk, ac3_chunk))
    {
      sink.process(ac3_chunk);
      frames++;

      /////////////////////////////////////////////////////
      // Statistics

      ms = double(cpu_total.get_system_time() * 1000);
      if (ms > old_ms + 100)
      {
        old_ms = ms;

        // Statistics
        fprintf(stderr, "%2.1f%% Frames: %i\tTime: %i:%02i.%03i\tFPS: %i CPU: %.1f%%  \r", 
          double(src.pos()) * 100.0 / src.size(), 
          frames,
          int(ms/60000), int(ms) % 60000/1000, int(ms) % 1000,
          int(frames * 1000 / (ms+1)),
          cpu_usage.usage() * 100);
      } // if (ms > old_ms + 100)
    }

  /////////////////////////////////////////////////////
  // Flush the chain

  while (chain.flush(ac3_chunk))
  {
    sink.process(ac3_chunk);
    frames++;
  }

  ms = double(cpu_total.get_system_time() * 1000);
  printf("%2.1f%% Frames: %i\tTime: %i:%02i.%03i\tFPS: %i CPU: %.1f%%  \n", 
    double(src.pos()) * 100.0 / src.size(), 
    frames,
    int(ms/60000), int(ms) % 60000/1000, int(ms) % 1000,
    int(frames * 1000 / (ms+1)),
    cpu_usage.usage() * 100);

  cpu_usage.stop();
  cpu_total.stop();

  printf("System time: %ims\n", int(cpu_total.get_system_time() * 1000));
  printf("Process time: %ims\n", int(cpu_total.get_thread_time() * 1000));

  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return ac3enc(argc, argv);
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  return 0;
}
