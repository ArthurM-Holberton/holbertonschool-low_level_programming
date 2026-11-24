# Crash report: null pointer write in `crash_example`

Lab: *AI Memory Visualizer + Valgrind Tracer*, task 2 (`analysis/crash_report.md`). Program: `crash_example.c` (38 lines), built with the Makefile flags (`-Wall -Wextra -Werror -pedantic -std=gnu89 -g`). Environment: Ubuntu 24.04 x86-64, Linux 6.18, gcc 13.3.0, glibc 2.39, gdb 15.1, Valgrind 3.22.0, clang 18.1.3 (static analysis only). Line numbers refer to `crash_example.c`.

**Process.** The crash was reproduced and measured on the unmodified program (a plain run, gdb, strace, a core dump, Valgrind). An AI tool (Claude) was asked for possible causes and fixes. Its answer was saved unchanged before the program was run for this task; section 4 gives a verdict on every item and Appendix B reproduces the answer word for word. Experiments that change the code (variants of the program) were run on scratch copies; the project files were not modified.

**Summary**

- **Root cause.** `main` uses the pointer returned by `allocate_numbers(0)` without checking it. The function returns NULL for `n <= 0` (lines 9-10), before any `malloc`, so `nums` is NULL and line 32 stores 4 bytes at address 0.
- **Category of undefined behaviour.** Dereference of a null pointer (C11 6.5.3.2p4; CWE-476; CERT EXP34-C), reached through an unchecked return value (CWE-252).
- **Memory involved.** The faulting address is in neither the stack nor the heap. The stack holds the null pointer, the heap array was never created, and address 0 is not mapped.
- **Why the process receives a signal.** No page is mapped at address 0. The store raises a page fault, the kernel finds no mapping for the address and sends SIGSEGV with `SEGV_MAPERR` and address 0.
- **AI assistance.** The AI's main diagnosis and its main fix were correct. Two of its secondary predictions, about `-O2` and about static analysis, were partly wrong or wrong, and five of its low-confidence alternative causes were ruled out by the evidence (section 4).

---

## 1. Description of the crash

The program prints its title and the requested size (`n=0`), then dies on line 32, before it can print `nums[0]=...`. What happens on that line is a 4-byte store of the value 42 to address 0, and "Segmentation fault" is only the name the shell gives to the way that store ends.

| Observation | Value | Evidence |
|---|---|---|
| Output on a terminal | two lines, the title and `  requesting n=0`; never `nums[0]=...` | E1 |
| Output with stdout redirected to a file or a pipe | 0 bytes: the two lines are still in the stdio buffer when the process dies | E1, E5 |
| End of the process | killed by signal 11; the shell reports status 139 (128 + 11) and prints "Segmentation fault" itself, the program writes nothing to stderr | E1 |
| Repeatability | 20 runs out of 20, ASLR on | E1 |
| Instruction that faults | `movl $0x2a,(%rax)` at `crash_example.c:32` (`main+85`), with `rax = 0` | E2 |
| Fault address and kind | `si_addr = 0x0`, `si_code = SEGV_MAPERR` ("address not mapped to object") | E2, E5, E8 |
| Values in `main` at the fault | `nums = 0x0`, `n = 0` | E2 |

```text
Program received signal SIGSEGV, Segmentation fault.
0x000055555555528a in main () at crash_example.c:32
32          nums[0] = 42;
=> 0x55555555528a <main+85>:     movl   $0x2a,(%rax)
rax            0x0                 0
p nums               -> (int *) 0x0
p n                  -> 0
p $_siginfo.si_code  -> 1   (SEGV_MAPERR)
```

---

## 2. Root cause analysis

### 2.1 The code involved

```text
 4  static int *allocate_numbers(int n)
 5  {
 6      int *arr = NULL;
 7      int i = 0;
 8
 9      if (n <= 0)
10          return NULL;                 <- n = 0 leaves the function here
11
12      arr = (int *)malloc((size_t)n * sizeof(int));     <- never executed
13      if (!arr)
14          return NULL;                 <- the other way to return NULL
...
22  int main(void)
23  {
24      int *nums = NULL;
25      int n = 0;                       <- the value that triggers the failure
26
27      printf("crash_example: deterministic NULL dereference (segmentation fault)\n");
28      printf("  requesting n=%d\n", n);
29
30      nums = allocate_numbers(n);      <- NULL is stored and never checked
31
32      nums[0] = 42;                    <- *(nums + 0) = 42: a store at address 0
```

### 2.2 Causal chain, derived from the source

| Step | Line | What happens | State afterwards |
|---|---|---|---|
| 1 | 24-25 | the frame of `main` (stack) holds `nums = NULL` and `n = 0` | `n` = 0 |
| 2 | 30 | `allocate_numbers(0)` is called | argument `n` = 0 |
| 3 | 9-10 | the condition `n <= 0` is true and the function returns NULL | lines 12-19 never run: no `malloc`, no loop, no array |
| 4 | 30 | the return value is stored into `nums` | `nums` = NULL |
| 5 | 32 | `nums[0] = 42` means `*(nums + 0) = 42`: the code loads `nums` from the stack and stores 4 bytes through it | the address of the store is 0 |
| 6 | 32 | the operand of `*` is a null pointer: undefined behaviour (C11 6.5.3.2p4). A null pointer points to no object (C11 6.3.2.3p3), so there is nothing to write to | the language promises nothing about what follows |
| 7 | CPU, kernel | the store to address 0 raises a page fault, no mapping covers address 0, the kernel sends SIGSEGV | process terminated, status 139 |

Steps 1 to 6 follow from the source alone; step 7 adds one platform rule, that page 0 is not mapped (section 3). The machine code makes the same choices (E3): `allocate_numbers` compares `n` with 0 and returns 0 when it is not greater, the call to `malloc` is only reached when it is, and `main` stores the returned value, reloads it and executes the store.

### 2.3 What exactly is "the bug"

| Level | What it is | Where |
|---|---|---|
| Trigger | the request `n = 0`, which `allocate_numbers` treats as invalid | line 25 |
| Defect | the result of `allocate_numbers` is used without being checked, although the function returns NULL both for `n <= 0` and when `malloc` fails | lines 30 and 32 |
| Undefined behaviour | dereference of a null pointer | line 32 |
| Fault | page fault at address 0, delivered to the process as SIGSEGV | CPU and kernel |
| Crash | termination of the process | shell: status 139 |

The segmentation fault is the last item of this chain, not its cause: the missing check belongs between lines 30 and 32, and the trigger (line 25) comes seven lines before the faulting store.

### 2.4 Category of undefined behaviour

**Null pointer dereference.** The language says that applying `*` to an invalid value is undefined behaviour and lists the null pointer among the invalid values (C11 6.5.3.2p4). The same defect is catalogued as CWE-476 (NULL Pointer Dereference), here reached through CWE-252 (Unchecked Return Value), and as CERT C rule EXP34-C (do not dereference null pointers).

It is not a use after free (nothing was freed), not a buffer overflow (no object exists to overflow) and not the use of an uninitialised pointer: `nums` is initialised to NULL on line 24 (E3).

### 2.5 Stack, heap or both

| Region | Role in this crash | Evidence |
|---|---|---|
| Stack | holds `n` (at `0x7fffffff9a94`) and `nums` (at `0x7fffffff9a98`) in the frame of `main` (frame address `0x7fffffff9ab0`). Reading and writing these two variables is valid; `nums` contains 0 | E2 |
| Heap | would have held the array of `n` ints, which was never created. The code of the program never reaches `malloc`: the only call to it after `main` starts comes from libc, which allocates the 4,096-byte `stdout` buffer inside `puts`. At the fault the heap holds only libc's own blocks: the tcache structure (size field `0x291`) and the `stdout` buffer (`0x1011`) | E2, E4 |
| Address 0 | target of the faulting store; belongs to no mapping | E2 |

Both regions appear in the story, because the bad pointer lives on the stack and the missing array would have lived on the heap, but the invalid access itself touches neither of them.

### 2.6 A second defect behind the first

Checking for NULL is necessary but is not the whole story. `n = 0` means an array of zero elements, and such an array has no element 0. If the guard on lines 9-10 is removed, `malloc(0)` returns a non-NULL pointer with glibc, the crash disappears, and `nums[0] = 42` becomes a write into a block requested with size 0: Valgrind reports it as an invalid write "0 bytes after a block of size 0 alloc'd" (E9b). Line 32 is wrong for `n = 0` whatever the pointer is. The check of the return value is sufficient only because `allocate_numbers` returns non-NULL exactly when `n >= 1` and `malloc` succeeded.

### 2.7 Why it crashes every time

All inputs are constants of the source (`n = 0` on line 25). Nothing depends on time or on user input, and the faulting address is 0, which ASLR does not move. 20 runs out of 20 ended with status 139 (E1), and each tool that prints the fault address (gdb, strace and the handler of E8) showed 0. The failure is a deterministic consequence of the code, not an unpredictable event.

---

## 3. Why the memory access is invalid

### 3.1 In the language: there is no object

A pointer holding 0 is a null pointer, which by definition points to no object (C11 6.3.2.3p3). The array that `nums` was supposed to designate does not exist, so the store has no target. Nothing about the access is "almost valid": there is no element, no bounds, no lifetime to discuss.

### 3.2 In the process: there is no mapping

The address space of the process at the fault (gdb, E2):

```text
 0x0000000000000000  nothing mapped                  <- the store goes here
          ...        (no mapping below the program image)
 0x0000555555554000  crash_example (r--, r-x, r--, r--, rw-)
 0x0000555555559000  [heap]   up to 0x55555557a000
 0x00007ffff7c00000  libc, ld.so, vdso, ...
 0x00007ffffffda000  [stack]  up to 0x7ffffffff000     (n and nums are in here)
```

The kernel also refuses to let an ordinary program map the lowest pages: `vm.mmap_min_addr` (file `/proc/sys/vm/mmap_min_addr`) is 4096 on this machine (E2c), so with this setting page 0 cannot be mapped by a normal program. A null dereference therefore lands on an unmapped page.

### 3.3 From the store to the signal

1. The CPU cannot translate address 0: the page tables have no entry for page 0.
2. It raises a page fault (trap 14) with the error code 6: user mode, write access, page not present. A signal handler can read both numbers from the saved context (E8).
3. The kernel looks for a mapping that contains the address and finds none, so it sends SIGSEGV with `si_code = SEGV_MAPERR` and `si_addr = 0` (E2, E5, E8). `sigaction(2)` documents the other code, `SEGV_ACCERR`, for an address that is mapped but not accessible in the requested way; it was not produced here.
4. The default action of SIGSEGV is to terminate the process, with a core dump if the limits allow it (E6).
5. The shell sees a process killed by signal 11, prints "Segmentation fault" and reports status 139.

### 3.4 The same event seen by other tools

| Tool | What it prints | Evidence |
|---|---|---|
| Valgrind | `Invalid write of size 4` at line 32, `Address 0x0 is not stack'd, malloc'd or (recently) free'd`, then `Access not within mapped region at address 0x0` | E7 |
| AddressSanitizer | `SEGV on unknown address 0x000000000000`, `The signal is caused by a WRITE memory access`, `address points to the zero page` | E13 |
| strace | `--- SIGSEGV {si_signo=SIGSEGV, si_code=SEGV_MAPERR, si_addr=NULL} ---` | E5 |

These are three descriptions of the same store. The Valgrind view of the program is analysed further in `valgrind_analysis.md`, section 5.

### 3.5 The crash is not guaranteed by the language

SIGSEGV is what this platform does with the undefined behaviour at `-O0`; the standard does not promise it. With `-O2`, GCC 13.3 recognises that `nums` is NULL at line 32 and rewrites the statement: the call to `allocate_numbers` disappears, the store writes 0 instead of 42, and a `ud2` trap instruction follows it (E12). The process still dies with SIGSEGV, because the store faults before the trap, but the code that runs is no longer the code that was written. On a system that has memory mapped at address 0 (not tested here), the store would not fault at all.

---

## 4. AI-assisted analysis

### 4.1 Method and limits

The AI answer was produced from the source of `crash_example.c` and the symptom "Segmentation fault", before the program was run for this task, and was saved unchanged (Appendix B; sha256 of the saved file: `bbb4bc0d2427536bb2bb1d7c63937827ba8caab108e5c9d24224c1087611f809`). Each item has the AI's own confidence (high, medium, low) and was then tested against an observation (section 6). The tables below condense the answer; the verdicts were written after the tests.

Limit: the same assistant had already analysed this program for the Valgrind task, so its answer is not a blind test of its ability to diagnose. A correct diagnosis says little about its reliability; the wrong predictions say more.

### 4.2 Possible causes and mechanism, with verdicts

| Id | What the AI said | AI conf. | Verdict | Evidence |
|---|---|---|---|---|
| A1 | `nums` is NULL: `allocate_numbers(0)` returns NULL at the guard (lines 9-10), `main` stores it and writes `nums[0]` | high | **Correct** | E2, E3, E4, E6, E7 |
| A2 | `malloc` failed and returned NULL | low | Speculative: refuted here, but a real second route to the same crash | E4, E11 |
| A3 | `nums` is an indeterminate (wild) pointer | low | Refuted: initialised to NULL on line 24, fault address exactly 0 | E2, E3 |
| A4 | a heap overflow in the fill loop corrupted metadata | low | Refuted: the loop body (line 17) never runs | E4 |
| A5 | stack overflow | low | Refuted: only `main` is on the stack, the fault address is 0 | E2 |
| A6 | invalid or double `free(nums)` on line 36 | low | Refuted: line 36 is never reached | E4 |
| A7 | neither stack nor heap is the target: the pointer is on the stack, the array was never created, address 0 is unmapped | high | **Correct** | E2, E4 |
| A8 | page 0 is not mapped; page fault; SIGSEGV with `SEGV_MAPERR` and address 0; status 139 | high | **Correct**, with one over-statement (section 4.4) | E1, E2, E5, E8 |
| A8b | the page fault is trap 14 with error code 6 (user, write, not present) | medium | **Correct** | E8 |
| A9 | at `-O2` GCC may emit a trap, so the program could die with SIGILL instead of SIGSEGV | medium | **Partly incorrect** (D1) | E12 |
| A10 | a static analyser (`clang --analyze`, `clang-tidy`) reports the null dereference at line 32 without running the program | medium | **Incorrect** for the default settings (D2) | E14, E14c |
| A10b | `gcc -O2 -Wnull-dereference` warns about line 32; it is silent at `-O0` | low | **Correct** | E12 |
| A11 | AddressSanitizer reports `SEGV on unknown address 0x000000000000`, a WRITE, the zero page | medium | **Correct** | E13 |
| A12 | the output is lost when stdout is redirected, visible on a terminal | high | **Correct** | E1, E5 |
| A13 | even with a non-NULL pointer for `n = 0`, `nums[0] = 42` is out of bounds | medium | **Correct** | E9b |

### 4.3 Possible fixes, with verdicts

| Id | What the AI proposed | AI conf. that it is a fix | Verdict | Evidence |
|---|---|---|---|---|
| F1 | in `main`, test `nums == NULL` before using it | high | **Correct.** Sufficient because `allocate_numbers` returns non-NULL only when `n >= 1` and `malloc` succeeded | E9c |
| F2 | pass a positive `n` | low | Correct as stated: it removes the trigger, not the defect (another NULL source crashes the same way) | E9a, E11 |
| F3 | remove the guard, or allocate at least one element | low | Correct as stated: the crash disappears but the write stays invalid | E9b |
| F4 | install a SIGSEGV handler | low | Correct as stated: returning from the handler repeats the faulting store forever | E10 |
| F5 | document the contract, make the two failures distinguishable, build with `-O2 -Wnull-dereference`, run Valgrind or ASan in the tests | medium | Correct, with a limit: with default settings static analysis alone did not find this bug (D2) | E7, E12, E13, E14 |
| F6 | `assert(nums != NULL)` before the write | low | Correct as stated: aborts with a message, compiled out by `NDEBUG`, so the crash returns | E15 |

Count: of the 21 items, 14 were correct as stated, 5 were low-confidence alternatives that the evidence rules out (A2 to A6), 1 was partly incorrect (A9) and 1 was incorrect (A10).

### 4.4 Where the AI was wrong or imprecise

**D1. A9, "at `-O2` the program could die with SIGILL": partly incorrect.**

- *What the AI said (hedged with "may" and "could"):* undefined behaviour does not promise SIGSEGV; at `-O2` GCC may treat a store through a provably null pointer as erroneous and emit a trap instruction, so the program could die with SIGILL instead.
- *Check:* the `-O2` binary still dies with SIGSEGV, status 139. Its `main` ends with `movl $0x0,0x0` followed by `ud2`: GCC did emit a trap, but after a store to address 0, so the store faults first and the `ud2` is never executed. The compiler also changed the program: the call to `allocate_numbers` is gone and the stored value is 0, not 42. With `-fno-isolate-erroneous-paths-dereference` the store is again `movl $0x2a,0x0` and there is no `ud2` (E12).
- *Why it was wrong:* the first half is right (GCC does emit a trap, and it does change the program), the second half is not: what decides the symptom is the order of the instructions, and GCC puts the `ud2` after the store, which faults first. That undefined behaviour allows anything does not say which thing happens.
- *Consequence:* reading the source of an optimised build misleads (the value 42 is not stored); the evidence has to come from the binary.

**D2. A10, "a static analyser reports the null dereference": incorrect for the default settings.**

- *What the AI said:* `clang --analyze` and `clang-tidy` (with the `clang-analyzer` checks) report the null dereference at line 32 without running the program, because they can follow `n = 0` through the static function `allocate_numbers`.
- *Check:* with default settings neither tool reports anything on the original source (clang 18.1.3, E14). The analyser itself works: it reports a trivial `*p = 1` with `p = 0`, and it reports the store `nums[0] = 42` in a variant where the call is replaced by `if (n <= 0) nums = NULL; else ...` written in `main`. When the null comes back from a call, it stays silent, even with the literal argument `allocate_numbers(0)`.
- *Why it was wrong:* the analyser can follow `n = 0` through the function, as the AI said, but by default it discards every report whose path goes through a null return of an inlined function. The option is `suppress-null-return-paths` (default true); clang's own help describes it as a heuristic against "inlined functions that are more defensive than their callers". With `-Xclang -analyzer-config -Xclang suppress-null-return-paths=false` both tools report line 32 with the whole path: "Calling `allocate_numbers`", "`n` is <= 0", "Returning null pointer", "Null pointer value stored to `nums`" (E14c). The AI was right about the capability and wrong about what the tools print when they are run in the usual way.
- *Consequence:* a clean run of the default analyser is not evidence that a null pointer cannot be dereferenced, and this heuristic hides exactly the pattern of this bug: a function that returns NULL and a caller that does not check. GCC's `-O2 -Wnull-dereference`, Valgrind and AddressSanitizer found the bug with their default settings; the analyser needed a non-default option.

**Imprecision in A8.** The AI wrote "nothing is ever mapped at page 0 for a user program". The parenthesis that follows in its answer gives the real rule: the kernel refuses user mappings below `vm.mmap_min_addr`, a setting that the administrator controls (4096 on this machine, E2c). For this process the claim holds (E2), and the effect on the diagnosis is nil, but "ever" is stronger than what the evidence supports.

### 4.5 What the critique shows

The central claim (A1) and the main fix (F1) were right; the AI's own caveats on F2, F3, F4 and F6 were right too. Its errors were about tools and compiler behaviour, in the part of the answer where it extrapolated instead of observing. Its list of "possible causes" also contained five alternatives (A2 to A6) that a breakpoint or a look at the disassembly rules out in minutes: such a list is a set of hypotheses, not a diagnosis.

---

## 5. Suggested fix (optional, not applied)

The program was not modified. The following change is a suggestion, tested on a scratch copy (E9c): insert these five lines right after line 30, the call to `allocate_numbers`.

```c
    if (nums == NULL)
    {
        fprintf(stderr, "crash_example: allocate_numbers(%d) returned NULL\n", n);
        return 1;
    }
```

With this change the program prints the message, exits with status 1, and Valgrind reports 0 errors. The check is sufficient under the current contract of `allocate_numbers` (non-NULL only for `n >= 1` and a successful `malloc`). If that contract changes, for example by removing the guard (F3), line 32 must be guarded by `n > 0` as well. Writing the contract above `allocate_numbers` in a comment would make it visible to callers.

Changing `n` to a positive value (F2) makes the demonstration run to the end (E9a) but leaves the defect in place: any other NULL return crashes the same way (E11).

---

## 6. Evidence and reproduction

Sections 2 and 3 follow from the source and from two rules (dereferencing a null pointer is undefined behaviour; page 0 is not mapped). The runs below confirm them; none of them is needed to derive the chain. A core dump of the original run (E6) preserves the state at the fault, so the analysis does not depend on running the program again.

| Id | What was done | Result |
|---|---|---|
| E1 | run on a pseudo-terminal, with stdout redirected to a file and to a pipe, and 20 runs | terminal: 2 lines, killed by signal 11; file and pipe: 0 bytes, status 139; stderr empty; 20 runs out of 20 with status 139 |
| E2 | gdb on the original binary: registers, `p nums`, `p n`, `$_siginfo`, `info proc mappings`, `info frame`, heap headers | fault at `crash_example.c:32`, `rax = 0`, `nums = 0x0`, `n = 0`, `si_code = 1`, `si_addr = 0x0`; lowest mapping `0x555555554000`; frame address `0x7fffffff9ab0`; heap size fields `0x291` and `0x1011` |
| E2c | `cat /proc/sys/vm/mmap_min_addr` | 4096 |
| E3 | disassembly of `allocate_numbers` and `main` | `cmpl $0x0,-0x14(%rbp)`, `jg`, `mov $0x0,%eax` (return 0); `movq $0x0,-0x8(%rbp)` for line 24; `mov %rax,-0x8(%rbp)`, `mov -0x8(%rbp),%rax`, `movl $0x2a,(%rax)` |
| E4 | breakpoints on lines 10, 12, 17 and 36; `finish` out of `allocate_numbers`; breakpoint on `malloc` once `main` has started | line 10 hit once; lines 12, 17 and 36 never hit; `Value returned is (int *) 0x0`; the only `malloc` is the 4,096-byte buffer allocated by `puts` |
| E5 | `strace` of the original, stdout on a file and on a pseudo-terminal | `SIGSEGV {si_code=SEGV_MAPERR, si_addr=NULL}`, `killed by SIGSEGV`; no `write` with a file, two `write` calls (67 and 17 bytes) before the signal on a terminal |
| E6 | `ulimit -c unlimited`, run, then gdb on the core file only | core file of 475,136 bytes; `bt` at line 32, `movl $0x2a,(%rax)`, `nums = 0x0`, `n = 0`, `si_addr = 0x0` |
| E7 | Valgrind on the original | `Invalid write of size 4` at line 32, `Address 0x0 is not stack'd, malloc'd or (recently) free'd`, 1 allocation and 0 frees (the `stdout` buffer), `1 errors from 1 contexts` |
| E8 | scratch program (listed after the commands): SIGSEGV handler printing `si_code`, `si_addr` and, from the saved context, trap number and error code | `si_code=1`, `si_addr=(nil)`, `trapno=14`, `err=6` |
| E9a | variant with `int n = 3;` | prints `nums[0]=42`, status 0; Valgrind: 0 errors |
| E9b | variant without the guard (lines 9-10 deleted) | no crash, prints `nums[0]=42`, status 0; Valgrind: `Invalid write of size 4`, `0 bytes after a block of size 0 alloc'd`, plus an invalid read, 2 errors |
| E9c | variant with the check of section 5 | message on stderr, status 1; Valgrind: 0 errors |
| E10 | variant whose SIGSEGV handler returns, run under `timeout 1` | several hundred thousand handler calls in one second (610,866 in the run kept, about 255,000 in two replays of Appendix A; the count depends on the load of the machine), killed by the timeout |
| E11 | variant with `n = 100000000` under `ulimit -v 100000` | `malloc` fails, `nums=(nil)`, same crash, status 139 |
| E12 | `gcc -Wnull-dereference` at `-O0` and `-O2`; disassembly of the `-O2` `main`; `-O2 -fno-isolate-erroneous-paths-dereference` | `-O0`: no warning; `-O2`: `warning: null pointer dereference` at line 32, status 139, `movl $0x0,0x0` then `ud2`; without the isolation pass: `movl $0x2a,0x0`, no `ud2`, status 139 |
| E13 | build with `-fsanitize=address` | `SEGV on unknown address 0x000000000000`, `WRITE`, `address points to the zero page`, at `crash_example.c:32` |
| E14 | `clang --analyze` and `clang-tidy` with default settings on the original; probes: a trivial null store, the call replaced by `if (n <= 0) nums = NULL; else ...` in `main`, the literal argument `allocate_numbers(0)` | original: no report from either tool; trivial store: reported; test written in `main`: reported; literal argument: no report |
| E14c | the same tools with `-Xclang -analyzer-config -Xclang suppress-null-return-paths=false`, on the original (both tools) and on the literal-argument probe (`clang --analyze`); `clang -cc1 -analyzer-config-help` | line 32 reported by both tools on the original, with the path through `allocate_numbers` (`n` is <= 0, returning null pointer, null pointer value stored to `nums`); also reported on the literal-argument probe; the help text calls the option a heuristic against callees that are more defensive than their callers, default true |
| E15 | variant with `assert(nums != NULL)`, built with and without `-DNDEBUG` | `Assertion 'nums != NULL' failed`, status 134; with `-DNDEBUG` the crash returns, status 139 |

Absolute stack addresses depend on the size of the environment (a different working directory shifted them by 16 bytes here); the relations between them (`nums` and `n` in the frame of `main`, the null value) do not.

### Appendix A: commands

The program in `programs/` is never edited. Each variant is built from a copy of `crash_example.c` with `sed` (the files are byte for byte the ones that were tested), and `CF` holds the flags of the Makefile. Comments give the result on the machine of this report; addresses and process numbers differ on other machines.

```bash
cd programs                          # the directory that holds crash_example.c
CF="-Wall -Wextra -Werror -pedantic -std=gnu89 -g"
make crash_example

# E1: terminal, redirected output, 20 runs
./crash_example; echo "status $?"                            # two lines, the shell's message, status 139
./crash_example > out.txt; echo "status $?"; wc -c < out.txt    # status 139, then 0 bytes
for i in $(seq 20); do ./crash_example > /dev/null 2>&1; echo $?; done | sort | uniq -c    # 20 139

# E2, E2c: the state at the fault (take the [heap] start for the x/2gx lines from the mapping list)
cat /proc/sys/vm/mmap_min_addr                               # 4096 here
gdb -q -batch -ex 'run > /dev/null' -ex 'bt' -ex 'info registers rip rax rsp rbp' -ex 'x/i $pc' \
    -ex 'p nums' -ex 'p n' -ex 'p &nums' -ex 'p &n' -ex 'p $sp' \
    -ex 'p $_siginfo.si_signo' -ex 'p $_siginfo.si_code' \
    -ex 'p $_siginfo._sifields._sigfault.si_addr' -ex 'info frame' -ex 'info proc mappings' \
    -ex 'x/2gx 0x555555559000' -ex 'x/2gx 0x555555559290' -ex 'x/2gx 0x55555555a2a0' ./crash_example

# E3: the machine code of allocate_numbers and main
objdump -d --no-show-raw-insn -M att -l ./crash_example > disasm.txt

# E4: which lines run, what allocate_numbers returns, which malloc calls happen
gdb -q -batch -ex 'break crash_example.c:10' -ex 'break crash_example.c:12' \
    -ex 'break crash_example.c:17' -ex 'break crash_example.c:36' \
    -ex 'run > /dev/null' -ex 'continue' -ex 'info breakpoints' ./crash_example
gdb -q -batch -ex 'break allocate_numbers' -ex 'run > /dev/null' -ex 'finish' ./crash_example
gdb -q -batch -ex 'break main' -ex 'run > /dev/null' -ex 'break malloc' -ex 'continue' -ex 'bt' \
    -ex 'continue' -ex 'info breakpoints' ./crash_example | grep -vE '^2\.[0-9]+ '    # one hit, from puts

# E5, E6, E7: system calls, core dump, Valgrind
strace -o trace.txt ./crash_example > /dev/null; tail -3 trace.txt
strace -o trace_tty.txt ./crash_example; grep -E 'write\(1|SIGSEGV' trace_tty.txt     # in a terminal
(ulimit -c unlimited; ./crash_example > /dev/null)           # /proc/sys/kernel/core_pattern says where the core goes
gdb -q -batch -ex 'bt' -ex 'frame' -ex 'x/i $pc' -ex 'p nums' -ex 'p n' \
    -ex 'p $_siginfo._sifields._sigfault.si_addr' -ex 'info registers rax' ./crash_example core
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./crash_example > /dev/null

# E9a, E9b, E9c, E10, E11, E15: variants of a copy of crash_example.c
sed 's/int n = 0;/int n = 3;/' crash_example.c > v_n3.c
sed '9,10d' crash_example.c > v_noguard.c
cat > fix_block.txt <<'EOF'
    if (nums == NULL)
    {
        fprintf(stderr, "crash_example: allocate_numbers(%d) returned NULL\n", n);
        return 1;
    }
EOF
sed '30r fix_block.txt' crash_example.c > v_check.c
sed -e '1i\
#include <signal.h>\
#include <unistd.h>\
\
static void on_segv(int sig)\
{\
    (void)sig;\
    write(2, "caught SIGSEGV\\n", 15);\
}\
' -e '30i\
    signal(SIGSEGV, on_segv);' crash_example.c > v_handler_loop.c
sed -e 's/int n = 0;/int n = 100000000;/' -e '30a\
    fprintf(stderr, "  nums=%p\\n", (void *)nums);' crash_example.c > v_mallocfail.c
sed -e '1i\
#include <assert.h>' -e '30a\
    assert(nums != NULL);' crash_example.c > v_assert.c
for v in v_n3 v_noguard v_check v_handler_loop v_mallocfail v_assert; do gcc $CF -o $v $v.c; done

./v_n3; echo "status $?"                                     # E9a: prints nums[0]=42, status 0
valgrind ./v_n3 2>&1 > /dev/null | grep 'ERROR SUMMARY'      # 0 errors
./v_noguard; echo "status $?"                                # E9b: no crash, prints nums[0]=42, status 0
valgrind ./v_noguard 2>&1 > /dev/null | grep -E 'Invalid|after a block|ERROR SUMMARY'   # 2 errors
./v_check; echo "status $?"                                  # E9c: message on stderr, status 1
valgrind ./v_check 2>&1 > /dev/null | grep 'ERROR SUMMARY'   # 0 errors
timeout 1 ./v_handler_loop 2>&1 > /dev/null | wc -l          # E10: several hundred thousand lines in one second
bash -c 'ulimit -v 100000; ./v_mallocfail > /dev/null'; echo "status $?"    # E11: nums=(nil), status 139
./v_assert; echo "status $?"                                 # E15: Assertion failed, status 134
gcc $CF -DNDEBUG -o v_assert_nd v_assert.c; ./v_assert_nd; echo "status $?"    # status 139 again

# E8: save the program listed after this block as v_siginfo.c
gcc -std=gnu99 -Wall -Wextra -g -o v_siginfo v_siginfo.c; ./v_siginfo; echo "status $?"    # status 3

# E12: optimisation and warnings
gcc -O0 -Wnull-dereference -std=gnu89 -g -o crash_O0w crash_example.c                # no warning
gcc -O2 -Wnull-dereference -std=gnu89 -g -o crash_O2 crash_example.c                 # warning at line 32
./crash_O2; echo "status $?"                                 # still SIGSEGV: status 139
objdump -d --no-show-raw-insn -M att crash_O2 | sed -n '/<main>:/,/ud2/p'            # movl $0x0,0x0 then ud2
gcc -O2 -fno-isolate-erroneous-paths-dereference -std=gnu89 -g -o crash_O2b crash_example.c
objdump -d --no-show-raw-insn -M att crash_O2b | grep -E 'movl|ud2'                  # movl $0x2a,0x0 and no ud2

# E13: AddressSanitizer
gcc -fsanitize=address -std=gnu89 -g -o crash_asan crash_example.c; ./crash_asan

# E14, E14c: static analysis, default settings and without the null-return heuristic
CA="clang --analyze -Xclang -analyzer-output=text -std=gnu89"
NR="-Xclang -analyzer-config -Xclang suppress-null-return-paths=false"
printf 'int main(void)\n{\n    int *p = 0;\n    *p = 1;\n    return 0;\n}\n' > trivial_null.c
sed 's/nums = allocate_numbers(n);/nums = allocate_numbers(0);/' crash_example.c > probe_literal.c
sed '30c\
    if (n <= 0)\
        nums = NULL;\
    else\
        nums = (int *)malloc(sizeof(int));' crash_example.c > probe_inline.c
$CA crash_example.c                                                  # no output
clang-tidy -checks='-*,clang-analyzer-*' crash_example.c -- -std=gnu89              # no output
for p in trivial_null probe_literal probe_inline; do $CA $p.c; done   # reported, no output, reported
$CA $NR crash_example.c                                              # line 32, with the whole path
$CA $NR probe_literal.c                                              # line 32
clang-tidy -checks='-*,clang-analyzer-*' crash_example.c -- -std=gnu89 $NR          # line 32
clang -cc1 -analyzer-config-help | grep -A6 suppress-null-return-paths               # what the option says
```

The program used for E8 (`v_siginfo.c`):

```c
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

static void on_segv(int sig, siginfo_t *si, void *ctx)
{
    ucontext_t *uc = (ucontext_t *)ctx;
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "signal=%d si_code=%d (SEGV_MAPERR=%d SEGV_ACCERR=%d) si_addr=%p trapno=%lld err=%lld rip=%#llx\n",
        sig, si->si_code, SEGV_MAPERR, SEGV_ACCERR, si->si_addr,
        (long long)uc->uc_mcontext.gregs[REG_TRAPNO], (long long)uc->uc_mcontext.gregs[REG_ERR],
        (unsigned long long)uc->uc_mcontext.gregs[REG_RIP]);
    write(2, buf, (size_t)len);
    _exit(3);
}

int main(void)
{
    struct sigaction sa;
    int *p = NULL;

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_segv;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    p[0] = 42;
    return 0;
}
```

### Appendix B: the AI answer, word for word

The text below is the answer of the AI tool, saved before the program was run for this task. Its header says what the AI was given and what it did not know. The block between the two fences is the saved file: hashing it gives the sha256 quoted in section 4.1.

```text
# AI first pass (Claude) for the crash report task - written BEFORE any check of this task

Inputs given to the AI: the source of crash_example.c (38 lines) and the symptom "Segmentation fault".
At the time of writing, crash_example has not been run, under a debugger or otherwise, for this task.
A tool inventory (versions, kernel settings) was done just before; this first pass makes no claim about
those values.
Limit: the same assistant analysed this program in the Valgrind task, so this pass is NOT a blind test of
its ability to find the cause. The point of the critique that follows is to check each claim against
evidence, not to trust the author.

Each item has an id, a confidence (high / medium / low) and the observation that would confirm or refute it.

## Possible causes

- A1 (high)    `nums` is NULL at line 32. Path: n = 0 (line 25); allocate_numbers(0) takes the guard
               `n <= 0` (lines 9-10) and returns NULL before any malloc; main stores NULL in `nums`
               (line 30), never checks it, and `nums[0] = 42` (line 32), which means `*(nums + 0) = 42`,
               stores 4 bytes at address 0. Undefined behaviour: dereference of a null pointer
               (C11 6.5.3.2p4).
               Confirm: at the fault `nums` = 0x0, `n` = 0, the store is `movl $0x2a,(%rax)` with rax = 0,
               the fault address is 0.
- A2 (low)     `malloc` failed (out of memory) and returned NULL: same symptom, other source of the NULL.
               Refute: malloc (line 12) is never executed in this run.
- A3 (low)     `nums` holds an indeterminate (wild) pointer.
               Refute: line 24 initialises it to NULL; the fault address is exactly 0.
- A4 (low)     A heap buffer overflow in the fill loop (lines 16-17) corrupted heap metadata and the
               crash is a later consequence.
               Refute: the loop is never entered (n = 0).
- A5 (low)     Stack overflow (runaway recursion or huge locals).
               Refute: no recursion, no large local; only `main` is on the stack at the fault and the
               fault address (0) is nowhere near the stack pointer.
- A6 (low)     A bad `free(nums)` (line 36): invalid or double free.
               Refute: line 36 is never reached; and free(NULL) is harmless anyway.
- A7 (high)    Stack or heap? Neither is the target. The faulting address is 0, which belongs to no
               mapping. The stack holds the pointer variable `nums` (value NULL) and `n`; the heap would
               have held the array but it was never created. At the fault the heap contains only libc's
               own blocks (the tcache structure and the stdout buffer).
- A8 (high)    Mechanism. On Linux x86-64 nothing is ever mapped at page 0 for a user program (the kernel
               also refuses user mappings below vm.mmap_min_addr). The store raises a page fault with
               CR2 = 0; the kernel finds no mapping for that address and delivers SIGSEGV with
               si_code = SEGV_MAPERR and si_addr = 0; the default action kills the process; the shell
               reports status 139 = 128 + 11.
- A8b (medium) The page-fault details seen from user space are trap number 14 and error code 6
               (user mode, write, page not present).
- A9 (medium)  The symptom depends on the build. Undefined behaviour does not promise SIGSEGV: at -O2
               GCC may treat the store to a provably null pointer as erroneous and emit a trap
               instruction, so the program could die with SIGILL instead.
- A10 (medium) A static analyser (clang --analyze, clang-tidy with clang-analyzer checks) reports the
               null dereference at line 32 without running the program, because it can follow n = 0
               through the static function allocate_numbers.
- A10b (low)   gcc -O2 -Wnull-dereference also warns about line 32 (the warning needs optimisation, so
               it is silent at -O0).
- A11 (medium) AddressSanitizer prints "SEGV on unknown address 0x000000000000", "The signal is caused by
               a WRITE memory access" and "address points to the zero page".
- A12 (high)   When stdout is a file or a pipe the program's two output lines are lost (the buffer is not
               flushed when the process is killed); in a terminal both lines appear before the crash.
- A13 (medium) Even with a non-NULL pointer for n = 0 the line `nums[0] = 42` would still be wrong: it
               writes element 0 of an array of zero elements. The defect is not only the NULL, it is
               using the array without checking that it exists and has an element 0.

## Possible fixes (confidence that the item is a correct fix)

- F1 (high)    In main, test the result before using it:
                   if (nums == NULL) { fprintf(stderr, "..."); return 1; }
               allocate_numbers returns non-NULL only when n >= 1 and malloc succeeded, so nums[0] is
               then inside the array. This removes the defect.
- F2 (low)     Pass a positive n (for example 5). This run then succeeds, but any invalid n or any
               malloc failure crashes the same way: it removes the trigger, not the defect.
- F3 (low)     Remove the `n <= 0` guard, or allocate at least one element, so that n = 0 yields a
               non-NULL pointer. With glibc, malloc(0) returns a non-NULL minimal block, so the crash
               would disappear, but `nums[0] = 42` would then write 4 bytes into a block requested with
               size 0: undefined behaviour (heap buffer overflow) that Valgrind and ASan still report
               ("0 bytes after a block of size 0 alloc'd"). It hides the crash; it is not a fix.
- F4 (low)     Install a SIGSEGV handler. It cannot repair the access: returning from the handler
               re-executes the faulting store and the signal is raised again, forever. A handler is only
               useful to print a diagnostic and call _exit.
- F5 (medium)  Hardening: document the contract ("returns NULL if n <= 0 or if malloc fails"), make the
               two failures distinguishable (error code plus an out-parameter), compile with
               -Wall -Wextra -O2 -Wnull-dereference and run Valgrind or ASan in the tests.
- F6 (low)     `assert(nums != NULL)` before the write. Good while debugging, but compiled out with
               NDEBUG, so it is not run-time error handling.
```