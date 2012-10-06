#include "source/wav_source.h"
#include "sink/sink_wav.h"
#include "filters/filter_graph.h"
#include "filters/agc.h"
#include "filters/convert.h"
#include "filters/convolver.h"
#include "filters/dither.h"
#include "fir/param_fir.h"
#include "vtime.h"
#include "vargs.h"
#include "filter_usage.txt.h"

const int block_size = 65536;

int filter_proc(int argc, const char **argv)
{
  if (argc < 3)
  {
    printf(usage);
    return 0;
  }

  const char *input_filename = argv[1];
  const char *output_filename = argv[2];
  double f = 0;
  double f2 = 0;
  double df = 0;
  double a = 100;
  bool norm = false;
  bool do_dither = false;
  ParamFIR::filter_t type = ParamFIR::low_pass;

  /////////////////////////////////////////////////////////////////////////////
  // Parse arguments

  for (int iarg = 3; iarg < argc; iarg++)
  {
    // -<type>
    if (is_arg(argv[iarg], "lowpass", argt_exist))
    {
      type = ParamFIR::low_pass;
      continue;
    }

    if (is_arg(argv[iarg], "highpass", argt_exist))
    {
      type = ParamFIR::high_pass;
      continue;
    }

    if (is_arg(argv[iarg], "bandpass", argt_exist))
    {
      type = ParamFIR::band_pass;
      continue;
    }

    if (is_arg(argv[iarg], "bandstop", argt_exist))
    {
      type = ParamFIR::band_stop;
      continue;
    }

    // -f
    if (is_arg(argv[iarg], "f", argt_num))
    {
      f = arg_num(argv[iarg]);
      continue;
    }

    // -f2
    if (is_arg(argv[iarg], "f2", argt_num))
    {
      f2 = arg_num(argv[iarg]);
      continue;
    }

    // -df
    if (is_arg(argv[iarg], "df", argt_num))
    {
      df = arg_num(argv[iarg]);
      continue;
    }

    // -a
    if (is_arg(argv[iarg], "a", argt_num))
    {
      a = arg_num(argv[iarg]);
      continue;
    }

    // -norm
    if (is_arg(argv[iarg], "norm", argt_exist))
    {
      norm = true;
      continue;
    }

    // -dither
    if (is_arg(argv[iarg], "dither", argt_exist))
    {
      do_dither = true;
      continue;
    }

    printf("Error: unknown option: %s\n", argv[iarg]);
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Validate arguments

  if (type == -1)
  {
    printf("Please, specify the filter type\n");
    return -1;
  }

  if (f == 0)
  {
    printf("Please, specify the filter frequency with -f option\n");
    return -1;
  }

  if (type == ParamFIR::band_pass || type == ParamFIR::band_stop) if (f2 == 0)
  {
    printf("Please, specify the second bound frequency with -f2 option\n");
    return -1;
  }

  if (df == 0)
  {
    printf("Please, specify the trnasition band width with -df option\n");
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Open files

  WAVSource src(input_filename, block_size);
  if (!src.is_open())
  {
    printf("Error: cannot open file: %s\n", input_filename);
    return -1;
  }

  Speakers spk = src.get_output();

  WAVSink sink(output_filename);
  if (!sink.is_file_open() || !sink.open(spk))
  {
    printf("Error: cannot open file: %s with format %s\n", output_filename, spk.print().c_str());
    return -1;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Print processing info

  const char *type_text = "Unknown";
  switch (type)
  {
    case ParamFIR::low_pass:  type_text = "Low-pass"; break;
    case ParamFIR::high_pass: type_text = "High-pass"; break;
    case ParamFIR::band_pass: type_text = "Band-pass"; break;
    case ParamFIR::band_stop: type_text = "Band-stop"; break;
  };

  if (type == ParamFIR::band_pass || type == ParamFIR::band_stop)
    if (norm) printf("Filter: %s\nFirst bound freq: %.8g (%.8g Hz)\nSecound bound freq: %.8g (%.8g Hz)\nTransition band width: %.8g (%.8g Hz)\n", type_text, f, f * spk.sample_rate , f2, f2 * spk.sample_rate, df, df * spk.sample_rate);
    else printf("Filter: %s\nFirst bound freq: %.8g Hz\nSecound bound freq: %.8g Hz\nTransition band width: %.8g Hz\n", type_text, f, f2, df);
  else
    if (norm) printf("Filter: %s\nBound freq: %.8g (%.8g Hz)\nTransition band width: %.8g (%.8g Hz)\n", type_text, f, f * spk.sample_rate, df, df * spk.sample_rate);
    else printf("Filter: %s\nBound freq: %.8g Hz\nTransition band width: %.8g Hz\n", type_text, f, df);
  printf("Attenuation: %.8g dB\n", a);

  /////////////////////////////////////////////////////////////////////////////
  // Init FIR and print its info

  ParamFIR fir(type, f, f2, df, a, norm);
  const FIRInstance *data = fir.make(spk.sample_rate);

  if (!data)
  {
    printf("Error in filter parameters!\n");
    return -1;
  }

  printf("Filter length: %i\n", data->length);
  delete data;


  /////////////////////////////////////////////////////////////////////////////
  // Build processing chain

  Converter iconv(block_size);
  Converter oconv(block_size);
  iconv.set_format(FORMAT_LINEAR);
  oconv.set_format(src.get_output().format);

  Convolver filter(&fir);
  AGC agc;
  Dither dither;

  FilterChain chain;
  chain.add_back(&iconv);
  chain.add_back(&filter);
  if (do_dither && !spk.is_floating_point())
  {
    chain.add_back(&dither);
    dither.level = 0.5 / spk.level;
  }
  chain.add_back(&agc);
  chain.add_back(&oconv);

  /////////////////////////////////////////////////////////////////////////////
  // Process

  if (!chain.open(spk))
  {
    printf("Error: cannot start processing\n");
    return -1;
  }

  printf("0%%\r");

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
      printf("%i%%\r", (int)pos);
    }
  }

  while (chain.flush(out_chunk))
    sink.process(out_chunk);

  sink.flush();

  printf("100%%\n");
  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return filter_proc(argc, argv);
  }
  catch (ValibException &e)
  {
    printf("Processing error: %s\n", boost::diagnostic_information(e).c_str());
    return -1;
  }
  return 0;
}
