/* Small circular queue buffer with median and average functions.
   Ljubomir Blanusa, v0.7 2018; released under MIT licence

   Buffer functions returns:
   -median() median value (the same, original value as in buffer)
   -medianAverage() median value, but average of 2 or 3 entries
   -average() average value, with slight error to avoid big number problem;
   test and choose <double> it is important for you
   -rateOfChange() 1/averageInterval (used for example for interrupts/sec cases)

   Usecase:
   sometimes you just need to get median value out of 5-10 entries on low resources,
   Arduino like systems. Most often, those values are simple uint8_t or uint16_t,
   and there is no need for bigger or more complex solutions.

   Important:
   type of <T> must be big enogh to hold SUM of entire array if any function that averages
   values is used. To handle this, just set T big enough.
   However, humans tend to forget this, and thus unexpected values could appear unnoticed
   because of overflow. So, approach here is the oposite: approximation on averaging is used,
   with introduction of small error every time, but less fail when bigger numbers appear.
   If you happen to notice that precission is not good enough for you, turn that off,
   and set <T> as <double> (preffered), or if you use integer, then double its size,
   so <uint32_t> instead of <uint16_t>.

   Implementation:
   Optimised to be used in cases when there is a constant stream of data,
   but read is less often needed. Therefore, buffer would always be full and ready.

   Other designs of queue include one with +1 item in queue, or keeping track of lenght,
   or using various pointer designs. This one is simple, and tries to conserve memory,
   to not use multiple copies of array. For this reason, each sort reshuffes original.

   The problem of adding to circular buffer is that if sorting is done after each add
   and if buffer was full, and then sorted, old and new items are no longer in sequence.
   This matters only when items are added (array is optimised for fast adding),
   because age of entry is important for circular approach. Also, no holes should exist,
   so the same should be done before pop.
   It is here done by using struct with insertOrder info, values of wich are valid just
   before sorting.

   Note on values:
   -<T> type of value used - integer or double or whatever
   -<unsignedT> type of time value, either unsigned <uint8_t>, <uint16_t>, <uint32_t>,
   depending how deep down you plan to remember insertion time
   -each data entry is size <T> + <unsignedT> + 1 byte
   -max count 255
   -take care, any average() operation is with slight error due to approximations
   -it is a smart thing to clear buffer if you do not use time track, after each median,
    so you always have a fresh set of data
   */

#ifndef qmedianbuffer_h
#define qmedianbuffer_h
//#if defined(ARDUINO)
//#include "Arduino.h"
//#else
#include <stdint.h>
//#endif

#define EXPECT_BIG_NUMBERS 1

//using namespace std;
//<T> is any type of numeric data stored; <unsignedT> is any, but strictly UNSIGNED, numeric type for time data
template<typename T, typename unsignedT>
class qmedianbuffer
{
public:
	qmedianbuffer(uint8_t capacity) {
		_capacity = capacity;
		items = new itemQ[capacity]; //={1,200,2,6,9};
	}
	~qmedianbuffer() {
		delete[] items;
	}

	void push(T number, unsignedT currentTime);
	T pop();
	T peek();
	unsignedT peekTime();
	void clear();

	bool isFull();
	bool isEmpty();
	uint8_t getCount();

	uint8_t getPushCount();
	void resetPushCount();

	bool deleteOlderThanInterval(unsignedT currentTimeStamp, unsignedT interval);

	T max();
	T min();

	T median();
	T medianAverage();
	T average();

	T medianInterval();
	T medianAverageInterval();
	T medianRateOfChange();			// 1/medianInterval
	T medianAverageRateOfChange();	// 1/medianAverageInterval
	T averageInterval();
	T averageRateOfChange();

	/*void debug(){
		cout << "-----------" << endl;
		for (uint8_t i = 0; i < getCount(); i++){
			itemQ *item = getItemAtPositionPtr(i);
			cout << "V: " << item->value << "\t O: " << (int)item->insertOrder << "\t T: " << item->time << endl;
		}
	}*/

private:
	struct itemQ {
		uint8_t insertOrder{};
		T value{};
		unsignedT time{};
	};

	static uint8_t getTruePos(uint8_t pos, uint8_t len, uint8_t capacity);

	static T _median2(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static T _medianAverage2(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));

	static T _average(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));

	//to be depreciated
	//static T quick_select(uint8_t k, uint8_t head, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static void sort(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));

	//itemQ items[_capacity]; //={1,200,2,6,9};
	itemQ* items;
	uint8_t _capacity{};
	uint8_t _head{};
	uint8_t _tail{};
	bool _isFull{};

	uint8_t _pushCount;

	static T getItemValue(const itemQ &item);
	static T getItemInsertOrder(const itemQ &item);

	bool isInInsertSequence = true;	//items are in the same order as when being put
	bool valuesAreIntervals = false;

	void resetItemOrderOldestToZero();	//oldest item will have internal counter set to zero, others will increment
	void sortToInsertSequenceIfNeeded();
	void sortToValuesIfNeeded();
	void intervalsToValues();

	itemQ* peekItem();
	itemQ* getItemAtPositionPtr(uint8_t position);
};

//------------------pop push peek-----------------

template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::push(T number, unsignedT currentTime) {
	_pushCount++; //non important, user info counter of all push operations
	
	sortToInsertSequenceIfNeeded();

	itemQ newitem;
	newitem.value = number;
	valuesAreIntervals = false;

	newitem.time = currentTime;

	//insertOrder is not important now, it is written pre shuffle, for returning back

	items[_head] = newitem;
	if (_isFull){
		_tail = (_tail + 1) % _capacity;
	}
	_head = (_head + 1) % _capacity;
	_isFull = _head == _tail;
}

//pop will take the oldes one out, tracking insertion order;
//if reason is timestamp being too old, it still works (insertOrder is unique)
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::pop() {

	if (isEmpty()) return T();

	sortToInsertSequenceIfNeeded();

	itemQ *item = getItemAtPositionPtr(0);
	_isFull = false; //it will for sure not be full
	_tail = (_tail + 1) % _capacity;
	return item->value;
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::peek() {
	return peekItem()->value;
}

template<typename T, typename unsignedT>
unsignedT qmedianbuffer<T, unsignedT>::peekTime() {
	return peekItem()->time;
}

template<typename T, typename unsignedT>
typename qmedianbuffer<T, unsignedT>::itemQ* qmedianbuffer<T, unsignedT>::peekItem() {

	sortToInsertSequenceIfNeeded();

	itemQ* item = getItemAtPositionPtr(0);
	return item;
}

template<typename T, typename unsignedT>
bool qmedianbuffer<T, unsignedT>::deleteOlderThanInterval(unsignedT currentTimeStamp, unsignedT interval) {

	if (isEmpty()){
		return false; //play stupid, do not throw error
	}
	if ((currentTimeStamp - peekTime()) < interval){
		pop();
		return true;
	}
	return false;
}

//never deletes, only resets counter, sorting is only between tail and tail+len
template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::clear() {
	_head = _tail;
	_isFull = false;
}

template<typename T, typename unsignedT>
bool qmedianbuffer<T, unsignedT>::isFull() {
	return _isFull;
}

//isEmpty() will be calculated each time it is called
template<typename T, typename unsignedT>
bool qmedianbuffer<T, unsignedT>::isEmpty() {
	return (!_isFull && (_head == _tail));
}

//getCount() will be calculated each time it is called
template<typename T, typename unsignedT>
uint8_t qmedianbuffer<T, unsignedT>::getCount() {
	uint8_t retCount = _capacity;
	if (!_isFull){
		if (_head >= _tail){
			retCount = (_head - _tail);
		}
		else{
			retCount = _capacity + _head - _tail;
		}
	}
	return retCount;
}

//getPushCount() returns simple count of push operations
template<typename T, typename unsignedT>
uint8_t qmedianbuffer<T, unsignedT>::getPushCount(){
	return _pushCount;
}

template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::resetPushCount(){
	_pushCount = 0;
}

//------helper function to get pointer to item at position-----

template<typename T, typename unsignedT>
typename qmedianbuffer<T, unsignedT>::itemQ* qmedianbuffer<T, unsignedT>::getItemAtPositionPtr(uint8_t position) {

	itemQ *item;

	if (position == 0){ //micro optimisation
		item = &items[_tail];
	}
	else{
		uint8_t realPosition = (_tail + position) % _capacity;
		item = &items[realPosition];
	}

	if (isEmpty()){
		itemQ emptyItem{};
		item = &emptyItem;//clear it, and return clean one; to array also, although it does not matter there
	}
	return item;
}


//------predicates to pass as function to get value for sorting-----

//predicate to pass to quick select algorithm to sort by value
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::getItemValue(const itemQ &item) {
	return item.value;
}

//predicate to pass to sort algorithm to sort by initial order
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::getItemInsertOrder(const itemQ &item) {
	return (T)item.insertOrder;
}

//----------------order functions-------------

//call this to prepare array to sort it back to the original order later
template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::resetItemOrderOldestToZero() {
	//must be in insert sequence prior to this!
	for (uint8_t i = 0; i < getCount(); i++){
		itemQ *item = getItemAtPositionPtr(i);
		item->insertOrder = i;
	}
}

//back to input sequence, from numbers saved in each item, so we can push in sequence
template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::sortToInsertSequenceIfNeeded() {
	if (!isInInsertSequence && !isEmpty()){
		sort(_tail, getCount(), items, _capacity, getItemInsertOrder);
	}
	isInInsertSequence = true;
}

//sort values; remember to return it to insert order for pop/push/peek
template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::sortToValuesIfNeeded() {
	if (isInInsertSequence && !isEmpty()){
		resetItemOrderOldestToZero();
		sort(_tail, getCount(), items, _capacity, getItemValue);
	}
	isInInsertSequence = false;
}

//---------fill values with interval between them----------

//calculate intervals between items in sequence (or force them in sequence if needed)
template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::intervalsToValues() {

	if (!valuesAreIntervals){

		//it may happen that median was called, and everything is sorted by value, but values are not intervals
		sortToInsertSequenceIfNeeded();

		uint8_t count = getCount();
		if (count < 2)	return; //or array error in loop

		//only intervals between items are measured, and they should be in sequence
		//result is written in previous item as value, so we are only measuring occurence
		itemQ* itemPrev = getItemAtPositionPtr(0); //tail
		for (uint8_t i = 1; i < count; i++){
			itemQ *itemNext = getItemAtPositionPtr(i);
			unsignedT intervalDifference = (itemNext->time - itemPrev->time);
			itemPrev->value = intervalDifference;
			itemPrev = NULL;
			itemPrev = itemNext;
			i = i;
		}
		itemPrev->value = 0; //but median should ignore it anyway
		valuesAreIntervals = true;
	}	
}


//-----------statistical functions-------------

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::min() {
	sortToValuesIfNeeded();
	return getItemAtPositionPtr(0)->value;
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::max() {
	sortToValuesIfNeeded();
	return getItemAtPositionPtr(getCount() - 1)->value;
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::medianAverage() {
	sortToValuesIfNeeded();
	return _medianAverage2(_tail, getCount(), items, _capacity, getItemValue);
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::median() {
	sortToValuesIfNeeded();
	return _median2(_tail, getCount(), items, _capacity, getItemValue);
}

//if items in buffer are type of occurence, of no important value
//then measure average interval (at least 2 items to make any sense)
//items are sorted if needed, intervals are written to value field
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::medianInterval() {

	uint8_t length = getCount();
	if (length < 2)	return T();

	intervalsToValues();	//will not run if already done
	sortToValuesIfNeeded();	//will not run if already sorted by value
	
	//check all, but ignore last one, it should be 0!
	return _median2(_tail, length - 1, items, _capacity, getItemValue);
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::medianAverageInterval() {
	
	uint8_t length = getCount();
	if (length < 2)	return T();

	intervalsToValues();	//will not run if already done
	sortToValuesIfNeeded();	//will not run if already sorted by value

	//check all, but ignore last one, it should be 0!
	return _medianAverage2(_tail, length - 1, items, _capacity, getItemValue);
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::medianRateOfChange() {
	return 1 / medianInterval();
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::medianAverageRateOfChange() {
	return 1 / medianAverageInterval();
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::averageInterval() {

	uint8_t length = getCount();
	if (length < 2)	return T();

	//if doing average, order of items is not that important
	//but items may be sorted by different functions
	intervalsToValues();

	//check all, but ignore last one, it should be 0!
	return _average(_tail, length - 1, items, _capacity, getItemValue);
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::averageRateOfChange() {
	return 1 / averageInterval();
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::average() {
	//average does not shuffle order of items
	return _average(_tail, getCount(), items, _capacity, getItemValue);
}


//---------------static median and average functions------------------

//helper function to get actual position of item in array, regarding tail (integer overflow calculations)
template<typename T, typename unsignedT>
uint8_t qmedianbuffer<T, unsignedT>::getTruePos(uint8_t posSeek, uint8_t tail, uint8_t capacity){
	uint8_t truePos = (posSeek + tail) % capacity;
	return truePos;
}

//max sum must never be bigger then T
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::_average(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)){
	T sum{}, avg{}, itemValue{};
	for (uint8_t i = 0; i < len; i++){
		itemValue = getSortValueFunc(arr[getTruePos(i, tail, arrCapacity)]);
#if EXPECT_BIG_NUMBERS
		avg = (itemValue - avg) / (i + 1) + avg; //simple approach to try to avoid overflow with big numbers; use double type if need more precision
	}
#else		
		sum = sum + itemValue; //use double type, or any bigger type, big enough to hold sum of array, since it will owerflow
	}
	avg = sum / len;
#endif		
	return avg;
}

//pick median in previously sorted array, it will always be exact number
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::_median2(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)){
	if (len == 0) {
		return T();
	}
	if (len == 1) {
		return getSortValueFunc(arr[getTruePos(0, tail, arrCapacity)]);
	}
	// else, always pick at least one original value
	return getSortValueFunc(arr[getTruePos(len / 2, tail, arrCapacity)]);
}

//pick median in previously sorted array
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::_medianAverage2(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)){
	if (len == 0) {
		return T();
	}
	if (len == 1) {
		return getSortValueFunc(arr[getTruePos(0, tail, arrCapacity)]);
	}

	//there appears to be two ways people calculate median; the latter is used in this library
	//	(double)(a[(n - 1) / 2] + a[n / 2]) / 2.0
	//	(double)(a[(n / 2) - 1] + a[n / 2]) / 2.0

	uint8_t pos = len / 2;
	T median;
	if (len % 2 == 0)	// Even number of elements, median is average of middle two
	{
		T medi = getSortValueFunc(arr[getTruePos(pos, tail, arrCapacity)]);
		T mediminus = getSortValueFunc(arr[getTruePos(pos - 1, tail, arrCapacity)]);
#if EXPECT_BIG_NUMBERS
		T avg2 = medi / 2 + mediminus / 2; //devide here to try to avoid overflow of big numbers
#else
		T avg2 = (medi + mediminus) / 2;
#endif
		median = avg2;
	}
	else // select middle element, -1, +1, and average of that
	{
		//NOTE: quick select will exit without rearanging all if it finds its goal
		T medi = getSortValueFunc(arr[getTruePos(pos, tail, arrCapacity)]);
		T mediminus = getSortValueFunc(arr[getTruePos(pos - 1, tail, arrCapacity)]);
		T mediplus = getSortValueFunc(arr[getTruePos(pos + 1, tail, arrCapacity)]);
#if EXPECT_BIG_NUMBERS
		T avg3 = medi / 3 + mediminus / 3 + mediplus / 3; //devide here to try to avoid overflow of big numbers
#else
		T avg3 = (medi + mediminus + mediplus) / 3;
#endif
		median = avg3;
	}
	return median;
}

//---------------static select and sort functions------------------

//standard insertionSort algorithm, done in one pass
template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::sort(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)) {
	int i, j;
	itemQ key;
	for (i = 1; i < len; i++)
	{
		key = arr[getTruePos(i, tail, arrCapacity)];
		j = i - 1;

		while (j >= 0 && getSortValueFunc(arr[getTruePos(j, tail, arrCapacity)]) > getSortValueFunc(key))
		{
			arr[getTruePos(j + 1, tail, arrCapacity)] = arr[getTruePos(j, tail, arrCapacity)];
			j = j - 1;
		}
		arr[getTruePos(j + 1, tail, arrCapacity)] = key;
	}
}
#endif
