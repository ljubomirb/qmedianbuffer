# qmedianbuffer

**Small circular queue buffer with median.**

   Buffer returns:
  

 -  1 item, same value
 -  2 items, average of 2
 -  3 items, average of 3
 -  4 and more (even), median, and then average of 2 middle items
 -  5 and more (uneven), median, and then average of 3 middle items

   **Important**: 
   It is possible to add infinitely to buffer, however it is a good idea to reset it to have fresh values at hand!

   **Implementation**:
   Optimised to be used in cases when there is a constant stream of data,
   but read is less often needed. Therefore, buffer would always be full and ready.

   Other designs of queue include one with +1 item in queue, or keeping track of lenght,
   or using pointers. This one is simple, and tries to conserve memory, to not use
   multiple copies of array. For this reason, after each median, array itself is reshuffled.
   This must be fixed only if adding new (array is optimised for fast adding),
   because otherise, we do not know age of entry. Also, no holes should exist so
   the same should be done before pop.

   The problem of adding to circular buffer is that if sorting is done after each add
   and if buffer was full, and then sorted, old and new items are no longer in sequence
   It is here done by using struct with insertOrder info, values of wich are valid just
   before median sort.

   **Types**:
-  **`T`** type of value used, integer or double or whatever
-  **`unsignedT`** type of time value, unsigned `uint8_t`, `uint16_t`, `uint32_t`,
depending how deep down you plan to remember

**NOTE**:
-each data entry is size `T + unsignedT + 1 byte`
-max count (length) 255
