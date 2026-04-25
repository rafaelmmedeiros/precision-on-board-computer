#pragma once

#include <Arduino.h>

struct ResetOption {
    const char* label;
    void      (*fn)();
};

struct ResetSet {
    uint8_t      count;
    ResetOption  opts[2];
};
