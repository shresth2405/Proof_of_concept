# Proof of Concept: PFN-Based Shared Page Detection

A proof-of-concept demonstrating how to detect shared (copy-on-write) pages between Linux processes using Page Frame Numbers (PFNs) from `/proc/[pid]/pagemap`.

## Objective

Demonstrate that after a `fork()`, parent and child processes initially share the same physical pages (CoW), and that this sharing can be detected programmatically by reading PFNs and identifying duplicates.

## How It Works

1. **`fork.c`** — Creates a parent process that allocates a page of memory, then forks a child. Both processes sleep to keep the shared CoW mapping alive, allowing the detector to observe them.

2. **`dedup_detector.c`** — Takes one or more PIDs as arguments. For each PID, it walks `/proc/[pid]/maps` to find anonymous private mappings, reads the PFN for each page via `/proc/[pid]/pagemap`, and flags any PFN that has already been seen (i.e., shared with another process).

## Files

| File | Description |
|------|-------------|
| `fork.c` | Test process: allocates memory and forks a child |
| `dedup_detector.c` | Scanner: reads PFNs and detects duplicate (shared) pages |
| `Makefile` | Builds both binaries |

## Build

```bash
make
```

This produces two binaries: `fork` and `dedup`.

## Usage

### 1. Start the test process

```bash
./fork &
```

Note the printed **Parent PID** and **Child PID**.

### 2. Run the detector

```bash
sudo ./dedup_detector <parent_pid> <child_pid>
```

> **Note:** Root (or `CAP_SYS_PTRACE`) is required to read `/proc/[pid]/pagemap` for another process.

### 3. Expected output

Pages that map to the same physical frame will be reported as duplicates:

```
Scanning PID 12345
PID 12345: PFN 123456 -> dumped
...
Scanning PID 12346
PID 12346: PFN 123456 -> duplicate skipped
...
Summary:
Unique PFNs: N
```

## Result

Shared PFNs between parent and child are detected successfully, confirming that copy-on-write pages can be identified via `/proc/pagemap` before any write triggers a CoW split.