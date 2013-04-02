#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <string>

// parsers
#include "source/file_parser.h"

#include "parsers/ac3/ac3_header.h"
#include "parsers/dts/dts_header.h"
#include "parsers/mpa/mpa_header.h"
#include "parsers/spdif/spdif_header.h"
#include "parsers/multi_header.h"

// sinks
#include "sink/sink_raw.h"
#include "sink/sink_wav.h"
#include "sink/sink_dsound.h"
#include "sink/sink_null.h"

// filters
#include "filters/dvd_graph.h"

// other
#include "win32/cpu.h"
#include "vargs.h"
#include "log.h"

#include "valdec_usage.txt.h"

const struct {
  const char *name;
  int ch;
} ch_map[] =
{
  { "l",  CH_L  },
  { "c",  CH_C  },
  { "r",  CH_R  },
  { "sl", CH_SL },
  { "sr", CH_SR },
  { "bl", CH_BL },
  { "bc", CH_BC },
  { "br", CH_BR },
  { "cl", CH_CL },
  { "cr", CH_CR },
  { "lfe", CH_LFE }
};

const enum_opt mask_tbl[] =
{
  { "mono",   MODE_MONO },
  { "stereo", MODE_STEREO },
  { "quadro", MODE_2_2 },
  { "2.1",    MODE_2_0_LFE },
  { "4.1",    MODE_2_2_LFE },
  { "5.1",    MODE_5_1 },
  { "6.1",    MODE_6_1 },
  { "7.1",    MODE_7_1 },
  { "l",      CH_MASK_L },
  { "c",      CH_MASK_C },
  { "r",      CH_MASK_R },
  { "sl",     CH_MASK_SL },
  { "sr",     CH_MASK_SR },
  { "cl",     CH_MASK_CL },
  { "cr",     CH_MASK_CR },
  { "lfe",    CH_MASK_LFE },

  // Backwards compatibility
  { "0",      0 },
  { "1",      MODE_MONO },
  { "2",      MODE_STEREO },
  { "3",      MODE_3_0 },
  { "4",      MODE_QUADRO },
  { "5",      MODE_3_2 },
  { "6",      MODE_5_1 },
};

const enum_opt format_tbl[] = 
{
  { "pcm16",   FORMAT_PCM16 },
  { "pcm24",   FORMAT_PCM24 },
  { "pcm32",   FORMAT_PCM32 },
  { "pcm16be", FORMAT_PCM16_BE },
  { "pcm24be", FORMAT_PCM24_BE },
  { "pcm32be", FORMAT_PCM32_BE },
  { "pcm_float",  FORMAT_PCMFLOAT },
  { "pcm_double", FORMAT_PCMDOUBLE },

  // Backwards compatibility
  { "0", FORMAT_PCM16 },
  { "1", FORMAT_PCM24 },
  { "2", FORMAT_PCM32 },
  { "3", FORMAT_PCM16_BE },
  { "4", FORMAT_PCM24_BE },
  { "5", FORMAT_PCM32_BE },
  { "6", FORMAT_PCMFLOAT },
  { "7", FORMAT_PCMDOUBLE },
};

const enum_opt delay_units_tbl[] =
{
  { "sm", DELAY_SP },
  { "ms", DELAY_MS },
  { "m",  DELAY_M  },
  { "cm", DELAY_CM },
  { "ft", DELAY_FT },
  { "in", DELAY_IN },

  // Backwards compatibility
  { "0", DELAY_SP },
  { "1", DELAY_MS },
  { "2", DELAY_M  },
  { "3", DELAY_CM },
  { "4", DELAY_FT },
  { "5", DELAY_IN },
};

const enum_opt log_level_tbl[] =
{
  { "critical",  log_critical  },
  { "exception", log_exception },
  { "error",     log_error     },
  { "warning",   log_warning   },
  { "event",     log_event     },
  { "trace",     log_trace     },
  { "all",       log_all       },
};

int valdec(const arg_list_t &args)
{
  using std::string;
  if (args.size() < 2)
  {
    printf(usage);
    return 1;
  }

  int i, j;
  bool print_info = false;
  bool print_opt  = false;
  bool print_hist = false;

  /////////////////////////////////////////////////////////
  // Processing log

  LogFile logfile;

  /////////////////////////////////////////////////////////
  // Input file

  const char *input_filename = args[1].raw.c_str();
  FileParser file;

  /////////////////////////////////////////////////////////
  // Parsers

  UniFrameParser uni;
  FrameParser *parser = 0;

  /////////////////////////////////////////////////////////
  // Arrays

  int delay_units = DELAY_SP;
  float delays[CH_NAMES];
  for (i = 0; i < CH_NAMES; i++)
    delays[i] = 0;

  sample_t gains[CH_NAMES];
  for (i = 0; i < CH_NAMES; i++)
    gains[i] = 1.0;

  matrix_t m;
  for (i = 0; i < CH_NAMES; i++)
    for (j = 0; j < CH_NAMES; j++)
      m[i][j] = 0;

  /////////////////////////////////////////////////////////
  // Output format

  int format = FORMAT_PCM16;
  int mask = 0;
  int sample_rate = 0;

  /////////////////////////////////////////////////////////
  // Sinks

  enum { mode_undefined, mode_nothing, mode_play, mode_raw, mode_wav, mode_decode } mode = mode_undefined;
  const char *out_filename = 0;

  RAWSink    raw;
  WAVSink    wav;
  DSoundSink dsound;
  NullSink   null;

  Sink *sink = 0;
  PlaybackControl *control = 0;

  /////////////////////////////////////////////////////////
  // Filters
  /////////////////////////////////////////////////////////

  DVDGraph dvd_graph;

  /////////////////////////////////////////////////////////
  // Parse arguments
  /////////////////////////////////////////////////////////

  for (size_t iarg = 2; iarg < args.size(); iarg++)
  {
    const arg_t &arg = args[iarg];

    ///////////////////////////////////////////////////////
    // Parsers
    ///////////////////////////////////////////////////////

    // -ac3 - force ac3 parser (do not autodetect format)
    if (arg.is_option("ac3", argt_exist))
    {
      if (parser)
      {
        printf("-ac3 : ambiguous parser\n");
        return 1;
      }

      parser = &uni.ac3;
      continue;
    }

    // -dts - force dts parser (do not autodetect format)
    if (arg.is_option("dts", argt_exist))
    {
      if (parser)
      {
        printf("-dts : ambiguous parser\n");
        return 1;
      }

      parser = &uni.dts;
      continue;
    }

    // -mpa - force mpa parser (do not autodetect format)
    if (arg.is_option("mpa", argt_exist))
    {
      if (parser)
      {
        printf("-mpa : ambiguous parser\n");
        return 1;
      }

      parser = &uni.mpa;
      continue;
    }

    ///////////////////////////////////////////////////////
    // Output format
    ///////////////////////////////////////////////////////

    // -spk - number of speakers
    if (arg.is_option("spk", argt_enum))
    {
      int new_mask = arg.choose(mask_tbl, array_size(mask_tbl));
      mask |= new_mask;
      continue;
    }

    // -fmt - sample format
    if (arg.is_option("fmt", argt_enum))
    {
      format = arg.choose(format_tbl, array_size(format_tbl));
      continue;
    }

    // -rate - sample rate
    if (arg.is_option("rate", argt_int))
    {
      sample_rate = arg.as_int();
      continue;
    }

    ///////////////////////////////////////////////////////
    // Sinks
    ///////////////////////////////////////////////////////

    // -d[ecode] - decode
    if (arg.is_option("d", argt_exist) || 
        arg.is_option("decode", argt_exist))
    {
      if (sink)
      {
        printf("-decode : ambiguous output mode\n");
        return 1;
      }

      sink = &null;
      control = 0;
      mode = mode_decode;
      continue;
    }

    // -p[lay] - play
    if (arg.is_option("p", argt_exist) || 
        arg.is_option("play", argt_exist))
    {
      if (sink)
      {
        printf("-play : ambiguous output mode\n");
        return 1;
      }

      sink = &dsound;
      control = &dsound;
      mode = mode_play;
      continue;
    }
    
    // -r[aw] - RAW output
    if (arg.is_option("r", argt_exist) ||
        arg.is_option("raw", argt_exist))
    {
      if (sink)
      {
        printf("-raw : ambiguous output mode\n");
        return 1;
      }
      if (args.size() - iarg < 1)
      {
        printf("-raw : specify a file name\n");
        return 1;
      }

      out_filename = args[++iarg].raw.c_str();
      sink = &raw;
      control = 0;
      mode = mode_raw;
      continue;
    }

    // -w[av] - WAV output
    if (arg.is_option("w", argt_exist) ||
        arg.is_option("wav", argt_exist))
    {
      if (sink)
      {
        printf("-wav : ambiguous output mode\n");
        return 1;
      }
      if (args.size() - iarg < 1)
      {
        printf("-wav : specify a file name\n");
        return 1;
      }

      out_filename = args[++iarg].raw.c_str();
      sink = &wav;
      control = 0;
      mode = mode_wav;
      continue;
    }

    // -n[othing] - no output
    if (arg.is_option("n", argt_exist) || 
        arg.is_option("nothing", argt_exist))
    {
      if (sink)
      {
        printf("-nothing : ambiguous output mode\n");
        return 1;
      }

      sink = &null;
      control = 0;
      mode = mode_nothing;
      continue;
    }
    
    ///////////////////////////////////////////////////////
    // Info
    ///////////////////////////////////////////////////////

    // -i - print bitstream info
    if (arg.is_option("i", argt_exist))
    {
      print_info = true;
      continue;
    }

    // -opt - print decoding options
    if (arg.is_option("opt", argt_exist))
    {
      print_opt = true;
      continue;
    }

    // -hist - print levels histogram
    if (arg.is_option("hist", argt_exist))
    {
      print_hist = true;
      continue;
    }

    // -log dump processing log to a file
    if (arg.is_option("log", argt_exist))
    {
      if (args.size() - iarg < 1)
      {
        printf("-log : specify a file name\n");
        return 1;
      }

      const char *log_filename = args[++iarg].raw.c_str();
      logfile.open(log_filename);
      logfile.subscribe(&valib_log_dispatcher);
      continue;
    }

    // -log_level
    if (arg.is_option("log_level", argt_enum))
    {
      int level = arg.choose(log_level_tbl, array_size(log_level_tbl));
      logfile.set_max_log_level(level);
      continue;
    }

    ///////////////////////////////////////////////////////
    // Mixer options
    ///////////////////////////////////////////////////////

    // -auto_matrix
    if (arg.is_option("auto_matrix", argt_bool))
    {
      dvd_graph.proc.set_auto_matrix(arg.as_bool());
      continue;
    }

    // -normalize_matrix
    if (arg.is_option("normalize_matrix", argt_bool))
    {
      dvd_graph.proc.set_normalize_matrix(arg.as_bool());
      continue;
    }

    // -voice_control
    if (arg.is_option("voice_control", argt_bool))
    {
      dvd_graph.proc.set_voice_control(arg.as_bool());
      continue;
    }

    // -expand_stereo
    if (arg.is_option("expand_stereo", argt_bool))
    {
      dvd_graph.proc.set_expand_stereo(arg.as_bool());
      continue;
    }

    // -clev
    if (arg.is_option("clev", argt_double))
    {
      dvd_graph.proc.set_clev(db2value(arg.as_double()));
      continue;
    }

    // -slev
    if (arg.is_option("slev", argt_double))
    {
      dvd_graph.proc.set_slev(db2value(arg.as_double()));
      continue;
    }

    // -lfelev
    if (arg.is_option("lfelev", argt_double))
    {
      dvd_graph.proc.set_lfelev(db2value(arg.as_double()));
      continue;
    }

    // -gain
    if (arg.is_option("gain", argt_double))
    {
      dvd_graph.proc.set_master(db2value(arg.as_double()));
      continue;
    }

    // -gain_{ch}
    bool have_gain = false;
    for (i = 0; i < array_size(ch_map) && !have_gain; i++)
    {
      string opt = string("gain_") + ch_map[i].name;
      if (arg.is_option(opt, argt_double))
      {
        gains[ch_map[i].ch] = db2value(arg.as_double());
        have_gain = true;
      }
    }
    if (have_gain) continue;

    // -{ch}_{ch}
    bool have_matrix = false;
    for (i = 0; i < array_size(ch_map) && !have_matrix; i++)
      for (j = 0; j < array_size(ch_map) && !have_matrix; j++)
      {
        string opt = string(ch_map[i].name) + string("_") + string(ch_map[j].name);
        if (arg.is_option(opt, argt_double))
        {
          m[ch_map[i].ch][ch_map[j].ch] = arg.as_double();
          have_matrix = true;
        }
      }
    if (have_matrix) continue;

    ///////////////////////////////////////////////////////
    // Auto gain control options
    ///////////////////////////////////////////////////////

    // -agc
    if (arg.is_option("agc", argt_bool))
    {
      dvd_graph.proc.set_auto_gain(arg.as_bool());
      continue;
    }

    // -normalize
    if (arg.is_option("normalize", argt_bool))
    {
      dvd_graph.proc.set_normalize(arg.as_bool());
      continue;
    }

    // -drc
    if (arg.is_option("drc", argt_bool))
    {
      dvd_graph.proc.set_drc(arg.as_bool());
      continue;
    }

    // -drc_power
    if (arg.is_option("drc_power", argt_double))
    {
      dvd_graph.proc.set_drc(true);
      dvd_graph.proc.set_drc_power(arg.as_double());
      continue;
    }

    // -attack
    if (arg.is_option("attack", argt_double))
    {
      dvd_graph.proc.set_attack(db2value(arg.as_double()));
      continue;
    }

    // -release
    if (arg.is_option("release", argt_double))
    {
      dvd_graph.proc.set_release(db2value(arg.as_double()));
      continue;
    }

    ///////////////////////////////////////////////////////
    // Delay options
    ///////////////////////////////////////////////////////

    // -delay
    if (arg.is_option("delay", argt_bool))
    {
      dvd_graph.proc.set_delay(arg.as_bool());
      continue;
    }

    // -delay_units
    if (arg.is_option("delay_units", argt_enum))
    {
      delay_units = arg.choose(delay_units_tbl, array_size(delay_units_tbl));
      continue;
    }

    // -delay_{ch}
    bool have_delay = false;
    for (i = 0; i < array_size(ch_map) && !have_delay; i++)
    {
      string opt = string("delay_") + string(ch_map[i].name);
      if (arg.is_option(opt, argt_double))
      {
        delays[ch_map[i].ch] = float(arg.as_double());
        have_delay = true;
      }
    }
    if (have_delay) continue;

    printf("Error: unknown option: %s\n", arg.raw.c_str());
    return 1;
  }

  /////////////////////////////////////////////////////////
  // Open input file and load the first frame
  /////////////////////////////////////////////////////////

  if (!parser)
    parser = &uni;

  if (!file.open(input_filename, parser, 1000000))
  {
    printf("Error: Cannot open file '%s'\n", input_filename);
    return 1;
  }

  if (!file.stats() || !file.probe())
  {
    printf("Error: Cannot detect input file format\n", input_filename);
    return 1;
  }

  /////////////////////////////////////////////////////////
  // Print file info
  /////////////////////////////////////////////////////////

  if (print_info)
  {
    printf("%s\n", file.file_info().c_str());
    printf("%s", file.stream_info().c_str());
  }

  if (mode == mode_nothing)
    return 0;

  /////////////////////////////////////////////////////////
  // Setup processing
  /////////////////////////////////////////////////////////

  dvd_graph.proc.set_delay_units(delay_units);
  dvd_graph.proc.set_delays(delays);
  dvd_graph.proc.set_output_gains(gains);
  dvd_graph.proc.set_input_order(std_order);
  dvd_graph.proc.set_output_order(win_order);
  if (!dvd_graph.proc.get_auto_matrix())
    dvd_graph.proc.set_matrix(m);
  
  Speakers user_spk(format, mask, sample_rate);
  if (!dvd_graph.set_user(user_spk))
  {
    printf("Error: unsupported user format %s\n", user_spk.print());
    return 1;
  }

  Speakers in_spk = file.get_output();
  if (!dvd_graph.open(in_spk))
  {
    printf("Error: unsupported file format %s\n", in_spk.print());
    return 1;
  }

  Speakers out_spk = user_spk;
  if (!out_spk.mask)
    out_spk.mask = in_spk.mask;
  if (!out_spk.sample_rate)
    out_spk.sample_rate = in_spk.sample_rate;

  /////////////////////////////////////////////////////////
  // Print processing options
  /////////////////////////////////////////////////////////

  //
  // TODO
  //

  /////////////////////////////////////////////////////////
  // Open output file
  /////////////////////////////////////////////////////////

  if (!sink)
  {
    sink = &dsound;
    control = &dsound;
    mode = mode_play;
  }
   
  switch (mode)
  {
    case mode_raw:
      if (!out_filename || !raw.open_file(out_filename))
      {
        printf("Error: failed to open output file '%s'\n", out_filename);
        return 1;
      }
      break;

    case mode_wav:
      if (!out_filename || !wav.open_file(out_filename))
      {
        printf("Error: failed to open output file '%s'\n", out_filename);
        return 1;
      }
      break;

    case mode_play:
      if (!dsound.open_dsound(0))
      {
        printf("Error: failed to init DirectSound\n");
        return 1;
      }
      break;

    // do nothing for other modes
  }

  /////////////////////////////////////////////////////////
  // Process
  /////////////////////////////////////////////////////////

  Chunk chunk, out_chunk;

  CPUMeter cpu_current;
  CPUMeter cpu_total;

  cpu_current.start();
  cpu_total.start();

  double time = 0;
  double old_time = 0;

  sample_t levels[CH_NAMES];
  sample_t level = 0;

  int streams = 0;

//  fprintf(stderr, " 0.0%% Frs:      0 Err: 0 Time:   0:00.000i Level:    0dB FPS:    0 CPU: 0%%\r"); 

  file.seek(0);

  #define PRINT_STAT                                                                                           \
  {                                                                                                            \
    if (control)                                                                                               \
    {                                                                                                          \
      dvd_graph.proc.get_output_levels(control->get_playback_time(), levels);                                  \
      level = levels[0];                                                                                       \
      for (i = 1; i < CH_NAMES; i++)                                                                           \
        if (levels[i] > level)                                                                                 \
          level = levels[i];                                                                                   \
    }                                                                                                          \
    fprintf(stderr, "%4.1f%% Frames: %-6i Time: %3i:%02i.%03i Level: %-4idB FPS: %-4i CPU: %.1f%%  \r",        \
      file.get_pos(file.relative) * 100,                                                                       \
      file.get_frames(),                                                                                       \
      int(time/60), int(time) % 60, int(time * 1000) % 1000,                                                   \
      int(value2db(level)),                                                                                    \
      int(file.get_frames() / time),                                                                           \
      cpu_current.usage() * 100);                                                                              \
  }

  #define DROP_STAT \
    fprintf(stderr, "                                                                             \r");

  while (file.get_chunk(chunk))
  {
    /////////////////////////////////////////////////////
    // Switch to a new stream

    if (file.new_stream())
    {
      if (streams > 0)
        PRINT_STAT;

      if (streams > 0 && print_info)
        printf("\n\n%s", file.stream_info().c_str());

      streams++;
      if (mode == mode_nothing)
        return 0;
    }

    while (dvd_graph.process(chunk, out_chunk))
    {
      if (dvd_graph.new_stream())
      {
        Speakers new_spk = dvd_graph.get_output();
        if (sink->open(new_spk))
        {
          DROP_STAT;
          printf("Opening audio output %s...\n", new_spk.print().c_str());
        }
        else
        {
          printf("\nOutput format %s is unsupported\n", new_spk.print().c_str());
          return 1;
        }
      }
      sink->process(out_chunk);
    }

    /////////////////////////////////////////////////////
    // Statistics

    time = cpu_total.get_system_time();
    if (time > old_time + 0.1)
    {
      old_time = time;
      PRINT_STAT;
    }

  }

  while (dvd_graph.flush(out_chunk))
  {
    if (dvd_graph.new_stream())
    {
      Speakers new_spk = dvd_graph.get_output();
      if (sink->open(new_spk))
      {
        DROP_STAT;
        printf("Opening audio output %s...\n", new_spk.print().c_str());
      }
      else
      {
        printf("\nOutput format %s is unsupported\n", new_spk.print().c_str());
        return 1;
      }
    }
    sink->process(out_chunk);
  }

  sink->flush();

  /////////////////////////////////////////////////////
  // Stop

  cpu_current.stop();
  cpu_total.stop();

  /////////////////////////////////////////////////////
  // Final statistics

  PRINT_STAT;
  printf("\n---------------------------------------\n");
  if (streams > 1)
    printf("Streams found: %i\n", streams);
  printf("Frames: %i\n", file.get_frames());
  printf("System time: %ims\n", int(cpu_total.get_system_time() * 1000));
  printf("Process time: %ims\n", int(cpu_total.get_thread_time() * 1000 ));
  printf("Approx. %.2f%% realtime CPU usage\n", double(cpu_total.get_thread_time() * 100) / file.get_size(file.time));

  /////////////////////////////////////////////////////////
  // Print levels histogram
  /////////////////////////////////////////////////////////

  if (print_hist)
  {
    sample_t hist[MAX_HISTOGRAM];
    sample_t max_level;
    int dbpb;
    int i, j;

    dvd_graph.proc.get_output_histogram(hist, MAX_HISTOGRAM);
    max_level = dvd_graph.proc.get_max_level();
    dbpb = dvd_graph.proc.get_dbpb();

    printf("\nHistogram:\n");
    printf("------------------------------------------------------------------------------\n");
    for (i = 0; i*dbpb < 100 && i < MAX_HISTOGRAM; i++)
    {
      printf("%2idB: %4.1f ", i * dbpb, hist[i] * 100);
      for (j = 0; j < 67 && j < hist[i] * 67; j++)
        printf("*");
      printf("\n");
    }
    printf("------------------------------------------------------------------------------\n");
    printf("max_level;%f\ndbpb;%i\nhistogram;", max_level, dbpb);
    for (i = 0; i < MAX_HISTOGRAM; i++)
      printf("%.4f;", hist[i]);
    printf("\n");
    printf("------------------------------------------------------------------------------\n");
  }

  return 0;
}

int main(int argc, const char *argv[])
{
  try
  {
    return valdec(args_utf8(argc, argv));
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
