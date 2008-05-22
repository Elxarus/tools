@echo off
msdev ac3enc.dsp     /MAKE "ac3enc - Win32 Release"
msdev bsconvert.dsp  /MAKE "bsconvert - Win32 Release"
msdev equalizer.dsp  /MAKE "equalizer - Win32 Release"
msdev filter.dsp     /MAKE "filter - Win32 Release"
msdev mpeg_demux.dsp /MAKE "mpeg_demux - Win32 Release"
msdev noise.dsp      /MAKE "noise - Win32 Release"
msdev resample.dsp   /MAKE "resample - Win32 Release"
msdev spdifer.dsp    /MAKE "spdifer - Win32 Release"
msdev swab.dsp       /MAKE "swab - Win32 Release"
msdev valdec.dsp     /MAKE "valdec - Win32 Release"
call _clear.bat
