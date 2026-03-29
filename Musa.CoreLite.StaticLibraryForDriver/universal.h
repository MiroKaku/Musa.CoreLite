#pragma once
// 6101: SAL "return value not set" — suppress false positives for NT API out-param patterns
// 28101: function class annotation mismatch — driver routines use non-standard calling conventions
// 28167: IRQL annotation mismatch — suppress for mixed-IRQL driver helper functions
#pragma warning(disable: 6101 28101 28167)

// Config Macro
#define POOL_NX_OPTIN 1
#define POOL_ZERO_DOWN_LEVEL_SUPPORT 1
#define RTL_USE_AVL_TABLES

// 4117: macro redefinition of _KERNEL_MODE — required for non-WDK builds to enable kernel-mode code paths
#pragma warning(suppress: 4117)
#define _KERNEL_MODE 1

// System Header
#include <Veil.h>

// C/C++  Header
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Local  Header
#include "Musa.CoreLite/Musa.CoreLite.h"

// Global Variable

// Global Macro
#define MUSA_NAME_PUBLIC(name)  _VEIL_CONCATENATE(_Musa_, name)
#define MUSA_NAME_PRIVATE(name) _VEIL_CONCATENATE(_Musa_Private_, name)
#define MUSA_NAME MUSA_NAME_PUBLIC

#if defined _M_IX86
#define MUSA_IAT_SYMBOL(name, stack) _VEIL_DEFINE_IAT_RAW_SYMBOL(name ## @ ## stack, MUSA_NAME(name))
#else
#define MUSA_IAT_SYMBOL(name, stack) _VEIL_DEFINE_IAT_SYMBOL(name, MUSA_NAME(name))
#endif

#define MUSA_SWAP(a, b) \
    do { \
        auto _tmp = (a); \
        (a) = (b); \
        (b) = _tmp; \
    } while (0)

// Logging
#ifdef _DEBUG
#define MusaLOG(fmt, ...) DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, \
    "[Musa.CoreLite][%s():%u]" fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define MusaLOG(...)
#endif
