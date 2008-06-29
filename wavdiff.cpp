#include <math.h>
#include "source/wav_source.h"
#include "sink/sink_wav.h"
#include "filter_graph.h"
#include "filters/agc.h"
#include "filters/convert.h"
#include "filters/gain.h"
#include "vtime.h"
#include "vargs.h"

const int block_size = 65536;

int main(int argc, char **argv)
{
  if (argc < 3)
  {
    printf(

"WAVDiff\n"
"=======\n"
"Find the difference between 2 audio files. Calculates maximum amplitude\n"
"difference and RMS difference. Can compare files of different types, like\n"
"PCM16 and PCM Float (number of channels and sample rate must match).\n"
"Can write the difference into the difference file."
"\n"
"This utility is a part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2008 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  > wavdiff a.wav b.wav [-diff c.wav]\n"
"\n"
"Options:\n"
"  a.wav - First file to compare\n"
"  b.wav - Second file to compare\n"
"  c.wav - Difference file\n"
"\n"
"Example:\n"
" > wavdiff a.wav b.wav -diff c.wav\n"
" Comapre a.wav and b.wav and write the difference into c.wav\n"
    );
    return 0;
  }

  const char *filename1 = argv[1];
  const char *filename2 = argv[2];
  const char *diff_filename = 0;
  double min_rms = 0;
  double max_rms = 0;
  double min_diff = 0;
  double max_amp = 0;

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (int iarg = 3; iarg < argc; iarg++)
  {
    if (is_arg(argv[iarg], "diff", argt_exist))
    {
      if (argc - iarg < 1)
      {
        printf("-diff : specify a file name\n");
        return 1;
      }

      diff_filename = argv[++iarg];
      continue;
    }

    printf("Error: unknown option: %s\n", argv[iarg]);
    return -1;
  }

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
    if (!f_diff.open(diff_filename))
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
  
  /////////////////////////////////////////////////////////////////////////////
  // Build the chain

  Converter conv1(block_size);
  Converter conv2(block_size);
  Converter conv_diff(block_size);
  conv1.set_format(FORMAT_LINEAR);
  conv2.set_format(FORMAT_LINEAR);
  conv_diff.set_format(spk_diff.format);

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
  if (!conv1.set_input(spk1))
  {
    printf("Error: unsupported file type: %s\n", filename1);
    return -1;
  }
  if (!conv2.set_input(spk2))
  {
    printf("Error: unsupported file type: %s\n", filename2);
    return -1;
  }

  SourceFilter src1(&f1, &conv1);
  SourceFilter src2(&f2, &conv2);
  SinkFilter diff(&f_diff, &conv_diff);

  /////////////////////////////////////////////////////////////////////////////
  // Do the job

  Chunk chunk1;
  Chunk chunk2;
  Chunk chunk_diff;
  size_t n = 0;

  double max_diff = 0.0;
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
    {
      if (src1.is_empty()) break;
      if (!src1.get_chunk(&chunk1))
      {
        printf("src1.get_chunk() fails");
        return -1;
      }
      if (chunk1.is_dummy()) continue;
    }

    if (!chunk2.size)
    {
      if (src2.is_empty()) break;
      if (!src2.get_chunk(&chunk2))
      {
        printf("src1.get_chunk() fails");
        return -1;
      }
      if (chunk2.is_dummy()) continue;
    }

    ///////////////////////////////////////////////////////
    // Compare data

    len = MIN(chunk2.size, chunk1.size);
    double norm1 = 1.0 / spk1.level;
    double norm2 = 1.0 / spk2.level;
    for (ch = 0; ch < spk1.nch(); ch++)
      for (s = 0; s < len; s++)
      {
        sample_t diff = chunk1.samples[ch][s] * norm1 - chunk2.samples[ch][s] * norm2;
        if (fabs(diff) > max_diff)
          max_diff = fabs(diff);
        rms += diff * diff;
        mean += diff;
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

      chunk_diff.set_linear(spk_diff_linear, chunk1.samples, len);
      if (!diff.process(&chunk_diff))
      {
        printf("Error: cannot write to the difference file\n");
        return -1;
      }
    }

    n += len;
    chunk1.drop(len);
    chunk2.drop(len);

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
  {
    chunk_diff.set_empty(spk_diff);
    chunk_diff.set_eos();
    if (!diff.process(&chunk_diff))
    {
      printf("Error: cannot write to the difference file\n");
      return -1;
    }
  }

  printf("100%%\n");
  rms = sqrt(rms/n);
  mean /= n;

  printf("Max diff: %gdB\n", value2db(max_diff));
  printf("RMS diff: %gdB\n", value2db(rms));

  return 0;
}
