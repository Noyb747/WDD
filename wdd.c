#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define DEFAULT_BS 512

typedef enum {
    STATUS_DEFAULT,
    STATUS_NONE,
    STATUS_PROGRESS
} status_mode;

typedef struct {
    const char *ifile;
    const char *ofile;
    uint64_t ibs, obs;
    uint64_t count, skip, seek;
    int sync, noerror, notrunc;
    int if_direct, of_direct;
    int sparse;
    status_mode status;
} dd_opts;

static volatile LONG interrupted = 0;

BOOL WINAPI ctrl_handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        interrupted = 1;
        return TRUE;
    }
    return FALSE;
}

void die(const char *msg) {
    fprintf(stderr, "wdd: %s (err=%lu)\n", msg, GetLastError());
    exit(1);
}

uint64_t parse_size(const char *s) {
    char *end;
    uint64_t v = strtoull(s, &end, 10);
    switch (*end) {
        case 'k': case 'K': v *= 1024ULL; break;
        case 'm': case 'M': v *= 1024ULL * 1024ULL; break;
        case 'g': case 'G': v *= 1024ULL * 1024ULL * 1024ULL; break;
    }
    return v;
}

void parse_args(int argc, char **argv, dd_opts *o) {
    memset(o, 0, sizeof(*o));
    o->ibs = o->obs = DEFAULT_BS;
    o->status = STATUS_DEFAULT;

    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "if=", 3)) o->ifile = argv[i] + 3;
        else if (!strncmp(argv[i], "of=", 3)) o->ofile = argv[i] + 3;
        else if (!strncmp(argv[i], "ibs=", 4)) o->ibs = parse_size(argv[i] + 4);
        else if (!strncmp(argv[i], "obs=", 4)) o->obs = parse_size(argv[i] + 4);
        else if (!strncmp(argv[i], "bs=", 3))
            o->ibs = o->obs = parse_size(argv[i] + 3);
        else if (!strncmp(argv[i], "count=", 6)) o->count = parse_size(argv[i] + 6);
        else if (!strncmp(argv[i], "skip=", 5)) o->skip = parse_size(argv[i] + 5);
        else if (!strncmp(argv[i], "seek=", 5)) o->seek = parse_size(argv[i] + 5);
        else if (!strncmp(argv[i], "conv=", 5)) {
            char *p = argv[i] + 5;
            if (strstr(p, "sync")) o->sync = 1;
            if (strstr(p, "noerror")) o->noerror = 1;
            if (strstr(p, "notrunc")) o->notrunc = 1;
            if (strstr(p, "sparse")) o->sparse = 1;
        }
        else if (!strncmp(argv[i], "iflag=", 6)) {
            if (strstr(argv[i] + 6, "direct")) o->if_direct = 1;
        }
        else if (!strncmp(argv[i], "oflag=", 6)) {
            if (strstr(argv[i] + 6, "direct")) o->of_direct = 1;
        }
        else if (!strncmp(argv[i], "status=", 7)) {
            const char *s = argv[i] + 7;
            if (!strcmp(s, "none")) o->status = STATUS_NONE;
            else if (!strcmp(s, "progress")) o->status = STATUS_PROGRESS;
        }
        else if (!strcmp(argv[i], "--help")) {
            printf("Usage: wdd [OPTIONS]\n"
                   "Options:\n"
                   "  if=FILE       Input file (default: -)\n"
                   "  of=FILE       Output file (default: -)\n"
                   "  bs=SIZE       Block size (default: 512)\n"
                   "  count=N       Copy only N blocks\n"
                   "  skip=N        Skip N blocks from input\n"
                   "  seek=N        Skip N blocks on output\n"
                   "  conv=CONVS    Comma-separated conversions (sync,noerror,notrunc,sparse)\n"
                   "  iflag=FLAGS   Input flags (direct)\n"
                   "  oflag=FLAGS   Output flags (direct)\n"
                   "  status=MODE   none, progress\n"
                   "  --help        Show this message\n"
                   "  --version     Show version\n");
            exit(0);
        }
        else if (!strcmp(argv[i], "--version")) {
            printf("wdd version 1.0\n");
            exit(0);
        }
    }

    if (!o->ifile) o->ifile = "-";
    if (!o->ofile) o->ofile = "-";
}

HANDLE open_file(const char *path, DWORD access, DWORD create, int direct) {
    if (!strcmp(path, "-"))
        return (access & GENERIC_READ) ? GetStdHandle(STD_INPUT_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
    if (direct)
        flags |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;

    HANDLE h = CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, create, flags, NULL);
    if (h == INVALID_HANDLE_VALUE)
        die("CreateFile");

    return h;
}

DWORD get_sector_size(HANDLE h) {
    FILE_STORAGE_INFO info;
    if (!GetFileInformationByHandleEx(h, FileStorageInfo, &info, sizeof(info)))
        return DEFAULT_BS;
    return info.LogicalBytesPerSector;
}

void set_sparse(HANDLE h) {
    DWORD tmp;
    DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &tmp, NULL);
}

void seek_blocks(HANDLE h, uint64_t blocks, uint64_t bs) {
    LARGE_INTEGER off;
    off.QuadPart = blocks * bs;
    if (!SetFilePointerEx(h, off, NULL, FILE_BEGIN))
        die("seek");
}

int is_all_zero(const void *buf, size_t n) {
    const uint8_t *p = buf;
    for (size_t i = 0; i < n; i++)
        if (p[i]) return 0;
    return 1;
}

int wmain(int argc, char **argv) {
    dd_opts o;
    parse_args(argc, argv, &o);
    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    HANDLE hin = open_file(o.ifile, GENERIC_READ, OPEN_EXISTING, o.if_direct);
    HANDLE hout = open_file(o.ofile, GENERIC_WRITE, o.notrunc ? OPEN_ALWAYS : CREATE_ALWAYS, o.of_direct);

    DWORD sec_in = get_sector_size(hin);
    DWORD sec_out = get_sector_size(hout);

    if (o.if_direct && (o.ibs % sec_in))
        die("ibs not aligned to input sector size");
    if (o.of_direct && (o.obs % sec_out))
        die("obs not aligned to output sector size");

    if (o.skip) seek_blocks(hin, o.skip, o.ibs);
    if (o.seek) seek_blocks(hout, o.seek, o.obs);

    if (o.sparse) set_sparse(hout);

    SIZE_T bufsize = (SIZE_T)o.ibs;
    void *buf = VirtualAlloc(NULL, bufsize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf) die("VirtualAlloc");

    uint64_t blocks = 0, bytes = 0;
    DWORD rd, wr;

    while (!interrupted) {
        if (o.count && blocks >= o.count) break;

        if (!ReadFile(hin, buf, (DWORD)o.ibs, &rd, NULL)) {
            if (o.noerror) continue;
            die("read");
        }
        if (rd == 0) break;

        if (o.sync && rd < o.ibs)
            memset((uint8_t *)buf + rd, 0, (size_t)(o.ibs - rd));

        DWORD outsz = o.sync ? (DWORD)o.ibs : rd;

        if (o.sparse && is_all_zero(buf, outsz)) {
            LARGE_INTEGER z;
            z.QuadPart = outsz;
            SetFilePointerEx(hout, z, NULL, FILE_CURRENT);
        } else {
            if (!WriteFile(hout, buf, outsz, &wr, NULL))
                die("write");
        }

        blocks++;
        bytes += outsz;

        // === STATUS_PROGRESS: print every 100 blocks ===
        if (o.status == STATUS_PROGRESS && (blocks % 100 == 0)) {
            fprintf(stderr, "\r%llu bytes copied", bytes);
            fflush(stderr);
        }
    }

    // Final flush and newline for progress
    FlushFileBuffers(hout);
    if (o.status == STATUS_PROGRESS)
        fprintf(stderr, "\n");

    if (o.status != STATUS_NONE) {
        fprintf(stderr,
            "%llu+0 records in\n"
            "%llu+0 records out\n"
            "%llu bytes copied\n",
            blocks, blocks, bytes);
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hin);
    CloseHandle(hout);
    return 0;
}
