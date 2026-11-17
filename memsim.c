/*
 * Memory Management Simulator
 * ---------------------------------------------------------------
 * 1. Contiguous allocation (First Fit / Best Fit / Worst Fit)
 *    with free, coalescing, compaction and fragmentation stats
 * 2. Paging (page table, logical -> physical address translation)
 * 3. Page replacement (FIFO / LRU / Optimal) with step-by-step trace
 *
 * Build: gcc -Wall -Wextra -O2 -o memsim memsim.c
 * Run  : ./memsim
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------- helpers ----------------------------- */

static int read_int(const char *prompt, int min, int max)
{
    char buf[128];
    long v;
    char *end;
    for (;;) {
        printf("%s", prompt);
        if (!fgets(buf, sizeof buf, stdin)) exit(0);
        v = strtol(buf, &end, 10);
        if (end != buf && (*end == '\n' || *end == '\0') && v >= min && v <= max)
            return (int)v;
        printf("  Invalid input. Enter a number between %d and %d.\n", min, max);
    }
}

static void line(char c, int n)
{
    while (n--) putchar(c);
    putchar('\n');
}

/* ================================================================== */
/*                     PART 1: CONTIGUOUS ALLOCATION                  */
/* ================================================================== */

#define MAX_BLOCKS 128

typedef struct { int start, size, pid; } Block;   /* pid 0 => free hole */

static Block blocks[MAX_BLOCKS];
static int nblocks = 0;
static int mem_total = 0;

typedef enum { FIRST_FIT = 1, BEST_FIT, WORST_FIT } FitAlgo;

static const char *algo_name(FitAlgo a)
{
    return a == FIRST_FIT ? "First Fit" : a == BEST_FIT ? "Best Fit" : "Worst Fit";
}

static void cont_init(int total)
{
    mem_total = total;
    blocks[0].start = 0;
    blocks[0].size = total;
    blocks[0].pid = 0;
    nblocks = 1;
}

static void insert_block(int idx, Block b)
{
    for (int i = nblocks; i > idx; i--) blocks[i] = blocks[i - 1];
    blocks[idx] = b;
    nblocks++;
}

static void remove_block(int idx)
{
    for (int i = idx; i < nblocks - 1; i++) blocks[i] = blocks[i + 1];
    nblocks--;
}

static void coalesce(void)
{
    for (int i = 0; i < nblocks - 1;) {
        if (blocks[i].pid == 0 && blocks[i + 1].pid == 0) {
            blocks[i].size += blocks[i + 1].size;
            remove_block(i + 1);
        } else {
            i++;
        }
    }
}

static int find_pid(int pid)
{
    for (int i = 0; i < nblocks; i++)
        if (blocks[i].pid == pid) return i;
    return -1;
}

static void cont_alloc(int pid, int size, FitAlgo algo)
{
    int idx = -1, total_free = 0;

    if (find_pid(pid) >= 0) {
        printf("  Process %d is already in memory.\n", pid);
        return;
    }
    if (nblocks >= MAX_BLOCKS - 1) {
        printf("  Block table full.\n");
        return;
    }

    for (int i = 0; i < nblocks; i++) {
        if (blocks[i].pid != 0) continue;
        total_free += blocks[i].size;
        if (blocks[i].size < size) continue;
        if (idx < 0) { idx = i; if (algo == FIRST_FIT) break; continue; }
        if (algo == BEST_FIT  && blocks[i].size < blocks[idx].size) idx = i;
        if (algo == WORST_FIT && blocks[i].size > blocks[idx].size) idx = i;
    }

    if (idx < 0) {
        if (algo == FIRST_FIT) {          /* loop broke early only on success */
            total_free = 0;
            for (int i = 0; i < nblocks; i++)
                if (blocks[i].pid == 0) total_free += blocks[i].size;
        }
        printf("  Allocation FAILED for P%d (%d KB) using %s.\n", pid, size, algo_name(algo));
        if (total_free >= size)
            printf("  -> %d KB free in total, but no single hole is big enough "
                   "(EXTERNAL FRAGMENTATION). Try compaction.\n", total_free);
        else
            printf("  -> Only %d KB free in total. Not enough memory.\n", total_free);
        return;
    }

    if (blocks[idx].size > size) {
        Block rest = { blocks[idx].start + size, blocks[idx].size - size, 0 };
        insert_block(idx + 1, rest);
    }
    blocks[idx].size = size;
    blocks[idx].pid = pid;
    printf("  P%d allocated %d KB at address %d (%s).\n", pid, size, blocks[idx].start, algo_name(algo));
}

static void cont_free(int pid)
{
    int idx = find_pid(pid);
    if (idx < 0) { printf("  Process %d not found.\n", pid); return; }
    blocks[idx].pid = 0;
    coalesce();
    printf("  P%d freed. Adjacent holes merged.\n", pid);
}

static void cont_compact(void)
{
    Block packed[MAX_BLOCKS];
    int n = 0, addr = 0, moved = 0;

    for (int i = 0; i < nblocks; i++) {
        if (blocks[i].pid == 0) continue;
        if (blocks[i].start != addr) moved++;
        packed[n].start = addr;
        packed[n].size = blocks[i].size;
        packed[n].pid = blocks[i].pid;
        addr += blocks[i].size;
        n++;
    }
    if (addr < mem_total) {
        packed[n].start = addr;
        packed[n].size = mem_total - addr;
        packed[n].pid = 0;
        n++;
    }
    memcpy(blocks, packed, sizeof(Block) * n);
    nblocks = n;
    printf("  Compaction done. %d process(es) relocated.\n", moved);
}

static void cont_display(void)
{
    int used = 0, freeb = 0, largest = 0, holes = 0;

    printf("\n  %-8s %-8s %-8s %s\n", "Start", "End", "Size", "Status");
    line('-', 44);
    for (int i = 0; i < nblocks; i++) {
        Block *b = &blocks[i];
        printf("  %-8d %-8d %-8d ", b->start, b->start + b->size - 1, b->size);
        if (b->pid) { printf("P%d\n", b->pid); used += b->size; }
        else {
            printf("FREE\n");
            freeb += b->size; holes++;
            if (b->size > largest) largest = b->size;
        }
    }
    line('-', 44);

    /* memory bar */
    const int W = 60;
    printf("  [");
    for (int i = 0; i < nblocks; i++) {
        int w = (int)((long)blocks[i].size * W / mem_total);
        if (w < 1) w = 1;
        char ch = blocks[i].pid ? (char)('0' + blocks[i].pid % 10) : '.';
        for (int k = 0; k < w; k++) putchar(ch);
    }
    printf("]\n  (digits = process id mod 10, '.' = free)\n\n");

    printf("  Total: %d KB | Used: %d KB (%.1f%%) | Free: %d KB in %d hole(s)\n",
           mem_total, used, 100.0 * used / mem_total, freeb, holes);
    if (freeb > 0)
        printf("  Largest hole: %d KB | External fragmentation: %.1f%%\n",
               largest, 100.0 * (freeb - largest) / freeb);
}

static void contiguous_menu(void)
{
    int total = read_int("Total memory size in KB (16-4096): ", 16, 4096);
    FitAlgo algo = FIRST_FIT;
    cont_init(total);

    for (;;) {
        printf("\n--- Contiguous Allocation [%s] ---\n", algo_name(algo));
        printf(" 1. Allocate process\n 2. Free process\n 3. Display memory\n"
               " 4. Compact memory\n 5. Change algorithm\n 6. Reset memory\n 0. Back\n");
        int c = read_int("Choice: ", 0, 6);
        if (c == 0) return;
        if (c == 1) {
            int pid = read_int("  Process id (1-99): ", 1, 99);
            int size = read_int("  Size in KB: ", 1, mem_total);
            cont_alloc(pid, size, algo);
        } else if (c == 2) {
            cont_free(read_int("  Process id to free: ", 1, 99));
        } else if (c == 3) {
            cont_display();
        } else if (c == 4) {
            cont_compact();
        } else if (c == 5) {
            printf("  1. First Fit  2. Best Fit  3. Worst Fit\n");
            algo = (FitAlgo)read_int("  Select: ", 1, 3);
        } else if (c == 6) {
            cont_init(mem_total);
            printf("  Memory reset.\n");
        }
    }
}

/* ================================================================== */
/*                            PART 2: PAGING                          */
/* ================================================================== */

#define MAX_FRAMES 256
#define MAX_PROCS  32
#define MAX_PAGES  128

typedef struct {
    int pid, size, npages, active;
    int page_table[MAX_PAGES];
} Proc;

static int pg_frames = 0, pg_size = 0;
static int frame_pid[MAX_FRAMES], frame_page[MAX_FRAMES];
static Proc procs[MAX_PROCS];

static void paging_init(int frames, int pagesize)
{
    pg_frames = frames;
    pg_size = pagesize;
    for (int i = 0; i < MAX_FRAMES; i++) { frame_pid[i] = 0; frame_page[i] = -1; }
    memset(procs, 0, sizeof procs);
}

static Proc *find_proc(int pid)
{
    for (int i = 0; i < MAX_PROCS; i++)
        if (procs[i].active && procs[i].pid == pid) return &procs[i];
    return NULL;
}

static void paging_create(int pid, int size)
{
    int need = (size + pg_size - 1) / pg_size, avail = 0;
    Proc *p = NULL;

    if (find_proc(pid)) { printf("  Process %d already exists.\n", pid); return; }
    if (need > MAX_PAGES) { printf("  Process too large for page table.\n"); return; }
    for (int i = 0; i < pg_frames; i++) if (!frame_pid[i]) avail++;
    if (need > avail) {
        printf("  FAILED: needs %d frame(s), only %d free.\n", need, avail);
        return;
    }
    for (int i = 0; i < MAX_PROCS; i++) if (!procs[i].active) { p = &procs[i]; break; }
    if (!p) { printf("  Process table full.\n"); return; }

    p->active = 1; p->pid = pid; p->size = size; p->npages = need;
    int pg = 0;
    for (int f = 0; f < pg_frames && pg < need; f++) {
        if (frame_pid[f]) continue;
        frame_pid[f] = pid; frame_page[f] = pg;
        p->page_table[pg++] = f;
    }
    int internal = need * pg_size - size;
    printf("  P%d created: %d bytes -> %d page(s). Internal fragmentation: %d bytes.\n",
           pid, size, need, internal);
}

static void paging_kill(int pid)
{
    Proc *p = find_proc(pid);
    if (!p) { printf("  Process %d not found.\n", pid); return; }
    for (int f = 0; f < pg_frames; f++)
        if (frame_pid[f] == pid) { frame_pid[f] = 0; frame_page[f] = -1; }
    p->active = 0;
    printf("  P%d terminated; its frames are free.\n", pid);
}

static void paging_translate(int pid, int laddr)
{
    Proc *p = find_proc(pid);
    if (!p) { printf("  Process %d not found.\n", pid); return; }
    if (laddr < 0 || laddr >= p->size) {
        printf("  SEGMENTATION FAULT: address %d outside process size %d.\n", laddr, p->size);
        return;
    }
    int page = laddr / pg_size, off = laddr % pg_size, frame = p->page_table[page];
    printf("  Logical address  : %d\n", laddr);
    printf("  Page number      : %d / %d = %d, offset = %d\n", laddr, pg_size, page, off);
    printf("  Page table[%d]    : frame %d\n", page, frame);
    printf("  Physical address : %d * %d + %d = %d\n", frame, pg_size, off, frame * pg_size + off);
}

static void paging_display(void)
{
    int used = 0;
    printf("\n  Frame table (page size = %d bytes)\n", pg_size);
    printf("  %-7s %-9s %s\n", "Frame", "Process", "Page");
    line('-', 28);
    for (int f = 0; f < pg_frames; f++) {
        if (frame_pid[f]) { printf("  %-7d P%-8d %d\n", f, frame_pid[f], frame_page[f]); used++; }
        else printf("  %-7d %-9s -\n", f, "free");
    }
    line('-', 28);
    printf("  Frames used: %d / %d (%.1f%%)\n", used, pg_frames, 100.0 * used / pg_frames);

    for (int i = 0; i < MAX_PROCS; i++) {
        if (!procs[i].active) continue;
        printf("\n  Page table of P%d (%d bytes):\n   page :", procs[i].pid, procs[i].size);
        for (int k = 0; k < procs[i].npages; k++) printf(" %3d", k);
        printf("\n   frame:");
        for (int k = 0; k < procs[i].npages; k++) printf(" %3d", procs[i].page_table[k]);
        printf("\n");
    }
}

static void paging_menu(void)
{
    int frames = read_int("Number of physical frames (2-256): ", 2, 256);
    int psize = read_int("Page size in bytes (e.g. 256): ", 1, 65536);
    paging_init(frames, psize);
    printf("  Physical memory = %d bytes.\n", frames * psize);

    for (;;) {
        printf("\n--- Paging ---\n 1. Create process\n 2. Terminate process\n"
               " 3. Translate logical address\n 4. Display frames & page tables\n 0. Back\n");
        int c = read_int("Choice: ", 0, 4);
        if (c == 0) return;
        if (c == 1) {
            int pid = read_int("  Process id (1-99): ", 1, 99);
            int size = read_int("  Size in bytes: ", 1, 10000000);
            paging_create(pid, size);
        } else if (c == 2) {
            paging_kill(read_int("  Process id: ", 1, 99));
        } else if (c == 3) {
            int pid = read_int("  Process id: ", 1, 99);
            int a = read_int("  Logical address: ", 0, 100000000);
            paging_translate(pid, a);
        } else if (c == 4) {
            paging_display();
        }
    }
}

/* ================================================================== */
/*                     PART 3: PAGE REPLACEMENT                       */
/* ================================================================== */

#define MAX_REF 128
#define MAX_RF  16

typedef enum { FIFO = 1, LRU, OPT } RepAlgo;

static int replace_sim(RepAlgo algo, const int *ref, int n, int nf, int verbose)
{
    int frame[MAX_RF], loaded[MAX_RF], last[MAX_RF];
    int faults = 0, used = 0;
    const char *names[] = { "", "FIFO", "LRU", "Optimal" };

    if (verbose) {
        printf("\n  === %s ===\n  Ref |", names[algo]);
        for (int f = 0; f < nf; f++) printf(" F%-2d", f);
        printf("| Result\n  ");
        line('-', 12 + 4 * nf);
    }

    for (int t = 0; t < n; t++) {
        int hit = -1, victim = -1;
        for (int f = 0; f < used; f++) if (frame[f] == ref[t]) { hit = f; break; }

        if (hit >= 0) {
            last[hit] = t;
        } else {
            faults++;
            if (used < nf) {
                victim = used++;
            } else if (algo == FIFO) {
                victim = 0;
                for (int f = 1; f < nf; f++) if (loaded[f] < loaded[victim]) victim = f;
            } else if (algo == LRU) {
                victim = 0;
                for (int f = 1; f < nf; f++) if (last[f] < last[victim]) victim = f;
            } else { /* OPT: evict page used farthest in the future */
                int far = -1;
                for (int f = 0; f < nf; f++) {
                    int next = n + 1;                 /* never used again */
                    for (int k = t + 1; k < n; k++) if (ref[k] == frame[f]) { next = k; break; }
                    if (next > far) { far = next; victim = f; }
                }
            }
            frame[victim] = ref[t];
            loaded[victim] = t;
            last[victim] = t;
        }

        if (verbose) {
            printf("  %3d |", ref[t]);
            for (int f = 0; f < nf; f++) {
                if (f < used) printf(" %-3d", frame[f]); else printf("  - ");
            }
            printf("| %s\n", hit >= 0 ? "hit" : "FAULT");
        }
    }
    if (verbose)
        printf("  Faults: %d / %d  (hit ratio %.1f%%)\n", faults, n, 100.0 * (n - faults) / n);
    return faults;
}

static void replacement_menu(void)
{
    int ref[MAX_REF], n, nf;

    n = read_int("Length of reference string (1-128): ", 1, MAX_REF);
    printf("Enter %d page numbers (0-99), separated by spaces:\n", n);
    for (int i = 0; i < n; i++) {
        char buf[32];
        long v;
        if (scanf("%31s", buf) != 1) exit(0);
        v = strtol(buf, NULL, 10);
        ref[i] = (v < 0) ? 0 : (int)v;
    }
    { int ch; while ((ch = getchar()) != '\n' && ch != EOF) {} }   /* flush line */
    nf = read_int("Number of frames (1-16): ", 1, MAX_RF);

    for (;;) {
        printf("\n--- Page Replacement ---\n 1. FIFO\n 2. LRU\n 3. Optimal\n"
               " 4. Compare all\n 0. Back\n");
        int c = read_int("Choice: ", 0, 4);
        if (c == 0) return;
        if (c <= 3) {
            replace_sim((RepAlgo)c, ref, n, nf, 1);
        } else {
            int f1 = replace_sim(FIFO, ref, n, nf, 0);
            int f2 = replace_sim(LRU,  ref, n, nf, 0);
            int f3 = replace_sim(OPT,  ref, n, nf, 0);
            printf("\n  %-10s %-8s %s\n", "Algorithm", "Faults", "Hit ratio");
            line('-', 32);
            printf("  %-10s %-8d %.1f%%\n", "FIFO",    f1, 100.0 * (n - f1) / n);
            printf("  %-10s %-8d %.1f%%\n", "LRU",     f2, 100.0 * (n - f2) / n);
            printf("  %-10s %-8d %.1f%%\n", "Optimal", f3, 100.0 * (n - f3) / n);
        }
    }
}

/* ================================================================== */
/*                               MAIN                                 */
/* ================================================================== */

int main(void)
{
    printf("=====================================\n");
    printf("     MEMORY MANAGEMENT SIMULATOR\n");
    printf("=====================================\n");
    for (;;) {
        printf("\n=== Main Menu ===\n 1. Contiguous allocation (First/Best/Worst Fit)\n"
               " 2. Paging & address translation\n 3. Page replacement (FIFO/LRU/Optimal)\n"
               " 0. Exit\n");
        int c = read_int("Choice: ", 0, 3);
        if (c == 0) { printf("Goodbye!\n"); return 0; }
        if (c == 1) contiguous_menu();
        else if (c == 2) paging_menu();
        else replacement_menu();
    }
}
