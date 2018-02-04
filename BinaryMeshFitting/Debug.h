#pragma once

#ifdef _DEBUG
#define DEBUG_TEXT (_NORMAL_BLOCK, __FILE, __LINE)
#else
#define DEBUG_TEXT
#endif