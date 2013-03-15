#include <stdio.h>

#include "auto_file.h"
#include "parser.h"
#include "bitstream.h"
#include "filters/spdifer.h"
#include "parsers/uni/uni_frame_parser.h"
#include "source/file_parser.h"
#include "source/source_filter.h"
#include "bsconvert_usage.txt.h"
#include "vargs.h"

inline const char *bs_name(int bs_type);
inline bool is_14bit(int bs_type)
{ return bs_type == BITSTREAM_14LE || bs_type == BITSTREAM_14BE; }

int bsconvert_proc(const arg_list_t &args)
{
  if (args.size() < 2)
  {
    printf(usage);
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Parse command line

  const char *in_filename = 0;
  const char *out_filename = 0;
  int bs_type = BITSTREAM_8;

  switch (args.size())
  {
  case 2:
    in_filename = args[1].raw.c_str();
    break;

  case 3:
    in_filename = args[1].raw.c_str();
    out_filename = args[2].raw.c_str();
    break;

  case 4:
    in_filename = args[1].raw.c_str();
    out_filename = args[2].raw.c_str();
    if (args[3].raw == "8")
      bs_type = BITSTREAM_8;
    else if (args[3].raw == "16le")
      bs_type = BITSTREAM_16LE;
    else if (args[3].raw == "14be")
      bs_type = BITSTREAM_14BE;
    else if (args[3].raw == "14le")
      bs_type = BITSTREAM_14LE;
    else
    {
      printf("Error: Unknown stream format: %s\n", args[3].raw.c_str());
      return -1;
    }
    break;

  default:
    printf(usage);
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Open file and print file info

  UniFrameParser uni;

  FileParser in_file;
  if (!in_file.open(in_filename, &uni, 1024*1024))
  {
    printf("Error: Cannot open file '%s'\n", in_filename);
    return -1;
  }

  if (!in_file.probe())
  {
    printf("Error: Cannot determine file format\n");
    return -1;
  }

  printf("%s", in_file.file_info().c_str());
  if (!out_filename)
  {
    printf("%s", in_file.stream_info().c_str());
    return 0;
  }

  /////////////////////////////////////////////////////////
  // Open output file

  AutoFile out_file(out_filename, "wb");
  if (!out_file.is_open())
  {
    printf("Error: Cannot open file for writing '%s'\n", out_filename);
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Raw stream source

  Despdifer despdifer;
  SourceFilter raw_source;
  Source *source = &in_file;
  if (in_file.get_output().format == FORMAT_SPDIF)
  {
    raw_source.set(&in_file, &despdifer);
    source = &raw_source;
  }

  /////////////////////////////////////////////////////////
  // Process data

  int frames = 0;
  int bs_target = bs_type;
  bs_conv_t conv;
  Chunk chunk;
  FrameInfo finfo;
  Rawdata buf(uni.sync_info().max_frame_size / 7 * 8 + 8);

  in_file.seek(0); // Force new stream
  while (source->get_chunk(chunk))
  {
    ///////////////////////////////////////////////////////
    // New stream

    if (source->new_stream())
    {
      if (frames) printf("\n\n");
      printf("%s", in_file.stream_info().c_str());

      finfo = (in_file.get_output().format == FORMAT_SPDIF)?
        despdifer.raw_frame_info():
        in_file.frame_info();

      bs_target = bs_type;
      if (is_14bit(bs_target) && finfo.spk.format != FORMAT_DTS)
      {
        printf(
          "\nWARNING!!!\n"
          "%s does not support 14bit stream format!\n"
          "It will be converted to byte stream.\n", finfo.spk.format_text());
        bs_target = BITSTREAM_8;
      }

      printf("Conversion from %s to %s\n", bs_name(finfo.bs_type), bs_name(bs_target));
      conv = bs_conversion(finfo.bs_type, bs_target);
      if (!conv)
        printf("Error: Cannot convert!\n");
    }
    frames++;

    ///////////////////////////////////////////////////////
    // Skip the stream if we cannot convert it

    if (!conv)
    {
      fprintf(stderr, "Skipping: %i\r", frames);
      continue;
    }

    ///////////////////////////////////////////////////////
    // Do the job

    uint8_t *new_frame = buf.begin();
    size_t new_size = conv(chunk.rawdata, chunk.size, new_frame);

    // Correct DTS header
    if (bs_target == BITSTREAM_14LE)
      new_frame[3] = 0xe8;
    else if (bs_target == BITSTREAM_14BE)
      new_frame[2] = 0xe8;

    out_file.write(new_frame, new_size);
    fprintf(stderr, "Frame: %i\r", frames);
  }

  printf("Frames found: %i\n\n", frames);
  return 0;
}

inline const char *bs_name(int bs_type)
{
  switch (bs_type)
  {
    case BITSTREAM_8:    return "byte";
    case BITSTREAM_16BE: return "16bit big endian";
    case BITSTREAM_16LE: return "16bit low endian";
    case BITSTREAM_32BE: return "32bit big endian";
    case BITSTREAM_32LE: return "32bit low endian";
    case BITSTREAM_14BE: return "14bit big endian";
    case BITSTREAM_14LE: return "14bit low endian";
    default: return "unknown";
  }
}

int main(int argc, const char *argv[])
{
  try
  {
    return bsconvert_proc(args_utf8(argc, argv));
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  return 0;
}
