#pragma once
#include <stdio.h>
#include <stdarg.h>

class ErrLog
{
 public:
   // 2 & 3 bcs this is method - see https://stackoverflow.com/questions/37194657/printf-wrapper-arguments-to-be-checked-by-gcc
   virtual void error(const char *fmt, ...) __attribute__ ((format (printf, 2, 3))) = 0;
   virtual void warning(const char *fmt, ...) __attribute__ ((format (printf, 2, 3))) = 0;
};

class FLog: public ErrLog
{
  public:
   FLog(FILE *fp): m_fp(fp) {};
   ~FLog() {
     if ( m_fp != stderr && NULL != m_fp ) fclose(m_fp);
   }
   virtual void error(const char *fmt, ...)
   {
     va_list argp;
     va_start(argp, fmt);
     fprintf(m_fp, "Error: ");
     vfprintf(m_fp, fmt, argp);
     va_end(argp);
   }
   virtual void warning(const char *fmt, ...)
   {
     va_list argp;
     va_start(argp, fmt);
     fprintf(m_fp, "Warning: ");
     vfprintf(m_fp, fmt, argp);
     va_end(argp);
   }
  protected:
   FILE *m_fp;
};