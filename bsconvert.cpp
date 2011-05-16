#include <stdio.h>

#include "auto_file.h"
#include "parser.h"
#include "bitstream.h"
#include "filters/spdifer.h"
#include "parsers/mpa/mpa_header.h"
#include "parsers/ac3/ac3_header.h"
#include "parsers/dts/dts_header.h"
#include "parsers/spdif/spdif_header.h"
#include "parsers/multi_header.h"
#include "source/file_parser.h"
#include "source/source_filter.h"

inline const char *bs_name(int bs_type);
inline bool is_14bit(int bs_type)
{ return bs_type == BITSTREAM_14LE || bs_type == BITSTREAM_14BE; }

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    printf(
"Bitstream converter\n"
"===================\n"
"This utility conversts files between numerous MPA/AC3/DTS stream types:\n"
"SPDIF padded, 8/14/16bit big/low endian. By default, it converts any\n"
"stream type to the most common byte stream.\n"
"\n"
"This utility is a part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2007-2011 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  Detect file type and print file information:\n"
"  > bsconvert input_file\n"
"\n"
"  Convert a file:\n"
"  > bsconvert input_file output_file [format]\n"
"\n"
"Options:\n"
"  input_file  - file to convert\n"
"  output_file - file to write result to\n"
"  format      - output file format:\n"
"    8     - byte stream (default)\n"
"    16le  - 16bit low endian\n"
"    14be  - 14bit big endian (DTS only)\n"
"    14le  - 14bit low endian (DTS only)\n"
"\n"
"Notes:\n"
"  File captured from SPDIF input may contain several parts of different type.\n"
"  For example SPDIF transmission may be started with 5.1 ac3 format then\n"
"  switch to 5.1 dts and then to stereo ac3. In this case all stream parts\n"
"  will be converted and writed to the same output file\n"
"\n"
"  SPDIF stream is padded with zeros, therefore output file size may be MUCH\n"
"  smaller than input. It is normal and this does not mean that some data was\n"
"  lost. This conversion is loseless! You can recreate SPDIF stream back with\n"
"  'spdifer' utility.\n"
"\n"
"  14bit streams are supported only for DTS format. Note, that conversion\n"
"  between 14bit and non-14bit changes actual frame size (frame interval),\n"
"  but does not change the frame header.\n"
    );
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Parse command line

  char *in_filename = 0;
  char *out_filename = 0;
  int bs_type = BITSTREAM_8;

  switch (argc)
  {
  case 2:
    in_filename = argv[1];
    break;

  case 3:
    in_filename = argv[1];
    out_filename = argv[2];
    break;

  case 4:
    in_filename = argv[1];
    out_filename = argv[2];
    if (!strcmp(argv[3], "8"))
      bs_type = BITSTREAM_8;
    else if (!strcmp(argv[3], "16le"))
      bs_type = BITSTREAM_16LE;
    else if (!strcmp(argv[3], "14be"))
      bs_type = BITSTREAM_14BE;
    else if (!strcmp(argv[3], "14le"))
      bs_type = BITSTREAM_14LE;
    else
    {
      printf("Error: Unknown stream format: %s\n", argv[3]);
      return -1;
    }
    break;

  default:
    printf("Error: Wrong number of arguments\n");
    return -1;
  }

  /////////////////////////////////////////////////////////
  // Open file and print file info

  const HeaderParser *headers[] = { &ac3_header, &mpa_header, &dts_header, &spdif_header };
  MultiHeader multi_header(headers, array_size(headers));

  FileParser in_file;
  if (!in_file.open(in_filename, &multi_header, 1024*1024))
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
  HeaderInfo hdr;
  Rawdata buf(multi_header.max_frame_size() / 7 * 8 + 8);
  
  try {
    in_file.seek(0); // Force new stream
    while (source->get_chunk(chunk))
    {
      ///////////////////////////////////////////////////////
      // New stream

      if (source->new_stream())
      {
        if (frames) printf("\n\n");
        printf("%s", in_file.stream_info().c_str());

        hdr = (in_file.get_output().format == FORMAT_SPDIF)?
          despdifer.raw_header_info():
          hdr = in_file.header_info();

        bs_target = bs_type;
        if (is_14bit(bs_target) && hdr.spk.format != FORMAT_DTS)
        {
          printf(
            "\nWARNING!!!\n"
            "%s does not support 14bit stream format!\n"
            "It will be converted to byte stream.\n", hdr.spk.format_text());
          bs_target = BITSTREAM_8;
        }

        printf("Conversion from %s to %s\n", bs_name(hdr.bs_type), bs_name(bs_target));
        conv = bs_conversion(hdr.bs_type, bs_target);
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
  }
  catch (ValibException &e)
  {
    printf("Processing error:\n%s", boost::diagnostic_information(e).c_str());
    return -1;
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
