#pragma once

#include <cstdio>
#include <cstdlib>

#ifdef __GNUG__
# define FUNCTION_NAME __PRETTY_FUNCTION__
#else
# define FUNCTION_NAME __func__
#endif

#define PANIC(...) ( \
        std::fprintf(stderr, "%s:%i: in %s: ", __FILE__, __LINE__, FUNCTION_NAME), \
        std::fprintf(stderr, __VA_ARGS__), \
        std::abort() \
        )
