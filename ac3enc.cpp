#include <stdio.h>

#include "source/wav_source.h"
#include "sink/sink_raw.h"
#include "parsers/ac3/ac3_enc.h"
#include "filters/convert.h"
#include "filters/filter_graph.h"
#include "win32/cpu.h"
#include "vargs.h"

int main(int argc, char *argv[])
{
  if (argc < 3)
  {
    printf(
"AC3 Encoder\n"
"===========\n"
"This utility is a part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2007-2011 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  ac3enc input.wav output.ac3 [-br:bitrate_kbps]\n"
    );
    return 1;
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
    return 1;
  }

  /////////////////////////////////////////////////////////
  // Open files
  /////////////////////////////////////////////////////////

  WAVSource src;
  if (!src.open(input_filename, 65536))
  {
    printf("Cannot open file %s (not a PCM file?)\n", input_filename);
    return 1;
  }

  RAWSink sink;
  if (!sink.open_file(output_filename))
  {
    printf("Cannot open file %s for writing!\n", argv[2]);
    return 1;
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
    printf("Wrong bitrate (%i)!\n", bitrate);
    return 1;
  }

  Speakers spk = src.get_output();
  if (!chain.open(spk))
  {
    printf("Cannot encode this file (%s)!\n", spk.print().c_str());
    return 1;
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

  printf("0.0%% Frs/err: 0/0\tTime: 0:00.000i\tFPS: 0 CPU: 0%%\r"); 
  int frames = 0;

  try
  {
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
          printf("%2.1f%% Frames: %i\tTime: %i:%02i.%03i\tFPS: %i CPU: %.1f%%  \r", 
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
  }
  catch (ValibException &e)
  {
    printf("Processing error:\n%s", boost::diagnostic_information(e).c_str());
    return 1;
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
