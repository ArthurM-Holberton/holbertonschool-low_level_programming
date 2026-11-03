# Green Efficiency Analysis

## Measurement Methodology

I compiled all my C programs with gcc -Wall -Werror -Wextra -pedantic -std=gnu89 -Wno-long-long.

I used the function clock() which is used to get the CPU time of a process,CPU time only counts while the CPU is working for the program, not while it is waiting.To get the time in seconds I subtracted the clock() value taken before the code from the one taken after, then divided the result by CLOCKS_PER_SEC.

Except instrumentation_lab I always ran three times in a row the programs and i kept all the measurements.

In instrumentation_lab I place a timers in each phase (build, process, reduce) and one for the total.I placed the printf() at the end because ,printf takes too much time to operate and i dont want it to be included in the result.

I always used the same machine.