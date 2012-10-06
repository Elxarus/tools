#include <stdio.h>
#include <fcntl.h>
#include <io.h>

#include "parsers/ac3/ac3_header.h"
#include "parsers/dts/dts_header.h"
#include "parsers/mpa/mpa_header.h"
#include "parsers/spdif/spdif_header.h"
#include "parsers/spdif/spdif_wrapper.h"
#include "parsers/multi_header.h"
#include "source/file_parser.h"

#include "sink/sink_raw.h"
#include "sink/sink_wav.h"
#include "vtime.h"

#include "spdifer_usage.txt.h"

///////////////////////////////////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////////////////////////////////

int spdifer_proc(int argc, const char **argv)
{
  bool stat = true;  // display statistics

  FileParser file;

  RAWSink   raw;
  WAVSink   wav;
  Sink     *sink;

  if (argc < 3)
  {
    printf(usage);
    return 0;
  }

  /////////////////////////////////////////////////////////
  // Parse command line

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];
  enum { mode_raw, mode_wav } mode = mode_raw;

  if (argc > 3)
  {
    if (!strcmp(argv[3], "-raw")) mode = mode_raw;
    if (!strcmp(argv[3], "-wav")) mode = mode_wav;
  }

  /////////////////////////////////////////////////////////
  // Open files

  SpdifableFrameParser spdifable;
  if (!file.open(input_filename, &spdifable))
  {
    fprintf(stderr, "Cannot open input file '%s'\n", input_filename);
    return 1;
  }

  switch (mode)
  {
  case mode_wav:
    if (!wav.open_file(output_filename))
    {
      fprintf(stderr, "Cannot open output file '%s'\n", output_filename);
      return 1;
    }
    sink = &wav;
    break;

  case mode_raw:
    if (!raw.open_file(output_filename))
    {
      fprintf(stderr, "Cannot open output file '%s'\n", output_filename);
      return 1;
    }
    sink = &raw;
    break;

  }

  /////////////////////////////////////////////////////////
  // Do the job

  SPDIFWrapper spdifer;
  int streams = 0;
  int frames = 0;
  int skipped = 0;
  int errors = 0;

  size_t size = 0;

  vtime_t time = local_time();
  vtime_t old_time = time;
  vtime_t start_time = time;

  Chunk chunk, out_chunk;
  while (file.get_chunk(chunk))
  {
    frames++;
    if (file.new_stream())
    {
      Speakers new_spk = file.get_output();
      printf("Input stream found: %s\n", new_spk.print().c_str());
      if (!spdifer.open(new_spk))
      {
        printf("Error: cannot process this format\n");
        return -1;
      }
      streams++;
    }

    while (spdifer.process(chunk, out_chunk))
    {
      if (spdifer.new_stream())
      {
        Speakers new_spk = spdifer.get_output();
        printf("Opening output stream: %s\n", new_spk.print().c_str());
        if (!sink->open(new_spk))
        {
          printf("Error: cannot open sink\n");
          return -1;
        }
      }
      sink->process(out_chunk);
    }

    time = local_time();
    if (file.eof() || time > old_time + 0.1)
    {
      old_time = time;

      float file_size = float(file.get_size());
      float processed = float(file.get_pos());
      float elapsed   = float(old_time - start_time);
      int eta = int((file_size / processed - 1.0) * elapsed);

      fprintf(stderr, "%02i%% %02i:%02i In: %iM (%2iMB/s) Out: %iM Frames: %i (%s)       \r", 
        int(processed * 100 / file_size),
        eta / 60, eta % 60,
        int(processed / 1000000), int(processed/elapsed/1000000), size / 1000000, frames,
        file.get_output().print().c_str());
    }
  } // while (!file.eof())

  while (spdifer.flush(out_chunk))
  {
    if (spdifer.new_stream())
    {
      Speakers new_spk = spdifer.get_output();
      printf("Opening output stream: %s\n", new_spk.print().c_str());
      if (!sink->open(new_spk))
      {
        printf("Error: cannot open sink\n");
        return -1;
      }
    }
    sink->process(out_chunk);
  }

  sink->flush();
  sink->close();

  // Final notes
  printf("\n");
  if (streams > 1) printf("%i streams converted\n", streams);

  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return spdifer_proc(argc, argv);
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  return 0;
}
