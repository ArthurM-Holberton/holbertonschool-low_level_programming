# Green Efficiency Analysis

## Measurement Methodology

All programs were compile with `gcc -Wall -Werror -Wextra -pedantic -std=gnu89 -Wno-long-long` and run on the same machine, .

I used the function clock() which is used to get the CPU time of a process,CPU time only counts while the CPU is working for the program, not while it is waiting.To get the time in seconds I subtracted the clock() value taken before the code from the one taken after, then divided the result by CLOCKS_PER_SEC.

Except for `instrumentation_lab`, I always ran the programs three times in a row, and I kept all the measurements. I ran `instrumentation_lab` only once.

In `instrumentation_lab`, I placed one timer around each phase (build, process, reduce) and one around the whole program. I printed the results after all the timers had stopped, so that `printf()` was not included in the measured times.

I used the same machine for all the measurements.

## Observed Performance Differences

The baseline loop of 100,000,000 iterations took 0.266475 s, 0.547261 s and 0.263418 s. Two runs agree within 1.2%, but the second one is about 2.1 times slower although the program is identical. A single measurement would therefore have been misleading and false. The median (0.266475 s) describes the typical run better than the mean (0.359051 s), which is pulled up by the outlier.

In the algorithm comparison, the naive version averaged 7.231356 s and the single-pass version 0.000313 s over three runs, so the naive one is about 23,100 times slower. It performs 1,250,025,000 inner iterations against 50,000 for the single-pass version, a theoretical ratio of 25,000, which is close to the measured one. The cost per iteration is almost the same (about 5.8 ns compared with 6.3 ns), so the gap comes from the number of iteration, not from how fast each one runs. The two versions do not return the same value: 625025000 for the naive one and 25000 for the single-pass one, because the naive  version recount each elements many times.

In the instrumentation lab, the measured times were TOTAL 0.001594 s, BUILD_DATA 0.000695 s, PROCESS 0.000574 s and REDUCE 0.000316 s. BUILD_DATA took the most time (about 44% of the total), followed by PROCESS (about 36%) and REDUCE (about 20%). The three phases add up to 0.001585 s, so TOTAL is larger than their sum by about 0.000009 s, which is consistent with TOTAL also covering the gaps between phases.

## Relation Between Runtime and Energy Consumption

Runtime is used here as a proxy. While a program runs, the CPU stays active, so a longer run generally means more energy used, all else being equal. the naive algorithm. kept the CPU busy for about 7.2 s, while the single-pass one finished in about 0.3 ms. The naive one therefore probably uses far more energy, and the gap would multiply if it ran on many machines or many times. This is an inference from runtime, not a measurement: none of these programs measured power in watts, and the two versions also do different amounts of counting work.

## Limitations of the Experiment

Only three runs were made for the baseline and the comparison, on a single machine, which is not enough for statistics, as the 2.1x outlier in the baseline shows. The instrumentation lab was measured only once, and each phase lasts well under a millisecond, close to the resolution of `clock()`, so the ordering of the phases could change from one run to another.


## Practical Engineering Takeaway

Choosing a better algorithm had more effect than anything else measured here: going from O(n^2) to O(n) reduced the time by a factor of about 23,100. Measure several times and report the median rather than a single value. Check what each implementation actually returns before comparing speed. Treat runtime as an indicator of energy use, not as a measurement of it.
