/******************************************************************************
This file sets the console code page to UTF-8 when programs starts and reverts
it back on finish. It is required for a program to be able to do UTF-8 output
in Windows. Just link this file with your programs.
******************************************************************************/

#include <windows.h>

class UTF8Console
{
public:
  UINT old_cp;

  UTF8Console()
  {
    old_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
  }

  ~UTF8Console()
  {
    SetConsoleOutputCP(old_cp);
  }
};

volatile static UTF8Console utf8_console;
