#include <stdio.h>
#include <process.h>
#include "win32/cpu.h"

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    printf(
"cpu_meter\n"
"=========\n"
"Measure the CPU time consumed by a program run. Useful for automated testing.\n"
"\n"
"This utility is a part of AC3Filter project (http://ac3filter.net)\n"
"Copyright (c) 2007 by Alexander Vigovsky\n"
"\n"
"Usage:\n"
"  > cpu_meter program [arg1 [arg2 [...]]\n"
    );
    return 0;
  }

  CPUMeter cpu;
  cpu.start();
  _spawnv(_P_WAIT, argv[1], argv+1);
  cpu.stop();
  printf("\n");
  printf("Program time: %ims\n", int(cpu.get_thread_time() * 1000));
  printf("System time: %ims\n", int(cpu.get_system_time() * 1000));
  return int(cpu.get_thread_time() * 1000);
}
