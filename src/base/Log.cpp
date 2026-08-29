#include <cstdio>
#include <cstdarg>

static FILE* g_log = nullptr;

void LogOpen(const char* apFileName) {
	if (!g_log)
		g_log = fopen(apFileName, "w");
}

void Log(const char* fmt, ...) {
	if (!g_log)
		return;
	va_list args;
	va_start(args, fmt);
	vfprintf(g_log, fmt, args);
	va_end(args);
	fputc('\n', g_log);
	fflush(g_log);
}
