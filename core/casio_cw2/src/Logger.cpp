#include "Logger.hpp"

#include <cstdarg>
#include <cstdio>

namespace casioemux
{
    namespace logger
    {
        void Info(const char *format, ...)
        {
#ifdef CASIOEMUX_ENABLE_LOG
            va_list args;
            va_start(args, format);
            std::vfprintf(stderr, format, args);
            va_end(args);
#else
            (void)format;
#endif
        }
    }
}
