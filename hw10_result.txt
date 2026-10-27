Data Count : 10000
Key Range  : 0 ~ 1000000
Seed       : 1793076148

Trial 1 Input Sample (Key 1, Key 2)
Data 1 : (547281, 80956)
Data 2 : (731707, 777119)
Data 3 : (682585, 6371)
Data 4 : (795649, 313186)
Data 5 : (686435, 786477)
...
Trial 1 Sorted Result Sample (Insertion Sort)
  First 5 : (74, 794759) (85, 3546) (177, 87657) (233, 487718) (658, 847620)
  Last 5  : (999643, 219249) (999741, 712138) (999785, 593188) (999870, 997221) (999986, 821656)

------------------------------------

Random Data
Number of Trials : 100

Selection Sort
Total Comparisons   : 4,999,500,000
Average Comparisons : 49995000.00
Min / Max Comparisons : 49,995,000 / 49,995,000
Total Movements     : 2,997,105
Average Movements   : 29971.05
Sorted Correctly    : 100 / 100

Bubble Sort
Total Comparisons   : 4,999,500,000
Average Comparisons : 49995000.00
Min / Max Comparisons : 49,995,000 / 49,995,000
Total Movements     : 7,491,149,088
Average Movements   : 74911490.88
Sorted Correctly    : 100 / 100

Bubble Sort (Early Termination)
Total Comparisons   : 4,998,460,065
Average Comparisons : 49984600.65
Min / Max Comparisons : 49,953,095 / 49,994,955
Total Movements     : 7,491,149,088
Average Movements   : 74911490.88
Average Passes      : 9870.30 (Min 9710 / Max 9990, Full = 9999)
Sorted Correctly    : 100 / 100

Insertion Sort
Total Comparisons   : 2,498,048,745
Average Comparisons : 24980487.45
Min / Max Comparisons : 24,630,900 / 25,359,327
Total Movements     : 2,499,049,496
Average Movements   : 24990494.96
Sorted Correctly    : 100 / 100

Early Termination Effect (Random)
Comparisons saved (average) : 10399.35 (0.02%)

------------------------------------

Sorted Data Input (Key 1) : 219, 248, 317, 441, 499, 565, ...

Sorted Data

Selection Sort
Comparisons : 49,995,000
Movements   : 0
Sorted      : OK

Bubble Sort
Comparisons : 49,995,000
Movements   : 0
Sorted      : OK

Bubble Sort (Early Termination)
Comparisons : 9,999
Movements   : 0
Passes      : 1
Sorted      : OK

Insertion Sort
Comparisons : 9,999
Movements   : 19,998
Sorted      : OK

------------------------------------

Reverse-Sorted Data Input (Key 1) : 999969, 999943, 999940, 999888, 999833, 999795, ...

Reverse-Sorted Data

Selection Sort
Comparisons : 49,995,000
Movements   : 15,075
Sorted      : OK

Bubble Sort
Comparisons : 49,995,000
Movements   : 149,984,847
Sorted      : OK

Bubble Sort (Early Termination)
Comparisons : 49,995,000
Movements   : 149,984,847
Passes      : 9999
Sorted      : OK

Insertion Sort
Comparisons : 49,995,000
Movements   : 50,014,947
Sorted      : OK

------------------------------------

Partially Sorted Data Input
Method            : sorted data + 100 random position swaps
Elements moved    : 199 / 10000 (1.99%)
Elements in place : 9801 / 10000 (98.01%)

Partially Sorted Data

Selection Sort
Comparisons : 49,995,000
Movements   : 309
Sorted      : OK

Bubble Sort
Comparisons : 49,995,000
Movements   : 1,963,998
Sorted      : OK

Bubble Sort (Early Termination)
Comparisons : 49,059,972
Movements   : 1,963,998
Passes      : 8632
Sorted      : OK

Insertion Sort
Comparisons : 664,665
Movements   : 674,664
Sorted      : OK

------------------------------------

Stability Verification

[Test 1] Small fixed data (8 items, Key 1 in {125, 527, 892})
Original Data
  Index  Key 1  Key 2
      0    527    100
      1    527    200
      2    125    300
      3    892    400
      4    125    500
      5    527    600
      6    892    700
      7    125    800

Selection Sort      -> Not Stable (order violations 2, sorted OK)
  Index  Key 1  Key 2
      2    125    300
      4    125    500
      7    125    800
      1    527    200
      5    527    600
      0    527    100
      6    892    700
      3    892    400

Bubble Sort         -> Stable (order violations 0, sorted OK)
  Index  Key 1  Key 2
      2    125    300
      4    125    500
      7    125    800
      0    527    100
      1    527    200
      5    527    600
      3    892    400
      6    892    700

Bubble Sort (Early) -> Stable (order violations 0, sorted OK)
  Index  Key 1  Key 2
      2    125    300
      4    125    500
      7    125    800
      0    527    100
      1    527    200
      5    527    600
      3    892    400
      6    892    700

Insertion Sort (>)  -> Stable (order violations 0, sorted OK)
  Index  Key 1  Key 2
      2    125    300
      4    125    500
      7    125    800
      0    527    100
      1    527    200
      5    527    600
      3    892    400
      6    892    700

Insertion Sort (>=) -> Not Stable (order violations 5, sorted OK)
  Index  Key 1  Key 2
      7    125    800
      4    125    500
      2    125    300
      5    527    600
      1    527    200
      0    527    100
      6    892    700
      3    892    400

[Test 2] Random data with many duplicate keys
Data Count        : 1000
Key 1 Range       : 0 ~ 99
Equal-key pairs   : 900 (adjacent pairs with the same Key 1 after sorting)

Algorithm             Result      Violations   Comparisons     Movements  Sorted
Selection Sort        Not Stable         289       499,500         2,952  OK
Bubble Sort           Stable               0       499,500       761,316  OK
Bubble Sort (Early)   Stable               0       498,597       761,316  OK
Insertion Sort (>)    Stable               0       254,766       255,770  OK
Insertion Sort (>=)   Not Stable         900       259,760       260,774  OK

Stability Verification (Summary)

Selection Sort       : Not Stable
Bubble Sort          : Stable
Bubble Sort (Early)  : Stable
Insertion Sort (>)   : Stable
Insertion Sort (>=)  : Not Stable