#ifndef MI_LOG_H
#define MI_LOG_H
#ifdef __cplusplus
extern "C" {
#endif

void CreateLogFile(const char *file, char* logpath);
void WriteLogFile(char *logbuff);
void CloseLogFile();
void WriteLog(const char *filename, const char *functionname, int linenumber, const char *level, const char *fmt, ...);

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)


#define LogT(fmt, ...) \
    WriteLog(__FILENAME__, __func__, __LINE__, "TRACE", fmt, ##__VA_ARGS__);
#define LogD(fmt, ...) \
    WriteLog(__FILENAME__, __func__, __LINE__, "DEBUG", fmt, ##__VA_ARGS__);
#define LogI(fmt, ...) \
    WriteLog(__FILENAME__, __func__, __LINE__, "INFO", fmt, ##__VA_ARGS__);
#define LogW(fmt, ...) \
    WriteLog(__FILENAME__, __func__, __LINE__, "WARN", fmt, ##__VA_ARGS__);
#define LogE(fmt, ...) \
    WriteLog(__FILENAME__, __func__, __LINE__, "ERROR", fmt, ##__VA_ARGS__);

#ifdef __cplusplus
}
#endif
#endif