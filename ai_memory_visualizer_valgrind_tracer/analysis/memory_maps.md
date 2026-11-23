# Memory maps: stack, heap and pointer aliasing

Lab: *AI Memory Visualizer + Valgrind Tracer*, task 0 (`analysis/memory_maps.md`).

**Process.** An AI tool (Claude) produced a first-pass memory map from the source code alone, before anything was compiled or run. That draft was then checked against the real programs (compiler output, native runs, `gdb` at key lines, Valgrind) and corrected. Section 5 records where the AI draft was incomplete, misleading or wrong in detail, with the evidence. Every map below is the corrected version.

**What the maps show, in short**

- **Stack** (section 1): every call gets a frame below its caller's. `dump_frame` lands at the same addresses at every call of a given depth, and the bytes of a dead frame stay in memory until a later frame overwrites them.
- **Heap and aliasing** (section 2): the block allocated by `make_numbers` outlives that function's frame. `a` and `b` name the same block; `free(a)` ends its lifetime but leaves both pointers unchanged, so both dangle.
- **Ownership** (section 3): each `Person` owns its name buffer. Freeing the struct without the buffer destroys the only pointer to the buffer and leaks 6 bytes.
- **AI review** (section 5): the draft was correct on 28 of 29 checkable predictions; four corrections are documented (three incompleteness items and one wrong detail).

## 0. Scope, environment, conventions

| Program | Status in this task | Section |
|---|---|---|
| `stack_example.c` | mandatory | 1 |
| `aliasing_example.c` | mandatory | 2 |
| `heap_example.c` | optional, included | 3 |

`crash_example.c` belongs to the crash-report task and is not analysed here.

**Environment of every reported run:** Ubuntu 24.04 x86-64, gcc 13.3.0, glibc 2.39, Valgrind 3.22.0, gdb. Flags from the Makefile: `-Wall -Wextra -Werror -pedantic -std=gnu89 -g`. There is no `-O` flag, so gcc compiles without optimisation and every local variable gets a slot in its stack frame. With optimisation many of them would live in registers, so the frame layouts below are specific to this build.

**Build note.** GCC 12 added `-Wuse-after-free`, and `-Wall` enables it. With GCC 12 or newer, `-Werror` turns four warnings in `aliasing_example.c` (lines 40, 42, 44 and 45) into errors and `make` stops, even though that file is meant to contain the bug. It was built with:

```bash
make aliasing_example CFLAGS="-Wall -Wextra -Werror -pedantic -std=gnu89 -g -Wno-error=use-after-free"
```

**How to read the addresses**

- Native runs use ASLR, so stack and heap addresses change on every run. Over three native runs of `stack_example` and of `heap_example`, the bases moved but every difference between two addresses stayed identical.
- Each diagram comes from a **reference run under gdb** (ASLR off), where addresses repeat from run to run on this machine. Even there, absolute stack addresses move with the size of the environment, and some values are random in every run (the stack-protector canary, the tcache key). Absolute values on another machine will differ; the offsets should match if the compiler and libc are close.
- The stack grows toward **lower** addresses, the heap toward **higher** addresses. In these runs the stack is at `0x7ff...` addresses (the digit after `0x7ff` changes from run to run), while the program image and the heap are at `0x55...` addresses.
- **CFA** (gdb prints it as "frame at") is the address just above a frame's return address, i.e. `rsp` just before the `call`. A frame occupies `[CFA - size, CFA)`.

**Legend.** `[alive]`: lifetime started and not ended. `[dead]`: lifetime ended; the bytes may still be there, but no pointer may be used to reach them. `[dangling]`: pointer to a dead object. `[leaked]`: allocated object that no pointer reaches any more. String literals live in a third region, read-only data, which is neither stack nor heap.

---

## 1. `stack_example.c`: recursion and stack frames

### 1.1 What the program does

`main` calls `walk_stack(0, 3)`. `walk_stack(d, 3)` declares `marker = 10 * d`, calls `dump_frame("enter", d)`, recurses with `d + 1` until `d == 3`, then calls `dump_frame("exit", d)` while unwinding. `dump_frame` declares `local_int = 100 + d`, `local_buf[16]` (holding `'A' + d` and a terminator) and `p_local = &local_int`, and prints their addresses. The program never calls `malloc`: it only uses the stack, plus string literals.

### 1.2 Key execution points

`wsN` means the `walk_stack` call with `depth = N`.

| Point | Event | Frames alive, bottom to top |
|---|---|---|
| K0 | `main` starts | main |
| K1 | ws0 runs `dump_frame("enter", 0)` | main, ws0, dump_frame |
| K2 | `dump_frame` returned, ws0 calls ws1 | main, ws0, ws1 |
| K3 | deepest point: ws3 runs `dump_frame("enter", 3)` | main, ws0, ws1, ws2, ws3, dump_frame |
| K4 | ws3 returned, ws2 runs `dump_frame("exit", 2)` | main, ws0, ws1, ws2, dump_frame |
| K5 | `walk_stack(0, 3)` returned | main |

### 1.3 Map at the deepest point (K3)

```text
 high addresses
 0x7fffffff9ab0 +-----------------------------------------------------------+  <- CFA of main
                | main              (no local variables)                    |
                |   return address -> libc (0x7ffff7c2a1ca)    @ ...9aa8    |
                |   saved rbp                                  @ ...9aa0    |
 0x7fffffff9aa0 +-----------------------------------------------------------+
                | walk_stack(0, 3)  [alive]   marker = 0       @ ...9a84    |  48 bytes
 0x7fffffff9a70 +-----------------------------------------------------------+
                | walk_stack(1, 3)  [alive]   marker = 10      @ ...9a54    |  48 bytes
 0x7fffffff9a40 +-----------------------------------------------------------+
                | walk_stack(2, 3)  [alive]   marker = 20      @ ...9a24    |  48 bytes
 0x7fffffff9a10 +-----------------------------------------------------------+
                | walk_stack(3, 3)  [alive]   marker = 30      @ ...99f4    |  48 bytes
 0x7fffffff99e0 +-----------------------------------------------------------+
                | dump_frame("enter", 3)  [alive]   detail below            |  80 bytes
 0x7fffffff9990 +-----------------------------------------------------------+  <- rsp
 low addresses                  the stack grows in this direction
```

**A `walk_stack` frame (48 bytes), shown for ws3** (CFA = `0x7fffffff9a10`):

| CFA offset | Slot | Content |
|---|---|---|
| -0x08 | return address | `0x5555555552be`, inside `walk_stack` just after the recursive call (back into ws2) |
| -0x10 | saved `rbp` | the `rbp` of ws2 |
| -0x18 | stack-protector canary | random 8 bytes, different on every run, but the same value in every protected frame of one run (`0x207556d2810ba300` here, also stored in the `dump_frame` frame) |
| -0x1c | `marker` | 30 |
| -0x20 | padding (4 bytes) | not used |
| -0x24 | `depth` | 3 (spilled copy of the argument) |
| -0x28 | `max_depth` | 3 (spilled copy of the argument) |
| -0x30 | unused (8 bytes) | not used |

**The `dump_frame` frame (80 bytes) at K3**, read with `x/10gx $rsp` at line 12, after the locals were initialised (CFA = `0x7fffffff99e0`):

| CFA offset | Address | Slot | Content |
|---|---|---|---|
| -0x08 | `0x7fffffff99d8` | return address | `0x555555555286`, back into ws3 right after its call |
| -0x10 | `0x7fffffff99d0` | saved `rbp` | `0x7fffffff9a00`, the `rbp` of ws3 |
| -0x18 | `0x7fffffff99c8` | canary | `0x207556d2810ba300`, random per run; its lowest byte is always `00` |
| -0x20 | `0x7fffffff99c0` | padding (8 bytes) | leftover bytes, never written by `dump_frame` |
| -0x30 | `0x7fffffff99b0` | `local_buf[16]` | `'D' '\0'`, then 14 untouched bytes `ff ff ff 7f 00 00 2b 52 55 55 55 55 00 00` |
| -0x38 | `0x7fffffff99a8` | `p_local` | `0x7fffffff99a4`, which is `&local_int` |
| -0x3c | `0x7fffffff99a4` | `local_int` | 103 |
| -0x40 | `0x7fffffff99a0` | padding (4 bytes) | not used |
| -0x48 | `0x7fffffff9998` | `label` (argument) | `0x555555556069`, the string `"enter"` in read-only data |
| -0x4c | `0x7fffffff9994` | `depth` (argument) | 3 |
| -0x50 | `0x7fffffff9990` | padding (4 bytes) | `rsp` points here |

What this map shows:

- The local variables are **not** laid out in declaration order. The char array is placed highest, next to the canary, then `p_local`, then `local_int` (`local_buf` is 0xc above `&local_int` in every run).
- `p_local` is a pointer from one slot of the frame to another slot of the same frame. It is valid only while `dump_frame` is alive.
- `local_buf[2..15]` is **not** zero. The program only writes `local_buf[0]` and `local_buf[1]`; the other 14 bytes are whatever earlier calls left on the stack. Here they are the upper bytes of a stack address (`ff ff ff 7f 00 00`), then `0x55555555522b`, a return address inside `dump_frame` itself (`dump_frame + 162`), pushed when an earlier `dump_frame` call called another function.
- `"enter"` and `"exit"` are string literals in read-only data (`0x5555555560xx`). The pointer `label` is on the stack, but what it points to is neither on the stack nor on the heap.
- `main` is not the bottom of the chain: its frame is 16 bytes (return address and saved `rbp`, no locals), and the return address `0x7ffff7c2a1ca` is `__libc_start_call_main + 122`, in libc. `main` is an ordinary function called by libc's start-up code.

### 1.4 Going back up (K4): same addresses, dead memory is not erased

`dump_frame` is called from `walk_stack(d)` with the same stack pointer every time, so its frame sits at the same addresses for `"enter"` and for `"exit"`. Native run 1 (ASLR on), identical for enter and exit at every depth:

| depth | `&marker` | `&local_int` | `local_buf` |
|---|---|---|---|
| 0 | `0x7ffe90781ef4` | `0x7ffe90781ea4` | `0x7ffe90781eb0` |
| 1 | `0x7ffe90781ec4` | `0x7ffe90781e74` | `0x7ffe90781e80` |
| 2 | `0x7ffe90781e94` | `0x7ffe90781e44` | `0x7ffe90781e50` |
| 3 | `0x7ffe90781e64` | `0x7ffe90781e14` | `0x7ffe90781e20` |

In three runs the base changed (`0x7ffe90781ef4`, `0x7ffd995c1444`, `0x7ffd39cf2684` for `&marker` at depth 0) but the steps did not: -0x30 per depth, `&marker - local_buf = 0x44`, `local_buf - &local_int = 0xc`.

K4, reference run (ws3 and its `dump_frame` calls are dead):

```text
 0x7fffffff9a40 +-----------------------------------------------------------+  <- CFA of walk_stack(2)
                | walk_stack(2, 3)  [alive]   marker = 20      @ ...9a24    |
 0x7fffffff9a10 +-----------------------------------------------------------+  <- CFA of this dump_frame
                | dump_frame("exit", 2)  [alive]   80 bytes                 |
                |   padding      @ ...99f0   the 4 bytes at ...99f4 still   |
                |                            hold 30: the old marker of the |
                |                            dead walk_stack(3)             |
                |   local_buf    @ ...99e0   bytes 8..15 hold the ints 3, 3:|
                |                            the old arguments of the dead  |
                |                            walk_stack(3)                  |
 0x7fffffff99c0 +-----------------------------------------------------------+  <- rsp
                | below rsp: dead bytes of the earlier dump_frame calls     |
```

The frame of ws3 no longer exists, but its bytes were not erased: they were overwritten only where the new `dump_frame` frame writes something. The old `marker` slot (`0x7fffffff99f4`) still reads 30, and the two old argument slots (`max_depth` at `0x7fffffff99e8`, `depth` at `0x7fffffff99ec`) still read 3 and 3; they now sit inside `local_buf[8..15]` of the new call. This is where the garbage inside an uninitialised local array comes from.

### 1.5 Lifetimes

| Object | Where | Lifetime starts | Lifetime ends |
|---|---|---|---|
| `marker`, `depth`, `max_depth` of ws*d* | frame of ws*d* | entry of `walk_stack(d)` | return of `walk_stack(d)` |
| `local_int`, `local_buf`, `p_local`, `label`, `depth` of one `dump_frame` call | frame of that call | entry of that `dump_frame` call | return of that call |
| the 8 `dump_frame` calls | 4 depths x (enter, exit) | each call creates new objects | each call destroys them; the next call reuses the same addresses |
| `"enter"`, `"exit"`, the printf format strings | read-only data | program start | program end |
| frame of `main` | stack | program start | `main` returns |

`marker` of ws*d* stays alive during the whole recursion below it, and no deeper frame overlaps it.

### 1.6 Pointer aliasing

- `p_local` and `&local_int` are the same address in all 8 `dump_frame` calls (checked in the output). Two names for one object, in the same frame.
- `label` points into read-only data. It is a copy of the address of the literal passed by the caller.
- No pointer leaves its frame, which is why the program is correct. If `dump_frame` returned `p_local`, the caller would hold a pointer to a dead object (use after return).

### 1.7 Heap

None in the source. Valgrind reports `total heap usage: 1 allocs, 1 frees, 4,096 bytes allocated`: that single block is the buffer that libc allocates for `stdout` at the first `printf`. 0 errors, no leak.

---

## 2. `aliasing_example.c`: aliasing and use after free

### 2.1 What the program does

`main` keeps `n`, `a` and `b` in its frame. `make_numbers(5)` calls `malloc(20)`, fills `{0, 11, 22, 33, 44}` and returns the address. `b = a` creates an alias. `free(a)` ends the block's lifetime. The program then reads `b[2]` (line 42), writes `b[3] = 1234` (line 44) and reads `b[3]` (line 45): three accesses through a dangling pointer.

### 2.2 Key execution points

| Point | Line | Event |
|---|---|---|
| K0 | 28 | `main` frame initialised: `n = 5`, `a = NULL`, `b = NULL` |
| K1 | 19 | inside `make_numbers`, about to `return arr` |
| K2 | 36 | back in `main`, after `a = make_numbers(n)` and `b = a` |
| K3 | 40 | after `free(a)` |
| K4 | 45 | after `b[3] = 1234` |

**K1: two frames on the stack, one block on the heap**

```text
 STACK (grows down)                                           HEAP (grows up)
 0x7fffffff9aa0 +---------------------------+
                | main                      |
                |   b = NULL     @ ...9a88  |
                |   a = NULL     @ ...9a80  |
                |   n = 5        @ ...9a7c  |
 0x7fffffff9a70 +---------------------------+                0x55555555a2b0
                | make_numbers              |                +----+----+----+----+----+
                |   arr = 0x55555555a2b0 ------------------> | 0  | 11 | 22 | 33 | 44 |  [alive]
                |   i   = 5      @ ...9a54  |                +----+----+----+----+----+
                |   n   = 5 (copy) @ ...9a4c|                 20 bytes requested,
                +---------------------------+                 32-byte chunk (header 0x21)
```

`n` in `make_numbers` is a copy: it lives at `0x7fffffff9a4c`, not at the address of the `n` of `main` (`0x7fffffff9a7c`). `arr` itself is at `0x7fffffff9a58`: it is a local variable that holds the address of the block.

**K2: the frame is gone, the block survives, two names for it**

```text
 STACK                                                         HEAP
 main (alive)
   b = 0x55555555a2b0 -----------+
   a = 0x55555555a2b0 -----------+---------------------------> [ 0 | 11 | 22 | 33 | 44 ]  [alive]
   n = 5
 make_numbers frame (just below 0x7fffffff9a70): [dead]
   (its `arr` died with it; the address survives in `a`)
```

**K3: `free(a)`. Both pointers still hold the address, the object is dead**

```text
 STACK                                                         HEAP
 main (alive)
   b = 0x55555555a2b0 -----------+   [dangling]
   a = 0x55555555a2b0 -----------+---------------------------> [ freed chunk, owned by the allocator ]  [dead]
   n = 5                                                           bytes  0..7   next pointer  -> b[0], b[1]
                                                                   bytes  8..15  tcache key    -> b[2], b[3]
                                                                   bytes 16..19  still 44      -> b[4]
```

**K4:** `b[3] = 1234` overwrites half of the allocator's key inside the freed chunk. Nothing crashes, because the page is still mapped and owned by the process. The program exits with status 0 and the operating system reclaims everything.

### 2.3 Lifetimes

| Object | Where | Lifetime |
|---|---|---|
| `n`, `a`, `b` of `main` | stack, frame of `main` | whole program |
| `n` (copy), `i`, `arr` of `make_numbers` | stack, frame of `make_numbers` | from the call at line 30 until the function returns |
| the 20-byte block | heap | `malloc` at line 12 until `free` at line 38 |

The block outlives the frame that created it: ownership leaves `make_numbers` through the returned pointer. After line 38 the block is dead, even though its bytes are still physically in memory.

### 2.4 Aliasing

- From line 34 (`b = a`) to line 38, `a` and `b` are two names for the same object. A write through `b[3]` would be visible through `a[3]`.
- `free(a)` receives a **copy** of `a`, so it cannot set `a` or `b` to `NULL`. gdb confirms that both still hold `0x55555555a2b0` afterwards. Freeing through either name kills both.
- Two different views of line 40 (`printf("... b=%p", b)`):
  - *Machine view:* the 8 bytes of `b` are unchanged, so the same address is printed.
  - *Language view:* after `free`, the value of every pointer to that block is indeterminate (C11 6.2.4). Using it is undefined behaviour, even just to print it. GCC reports lines 40, 42, 44 and 45 at compile time. Valgrind reports only lines 42, 44 and 45, because line 40 reads no heap memory. The absence of a Valgrind report on line 40 does not make line 40 valid.

### 2.5 What the dead block contains (undefined behaviour, observed on this machine)

Before `free`, `x/6dw a` shows `0 11 22 33 44 0`. After `free`, `x/6dw b` shows `1431655770 5 -1200905697 -1403121567 44 0` in the reference run (`b[2]` and `b[3]` come from a random key and change on every run):

- bytes 0..7: `0x000000055555555a`, which is `0x55555555a2b0 >> 12`. glibc 2.39 stores the "next" pointer of its free list mangled with the chunk address, and the list was empty. This gives `b[0] = 1431655770` and `b[1] = 5`.
- bytes 8..15: `0xac5e1061b86ba21f`, the random per-process tcache key, which gives `b[2]` and `b[3]`.
- bytes 16..19 (`b[4] = 44`) are untouched. The chunk header at `b - 8` still reads `0x21`.

So the line `reading b[2]` does not print the stale value 22 when run natively. Three native runs printed `183939990`, `962339381` and `1126703990`. Under Valgrind the same line prints `22`, because Valgrind replaces `malloc`/`free` and does not write allocator data into freed blocks. The same undefined behaviour gives different results with different allocators or tools, and none of them is guaranteed.

### 2.6 Valgrind evidence

Abridged output of `valgrind --leak-check=full --track-origins=yes ./aliasing_example` (library names and code addresses removed):

```text
Invalid read of size 4
   at main (aliasing_example.c:42)
 Address 0x4a7f088 is 8 bytes inside a block of size 20 free'd
   at free
   by main (aliasing_example.c:38)
 Block was alloc'd at
   at malloc
   by make_numbers (aliasing_example.c:12)
   by main (aliasing_example.c:30)

Invalid write of size 4   (line 44, 12 bytes inside a block of size 20 free'd)
Invalid read of size 4    (line 45, 12 bytes inside a block of size 20 free'd)

HEAP SUMMARY: total heap usage: 2 allocs, 2 frees, 4,116 bytes allocated
ERROR SUMMARY: 3 errors from 3 contexts
```

The 2 allocations are the `malloc(20)` of the program plus the 4,096-byte `stdout` buffer of libc (20 + 4,096 = 4,116 bytes).

---

## 3. `heap_example.c` (optional): ownership and a deliberate leak

### 3.1 What the program does

`Person` is `{ char *name; int age; }`, which is **16 bytes**: 8 for the pointer, 4 for the int, 4 of padding (gdb: `ptype /o Person`). `person_new` allocates the struct, then a name buffer of `len + 1` bytes, and copies the name. `main` builds `alice` and `bob`. It frees `bob` completely (`free(bob->name)`, then `free(bob)`), but frees `alice` with `person_free_partial`, which releases only the struct. `alice->name` is never freed.

### 3.2 Key execution points

| Point | Line | Event |
|---|---|---|
| H0 | 49 | `main` frame: `alice = NULL`, `bob = NULL` |
| H1 | 33 | inside `person_new("Alice", 30)`, about to return |
| H2 | 63 | both persons built |
| H3 | 69 | after `free(bob->name); free(bob);`, before `person_free_partial(alice)` |
| H4 | 71 | after `person_free_partial(alice)` |

**H1: the callee holds the only pointer to the new struct, for a moment**

```text
 STACK                                                         HEAP
 main (alive)       alice = NULL @ 0x7fffffff9a90
                    bob   = NULL @ 0x7fffffff9a98
 person_new (alive) p    = 0x55555555a2b0 @ ...9a78 ------>  struct { name = 0x55555555a2d0, age = 30 }
                    len  = 5             @ ...9a70                          |
                    name = 0x555555556038 ("Alice",                         v
                           read-only data)                   "Alice\0"  (6 bytes)
```

When `person_new` returns, `p` dies and the address is handed over: `alice` receives `0x55555555a2b0`.

**H2: both persons built (reference run, line 63)**

```text
 STACK (main, alive)                           HEAP, from low to high addresses
   alice = 0x55555555a2b0 @ 0x7fffffff9a90     0x555555559000  chunk header (size field 0x291): tcache structure   [libc]
   bob   = 0x55555555a2f0 @ 0x7fffffff9a98     0x555555559290  chunk header (size field 0x1011): stdout buffer     [libc]
                                               0x55555555a2a0  chunk header (size field 0x21)
                                               0x55555555a2b0  alice : { name = 0x55555555a2d0, age = 30 }   16 B
                                               0x55555555a2c0  chunk header (0x21)
                                               0x55555555a2d0  "Alice\0"   <- alice->name                      6 B
                                               0x55555555a2e0  chunk header (0x21)
                                               0x55555555a2f0  bob   : { name = 0x55555555a310, age = 41 }   16 B
                                               0x55555555a300  chunk header (0x21)
                                               0x55555555a310  "Bob\0"     <- bob->name                        4 B
                                               0x55555555a320  top chunk (unused rest of the heap)
```

Each request is served by a 32-byte chunk, and nothing is freed before the last allocation, so the four blocks are consecutive and `0x20` apart: `alice->name = alice + 0x20`, `bob = alice + 0x40`, `bob->name = alice + 0x60`. This held in all three native runs; only the base moved. The lowest bit of a size field is a flag (the previous chunk is in use), so `0x21` describes a chunk of `0x20` bytes, `0x291` one of `0x290` bytes and `0x1011` one of `0x1010` bytes.

Ownership: `main` owns two pointers; each struct owns exactly one name buffer through its `name` field.

```text
 alice -> [struct] -> [name "Alice"]          bob -> [struct] -> [name "Bob"]
```

**H3 and H4: what is freed, what is lost**

```text
 After H3 (line 69):   bob = 0x55555555a2f0 [dangling]
   0x55555555a2b0  alice struct    ALLOCATED   { name = 0x55555555a2d0, age = 30 }
   0x55555555a2d0  "Alice\0"       ALLOCATED
   0x55555555a2f0  bob struct      FREED, head of the free list, next -> 0x55555555a310
   0x55555555a310  bob->name       FREED, last of the free list (it was freed first)

 After H4 (line 71):   alice = 0x55555555a2b0 [dangling]    bob = 0x55555555a2f0 [dangling]
   0x55555555a2b0  alice struct    FREED, head of the free list, next -> 0x55555555a2f0
   0x55555555a2d0  "Alice\0"       ALLOCATED, no pointer to it anywhere              [leaked, 6 bytes]
   0x55555555a2f0  bob struct      FREED, next -> 0x55555555a310
   0x55555555a310  bob->name       FREED, last of the free list
```

Why the leak is total: the only copy of `0x55555555a2d0` was the `name` field inside the alice struct. At `free`, glibc overwrote the first 16 bytes of that struct with its free-list data. With gdb at line 71, the first word is `0x000055500000f7aa`, which is `0x55555555a2f0` (the address of bob's chunk) mangled with `0x55555555a`; the second word is the tcache key. A search at line 71 finds no copy of the 8 bytes of `0x55555555a2d0` in the heap or in the stack (including the dead part below `rsp`), and none in the registers. A scan of every readable mapping gave the same result. `"Alice"` itself is still intact at `0x55555555a2d0`: the data is there, but nothing can reach it, which is what Valgrind reports as *definitely lost*.

### 3.3 Lifetimes and ownership

| Object | Where | Lifetime |
|---|---|---|
| `alice`, `bob` (pointers) | stack, frame of `main` | whole program |
| `p`, `len`, `i` of `person_new` | stack, frame of `person_new` | from the call to the return (twice) |
| alice struct | heap | `malloc` line 14 (call at line 51) until `free` line 41 (call at line 69) |
| `alice->name` | heap | `malloc` line 21 (call at line 51), **never freed** |
| bob struct | heap | `malloc` line 14 (call at line 52) until `free` line 67 |
| `bob->name` | heap | `malloc` line 21 (call at line 52) until `free` line 66 |
| `"Alice"`, `"Bob"` literals | read-only data | whole program |

Order matters: `free(bob->name)` comes before `free(bob)` because after `free(bob)` the field `bob->name` could no longer be read. `person_free_partial` does the opposite mistake: it frees the parent and drops the only reference to the child.

### 3.4 Valgrind evidence

Abridged output of `valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./heap_example`:

```text
HEAP SUMMARY:
    in use at exit: 6 bytes in 1 blocks
  total heap usage: 5 allocs, 4 frees, 4,138 bytes allocated

6 bytes in 1 blocks are definitely lost in loss record 1 of 1
   at malloc
   by person_new (heap_example.c:21)
   by main (heap_example.c:51)

LEAK SUMMARY:  definitely lost: 6 bytes in 1 blocks   indirectly lost: 0   possibly lost: 0   still reachable: 0
ERROR SUMMARY: 1 errors from 1 contexts
```

5 allocations = the `stdout` buffer (4,096) + two structs (16 + 16) + two names (6 + 4). 4 frees = `bob->name`, `bob`, the alice struct, and the `stdout` buffer that libc releases at exit. The 1 error is the leak itself: no invalid read or write.

Under Valgrind the blocks are further apart (`alice`, then `+0x50`, `+0xa0`, `+0xf0`) than natively (`+0x20` steps): the spacing belongs to the allocator, not to the language. Ownership and aliasing relations are the same everywhere.

---

## 4. Stack and heap side by side

| | Stack | Heap |
|---|---|---|
| Created by | a function call (the frame is pushed) | `malloc` |
| Destroyed by | the return of the function, automatically | `free`, explicitly (or never: leak) |
| Lifetime is tied to | the call | the decisions of the program |
| Direction | grows toward lower addresses | grows toward higher addresses |
| Addresses in these runs | `0x7ff...` | `0x55...` |
| Objects here | `marker`, `local_int`, `local_buf`, `p_local`, `n`, `a`, `b`, `alice`, `bob`, `p`, `len` | the 20-byte int array, the two `Person` structs, the two name buffers, plus libc's own blocks (tcache structure, `stdout` buffer) |
| Typical bug | use after return (pointer to a dead frame) | use after free, leak, double free |
| Seen in this lab | none: no pointer leaves its frame (section 1) | dangling `a`, `b` (section 2); lost `alice->name` (section 3) |

Read-only data is a third region: string literals and format strings are neither on the stack nor on the heap, and they live as long as the program.

---

## 5. AI review log: where the AI draft was incomplete, misleading or wrong

### 5.1 Method

The first pass was written from the source code only, before compiling or running anything, with 29 numbered predictions that can be checked (address relations, frame sizes, printed values, Valgrind counts). It was then checked with the Makefile build, three native runs per program, `gdb` at key lines and Valgrind (`--leak-check=full --track-origins=yes`).

28 of the 29 predictions matched exactly; one (S11) was right in kind and wrong in detail (C4 below). The ones that mattered most, and the one that failed:

| AI draft prediction | Check | Result |
|---|---|---|
| frames of `walk_stack` are 48 bytes apart | `info frame`, three runs | confirmed (0x30) |
| enter and exit use the same addresses at a given depth | three runs | confirmed |
| `local_buf` sits above `local_int`, not in declaration order | addresses differ by 0xc | confirmed |
| 6 frames at the deepest point | `bt` in gdb | confirmed |
| `sizeof(Person) == 16` | gdb | confirmed |
| four blocks `0x20` apart natively | gdb, three runs | confirmed |
| `free(a)` leaves `a` and `b` unchanged | gdb | confirmed |
| `b[2]` is not 22 natively but is 22 under Valgrind | three runs + Valgrind | confirmed |
| 3 invalid accesses: lines 42, 44, 45 | Valgrind | confirmed |
| 6 bytes definitely lost, 5 allocs and 4 frees | Valgrind | confirmed |
| no pointer to `alice->name` survives anywhere | gdb at line 71 | confirmed |
| native stack addresses look like `0x7ffc...` / `0x7ffd...` | three native runs | partly wrong: `0x7ffe...` seen (C4) |

The draft was right on every measured value except the prefix of the stack addresses. Its main weakness was **incompleteness**: three places where it was incomplete or framed too narrowly (C1 to C3), plus one small wrong detail (C4).

### 5.2 Corrections

**C1 (incomplete). The frame diagram listed only the named variables.**

- *AI draft:* `dump_frame: local_int=103, local_buf="D", p_local=&local_int`, i.e. a frame made of its variables. It predicted that `local_buf` sits above `local_int`, but did not place `p_local` and did not show any other slot.
- *Evidence:* `info frame` and `x/10gx $rsp` at line 12: the frame is 80 bytes and only 4 + 16 + 8 of them are the three named locals. The rest is return address, saved `rbp`, stack-protector canary, spilled arguments (`label`, `depth`) and padding. The full order from high to low addresses is `local_buf`, `p_local`, `local_int`.
- *Why it was incomplete:* a variables-only map cannot explain why a frame is 0x30 or 0x50 bytes, nor why `local_buf` contains garbage after its second byte.
- *Correction:* section 1.3 gives the slot table of both frames and identifies the leftovers in `local_buf` (the upper bytes of a stack address and a return address into `dump_frame`). Section 1.4 shows the mechanism: the ints 3 and 3 left by the dead `walk_stack(3)` frame sit inside `local_buf[8..15]` of the next call.

**C2 (incomplete). The heap diagrams showed only the blocks that the source allocates.**

- *AI draft:* "one block" for `aliasing_example` and "four consecutive blocks" for `heap_example`. The draft counted libc's `stdout` buffer in its Valgrind prediction but did not draw it.
- *Evidence:* Valgrind reports `2 allocs` for the one `malloc(20)` of `aliasing_example` and `5 allocs` for the four `malloc` calls of `heap_example`. gdb shows two libc-owned chunks before `alice`: the tcache structure (size field `0x291`) and the `stdout` buffer (size field `0x1011`: 4,096 bytes of buffer, created by the first `printf`). On a terminal that buffer is 1,024 bytes, so `alice` ends in `...6b0` instead of `...2b0` (checked on a pseudo-terminal). Valgrind spaces the blocks `0x50` apart where glibc uses `0x20`.
- *Why it was incomplete:* a heap map that starts at the program's first block misplaces every address and hides allocations the program did not ask for. Offsets between blocks are also a property of the allocator, not of C.
- *Correction:* section 3.2 draws the whole heap, with the libc blocks and the chunk headers, and sections 2 and 3 state that spacing differs under Valgrind.

**C3 (incomplete, and misleading by omission). The draft said what `b` holds after `free`, not whether the program may use it.**

- *AI draft:* after `free(a)`, `b` prints the same address because `free` receives a copy of the pointer. The undefined behaviour was discussed only for the three accesses through `b` (lines 42, 44, 45); whether using the pointer value itself (line 40) is allowed was never mentioned.
- *Evidence:* gdb confirms that `a` and `b` keep the same bits. But GCC 13, with the `-Wall` of the Makefile, flags four lines as use after free (40, 42, 44, 45), and `-Werror` makes the provided Makefile fail on `aliasing_example.c`. Valgrind flags only three lines (42, 44, 45).
- *Why it was incomplete:* two questions were mixed. "Which bits does the variable hold?" (machine view: unchanged) is not "may the program use that value?" (language view: indeterminate after `free`, C11 6.2.4). A static check and a dynamic check apply different definitions, so "Valgrind says nothing about line 40" cannot be read as "line 40 is valid".
- *Correction:* section 2.4 states both views, and the build note in section 0 documents the compile error and the workaround.

**C4 (wrong in detail, minor). The draft gave the wrong prefixes for native stack addresses.**

- *AI draft:* prediction S11 said that stack addresses "look like `0x7ffc...` / `0x7ffd...`".
- *Evidence:* the three native runs of `stack_example` gave `0x7ffe90781ef4`, `0x7ffd995c1444` and `0x7ffd39cf2684` for `&marker` at depth 0; the gdb runs, with ASLR off, give `0x7fffffff....`.
- *Why it was wrong:* the draft turned two typical values into a rule. ASLR picks the digit after `0x7ff` anew for every run, so only the `0x7ff` prefix is a safe expectation. It does not affect any of the maps, which rely on differences between addresses.
- *Correction:* section 0 gives the prefix as `0x7ff...` and says that the next digit changes from run to run.

---

## Appendix: commands to reproduce the observations

Run them from the root of the lab directory. The hard-coded addresses and `info symbol` arguments are those of the reference runs on the machine described in section 0. With another libc or another environment, read the addresses from the `bt`, `info frame` and `p` output instead, and expect different absolute values.

```bash
cd programs
make stack_example heap_example crash_example
make aliasing_example CFLAGS="-Wall -Wextra -Werror -pedantic -std=gnu89 -g -Wno-error=use-after-free"

# native runs: repeat and compare the differences between addresses, not the addresses
./stack_example ; ./stack_example ; ./aliasing_example ; ./aliasing_example ; ./heap_example

# section 1: frames at the deepest point, full dump_frame frame (80 bytes), 6 frames in bt,
# and the code that two return addresses belong to
gdb -q -batch -ex 'break stack_example.c:12 if depth == 3' -ex 'run > /dev/null' \
    -ex 'bt' -ex 'info frame' -ex 'x/10gx $rsp' -ex 'p &local_int' -ex 'p &local_buf' \
    -ex 'x/2gx 0x7fffffff9aa0' -ex 'info symbol 0x00007ffff7c2a1ca' -ex 'info symbol 0x55555555522b' ./stack_example

# section 1.4: dead memory during the unwinding (old marker slot and old arguments of ws3)
gdb -q -batch -ex "break stack_example.c:12 if depth == 2 && label[1] == 'x'" -ex 'run > /dev/null' \
    -ex 'x/dw $rbp-0xc' -ex 'x/2dw (char *)&local_buf + 8' ./stack_example

# section 2: the block before and after free(a)
gdb -q -batch -ex 'break aliasing_example.c:38' -ex 'break aliasing_example.c:40' -ex 'run > /dev/null' \
    -ex 'x/6dw a' -ex 'continue' -ex 'p b' -ex 'x/6dw b' -ex 'x/4gx b' -ex 'x/2gx (char *)b - 8' ./aliasing_example

# section 3: sizeof, struct layout, raw heap with the chunk headers, state after the frees
gdb -q -batch -ex 'break heap_example.c:63' -ex 'run > /dev/null' -ex 'p sizeof(Person)' \
    -ex 'x/2gx 0x555555559000' -ex 'x/2gx 0x555555559290' -ex 'x/16gx (char *)alice - 0x10' ./heap_example
gdb -q -batch -ex 'break heap_example.c:71' -ex 'run > /dev/null' -ex 'x/4gx alice' ./heap_example

# section 3.2: no copy of the lost pointer survives (the first `find` is a control: it must find `alice`)
# the addresses are those of the gdb runs above (ASLR off)
gdb -q -batch -ex 'break heap_example.c:71' -ex 'run > /dev/null' \
    -ex 'find /g $rsp, +0x40, 0x55555555a2b0' \
    -ex 'find /g 0x555555559000, +0x21000, 0x55555555a2d0' \
    -ex 'find /g $rsp-0x2000, +0x7000, 0x55555555a2d0' ./heap_example

# Valgrind (errors, leak, allocation counts)
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./aliasing_example
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./heap_example
valgrind --leak-check=full ./stack_example
```