#include "stdafx.h"
#include "ViewCalculations.h"

int view_calc::HistoryScroll(int current, int maximum, int rows, unsigned command, int track)
{
    int64_t index = current;
    switch (command)
    {
    case SB_LINEUP: ++index; break;
    case SB_LINEDOWN: --index; break;
    case SB_PAGEUP: index += std::max(1, rows); break;
    case SB_PAGEDOWN: index -= std::max(1, rows); break;
    case SB_TOP: index = maximum; break;
    case SB_BOTTOM: index = 0; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: index = int64_t{maximum} - track; break;
    }
    return HistoryClamp(index, maximum);
}
