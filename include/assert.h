#ifndef ASSERT_H
#define ASSERT_H

#ifndef NULL
#define NULL ((void *)0)
#endif

void __assert_failed(const char *file, int line, const char *expr);
void __warn_triggered(const char *file, int line, const char *msg);

#ifdef NDEBUG
#define assert(expr) ((void)0)
#else
#define assert(expr) \
    ((expr) ? (void)0 : __assert_failed(__FILE__, __LINE__, #expr))
#endif

#define WARN(expr, msg) \
    ((expr) ? (void)0 : __warn_triggered(__FILE__, __LINE__, msg))

#endif