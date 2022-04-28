#include "milog.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

#define StartDebugMode 1
static FILE *gLogHandle = NULL;

void CreateLogFile(const char *file, char *logpath)
{
    char settingdir[512];
    struct tm *timenow;
    time_t now;
    time(&now);
    timenow = localtime(&now);
    sprintf(settingdir, "%s/%04d%02d%02d%02d%02d%02d-%s.log", logpath, timenow->tm_year + 1900,
            timenow->tm_mon + 1, timenow->tm_mday, timenow->tm_hour, timenow->tm_min, timenow->tm_sec, file);
    struct stat statbuff;
    if (stat(settingdir, &statbuff) < 0)
    {
        gLogHandle = fopen(settingdir, "w");
        if (gLogHandle == NULL)
        {
            perror("open log file failed");
        }
        return;
    }
    if (statbuff.st_size >= 20 * 1024 * 1024)
    {
        gLogHandle = fopen(settingdir, "w");
        if (gLogHandle == NULL)
        {
            perror("open log file failed");
        }
        return;
    }
    gLogHandle = fopen(settingdir, "a+");
    if (gLogHandle == NULL)
    {
        perror("open log file failed");
    }
}

void WriteLogFile(char *logbuff)
{
    if (gLogHandle != NULL)
    {
        fwrite(logbuff, 1, strlen(logbuff), gLogHandle);
        fflush(gLogHandle);
    }
}

void WriteLog(const char *filename, const char *functionname, int linenumber, const char *level, const char *fmt, ...)
{
    char buffer[1024] = {0};
    time_t now;
    time(&now);
    char *curtime = ctime(&now);
    curtime[strlen(curtime) - 1] = '\0';
    va_list args;
    va_start(args, fmt);
    sprintf(buffer, "%s %s %s:%d] ", curtime, level, filename, linenumber);
    vsprintf(buffer + strlen(buffer), fmt, args);
    va_end(args);
    // buffer[strlen(buffer)] = '\n';
    WriteLogFile(buffer);
#ifdef StartDebugMode
    fprintf(stderr, "%s", buffer);
#endif
}

void CloseLogFile()
{
    if (gLogHandle != NULL)
    {
        fclose(gLogHandle);
    }
}
