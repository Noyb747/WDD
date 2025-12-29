#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdint.h>
#include <wchar.h>

#define DEFAULT_BS 512
#define WDD_VERSION L"1.0"

typedef enum {
    STATUS_DEFAULT,
    STATUS_NONE,
    STATUS_PROGRESS
} status_mode;

typedef struct {
    const wchar_t *ifile;
    const wchar_t *ofile;
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

void die(const wchar_t *msg) {
    fwprintf(stderr, L"wdd: %s (err=%lu)\n", msg, GetLastError());
    exit(1);
}

uint64_t parse_size(const wchar_t *s) {
    wchar_t *end;
    uint64_t v = wcstoull(s, &end, 10);
    switch (*end) {
        case L'k': case L'K': v *= 1024ULL; break;
        case L'm': case L'M': v *= 1024ULL*1024ULL; break;
        case L'g': case L'G': v *= 1024ULL*1024ULL*1024ULL; break;
    }
    return v;
}

void print_help(void) {
    wprintf(L"Usage: wdd [OPTIONS]\n");
    wprintf(L"Options:\n");
    wprintf(L"  if=FILE       Input file (default: -)\n");
    wprintf(L"  of=FILE       Output file (default: -)\n");
    wprintf(L"  bs=SIZE       Block size (default: 512)\n");
    wprintf(L"  count=N       Copy only N blocks\n");
    wprintf(L"  skip=N        Skip N blocks from input\n");
    wprintf(L"  seek=N        Skip N blocks on output\n");
    wprintf(L"  conv=CONVS    Comma-separated conversions (sync,noerror,notrunc,sparse)\n");
    wprintf(L"  iflag=FLAGS   Input flags (direct)\n");
    wprintf(L"  oflag=FLAGS   Output flags (direct)\n");
    wprintf(L"  status=MODE   none, progress\n");
    wprintf(L"  --help        Show this message\n");
    wprintf(L"  --version     Show version\n");
}

void print_version(void) {
    wprintf(L"wdd version %ls\n", WDD_VERSION);
}

void parse_args(int argc, wchar_t **argv, dd_opts *o) {
    memset(o, 0, sizeof(*o));
    o->ibs = o->obs = DEFAULT_BS;
    o->status = STATUS_DEFAULT;

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"--help") == 0) {
            print_help();
            exit(0);
        }
        if (wcscmp(argv[i], L"--version") == 0) {
            print_version();
            exit(0);
        }
        if (!wcsncmp(argv[i], L"if=", 3)) o->ifile = argv[i]+3;
        else if (!wcsncmp(argv[i], L"of=", 3)) o->ofile = argv[i]+3;
        else if (!wcsncmp(argv[i], L"ibs=", 4)) o->ibs = parse_size(argv[i]+4);
        else if (!wcsncmp(argv[i], L"obs=", 4)) o->obs = parse_size(argv[i]+4);
        else if (!wcsncmp(argv[i], L"bs=", 3)) o->ibs = o->obs = parse_size(argv[i]+3);
        else if (!wcsncmp(argv[i], L"count=", 6)) o->count = parse_size(argv[i]+6);
        else if (!wcsncmp(argv[i], L"skip=", 5)) o->skip = parse_size(argv[i]+5);
        else if (!wcsncmp(argv[i], L"seek=", 5)) o->seek = parse_size(argv[i]+5);
        else if (!wcsncmp(argv[i], L"conv=", 5)) {
            wchar_t *p = argv[i]+5;
            if (wcsstr(p, L"sync")) o->sync = 1;
            if (wcsstr(p, L"noerror")) o->noerror = 1;
            if (wcsstr(p, L"notrunc")) o->notrunc = 1;
            if (wcsstr(p, L"sparse")) o->sparse = 1;
        }
        else if (!wcsncmp(argv[i], L"iflag=", 6)) {
            if (wcsstr(argv[i]+6, L"direct")) o->if_direct = 1;
        }
        else if (!wcsncmp(argv[i], L"oflag=", 6)) {
            if (wcsstr(argv[i]+6, L"direct")) o->of_direct = 1;
        }
        else if (!wcsncmp(argv[i], L"status=", 7)) {
            const wchar_t *s = argv[i]+7;
            if (!wcscmp(s,L"none")) o->status = STATUS_NONE;
            else if (!wcscmp(s,L"progress")) o->status = STATUS_PROGRESS;
        }
    }

    if (!o->ifile) o->ifile = L"-";
    if (!o->ofile) o->ofile = L"-";
}

HANDLE open_file(const wchar_t *path, DWORD access, DWORD create, int direct) {
    if (!wcscmp(path,L"-"))
        return (access & GENERIC_READ) ? GetStdHandle(STD_INPUT_HANDLE)
                                       : GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
    if (direct) flags |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;

    HANDLE h = CreateFileW(path, access, FILE_SHARE_READ|FILE_SHARE_WRITE,
                           NULL, create, flags, NULL);
    if (h == INVALID_HANDLE_VALUE) die(L"CreateFile");
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
    if (!DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &tmp, NULL))
        die(L"set_sparse");
}

void seek_blocks(HANDLE h, uint64_t blocks, uint64_t bs) {
    LARGE_INTEGER off; off.QuadPart = blocks * bs;
    if (!SetFilePointerEx(h, off, NULL, FILE_BEGIN)) die(L"seek");
}

int is_all_zero(const void *buf, size_t n) {
    const uint8_t *p = buf;
    for (size_t i=0;i<n;i++) if (p[i]) return 0;
    return 1;
}

int wmain(int argc, wchar_t **argv) {
    dd_opts o;
    parse_args(argc, argv, &o);
    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    HANDLE hin = open_file(o.ifile, GENERIC_READ, OPEN_EXISTING, o.if_direct);
    HANDLE hout = open_file(o.ofile, GENERIC_WRITE,
                            o.notrunc ? OPEN_ALWAYS : CREATE_ALWAYS, o.of_direct);

    DWORD sec_in = get_sector_size(hin);
    DWORD sec_out = get_sector_size(hout);
    if (o.if_direct && (o.ibs % sec_in)) die(L"ibs not aligned to input sector size");
    if (o.of_direct && (o.obs % sec_out)) die(L"obs not aligned to output sector size");

    if (o.skip) seek_blocks(hin, o.skip, o.ibs);
    if (o.seek) seek_blocks(hout, o.seek, o.obs);

    if (o.sparse) set_sparse(hout);

    SIZE_T bufsize = (SIZE_T)o.ibs;
    void *buf = VirtualAlloc(NULL, bufsize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    if (!buf) die(L"VirtualAlloc");

    uint64_t blocks = 0, bytes = 0;
    DWORD rd, wr;

    while (!interrupted) {
        if (o.count && blocks >= o.count) break;

        if (!ReadFile(hin, buf, (DWORD)o.ibs, &rd, NULL)) {
            if (o.noerror) continue;
            die(L"read");
        }
        if (rd==0) break;

        if (o.sync && rd < o.ibs)
            memset((uint8_t*)buf+rd, 0, (size_t)(o.ibs - rd));

        DWORD outsz = o.sync ? (DWORD)o.ibs : rd;

        if (o.sparse && is_all_zero(buf, outsz)) {
            LARGE_INTEGER z; z.QuadPart = outsz;
            SetFilePointerEx(hout, z, NULL, FILE_CURRENT);
        } else {
            if (!WriteFile(hout, buf, outsz, &wr, NULL)) die(L"write");
        }

        blocks++;
        bytes += outsz;

        if (o.status == STATUS_PROGRESS && (blocks % 1000 == 0)) {
            fwprintf(stderr, L"\r%llu bytes copied", bytes);
        }
    }

    FlushFileBuffers(hout);

    if (o.status != STATUS_NONE) {
        fwprintf(stderr,
            L"\n%llu+0 records in\n"
            L"%llu+0 records out\n"
            L"%llu bytes copied\n",
            blocks, blocks, bytes);
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    CloseHandle(hin);
    CloseHandle(hout);
    return 0;
}
