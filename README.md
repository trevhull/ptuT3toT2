# ptuT3toT2
This is a program that converts ptu files from picoquant from T3 mode to T2 mode. Modified from ptudemo and ht3toht2 files provided by picoquant. Not sure about license stuff pls dont sue me picoquant!

Tested on timeharp ptu files on windows 64 bit.

to do:

expand to non timeharp files (I'm never going to do this lol so feel free to fork!)


How to guide:
This is a command line tool, no GUI. use your shell in Linux or the Command Prompt in windows.
for Linux you may have to compile using gcc. If you're really stuck let me know but I mostly forgot which flags I used. Sorry! follow the error messages or message me for help.

Before use, make a T3 measurement on a picoquant instrument. It needs to be T3 to properly apply time gate. At the end you'll have a T2 with timegate. Also useful if you want both a T3 and T2 file from one measurement.

usage: ptuT3toT2 infile outfile [timegate_start timegate_end]
infile is a HydraHarp/TimeHarp PTU T3 file (binary)
outfile is a HydraHarp/TimeHarp PTU T2 file (binary)
the optional parameters timegate_start and timegate_end are in picoseconds but it seems like it's not really optional
If you just wanna have no time gate put in the time gate as 0 100000000000 or something

some command line examples:
  /path/to/ptuT3toT2 /path/to/sampleT3data.ptu /path/to/convertedT2data.ptu 0 1000000000000
  ----to avoid a time gate just make the time window so large it encompassess all timing events (in this case 0 ps to 1000000000000 ps)
  ----sampleT3data.ptu is the T3 file you measured from the instrument
  ----convertedT2data.ptu is the T2 file this program spits out
  
  for windows use:
  /path/to/ptuT3toT2.exe /path/to/sampleT3data.ptu /path/to/convertedT2data.ptu 0 1000000000000
  ----executable should work but you can also compile again
  
  for a 10 nanosecond time gate
  /path/to/ptuT3toT2 /path/to/sampleT3data.ptu /path/to/convertedT2data.ptu 0 10000
  ----remember, the timegate is in picoseconds
