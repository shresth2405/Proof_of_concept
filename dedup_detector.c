#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_PFNS 100
#define PAGE_SIZE 4096
#define PFN_MASK ((1ULL << 55) - 1)

uint64_t seen_pfns[MAX_PFNS];

int count = 0;

int pfn_exists(uint64_t pfn)
{
    for (int i = 0; i < count; i++)
    {
        if (seen_pfns[i] == pfn)
            return 1;
    }
    return 0;
}

void insert_pfn(uint64_t pfn)
{
    if (count < MAX_PFNS)
    {
        seen_pfns[count++] = pfn;
    }
}

uint64_t get_pfn(pid_t pid, unsigned long vaddr)
{
    char path[256];
    int fd;
    uint64_t entry;
    off_t offset;

    snprintf(path, sizeof(path),
             "/proc/%d/pagemap", pid);

    fd = open(path, O_RDONLY);

    if (fd < 0)
    {
        perror("open pagemap");
        exit(1);
    }

    offset = (vaddr / PAGE_SIZE) * sizeof(uint64_t);

    if (lseek(fd, offset, SEEK_SET) == (off_t)-1)
    {
        perror("lseek");
        close(fd);
        exit(1);
    }

    if (read(fd, &entry, sizeof(entry)) != sizeof(entry))
    {
        perror("read");
        close(fd);
        exit(1);
    }

    close(fd);

    if (!(entry & (1ULL << 63)))
    {
        // printf("Page not present\n");
        return 0;
    }

    return entry & PFN_MASK;
}

int read_dirty_info(pid_t pid,
                    unsigned long addr,
                    int *private_dirty,
                    int *shared_dirty)
{
    char path[256];
    char line[512];

    snprintf(path, sizeof(path),
             "/proc/%d/smaps", pid);

    FILE *fp = fopen(path, "r");

    if (!fp)
    {
        perror("smaps open");
        return 0;
    }

    unsigned long start, end;

    while (fgets(line, sizeof(line), fp))
    {

        if (sscanf(line,
                   "%lx-%lx",
                   &start,
                   &end) == 2)
        {
            if (addr >= start &&
                addr < end)
            {
                while (fgets(line,
                             sizeof(line),
                             fp))
                {
                    if (sscanf(line,
                               "Private_Dirty: %d",
                               private_dirty) == 1)
                        continue;

                    if (sscanf(line,
                               "Shared_Dirty: %d",
                               shared_dirty) == 1)
                        continue;

                    if (line[0] == '\n')
                        break;
                }

                fclose(fp);
                return 1;
            }
        }
    }

    fclose(fp);
    return 0;
}

// int is_anonymous_private(pid_t pid,
//                          unsigned long addr)
// {
//     char path[256];
//     char line[512];

//     snprintf(path,
//              sizeof(path),
//              "/proc/%d/maps",
//              pid);

//     FILE *fp = fopen(path, "r");

//     if (!fp)
//     {
//         perror("maps open");
//         return 0;
//     }

//     while (fgets(line,
//                  sizeof(line),
//                  fp))
//     {
//         unsigned long start, end;
//         char perms[8];
//         unsigned long inode;

//         if (sscanf(line,
//                    "%lx-%lx %4s %*s %*s %lu",
//                    &start,
//                    &end,
//                    perms,
//                    &inode) != 4)
//             continue;

//         if (addr >= start &&
//             addr < end)
//         {
//             int is_private =
//                 perms[3] == 'p';

//             int is_anonymous =
//                 inode == 0;

//             fclose(fp);

//             return is_private &&
//                    is_anonymous;
//         }
//     }

//     fclose(fp);
//     return 0;
// }

void scan_process(pid_t pid)
{
    char path[256];
    char line[512];

    unsigned long total_pages = 0;
    unsigned long MAX_TOTAL_PAGES = 64;

    snprintf(path,
             sizeof(path),
             "/proc/%d/maps",
             pid);

    FILE *fp = fopen(path, "r");

    if (!fp)
    {
        perror("maps open");
        return;
    }

    printf("\nScanning PID %d\n",
           pid);

    while (fgets(line,
                 sizeof(line),
                 fp))
    {
        unsigned long start;
        unsigned long end;
        char perms[8];

        unsigned long inode;

        if (sscanf(line,
                   "%lx-%lx %4s %*s %*s %lu",
                   &start,
                   &end,
                   perms,
                   &inode) != 4)
            continue;

        int is_private =
            perms[3] == 'p';

        int is_anonymous =
            inode == 0;

        if (!(is_private &&
              is_anonymous))
            continue;

        // unsigned long max_pages = 16;

        for (unsigned long addr = start;
             addr < end &&
             total_pages < MAX_TOTAL_PAGES;
             addr += PAGE_SIZE)
        {
            total_pages++;
            uint64_t pfn =
                get_pfn(pid, addr);

            if (pfn == 0)
                continue;

            int private_dirty = 0;
            int shared_dirty = 0;

            // read_dirty_info(pid,
            //                 addr,
            //                 &private_dirty,
            //                 &shared_dirty);

            printf(
                "PID %d Addr 0x%lx PFN %lu PD:%d SD:%d\n",
                pid,
                addr,
                pfn,
                private_dirty,
                shared_dirty);

            

            if (pfn_exists(pfn))
            {
                printf(
                    "PID %d: PFN %lu -> duplicate skipped\n",
                    pid,
                    pfn);
            }
            else
            {
                printf(
                    "PID %d: PFN %lu -> dumped\n",
                    pid,
                    pfn);

                insert_pfn(pfn);
            }
        }
    }
    
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf(
            "Usage:\n"
            "./dedup_detector <pid1> <pid2> ...\n");
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        pid_t pid =
            atoi(argv[i]);

        scan_process(pid);
    }

    printf("\nSummary:\n");

    printf(
        "Unique PFNs: %d\n",
        count);

    return 0;
}