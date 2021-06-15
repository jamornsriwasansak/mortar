#pragma once

// quite dangerous but have been working so far...

#define MAKE_NONCOPYABLE(class_name)         \
    /* copy */                               \
    class_name(const class_name &) = delete; \
    class_name & operator=(const class_name &) = delete;

#define MAKE_NONMOVABLE(class_name)                      \
    /* copy */                                           \
    class_name(const class_name &) = delete;             \
    class_name & operator=(const class_name &) = delete; \
    /* move */                                           \
    class_name(class_name &&) = delete;                  \
    class_name & operator=(class_name &&) = delete
