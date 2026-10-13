#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "ast.h"
#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

typedef struct yy_buffer_state *YY_BUFFER_STATE;
int yyparse(void);
YY_BUFFER_STATE yy_scan_bytes(const char *bytes, int length);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
int yylex_destroy(void);

#ifdef _WIN32
static wchar_t *wide(const char *text) {
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    wchar_t *result;
    if (!count) return NULL;
    result = arena_alloc((size_t)count * sizeof(*result));
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, count);
    return result;
}
static FILE *open_file(const char *path, const char *mode) {
    wchar_t *p = wide(path), *m = wide(mode);
    return p && m ? _wfopen(p, m) : NULL;
}
static int same_file(const char *input, const char *output) {
    wchar_t *a = wide(input), *b = wide(output);
    wchar_t *full_a = _wfullpath(NULL, a, 0), *full_b = _wfullpath(NULL, b, 0);
    int same = full_a && full_b && !_wcsicmp(full_a, full_b);
    HANDLE ha, hb;
    BY_HANDLE_FILE_INFORMATION ia, ib;
    free(full_a); free(full_b);
    if (same) return 1;
    ha = CreateFileW(a, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    hb = CreateFileW(b, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (ha != INVALID_HANDLE_VALUE && hb != INVALID_HANDLE_VALUE && GetFileInformationByHandle(ha, &ia) && GetFileInformationByHandle(hb, &ib))
        same = ia.dwVolumeSerialNumber == ib.dwVolumeSerialNumber && ia.nFileIndexHigh == ib.nFileIndexHigh && ia.nFileIndexLow == ib.nFileIndexLow;
    if (ha != INVALID_HANDLE_VALUE) CloseHandle(ha);
    if (hb != INVALID_HANDLE_VALUE) CloseHandle(hb);
    return same;
}
#else
static FILE *open_file(const char *path, const char *mode) { return fopen(path, mode); }
static int same_file(const char *input, const char *output) {
    struct stat a, b;
    return !stat(input, &a) && !stat(output, &b) && a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}
#endif

static void usage(FILE *out) {
    fputs("Matrix Translator " TRANSLATOR_VERSION " — транслятор матричного языка в Python\n"
          "Использование: translator input.mat -o output.py\n"
          "              translator --help | --version\n"
          "После трансляции: python output.py (требуется NumPy).\n", out);
}
static FILE *temporary(const char *output, char **name) {
    size_t size = strlen(output) + 80;
    int fd;
    *name = arena_alloc(size);
#ifdef _WIN32
    unsigned attempt;
    for (attempt = 0; attempt < 100; ++attempt) {
        snprintf(*name, size, "%s.tmp.%lu.%u", output, (unsigned long)_getpid(), attempt);
        fd = _wopen(wide(*name), _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, _S_IREAD | _S_IWRITE);
        if (fd >= 0) {
            FILE *out = _fdopen(fd, "wb");
            if (!out) { _close(fd); _wremove(wide(*name)); }
            return out;
        }
        if (errno != EEXIST) break;
    }
    return NULL;
#else
    snprintf(*name, size, "%s.tmp.XXXXXX", output);
    fd = mkstemp(*name);
    if (fd < 0) return NULL;
    FILE *out = fdopen(fd, "wb");
    if (!out) { close(fd); remove(*name); }
    return out;
#endif
}
static void discard(const char *name) {
#ifdef _WIN32
    _wremove(wide(name));
#else
    remove(name);
#endif
}
static int commit(const char *temporary_name, const char *output) {
#ifdef _WIN32
    return MoveFileExW(wide(temporary_name), wide(output), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary_name, output) == 0;
#endif
}
static int run(int argc, char **argv) {
    const size_t max_input = 16 * 1024 * 1024;
    FILE *input, *out;
    char *bytes, *temp_name = NULL;
    size_t count;
    int parse_result, ok;
    YY_BUFFER_STATE buffer;
    if (argc == 2 && !strcmp(argv[1], "--help")) { usage(stdout); return 0; }
    if (argc == 2 && !strcmp(argv[1], "--version")) { puts("Matrix Translator " TRANSLATOR_VERSION); return 0; }
    if (argc != 4 || !strcmp(argv[1], "-o") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "--version") || strcmp(argv[2], "-o") || !*argv[1] || !*argv[3]) {
        usage(stderr); return 2;
    }
    source_name = argv[1];
    if (same_file(argv[1], argv[3])) { fputs("Ошибка: входной и выходной файлы должны быть разными.\n", stderr); return 3; }
    input = open_file(argv[1], "rb");
    if (!input) { fprintf(stderr, "Ошибка: не удалось открыть входной файл '%s': %s\n", argv[1], strerror(errno)); return 3; }
    bytes = arena_alloc(max_input + 1);
    count = fread(bytes, 1, max_input + 1, input);
    ok = !ferror(input);
    if (fclose(input)) ok = 0;
    if (!ok) { fprintf(stderr, "Ошибка: не удалось прочитать входной файл '%s'.\n", argv[1]); return 3; }
    if (count > max_input) { fputs("Ошибка: входной файл превышает 16 МиБ.\n", stderr); return 3; }
    buffer = yy_scan_bytes(bytes, (int)count);
    parse_result = yyparse();
    yy_delete_buffer(buffer); yylex_destroy();
    if (parse_result || had_error || !analyze(program)) return 1;
    /* No output file is opened until both parsing and semantic checks succeed. */
    out = temporary(argv[3], &temp_name);
    if (!out) { fprintf(stderr, "Ошибка: не удалось создать выходной файл '%s': %s\n", argv[3], strerror(errno)); return 3; }
    ok = generate(out, program);
    if (fclose(out)) ok = 0;
    if (!ok || !commit(temp_name, argv[3])) {
        fprintf(stderr, "Ошибка: не удалось записать выходной файл '%s'.\n", argv[3]);
        discard(temp_name); return 3;
    }
    printf("Трансляция завершена: %s\n", argv[3]); return 0;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **wide_argv) {
    char **argv = arena_alloc((size_t)argc * sizeof(*argv));
    int i, code;
    SetConsoleOutputCP(CP_UTF8);
    setlocale(LC_NUMERIC, "C");
    for (i = 0; i < argc; ++i) {
        int count = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = arena_alloc((size_t)count);
        WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, argv[i], count, NULL, NULL);
    }
    code = run(argc, argv); arena_free(); return code;
}
#else
int main(int argc, char **argv) {
    int code;
    setlocale(LC_NUMERIC, "C");
    code = run(argc, argv); arena_free(); return code;
}
#endif
