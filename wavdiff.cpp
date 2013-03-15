#include <math.h>
#include "source/wav_source.h"
#include "source/source_filter.h"
#include "sink/sink_wav.h"
#include "sink/sink_filter.h"
#include "filters/filter_graph.h"
#include "filters/agc.h"
#include "filters/convert.h"
#include "filters/gain.h"
#include "vtime.h"
#include "vargs.h"
#include "wavdiff_usage.txt.h"

const int block_size = 65536;

int wavdiff(const arg_list_t &args)
{
  if (args.size() < 3)
  {
    printf(usage);
    return -1;
  }

  const char *filename1 = args[1].raw.c_str();
  const char *filename2 = args[2].raw.c_str();
  const char *diff_filename = 0;
  bool check_diff = false;
  bool check_rms  = false;
  bool check_mean = false;
  double max_diff = 0;
  double max_rms  = 0;
  double max_mean = 0;

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (size_t iarg = 3; iarg < args.size(); iarg++)
  {
    const arg_t &arg = args[iarg];

    if (arg.is_option("max_diff", argt_double))
    {
      max_diff = arg.as_double();
      check_diff = true;
      continue;
    }

    if (arg.is_option("max_mean", argt_double))
    {
      max_mean = arg.as_double();
      check_mean = true;
      continue;
    }

    if (arg.is_option("max_rms", argt_double))
    {
      max_rms = arg.as_double();
      check_rms = true;
      continue;
    }

    if (arg.is_option("diff", argt_exist))
    {
      if (args.size() - iarg < 1)
      {
        printf("-diff : specify a file name\n");
        return 1;
      }

      diff_filename = args[++iarg].raw.c_str();
      continue;
    }

    printf("Error: unknown option: %s\n", arg.raw.c_str());
    return -1;
  }

  printf("Comparing %s - %s...\n", filename1, filename2);

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource f1(filename1, block_size);
  if (!f1.is_open())
  {
    printf("Error: cannot open file: %s\n", filename1);
    return -1;
  }

  WAVSource f2(filename2, block_size);
  if (!f2.is_open())
  {
    printf("Error: cannot open file: %s\n", filename2);
    return -1;
  }

  WAVSink f_diff;
  if (diff_filename)
  {
    if (!f_diff.open_file(diff_filename))
    {
      printf("Error: cannot open file: %s\n", diff_filename);
      return -1;
    }
  }

  Speakers spk1 = f1.get_output();
  Speakers spk2 = f2.get_output();
  Speakers spk_diff = spk1;
  if (spk1.format == FORMAT_PCM16)
    spk_diff = spk2;
  else if (spk2.format == FORMAT_PCM16)
    spk_diff = spk1;
  else if (spk1.format == FORMAT_PCM24)
    spk_diff = spk2;
  else if (spk2.format == FORMAT_PCM24)
    spk_diff = spk1;
  else if (spk1.format == FORMAT_PCM32)
    spk_diff = spk2;

  if (spk1.nch() != spk2.nch())
  {
    printf("Error: different number of channels\n");
    return -1;
  }
  if (spk1.sample_rate != spk2.sample_rate)
  {
    printf("Error: different sample rates\n");
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Build the chain

  Converter conv1(block_size);
  Converter conv2(block_size);
  Converter conv_diff(block_size);
  conv1.set_format(FORMAT_LINEAR);
  conv2.set_format(FORMAT_LINEAR);
  conv_diff.set_format(spk_diff.format);

  SourceFilter src1(&f1, &conv1);
  SourceFilter src2(&f2, &conv2);
  SinkFilter sink_diff(&f_diff, &conv_diff);

  if (!conv1.open(spk1))
  {
    printf("Error: unsupported file type: %s\n", filename1);
    return -1;
  }
  if (!conv2.open(spk2))
  {
    printf("Error: unsupported file type: %s\n", filename2);
    return -1;
  }
  if (diff_filename && !sink_diff.open(src1.get_output()))
  {
    printf("Cannot open output file %s with type %s\n", diff_filename, spk_diff.print().c_str());
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Do the job

  Chunk chunk1;
  Chunk chunk2;
  Chunk chunk_diff;
  size_t n = 0;

  double diff = 0.0;
  double mean = 0.0;
  double rms = 0.0;

  Speakers spk;
  size_t len;
  int ch;
  size_t s;

  vtime_t t = local_time() + 0.1;
  while (1)
  {
    if (!chunk1.size)
      if (!src1.get_chunk(chunk1))
        break;

    if (!chunk2.size)
      if (!src2.get_chunk(chunk2))
        break;

    ///////////////////////////////////////////////////////
    // Compare data

    len = MIN(chunk2.size, chunk1.size);
    double norm1 = 1.0 / spk1.level;
    double norm2 = 1.0 / spk2.level;
    for (ch = 0; ch < spk1.nch(); ch++)
      for (s = 0; s < len; s++)
      {
        sample_t d = chunk1.samples[ch][s] * norm1 - chunk2.samples[ch][s] * norm2;
        if (fabs(d) > diff) diff = fabs(d);
        mean += fabs(d);
        rms += d * d;
      }

    ///////////////////////////////////////////////////////
    // Write difference file

    if (f_diff.is_open())
    {
      Speakers spk_diff_linear = spk_diff;
      spk_diff_linear.format = FORMAT_LINEAR;

      double norm1 = spk_diff.level / spk1.level;
      double norm2 = spk_diff.level / spk2.level;
      for (ch = 0; ch < spk1.nch(); ch++)
        for (s = 0; s < len; s++)
          chunk1.samples[ch][s] = chunk1.samples[ch][s] * norm1 - chunk2.samples[ch][s] * norm2;

      chunk_diff.set_linear(chunk1.samples, len);
      sink_diff.process(chunk_diff);
    }

    n += len;
    chunk1.drop_samples(len);
    chunk2.drop_samples(len);

    ///////////////////////////////////////////////////////
    // Statistics

    if (local_time() > t)
    {
      t += 0.1;
      double pos = double(f1.pos()) * 100 / f1.size();
      printf("%i%%\r", (int)pos);
    }

  } // while (1)

  if (f_diff.is_open())
    f_diff.flush();

  rms = sqrt(rms/n);
  mean /= n;

  printf("    \r");
  printf("Max diff: %gdB\n", value2db(diff));
  printf("RMS diff: %gdB\n", value2db(rms));
  printf("Mean diff: %gdB\n", value2db(mean));

  int result = 0;
  if (check_diff && diff > db2value(max_diff))
  {
    printf("Error: maximum difference is too large");
    result = -1;
  }
  if (check_rms && rms > db2value(max_rms))
  {
    printf("Error: RMS difference is too large");
    result = -1;
  }
  if (check_mean && mean > db2value(max_mean))
  {
    printf("Error: mean difference is too large");
    result = -1;
  }

  return result;
}

int main(int argc, const char *argv[])
{
  try
  {
    return wavdiff(args_utf8(argc, argv));
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  catch (arg_t::bad_value_e &e)
  {
    printf("Bad argument value: %s", e.arg.c_str());
    return -1;
  }
  return 0;
}
