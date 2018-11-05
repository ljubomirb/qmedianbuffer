# qmedianbuffer

**Small circular queue buffer with median.**

**Usecase**:
Sometimes you just need to get the median value out of 5-10 entries on low resource, Arduino type of system.
Most often, those values are simple uint8_t or uint16_t and there is no need for bigger or more complex solutions.

**Functions**:

<table class="tg">
  <tr>
    <th class="tg-0pky">function</th>
    <th class="tg-0pky">description</th>
  </tr>
  <tr>
    <td class="tg-0pky">`T push()`</td>
    <td class="tg-0pky">puts data in</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T pop()`</td>
    <td class="tg-0pky">pops data out (the oldest one)</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T peek()`</td>
    <td class="tg-0pky">reads data (the oldest one)</td>
  </tr>
  <tr>
    <td class="tg-0pky">`unsignedT`&nbsp;&nbsp;peekTime()</td>
    <td class="tg-0pky">reads timestamp of the oldest data entry</td>
  </tr>
  <tr>
    <td class="tg-0pky">`clear()`</td>
    <td class="tg-0pky">clears all</td>
  </tr>
  <tr>
    <td class="tg-0pky">`bool isFull()`</td>
    <td class="tg-0pky">returns if buffer is full</td>
  </tr>
  <tr>
    <td class="tg-0pky">`bool isEmpty()`</td>
    <td class="tg-0pky">returns if buffer is empty</td>
  </tr>
  <tr>
    <td class="tg-0pky">`uint8_t getCount()`</td>
    <td class="tg-0pky">returns count of items in buffer</td>
  </tr>
  <tr>
    <td class="tg-0pky">`uint8_t getPushCount()`</td>
    <td class="tg-0pky">returns count of all puts (255 max, then overflows)</td>
  </tr>
  <tr>
    <td class="tg-0pky">`resetPushCount()`</td>
    <td class="tg-0pky">resets push counter</td>
  </tr>
  <tr>
    <td class="tg-0pky">`bool deleteOlderThanInterval()`</td>
    <td class="tg-0pky">deletes older then (now - interval)</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T maxValue()`</td>
    <td class="tg-0pky">gets max value</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T minValue()`</td>
    <td class="tg-0pky">gets min value</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T median()`</td>
    <td class="tg-0pky">gets median, original entry no matter what</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T medianAverage()`</td>
    <td class="tg-0pky">gets median, average of 2 or 3 median values</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T average()`</td>
    <td class="tg-0pky">gets average of all items</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T medianInterval()`</td>
    <td class="tg-0pky">gets median time interval between sequential items</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T medianAverageInterval()`</td>
    <td class="tg-0pky">gets medianAverage time interval between sequential items</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T medianRateOfChange()`</td>
    <td class="tg-0pky">1 / medianInterval (usefull for count/second measurements)</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T medianAverageRateOfChange()`</td>
    <td class="tg-0pky">1 / medianAverageInterval (usefull for count/second measurements)</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T averageInterval()`</td>
    <td class="tg-0pky">gets average interval</td>
  </tr>
  <tr>
    <td class="tg-0pky">`T averageRateOfChange()`</td>
    <td class="tg-0pky">1 / averageInterval</td>
  </tr>
</table>


> **Note:** Median and sorting is done using insertion sort algorithm. It remans to be seen if maybe quick select algorithm is better suited in terms of memory/perfomance on average embedded system.

> **Note2:** Median is often expressed as one of two following equations. The latter is used here.

    (double)(a[(n - 1) / 2] + a[n / 2]) / 2.0
    (double)(a[(n / 2) - 1] + a[n / 2]) / 2.0
