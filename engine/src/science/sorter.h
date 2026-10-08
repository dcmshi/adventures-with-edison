// WMAIN.EXE segment 39's sorter (DS:1272): the quicksort the lists use (the
// high scores, a box's children).
#pragma once

namespace edison {

// f39_1183, through the list's compare (0 less, 1 the same, 2 more) and
// swap (f39_1940): none if all are the same; the pivot the first, unless
// the first different one after it is less: then that one. f39_129d
// partitions: the pivot to the end, the more ones to the front.
template <typename Compare, typename Swap>
void sorterSort(int lo, int hi, const Compare& cmp, const Swap& swapAt) {
    if (lo >= hi) return;
    int k = lo;
    while (k <= hi) {
        const int r = cmp(k, lo);
        if (r != 1 && r != 2) break;
        if (r == 2) {
            k = lo;
            break;
        }
        ++k;
    }
    if (k > hi) return;
    int i = lo, j = hi;
    swapAt(k, hi);
    for (;;) {
        bool done = false;
        while (cmp(i, hi) == 2)
            if (++i >= j) {
                done = true;
                break;
            }
        if (done) break;
        while (cmp(j, hi) != 2)
            if (--j == i) {
                done = true;
                break;
            }
        if (done) break;
        swapAt(i, j);
        if (++i >= j) break;
    }
    sorterSort(lo, i - 1, cmp, swapAt);
    sorterSort(i, hi, cmp, swapAt);
}

}  // namespace edison
