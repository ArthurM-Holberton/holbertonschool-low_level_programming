# Valgrind analysis: what each report means

Lab: *AI Memory Visualizer + Valgrind Tracer*, Valgrind analysis (`analysis/valgrind_analysis.md`). The memory maps of the same programs are in `memory_maps.md`; "maps 2.2" below means section 2.2 of that file.

**Process.** Valgrind was run on the four programs. An AI tool (Claude) then wrote a first interpretation of every report. That interpretation was used only as a starting point: each statement was checked against the source code, against the memory maps, and with small controlled experiments on scratch copies of the programs (section 8; no project file was changed). Section 7 records the statements that turned out to be partly incorrect or incomplete, and how they were corrected. Everything below is the checked version.

**In short**

- `stack_example`: no report. The program does not misuse memory; the uninitialised bytes of `local_buf` are never read.
- `aliasing_example`: three **use-after-free** errors (read, write, read) on one 20-byte heap block, freed at line 38 and then accessed through the dangling alias `b`.
- `heap_example`: one leak, **definitely lost**, caused by **lost ownership**: the struct that held the only pointer to a name buffer is freed, the buffer is not.
- `crash_example`: one **null-pointer write** and the SIGSEGV it causes. No memory object is involved. Valgrind also lists the `stdout` buffer of libc as "still reachable", which is not an error.
- No program uses uninitialised memory in a way Valgrind reports. Section 2 explains why, and shows what the report looks like when one does.

---

## 0. How the reports were produced

### 0.1 Environment and flags

Environment of every run: Ubuntu 24.04 x86-64, gcc 13.3.0, glibc 2.39, Valgrind 3.22.0. The programs were built with the Makefile flags (`-g` puts file names and line numbers in the reports); `aliasing_example` needs the build workaround described in maps section 0.

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./program
```

| Flag | Why it is needed here |
|---|---|
| `--leak-check=full` | The default (`summary`) prints only counts: no allocation stack, and the leak is not counted in ERROR SUMMARY (X9). |
| `--show-leak-kinds=all` | The default lists only definite and possible leaks. `all` also lists "still reachable" blocks; the `crash_example` report needs it. |
| `--track-origins=yes` | Shows where an uninitialised value was created. None of the four programs uses one, so the reports are identical with and without it (X10); X8 needs it. |
| defaults kept | `--freelist-vol=20000000`: freed blocks stay in a queue of up to 20 MB, which is what lets Valgrind say "free'd" (section 3.4). |

Absolute addresses under Valgrind differ from native or gdb runs: the 20-byte block of `aliasing_example` is at `0x4a7f080` here and at `0x55555555a2b0` in the gdb run of the maps. Offsets inside objects and the order of events are what carry over.

Code addresses in a report can be mapped back to lines without trusting Valgrind. Valgrind loads these programs at `0x108000`, so the report address `0x1092F5` is at offset `0x12f5` in the file, and `addr2line -e aliasing_example -f -s 0x12f5` prints `main` and `aliasing_example.c:42`, the line the report names. The same check gives line 44 for `0x109315`, line 45 for `0x109323`, line 38 for `0x1092D1`, line 12 (`make_numbers`) for `0x1091E4` and line 30 for `0x109272`. In `crash_example`, `0x10928A - 0x108000 = 0x128a`, the offset of the instruction `movl $0x2a,(%rax)`, which `addr2line` places at `crash_example.c:32`. `addr2line` reads its argument as hexadecimal, so the offset must be passed as `0x...`; a decimal result such as `4853` is read as `0x4853` and prints `??`.

### 0.2 Vocabulary

| Term in a report | Meaning |
|---|---|
| `Invalid read / write of size N` | A load or store of N bytes at an address the program may not use. N is the width of the access: 4 for an `int`, 8 for a pointer. |
| `Address A is K bytes inside a block of size S free'd` | A is K bytes after the start of a heap block that was requested with S bytes and has been freed: **use-after-free**. The report also shows where the block was freed and allocated. |
| `Address A is K bytes after a block of size S alloc'd` | A is just past the end of a live block: a heap **buffer overflow**. Not observed in these programs; X2 shows the same wording applied to a freed block. |
| `Address 0x0 is not stack'd, malloc'd or (recently) free'd` | A lies in no thread stack, in no live malloc block and in no block still remembered in the queue of freed blocks: a **wild or null pointer**. |
| error, context | Each occurrence is an error; errors of the same kind whose top four call-stack frames are equal form one context (`5 errors from 3 contexts`, X3). |
| definitely lost | At exit the block is still allocated and no pointer to it, or into it, exists in registers, stacks, globals or live heap blocks. |
| indirectly lost | The block is reachable only from blocks that are themselves lost. |
| possibly lost | Only a pointer into the middle of the block exists (not observed). |
| still reachable | A pointer to the block still exists at exit. Listed because of `--show-leak-kinds=all`; not an error. |
| leak due to lost ownership | The last pointer to a live block disappears (overwritten, freed together with its container, out of scope) before the block is freed. |

---

## 1. Every report, classified

| # | Program | Valgrind reports | Where | Type of error | Memory object | Lifetime violation or misuse |
|---|---|---|---|---|---|---|
| 1 | `stack_example` | nothing (0 errors) | - | none | - | none (section 2) |
| 2 | `aliasing_example` | `Invalid read of size 4`, 8 bytes inside a block of size 20 free'd | line 42 | heap use-after-free (read) | the 20-byte block allocated at line 12 (call at line 30) and freed at line 38 | read through the dangling pointer `b` after the block's lifetime ended at line 38 |
| 3 | `aliasing_example` | `Invalid write of size 4`, 12 bytes inside a block of size 20 free'd | line 44 | heap use-after-free (write) | same block | write through `b` into dead memory |
| 4 | `aliasing_example` | `Invalid read of size 4`, 12 bytes inside a block of size 20 free'd | line 45 | heap use-after-free (read) | same block | read, again through `b`, of the value just written |
| 5 | `heap_example` | `6 bytes in 1 blocks are definitely lost` | allocated at line 21 (call at line 51) | memory leak: leak due to lost ownership | the name buffer of `alice` (`"Alice"`, 6 bytes) | `person_free_partial` frees the struct (line 41) that held the only pointer to the buffer, and never frees the buffer: the buffer's lifetime never ends |
| 6 | `crash_example` | `Invalid write of size 4`, `Address 0x0 is not stack'd, malloc'd or (recently) free'd` | line 32 | null-pointer dereference (write) | none: no object exists at address 0 | `allocate_numbers(0)` returns NULL (lines 9-10), `main` never checks it, and `nums[0] = 42` writes through NULL |
| 7 | `crash_example` | `Process terminating with default action of signal 11 (SIGSEGV)`, `Access not within mapped region at address 0x0` | line 32 | the real fault that follows row 6 | none | consequence of row 6, not a second bug |
| 8 | `crash_example` | `4,096 bytes in 1 blocks are still reachable` | allocated by `puts`, line 27 | not an error: block still referenced at exit | the `stdout` buffer of libc | the process was killed by the signal before libc could free the buffer; a pointer to it still exists in libc |

Counts printed by Valgrind: `stack_example` 0 errors; `aliasing_example` 3 errors from 3 contexts; `heap_example` 1 error from 1 context; `crash_example` 1 error from 1 context. Rows 7 and 8 are reported but are not counted as errors. No report is a `Conditional jump or move depends on uninitialised value(s)`, so the category "use of uninitialised memory" is empty for these programs (section 2).

| Lifetime rule | How it is broken here | Valgrind symptom |
|---|---|---|
| An object may be accessed only while its lifetime lasts | access after `free` (rows 2-4) | `Invalid read/write ... free'd` |
| Every allocated block must be freed once, by whoever owns it, before the last pointer to it disappears | the last pointer disappears with a freed struct (row 5) | `definitely lost` |
| A pointer may be dereferenced only if it points to an object | `nums` never pointed to anything (rows 6-7) | `Invalid write ... Address 0x0 ...` and SIGSEGV |
| A byte may be read only after it was written | not broken by these programs | `Conditional jump or move depends on uninitialised value(s)` (X8) |

---

## 2. `stack_example`: no report, and why that is correct

Valgrind prints `0 errors from 0 contexts` and `All heap blocks were freed -- no leaks are possible`. The only heap block (`1 allocs, 1 frees, 4,096 bytes allocated`) is the buffer that libc allocates for `stdout` at the first output call, the title line (a `puts`, line 34); libc frees it at exit. Every access in the program is to a live frame, and no pointer outlives its frame (maps 1.5 and 1.6).

**Why Valgrind says nothing about `local_buf`.** Maps 1.3 shows that `local_buf[2..15]` holds leftovers of dead frames. Valgrind does not report that bytes are uninitialised; it reports a *use* of one: a conditional jump or move that depends on it, an undefined value used as an address, or one passed to a system call. `dump_frame` never reads those bytes. It reads `local_buf[0]` (`%c`) and passes the *address* of the array to `printf` (`%p`).

X8 confirms this. Two added lines after line 10, `if (local_buf[2] != 0)` and `puts("local_buf[2] is not zero");` (lines 11 and 12 of the scratch copy `e7_uninit.c`), make Valgrind report:

```text
Conditional jump or move depends on uninitialised value(s)
   at dump_frame (e7_uninit.c:11)
 Uninitialised value was created by a stack allocation
   at dump_frame (e7_uninit.c:4)
```

The origin is the frame of `dump_frame` itself: the bytes became undefined when the frame was allocated, and no instruction wrote `local_buf[2]` afterwards. Natively the same variant prints its message eight times, because the leftover byte happens to be non-zero in all eight calls; with another stack history it could print nothing. The result depends on data the program never wrote. Valgrind tracks whether the program wrote a byte, not what the byte happens to contain.

---

## 3. `aliasing_example`: three use-after-free errors

### 3.1 One memory object, one lifetime

All three errors concern the same object: the 20-byte block (five `int`) allocated at line 12 inside `make_numbers`, called from line 30, and freed at line 38. Valgrind's own labels say the same thing:

```text
Invalid read of size 4                                           <- what: a 4-byte read (one int)
   at 0x1092F5: main (aliasing_example.c:42)                     <- where: line 42 of main
 Address 0x4a7f088 is 8 bytes inside a block of size 20 free'd   <- the address is 8 bytes into a freed 20-byte block
   at 0x484988F: free (in .../vgpreload_memcheck-amd64-linux.so)
   by 0x1092D1: main (aliasing_example.c:38)                     <- the block was freed at line 38
 Block was alloc'd at
   at 0x4846828: malloc (in .../vgpreload_memcheck-amd64-linux.so)
   by 0x1091E4: make_numbers (aliasing_example.c:12)             <- allocated at line 12 ...
   by 0x109272: main (aliasing_example.c:30)                     <- ... by a call made at line 30
```

Timeline of the block, with Valgrind's addresses (the block spans `0x4a7f080` to `0x4a7f093`):

| Line | Event | State of the block | Maps |
|---|---|---|---|
| 12 | `malloc(20)` in `make_numbers` | allocated, alive | - |
| 17 | loop `arr[i] = i * 11` | filled: 0, 11, 22, 33, 44 | - |
| 19 | `return arr` | alive; only `arr` points to it | K1 |
| 34 | `b = a` | alive; `a` and `b` both hold `0x4a7f080` | K2 |
| 36 | reads `a[2]`, `b[2]` | alive: valid accesses | K2 |
| 38 | `free(a)` | **lifetime ends**; Valgrind marks the 20 bytes inaccessible and remembers where they were freed | K3 |
| 40 | prints the value of `b` | dead; no access to the block | K3 |
| 42 | reads `b[2]` | dead: **error 1** | K3 |
| 44 | writes `b[3] = 1234` | dead: **error 2** | K4 |
| 45 | reads `b[3]` | dead: **error 3** | K4 |
| 47 | `return 0` | already freed, nothing to leak | - |

### 3.2 The three errors

| # | Line | Valgrind says | The access | Offset | Type |
|---|---|---|---|---|---|
| 1 | 42 | `Invalid read of size 4`, 8 bytes inside a block of size 20 free'd | `b[2]` read as an `int` | 2 x 4 = 8 | heap use-after-free, read |
| 2 | 44 | `Invalid write of size 4`, 12 bytes inside a block of size 20 free'd | `b[3] = 1234` | 3 x 4 = 12 | heap use-after-free, write |
| 3 | 45 | `Invalid read of size 4`, 12 bytes inside a block of size 20 free'd | `b[3]` read for the `printf` | 3 x 4 = 12 | heap use-after-free, read |

`size 4` is `sizeof(int)`: each access touches one `int`. The reported addresses are `0x4a7f080 + 8 = 0x4a7f088` and `0x4a7f080 + 12 = 0x4a7f08c`. X2 checks the offsets further: reading `b[4]`, `b[5]` and `b[6]` gives `16 bytes inside`, `0 bytes after` and `4 bytes after` "a block of size 20 free'd". So 20 is the size requested from `malloc`, the block ends at offset 20, and the allocator's 32-byte chunk (maps 2.2) is not what Valgrind measures.

### 3.3 Why this is a lifetime violation, and why the alias matters

The lifetime of the block ended at line 38. `a` and `b` still hold its address, so both are dangling pointers (maps 2.4, K3); every dereference after line 38 reaches an object whose lifetime has ended. It does not matter which name is used: X1 rewrites lines 42, 44 and 45 to use `a` instead of `b`, and the reports are identical in kind, address, offset and line. Valgrind reasons about addresses and blocks, not variable names; following the aliasing is the reader's job, and `free(a)` ended the life of the object that `b` names as well.

Error 2 is the dangerous one. Natively, the write at line 44 lands on the allocator's own data inside the freed chunk: it replaces the upper half of the tcache key (maps 2.2, K4). The report text for the write is otherwise like that of the reads, so the severity has to be read from "write", not from the wording.

### 3.4 What Valgrind does not report, and why the output hides the bug

- **Line 40 is not reported.** `printf("... b=%p", b)` reads the pointer variable `b`, which lives in the live frame of `main`; it does not touch the block. That is why the report has three errors, not four. The program is still wrong there: after `free`, the value of `b` is indeterminate (maps 2.4), and GCC, with the Makefile flags, reports lines 40, 42, 44 and 45 (X14). "Valgrind says nothing" is not "valid".
- **The program prints a normal value under Valgrind.** The run prints `reading b[2]=22`. Valgrind makes the block inaccessible but does not change its bytes. Natively, glibc writes its free-list data into the first 16 bytes of the freed chunk, and the same line prints garbage (maps 2.5: `183939990`, `962339381`, `1126703990`). The program's output proves nothing in either case; the report is the evidence.
- **"free'd" depends on a queue.** Valgrind keeps freed blocks in a queue (20 MB by default), which is how it can name the free and allocation stacks long after `free`. X4 shows what happens when a block leaves the queue: with `--freelist-vol=0` and no later allocation the report is unchanged, but after a later `malloc(8)` it becomes `8 bytes inside an unallocated block of size 32 in arena "client"`, with no allocation or free stack and with the allocator's size (32) instead of the requested one (20). With the default queue the same `malloc(8)` changes nothing, so the queue size is what decides. A use-after-free is still detected, but it is much harder to diagnose (section 7, D3).

### 3.5 Errors and contexts

`3 errors from 3 contexts`: three accesses, three different places in the code. X3 replaces line 42 by a loop that reads `b[2]` three times on the same line: the summary becomes `5 errors from 3 contexts`. A context is not a line of code and not a bug; it is a group of errors of the same kind with the same top call-stack frames (section 7, D2).

---

## 4. `heap_example`: a leak due to lost ownership

### 4.1 The object and its owner

Valgrind's record says `6 bytes in 1 blocks`, allocated at `person_new (heap_example.c:21)`, called from `main (heap_example.c:51)`. Line 21 is `p->name = (char *)malloc(len + 1);`, so the block is the **name buffer of `alice`**: 6 = `strlen("Alice") + 1`. It is not the `Person` struct (16 bytes, line 14); that block was freed. Under Valgrind the buffer is at `0x4a7f0d0`.

Ownership chain (maps 3.2): `alice` (in `main`) owns the struct, and the struct owns the buffer through its `name` field. The `name` field is the only pointer to the buffer that ever existed.

| Line | Event | Maps |
|---|---|---|
| 14 (call at 51) | `malloc(16)`: the `alice` struct, at `0x4a7f080` | H1 |
| 21 (call at 51) | `malloc(6)`: the name buffer, at `0x4a7f0d0`; `alice->name` points to it | H1 |
| 52 | `bob`: struct at `0x4a7f120` (16 bytes), name at `0x4a7f170` (4 bytes) | H2 |
| 66, 67 | `free(bob->name)`, then `free(bob)`: correct order, both released | H3 |
| 69 (free at 41) | `person_free_partial(alice)`: **frees the struct only**. The struct's lifetime ends, and its `name` field, the only pointer to the buffer, goes with it | H4 |
| 71 | `return 0`: at exit the buffer is still allocated and nothing points to it | H4 |

### 4.2 Why "definitely lost" and not another category

At exit the leak checker looks for pointers to a block, or into it, in registers, stacks, global data and live heap blocks. For `0x4a7f0d0` it finds none, so the block is **definitely lost**. The misuse is a **leak due to lost ownership**: the owner (the struct) is destroyed without releasing what it owned.

X5 to X7 and X16 test this explanation:

- **X5:** adding `free(alice->name);` before line 69 removes the leak (`5 allocs, 5 frees`, `All heap blocks were freed`, 0 errors). The leak is exactly the missing free of that buffer.
- **X6:** keeping a copy of the pointer in a global (`kept = alice->name;` before line 69) changes the record to `6 bytes in 1 blocks are still reachable` and the error count to 0. Same allocation, same missing free; only the existence of a pointer changed the category.
- **X7:** deleting line 69, so the struct is never freed either, gives `22 (16 direct, 6 indirect) bytes in 1 blocks are definitely lost` for the struct and `6 bytes in 1 blocks are indirectly lost` for the buffer. "Indirectly lost" needs a parent that is itself lost. In the real program the parent was freed, not lost, so the buffer is "definitely lost".
- **X16:** after line 69, `printf("%p", *(void **)alice)` reads the freed struct. Valgrind reports it as one more error (`Invalid read of size 8`, 0 bytes inside a block of size 16 free'd) and the program prints `0x4a7f0d0`: under Valgrind the old pointer is still physically in the dead struct. The leak is nevertheless still `definitely lost`. The checker does not follow pointers stored in freed memory, so a pointer in a dead object is not ownership. Natively the read prints a different value, the free-list pointer that glibc wrote over the first word (maps 3.2, H4), so on a real run the pointer is gone altogether.

### 4.3 Accounting

`total heap usage: 5 allocs, 4 frees, 4,138 bytes allocated`. X13 (`--trace-malloc=yes`) lists the calls: allocations of 4,096 (the `stdout` buffer), 16, 6, 16 and 4 bytes (sum 4,138), and frees of `bob->name`, `bob`, the `alice` struct and the 4,096-byte buffer (released by libc at exit). The trace also lists 74 calls `free(0x0)` in this run: they do nothing and are not counted in the 4 frees. 5 allocations minus 4 frees leaves one block in use at exit, `6 bytes in 1 blocks`: the name buffer.

### 4.4 The error count, and what is not reported

- `1 errors from 1 contexts` is the `definitely lost` record. Under `--leak-check=full`, definite and possible leaks count as errors; indirectly lost and still reachable blocks do not (X6: 0 errors; X7: one error for a struct and a buffer that both leak). With the default `--leak-check=summary`, the same program prints the leak in `LEAK SUMMARY` but `0 errors from 0 contexts` (X9): this is why `full` is needed.
- No invalid read or write. `bob` is released in the right order, and `alice` and `bob` in `main` dangle after their frees (maps 3.2, H4) but are never dereferenced, so there is nothing to report. X16 shows the report that appears when one is dereferenced.

---

## 5. `crash_example`: a null-pointer write

### 5.1 The error

```text
Invalid write of size 4
   at 0x10928A: main (crash_example.c:32)
 Address 0x0 is not stack'd, malloc'd or (recently) free'd
```

A 4-byte store (`nums[0] = 42`, one `int`) to address 0. The faulting instruction, seen in gdb, is `movl $0x2a,(%rax)` with `rax = 0`, and `nums = 0x0` (X17).

Cause chain, from the code:

1. line 25: `n = 0`; line 30: `nums = allocate_numbers(n)`;
2. `allocate_numbers(0)` returns NULL at the guard `n <= 0` (lines 9-10). It is **not** a failed `malloc`: the program never calls `malloc` (the heap summary counts one allocation, the `stdout` buffer);
3. `main` does not check `nums`, so line 32 writes through NULL.

State at the fault (gdb run, same conventions as the maps):

```text
 STACK (main, CFA 0x7fffffff9ab0)            HEAP (the program allocated nothing)
   n    = 0     @ 0x7fffffff9a94              0x555555559000  chunk header (size field 0x291): tcache structure   [libc]
   nums = 0x0   @ 0x7fffffff9a98              0x555555559290  chunk header (size field 0x1011): stdout buffer     [libc]
        |                                     0x55555555a2a0  top chunk (unused rest of the heap)
        +--> address 0x0: no object
```

### 5.2 Classification: no object, so no lifetime violation

This is a **null-pointer dereference (write)**. Valgrind names no block, so it is not a use-after-free (no `free'd`) and not a buffer overflow (no `after a block`). There is no lifetime to violate: `nums` never pointed to an object, and the array the program intended to use (`n` integers) was never created. "Address 0x0 is not stack'd, malloc'd or (recently) free'd" is Valgrind's way of saying that address 0 belongs to nothing it knows (section 7, D3).

### 5.3 The termination message

```text
Process terminating with default action of signal 11 (SIGSEGV)
 Access not within mapped region at address 0x0
   at 0x10928A: main (crash_example.c:32)
```

This second message comes from Valgrind's core, about the real fault that follows the Memcheck error: Memcheck checks the store before it is executed, so its error is printed first. The signal is not a second error (`ERROR SUMMARY: 1 errors from 1 contexts`). The paragraph about a stack overflow is generic advice that Valgrind adds to a SIGSEGV in the main thread; address 0 is nowhere near the stack, so it does not apply. Valgrind ends with the same signal, which is why the shell sees status 139 (128 + 11).

### 5.4 The block that is "still reachable"

Abridged:

```text
4,096 bytes in 1 blocks are still reachable in loss record 1 of 1
   at malloc ... by _IO_file_doallocate ... by puts (ioputs.c:40) by main (crash_example.c:27)
```

This is the `stdout` buffer, allocated by the title line (`puts`, line 27). `total heap usage: 1 allocs, 0 frees`: it was never freed, because the process was killed by the signal before libc's exit code could run. It is not a leak: a pointer to it still exists in libc's `stdout` object (X12: in gdb, the field `_IO_buf_base` of that object holds `0x5555555592a0`, just after the chunk header at `0x555555559290`, and `_IO_buf_end` is 4,096 bytes later). So it is `still reachable`, listed only because of `--show-leak-kinds=all`, and not counted as an error.

The same crash also loses the program's own output. With stdout redirected to a file or a pipe, `crash_example` writes 0 bytes: its two output lines (the `puts` of line 27 and the `printf` of line 28) are still in the unflushed buffer when the process dies. In a terminal (line-buffered) both lines appear before the crash (X11). So the Valgrind report, not the program output, is what shows the problem.

---

## 6. Where the explanations were checked

| Report | Code | Memory maps | Experiments |
|---|---|---|---|
| use-after-free (rows 2-4) | free at line 38, accesses at lines 42, 44, 45; block of 20 bytes | 2.2 (K3, K4), 2.4 (dangling `a` and `b`), 2.5 (dead block contents) | X1, X2, X3, X4, X14 |
| leak (row 5) | `person_free_partial` frees only the struct (line 41); `person_new` stores the buffer only in `p->name` | 3.2 (H3, H4: `alice->name` marked leaked), 3.4 (same Valgrind counts) | X5, X6, X7, X9, X13, X16 |
| null write (rows 6-8) | guard at lines 9-10, unchecked result at line 30, write at line 32 | stack and heap conventions of 1.3 and 3.2; the `stdout` chunk of 3.2 | X11, X12, X17 |
| no report (row 1) | `local_buf[2..15]` never read | 1.3 (leftover bytes) | X8, X15 |

---

## 7. AI review log: what was incomplete or incorrect

### 7.1 Method

For `crash_example`, Claude first wrote 12 predictions (C1 to C12) from the source alone, before the program had been run, natively or under Valgrind. It then read the four raw reports and the sources and wrote 27 numbered claims (V1 to V27) about them. All of this was written before any verification experiment. Each claim was then tested against the source, against `memory_maps.md` and against the experiments of section 8.

### 7.2 Results

| Claims | Subject | Checked against | Verdict |
|---|---|---|---|
| V1 | the only heap block of `stack_example` is the `stdout` buffer | X13, X15 | confirmed |
| V2 | uninitialised bytes are reported only when used | maps 1.3, X8 | confirmed |
| V3-V6 | three use-after-free errors on one block, lifetime ended at line 38 | code, maps 2.2 and 2.4, X2 | confirmed |
| V7 | line 40 is not reported (no memory access) | X14, maps 2.4 | confirmed |
| V8 | Valgrind does not see variable names | X1 | confirmed |
| V9 | a context is a distinct call stack | X3, X8 | **partly incorrect (D2)** |
| V10-V11 | no leak; `22` printed under Valgrind hides the bug | maps 2.5 | confirmed |
| V12-V14 | the leak is `alice->name`; definitely lost by lost ownership; not indirectly lost, not reachable | maps 3.2, X5-X7, X16 | confirmed |
| V15, V18 | a leak is an error only with `--leak-check=full` | X7, X9 | confirmed |
| V16-V17 | allocation accounting; no invalid access | X13 | confirmed |
| V19-V20 | null write; cause is the guard in `allocate_numbers` | code, X17 | confirmed |
| V21 | meaning of `not stack'd, malloc'd or (recently) free'd` | X4 | **incomplete (D3)** |
| V22-V25 | termination block; `still reachable` buffer; one error; lost output | X11, X12 | confirmed |
| V26 | `--track-origins=yes` changes nothing here | X10 | confirmed |
| V27 | the size in `block of size S` is the requested size | X2 | confirmed |
| C1-C12 | predictions for `crash_example`, written before running it | the report, X11, X17 | 11 matched exactly; C8 matched except for the detail "created by the first printf" (D1) |

No claim was entirely wrong. Three statements were partly incorrect or incomplete (D1 to D3): D1 appeared in the memory maps and in prediction C8, D2 is claim V9 and D3 is claim V21. The other 25 claims were confirmed as written.

### 7.3 Incorrect or incomplete AI statements

**D1. The `stdout` buffer was attributed to the first `printf`: partly incorrect (first version of `memory_maps.md`, sections 1.7 and 5.2, and prediction C8).**

- *What the AI said:* the 4,096-byte block that appears in every Valgrind summary is created by the first `printf` of the program. Prediction C8, written before `crash_example` was run, says so in those words ("created by the first printf").
- *Check:* the `still reachable` record of `crash_example` gives the allocation stack `malloc <- _IO_file_doallocate <- ... <- puts (ioputs.c:40) <- main (crash_example.c:27)`. There is no `printf` in it. `objdump -d` shows exactly one call to `puts@plt` in each of the four programs, always in `main`, and a gdb breakpoint on `_IO_file_doallocate` shows, in each program, `puts` called from `main` at the title line: `stack_example.c:34`, `aliasing_example.c:28`, `heap_example.c:49`, `crash_example.c:27` (X15).
- *Why it was wrong:* the title line is `printf("...\n")` with no conversion specifier, and gcc turns such a call into `puts("...")`. In all four programs the first output call is that line. The AI described the source; Valgrind describes the binary. A reader who looks for `printf` in a Valgrind stack, with the source open, searches for a call that is not there.
- *Correction:* `memory_maps.md` now says, in sections 1.7 and 5.2, that the buffer is created by the first output call, a `puts` (entry C5 of its review log). The numbers in the maps (one 4,096-byte block, size field `0x1011`) were not affected. Prediction C9 repeats the wording ("the two printf lines"; one of them is a `puts`), without consequence for what it predicts (X11).

**D2. "A context is a distinct call stack" (claim V9): partly incorrect.**

- *What the AI said:* in `3 errors from 3 contexts`, a context is a distinct error site, i.e. a distinct call stack; lines 42, 44 and 45 are three different sites.
- *Check:* X3 reads `b[2]` three times on one line: `5 errors from 3 contexts`, so errors and contexts differ. X8 is more telling: the variant with an uninitialised read is hit by 8 `dump_frame` calls whose full call stacks are all different (recursion depth 0 to 3, "enter" and "exit"), and Valgrind prints `8 errors from 6 contexts`. The "enter" calls at depths 2 and 3 were merged because their top four frames are equal (`dump_frame` at line 11, `walk_stack` at line 24, `walk_stack` at line 28, `walk_stack` at line 28, in the scratch copy `e7_uninit.c`); the same happens for the two "exit" calls, whose common top four frames end with `walk_stack` at line 30, line 28, line 28. This matches the Valgrind manual (option `--num-callers`: "errors are commoned up using only the top four function locations"; https://valgrind.org/docs/manual/manual-core.html).
- *Why it was wrong:* the AI treated the whole call stack as the identity of a context. For `aliasing_example` the number is still right (three different lines), which is why the mistake was invisible there.
- *Correction:* the vocabulary (section 0.2) and section 3.5 define a context as a group of errors of the same kind with the same top four frames. "N contexts" must not be read as "N bugs" or "N lines".

**D3. "`(recently)` refers to a queue whose size is limited" (claim V21): incomplete, and the mental model behind the test was wrong.**

- *What the AI said:* `not stack'd, malloc'd or (recently) free'd` means that the address is in no stack, no live block and no block of the queue of freed blocks. That reading is correct for address 0 in `crash_example`. To test the word "recently", the AI expected that with `--freelist-vol=0` a freed block leaves the queue at once, and that a later access would then be reported with exactly that message.
- *Check:* X4a, `aliasing_example` with `--freelist-vol=0`: the report is unchanged (`8 bytes inside a block of size 20 free'd`). X4b, the same option and a `malloc(8)` added after `free(a)`: the report becomes `8 bytes inside an unallocated block of size 32 in arena "client"`. It is neither the expected message nor an attribution to the new block.
- *Why it was wrong:* the queue is trimmed when the next allocation happens, not when the block is freed, so with no allocation after `free` the block is still "recently" freed. And a block that has left the queue is still inside Valgrind's own heap, so it is reported as an unallocated block of that heap, with the allocator's size (32) and without allocation or free stacks. The `not stack'd, malloc'd or (recently) free'd` message is reserved for addresses outside every known region, such as 0.
- *Correction:* section 3.4 describes both forms, and the vocabulary (section 0.2) ties the message to unknown addresses. Practical consequence: the default 20 MB queue is what keeps use-after-free reports readable; with a tiny queue the bug is still found but the report loses its stacks.

---

## 8. Experiments

Scratch copies of the programs were modified one line at a time and run under Valgrind with the flags of section 0.1 (unless stated). The project files were not changed.

| ID | Change or command | Tests | Result |
|---|---|---|---|
| X1 | `aliasing_example.c`: lines 42, 44, 45 use `a` instead of `b` | V8 | reports identical: same kinds, data addresses, offsets and lines (only the file name of the scratch copy differs) |
| X2 | lines 42, 44, 45 access `b[4]`, `b[5]`, `b[6]` | V3-V5, V27 | `16 bytes inside`, `0 bytes after`, `4 bytes after` a block of size 20 free'd |
| X3 | line 42 replaced by a loop reading `b[2]` three times | V9 | `5 errors from 3 contexts` |
| X4 | `--freelist-vol=0` on the original (X4a); the same option on a copy with `q = malloc(8)` after `free(a)` and `free(q)` before the return (X4b); that copy with the default queue as a control (X4c) | V21 | X4a unchanged; X4b `8 bytes inside an unallocated block of size 32 in arena "client"`; X4c unchanged (`free'd`) |
| X5 | `heap_example.c`: `free(alice->name);` inserted before line 69 | V12, V13 | no leak, 5 allocs and 5 frees, 0 errors |
| X6 | `heap_example.c`: `static char *kept = NULL;` after line 2 and `kept = alice->name;` before line 69 | V13, V14 | `6 bytes in 1 blocks are still reachable`, 0 errors |
| X7 | `heap_example.c`: line 69 deleted | V14, V15 | `6 bytes ... indirectly lost`, `22 (16 direct, 6 indirect) bytes ... definitely lost`, 1 error |
| X8 | `stack_example.c`: `if (local_buf[2] != 0) puts(...)` after line 10 | V2, V9 | `Conditional jump ... uninitialised`, origin: stack allocation in `dump_frame`; 8 errors from 6 contexts; native run prints 8 times |
| X9 | `valgrind ./heap_example` (default flags) | V15, V18 | counts in `LEAK SUMMARY`, no allocation stack, 0 errors |
| X10 | the four programs without `--track-origins=yes` | V26 | reports identical |
| X11 | `crash_example` natively, stdout to a file, then to a pseudo-terminal | V25 | 0 bytes, then 2 lines; killed by SIGSEGV both times |
| X12 | gdb at the fault: fields of `_IO_2_1_stdout_` | V23 | `_IO_buf_base` = `0x5555555592a0`, `_IO_buf_end` = base + 4,096 |
| X13 | `valgrind --trace-malloc=yes ./heap_example` | V1, V16 | allocations 4096, 16, 6, 16, 4; frees of `bob->name`, `bob`, `alice`, the 4,096-byte buffer |
| X14 | gcc with the Makefile flags on `aliasing_example.c` | V7 | four `-Werror=use-after-free` errors, lines 40, 42, 44, 45 |
| X15 | `objdump -d` on the four programs; gdb breakpoint on `_IO_file_doallocate` in each of them | V1, D1 | one `puts@plt` call per program, in `main`; the buffer is allocated by `puts` at the title line (`stack_example.c:34`, `aliasing_example.c:28`, `heap_example.c:49`, `crash_example.c:27`) |
| X16 | `heap_example.c`: `printf("%p", *(void **)alice)` after line 69 | V13 | Valgrind: extra `Invalid read of size 8`, prints `0x4a7f0d0`, leak still definitely lost (2 errors); native: another value (`0x000055500000f7aa` in the gdb reference run, the mangled free-list pointer that glibc wrote over the first word) |
| X17 | gdb on `crash_example`: `x/i $pc`, `p $rax`, `p nums`, `p n` | V19, V20 | `movl $0x2a,(%rax)`, `rax = 0`, `nums = 0x0`, `n = 0` |

---

## Appendix: commands

```bash
cd programs
make stack_example heap_example crash_example
make aliasing_example CFLAGS="-Wall -Wextra -Werror -pedantic -std=gnu89 -g -Wno-error=use-after-free"

VG="valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes"
$VG ./stack_example
$VG ./aliasing_example
$VG ./heap_example
$VG ./crash_example > /dev/null; echo "status $?"       # 139: killed by SIGSEGV

# Valgrind's code addresses back to lines (load base 0x108000; addr2line wants a hexadecimal argument)
for a in 0x1092F5 0x109315 0x109323; do addr2line -e aliasing_example -f -s $(printf '0x%x' $((a - 0x108000))); done

# X2, X3: edit lines 42, 44, 45 (or 42 alone) of a copy of aliasing_example.c, rebuild with -g, rerun $VG
# X4: valgrind --leak-check=full --freelist-vol=0 ./aliasing_example
# X5-X7, X16: edit line 69 of a copy of heap_example.c as described in section 8, rebuild, rerun $VG
# X8: add the two lines of section 2 after line 10 of a copy of stack_example.c, rebuild, rerun $VG

# X9, X10: other flags
valgrind ./heap_example
valgrind --leak-check=full --show-leak-kinds=all ./crash_example > /dev/null

# X13: allocation accounting
valgrind --trace-malloc=yes ./heap_example 2>&1 >/dev/null | grep -E "malloc|free" | grep -vF "free(0x0)"

# X15: puts, not printf, for the title line (frames 5 and 6 of the backtrace: puts, then main)
for p in stack_example aliasing_example heap_example crash_example; do
    objdump -d --no-show-raw-insn ./$p | grep -c "call.*<puts@plt>"
    gdb -q -batch -ex 'set breakpoint pending on' -ex 'break _IO_file_doallocate' \
        -ex 'run > /dev/null' -ex 'bt 8' ./$p | grep -E "^#[56] " | cut -c1-110
done

# X12, X17: crash_example at the fault
gdb -q -batch -ex 'run > /dev/null' -ex 'x/i $pc' -ex 'p $rax' -ex 'p nums' -ex 'p n' \
    -ex 'set $f = *(long *)&stdout' -ex 'x/2gx $f + 0x38' ./crash_example

# X11: the output of crash_example is lost when stdout is not a terminal
./crash_example | cat        # prints nothing
./crash_example              # in a terminal: both lines appear, then "Segmentation fault"
```

Addresses printed by the gdb commands (for example `0x7fffffff9a98`) are those of the reference run under gdb, as in `memory_maps.md`; under another libc or environment they will differ, while the relations between them (a null `nums`, the buffer pointer inside `_IO_2_1_stdout_`) do not.