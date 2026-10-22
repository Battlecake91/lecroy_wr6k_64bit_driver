#pragma once

#include <windows.h>

#ifndef _In_
#define _In_
#define _Inout_
#define _Out_
#endif

#if defined(LECS65_DMA_COMPLETION_INTERLEAVING_TEST)
LONG LecTestCompareExchange(volatile LONG* Value, LONG Exchange, LONG Compare);
#undef InterlockedCompareExchange
#define InterlockedCompareExchange LecTestCompareExchange
#endif
