#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "mpeg_demux.h"
#include "vargs.h"
#include "vtime.h"
#include "mpeg_demux_usage.txt.h"

const int buf_size = 65536;

///////////////////////////////////////////////////////////////////////////////
// File information
///////////////////////////////////////////////////////////////////////////////

const char *stream_type(int stream)
{
  if (stream == 0xbc) return "Reserved stream";
  if (stream == 0xbe) return "Padding stream";
  if (stream == 0xbd) return "Private stream 1";
  if (stream == 0xbf) return "Private stream 2";
  if ((stream & 0xe0) == 0xc0) return "MPEG Audio stream";
  if ((stream & 0xf0) == 0xe0) return "MPEG Video stream";
  if ((stream & 0xf0) == 0xf0) return "Reserved data stream";
  return "unknown stream type";
}
const char *substream_type(int substream)
{
  if ((substream & 0xf8) == 0x80) return "AC3 Audio substream";
  if ((substream & 0xf8) == 0x88) return "DTS Audio substream";
  if ((substream & 0xf0) == 0xa0) return "LPCM Audio substream";
  if ((substream & 0xf0) == 0x20) return "Subtitle substream";
  if ((substream & 0xf0) == 0x30) return "Subtitle substream";
  return "unknown substream type";
}

void info(FILE *f)
{
  uint8_t buf[buf_size];
  uint8_t *data;
  uint8_t *end;

  int stream[256];
  int substream[256];

  memset(stream, 0, sizeof(stream));
  memset(substream, 0, sizeof(substream));

  PSParser ps;
  ps.reset();

  while (!feof(f) && (ftell(f) < 1024 * 1024)) // analyze only first 1MB
  {
    // read data from file
    data = buf;
    end = buf;
    end += fread(buf, 1, buf_size, f);

    while (data < end)
    {
      if (ps.payload_size)
      {
        if (!stream[ps.stream])
        {
          stream[ps.stream]++;
          fprintf(stderr, "Found stream %x (%s)      \n", ps.stream, stream_type(ps.stream));
        }
        if (ps.stream == 0xBD && !substream[ps.substream])
        {
          substream[ps.substream]++;
          fprintf(stderr, "Found substream %x (%s)   \n", ps.substream, substream_type(ps.substream));
        }

        size_t len = MIN(ps.payload_size, size_t(end - data));
        ps.payload_size -= len;
        data += len;
      }
      else
        ps.parse(&data, end);
    }  
  }

  if (ps.errors)
    fprintf(stderr, "Stream contains errors (not a MPEG program stream?)\n");
}

///////////////////////////////////////////////////////////////////////////////
// Demux
///////////////////////////////////////////////////////////////////////////////

void demux(FILE *f, FILE *out, int stream, int substream, bool pes)
{
  uint8_t buf[buf_size];
  uint8_t *data;
  uint8_t *end;

  vtime_t start_time;
  vtime_t old_time;

  PSParser ps;
  ps.reset();

  /////////////////////////////////////////////////////////
  // Determine file size

  long start_pos = ftell(f);
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, start_pos, SEEK_SET);

  old_time = 0;
  start_time = local_time();
  while (!feof(f))
  {
    ///////////////////////////////////////////////////////
    // Read data from file

    data = buf;
    end = buf;
    end += fread(buf, 1, buf_size, f);

    ///////////////////////////////////////////////////////
    // Demux data

    while (data < end)
    {
      if (ps.payload_size)
      {
        size_t len = MIN(ps.payload_size, size_t(end - data));

        // drop other streams
        if ((stream && stream != ps.stream) || 
            (stream && substream && substream != ps.substream))
        {
          ps.payload_size -= len;
          data += len;
          continue;
        }

        // write packet header
        if (pes && ps.header_size)
        {
          fwrite(ps.header, 1, ps.header_size, out);
          ps.header_size = 0; // do not write header anymore
        }

        // write packet payload
        fwrite(data, 1, len, out);
        ps.payload_size -= len;
        data += len;
      }
      else
        ps.parse(&data, end);
    }

    ///////////////////////////////////////////////////////
    // Statistics (10 times/sec)

    if (feof(f) || local_time() > old_time + 0.1)
    {
      old_time = local_time();
      long in_pos = ftell(f);
      long out_pos = ftell(out);

      float processed = float(in_pos - start_pos);
      float elapsed = float(old_time - start_time);

      int eta = int((float(file_size) / processed - 1.0) * elapsed);

      fprintf(stderr, "%02i:%02i %02i%% In: %iM (%2iMB/s) Out: %iK    ", 
        eta / 60, eta % 60,
        int(processed * 100 / float(file_size)),
        in_pos / 1000000, int(processed/elapsed/1000000), out_pos / 1000);

      if (stream? substream: ps.substream)
        fprintf(stderr, " stream:%02x/%02x\r", stream? stream: ps.stream, stream? substream: ps.substream);
      else
        fprintf(stderr, " stream:%02x   \r", stream? stream: ps.stream);
    }
  }

  fprintf(stderr, "\n");

  if (ps.errors)
    fprintf(stderr, "Stream contains errors (not a MPEG program stream?)\n");

};

///////////////////////////////////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////////////////////////////////

int mpeg_demux(const arg_list_t &args)
{
  int stream = 0;
  int substream = 0;

  const char *filename = 0;
  const char *filename_out = 0;

  FILE *f = 0;
  FILE *out = 0;

  if (args.size() < 2)
  {
    fprintf(stderr, usage);
    return -1;
  }

  enum { mode_none, mode_info, mode_es, mode_pes } mode = mode_none;

  // Parse arguments
  filename = args[1].raw.c_str();
  for (size_t iarg = 2; iarg < args.size(); iarg++)
  {
    const arg_t &arg = args[iarg];

    // -i - info
    if (arg.is_option("i", argt_exist))
    {
      if (mode != mode_none)
      {
        fprintf(stderr, "-i : ambiguous mode\n");
        return 1;
      }

      mode = mode_info;
      continue;
    }  

    // -d - demux to es
    if (arg.is_option("d", argt_exist))
    {
      if (args.size() - iarg < 2)
      {
        fprintf(stderr, "-d : specify a file name\n");
        return 1;
      }
      if (mode != mode_none)
      {
        fprintf(stderr, "-d : ambiguous mode\n");
        return 1;
      }

      mode = mode_es;
      filename_out = args[++iarg].raw.c_str();
      continue;
    }

    // -p - demux to pes
    if (arg.is_option("p", argt_exist))
    {
      if (args.size() - iarg < 2)
      {
        fprintf(stderr, "-p : specify a file name\n");
        return 1;
      }
      if (mode != mode_none)
      {
        fprintf(stderr, "-p : ambiguous mode\n");
        return 1;
      }

      mode = mode_pes;
      filename_out = args[++iarg].raw.c_str();
      continue;
    }

    // -stream - stream to demux
    if (arg.is_option("s", argt_int))
    {
      stream = arg.as_int();
      continue;
    }

    // -substream - substream to demux
    if (arg.is_option("ss", argt_int))
    {
      substream = arg.as_int();
      continue;
    }

    fprintf(stderr, "Unknown parameter: %s\n", arg.raw.c_str());
    return 1;
  }

  if (substream)
  {
    if (stream && stream != 0xbd)
    {
      fprintf(stderr, "Cannot demux substreams for stream 0x%x\n", stream);
      return 1;
    }
    stream = 0xbd;
  }

  if (!(f = fopen(filename, "rb")))
  {
    fprintf(stderr, "Cannot open file %s for reading\n", filename);
    return 1;
  }

  if (filename_out)
    if (!(out = fopen(filename_out, "wb")))
    {
      fprintf(stderr, "Cannot open file %s for writing\n", filename_out);
      return 1;
    }

  // Do the job

  switch (mode)
  {
  case mode_none:
  case mode_info:
    info(f);
    break;

  case mode_es:
    demux(f, out, stream, substream, false);
    break;

  case mode_pes:
    demux(f, out, stream, substream, true);
    break;
  }

  // Finish

  fclose(f);
  if (out) fclose(out);

  return 0;
}

int main(int argc, const char *argv[])
{
  try {
    return mpeg_demux(args_utf8(argc, argv));
  }
  catch (arg_t::bad_value_e &e)
  {
    fprintf(stderr, "Bad argument value: %s", e.arg.c_str());
    return -1;
  }
  return 0;
}
