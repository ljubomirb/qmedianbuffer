/* Small circular queue buffer with median and average functions.
   Ljubomir Blanusa, v0.80 2018; released under MIT licence

   Buffer functions returns:
   -median() median value (the same, original value as in buffer)
   -medianAverage() median value, but average of surrounding entries
   -average() average value, with slight error to avoid big number problem;
   test and choose <double> if it is important to you
   -rateOfChange() 1/averageInterval (used for example for interrupts/sec cases)

   Usecase:
   sometimes you just need to get median value out of 5-10 entries on low resource,
   Arduino like systems. Most often, those values are simple uint8_t or uint16_t,
   and there is no need for bigger or more complex solutions.

   NOTE important:
   type of <resultT> must be big enough to hold SUM of entire array if any function that averages
   values is used. To handle this, just set <resultT> big enough.
   However, humans tend to forget this, but even so, unexpected values could appear unnoticed
   because of overflow. So, approach here is the oposite: approximation on averaging is used,
   with introduction of small error every time, but less fail when bigger numbers appear.
   If you happen to notice this, and that precission is not good enough for you, turn it off,
   and set <resultingT> to <double> (preffered), or if you use integer, double its size, that is
   use <long> instead of <int>.

   NOTE also:
   calculating anything needing interval, will delete all original numeric values

   Implementation:
   Optimised to be used in cases when there is a constant stream of data,
   but read is less often needed. Therefore, buffer would always be full and ready.

   Other designs of queue include one with +1 item in queue, or keeping track of lenght,
   using various pointer designs, or sometimes creating copy of array during sorting.
   The one here tries to be simple and conserve memory. For this reason, each sort
   reshuffes original, and each interval calc replaces numbers with intervals.

   The problem of adding to circular buffer is that if sorting is done after each add
   and if buffer was full, and then sorted, old and new items are no longer in sequence.
   This matters only when items are added (this array is optimised for fast adding),
   because age of entry is important for circular approach. Also, no holes should exist,
   so the same should be done before pop.
   It is here done by using struct as array items, with insertOrder info, values of
   wich are valid just before sorting.

   Note on values:
   -<T> type of numeric value used - integer, or double or whatever
   -<timeT> type of time value, either unsigned <uint8_t>, <uint16_t>, <uint32_t>,
   depending how deep down you plan to remember insertion time
   -<resultingT> type used for math operations; the idea is that you can have
   any numeric type in buffer, like <uint16_t>, but had result as double (say, 32bits)

   -each data entry is the size of (<T> + <timeT> + 1 byte)

   -max count: 255
   -take care, any average() operation is with slight error due to approximations
   -it is a smart thing to clear buffer after each statistical function,
   to remove old values, so that way you always have a fresh set of data,
   or you should take care of time added, and use delete function
   */

#ifndef qmedianbuffer_h
#define qmedianbuffer_h
#if defined(ARDUINO)
#include "Arduino.h"
#else
#include <cstdint>
#include <cmath>
#endif


//-------------------------------tuning place---------------------------------------------------

//turn this off (0) if you expect sum will never be near type limit (e.g. will never overflow)
#define EXPECT_BIG_NUMBERS 1

//you should leave this turned on (1) unless you are disciplined enough to know what you're doing
#define RESTRICT_TYPES_OF_DATA 1

//-----------------------------------------------------------------------------------------------


//this implementation of abs() may be slower but works with both unsigned long and floating types
#if RESTRICT_TYPES_OF_DATA
#define absX(value) ((value) < 0 ? -1*(value) : (value))
#else
#define absX(value) abs(value) //standard abs has compile error if <resultingT> is unsigned long/int
#endif

//tests about types; if STD library is available, this may be removed, and std used
#define is_type_signed(my_type) (((my_type)-1) < 0)
#define is_type_unsigned(my_type) (((my_type)-1) > 0)


/*
tested: C-style or static_cast<>() casting of the same T primitive type to same type, being implicite or not,
or to a bigger type (e.g. int to long) has no extra cost after compilation; that is - the compiler will optimise
and the result would be the same as if there was no casting at all
*/


//<T> numeric data stored; <timeT> strictly UNSIGNED type for incremental time data, <resultingT> return type of math heavy functions
template<typename T, typename timeT, typename resultingT>
class qmedianbuffer
{

#if RESTRICT_TYPES_OF_DATA
	static_assert(is_type_signed(resultingT), "qmedianbuffer: non-recommended type for <resultingT>; should be any signed type (<int>, <float>, <double>...)");
	static_assert(is_type_unsigned(timeT), "qmedianbuffer: non-recommended type for <timeT>; should be any unsigned type (<uint16_t>, <uint32_t>...)");
#endif

public:

	//initiate circular buffer, capacity best to be uneven number
	qmedianbuffer(uint8_t capacity) {
		_capacity = capacity;
		items = new itemQ[capacity];
	}
	~qmedianbuffer() {
		delete[] items;
	}

	void push(T number, timeT currentTime);
	T pop();
	T peek();
	timeT peekTime();
	void clear();

	bool isFull();
	bool isEmpty();
	uint8_t getCount();

	uint8_t getPushCount();
	void resetPushCount();

	bool deleteOld(timeT currentTimeStamp, timeT interval);

	T maxValue();
	T minValue();

	T range();
	uint8_t occurenceOfValue(T testValue, T epsilon);
	resultingT frequencyOfValue(T testValue, T epsilon);

	resultingT meanAbsoluteDeviationAroundAverage();
	resultingT meanAbsoluteDeviationAroundMedianAverage(uint8_t maxDistanceFromMedian);

	resultingT average();

	T median();
	resultingT medianAverage();
	resultingT medianAverage(uint8_t maxDistance);

	resultingT averageInterval();
	resultingT averageRateOfChange();

	T medianInterval();
	resultingT medianAverageInterval(uint8_t maxDistanceFromMedian);
	resultingT medianRateOfChange();										// 1/medianInterval
	resultingT medianAverageRateOfChange(uint8_t maxDistanceFromMedian);	// 1/medianAverageInterval

	/*void debug(){
		std::cout << "-----------" << std::endl;
		for (uint8_t i = 0; i < getCount(); i++){
			itemQ *item = getItemAtPositionPtr(i);
			std::cout << "V: " << (int)item->value << "\t O: " << (int)item->insertOrder << "\t T: " << (int)item->time << std::endl;
		}
	}*/

private:
	struct itemQ {
		uint8_t insertOrder{};
		T value{};
		timeT time{};
	};

	static uint8_t getTruePos(uint8_t pos, uint8_t len, uint8_t capacity);

	static T _median(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static resultingT _medianAverage(uint8_t tail, uint8_t len, uint8_t maxDistanceFromMedian, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static resultingT _meanAbsoluteDeviationAroundMedianAverage(uint8_t tail, uint8_t len, uint8_t maxDistanceFromMedian, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));

	static resultingT _average(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static resultingT _meanAbsoluteDeviationAroundAverage(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));

	static void sort(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));

	itemQ* items;
	uint8_t _capacity{};
	uint8_t _head{};
	uint8_t _tail{};
	bool _isFull{};

	uint8_t _pushCount{};

	static T getItemValue(const itemQ &item);
	static T getItemInsertOrder(const itemQ &item);

	bool valuesAreGoodIntervals = false;

	void resetItemOrderOldestToZero();	//oldest item will have internal counter set to zero, others will increment
	void sortToInsertSequence();
	void sortToValues(uint8_t len);
	void intervalsToValues();

	itemQ* peekItem();
	itemQ* getItemAtPositionPtr(uint8_t position);
};

//------------------pop push peek-----------------

template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::push(T number, timeT currentTime) {
	_pushCount++; //non important, user info counter of all push operations

	itemQ newitem;
	newitem.value = number;
	valuesAreGoodIntervals = false;

	newitem.time = currentTime;
	//newitem.insertOrder is not important now; it is written pre shuffle, for reshuffling back

	items[_head] = newitem;
	if (_isFull){
		_tail = (_tail + 1) % _capacity;
	}
	_head = (_head + 1) % _capacity;
	_isFull = _head == _tail;
}

//pop will take the oldes one out by tracking insertion order (not time)
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::pop() {

	if (isEmpty()) return T();

	valuesAreGoodIntervals = false; //intervals are no longer valid

	itemQ *item = getItemAtPositionPtr(0);
	_isFull = false; //it will for sure not be full
	_tail = (_tail + 1) % _capacity;
	return item->value;
}

//returns value of oldest item
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::peek() {
	return peekItem()->value;
}

//returns time of oldest item
template<typename T, typename timeT, typename resultingT>
timeT qmedianbuffer<T, timeT, resultingT>::peekTime() {
	return peekItem()->time;
}

template<typename T, typename timeT, typename resultingT>
typename qmedianbuffer<T, timeT, resultingT>::itemQ* qmedianbuffer<T, timeT, resultingT>::peekItem() {

	itemQ* item = getItemAtPositionPtr(0);
	return item;
}

//deletes one item, older then current time - interval
template<typename T, typename timeT, typename resultingT>
bool qmedianbuffer<T, timeT, resultingT>::deleteOld(timeT currentTimeStamp, timeT interval) {

	if (isEmpty()){
		return false;
	}
	if ((currentTimeStamp - peekTime()) < interval){
		pop();
		return true;
	}
	return false;
}

//never deletes, only resets counter, sorting is only between tail and tail+len
template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::clear() {
	_head = _tail;
	_isFull = false;
}

template<typename T, typename timeT, typename resultingT>
bool qmedianbuffer<T, timeT, resultingT>::isFull() {
	return _isFull;
}

//tests if empty, and returns (mem consumption remains the same)
template<typename T, typename timeT, typename resultingT>
bool qmedianbuffer<T, timeT, resultingT>::isEmpty() {
	return (!_isFull && (_head == _tail));
}

//returns freshly calculated count, each time called
template<typename T, typename timeT, typename resultingT>
uint8_t qmedianbuffer<T, timeT, resultingT>::getCount() {
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

//returns simple count of push operations
template<typename T, typename timeT, typename resultingT>
uint8_t qmedianbuffer<T, timeT, resultingT>::getPushCount(){
	return _pushCount;
}

template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::resetPushCount(){
	_pushCount = 0;
}


//------helper function to get pointer to item at position-----

template<typename T, typename timeT, typename resultingT>
typename qmedianbuffer<T, timeT, resultingT>::itemQ* qmedianbuffer<T, timeT, resultingT>::getItemAtPositionPtr(uint8_t position) {

	itemQ *item;

	if (position == 0){ //micro optimisation
		item = &items[_tail];
	}
	else{
		uint8_t realPosition = (_tail + position) % _capacity; //micro optimisation exists for this also, but reduces readability
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
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::getItemValue(const itemQ &item) {
	return item.value;
}

//predicate to pass to sort algorithm to sort by initial order
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::getItemInsertOrder(const itemQ &item) {
	return (T)item.insertOrder; //casting needed for uniformity in sort
}


//----------------order functions-------------

//call this to prepare array to sort it back to the original order later
template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::resetItemOrderOldestToZero() {
	//must be in insert sequence prior to this!
	for (uint8_t i = 0; i < getCount(); i++){
		itemQ *item = getItemAtPositionPtr(i);
		item->insertOrder = i;
	}
}

//back to input sequence, from numbers saved within each item
template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::sortToInsertSequence() {
	if (!isEmpty()){
		sort(_tail, getCount(), items, _capacity, getItemInsertOrder);
	}
}

//sort array order by values;
template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::sortToValues(uint8_t len) {
	if (!isEmpty()){
		resetItemOrderOldestToZero();
		sort(_tail, len, items, _capacity, getItemValue);
	}
}


//--------fill values with interval between sequential items----------

//calculate intervals between items in sequence
template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::intervalsToValues() {

	if (!valuesAreGoodIntervals){

		if (getCount() < 2)	return; //or array error in loop
		/*
		only intervals between items are measured, and they should be in sequence
		result is written in previous item as value, so we are only measuring occurence
		there can be only len - 1 intervals
		*/
		itemQ* itemPrev = getItemAtPositionPtr(0); //tail
		for (uint8_t i = 1; i < getCount(); i++){
			itemQ *itemNext = getItemAtPositionPtr(i);
			timeT intervalDifference = (itemNext->time - itemPrev->time);
			itemPrev->value = intervalDifference; //make sure .value <T> is big enough to hold intervaldiff
			itemPrev = nullptr;
			itemPrev = itemNext;
			i = i;
		}
		itemPrev->value = 0; //but median should ignore it anyway
		valuesAreGoodIntervals = true;
	}
}


//-----------statistical functions-------------

template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::minValue() {

	T tempMinV = getItemAtPositionPtr(0)->value; //set default value (0)

	for (uint8_t i = 1; i < getCount(); i++)
	{
		T testValue = getItemAtPositionPtr(i)->value;
		if (testValue < tempMinV) tempMinV = testValue;
	}

	return tempMinV;
}

template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::maxValue() {

	T tempMaxV = getItemAtPositionPtr(0)->value;//set default value (0)

	for (uint8_t i = 1; i < getCount(); i++)
	{
		T testValue = getItemAtPositionPtr(i)->value;
		if (testValue > tempMaxV) tempMaxV = testValue;
	}
	return 	tempMaxV;
}

//max - min value, statistical function
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::range()
{
	return maxValue() - minValue();
}


//number of occurence of value within buffer, with difference less then epsilon
template<typename T, typename timeT, typename resultingT>
uint8_t qmedianbuffer<T, timeT, resultingT>::occurenceOfValue(T testValue, T epsilon)
{
	uint8_t nOfTimes = 0;
	for (uint8_t i = 0; i < getCount(); i++){

		T arrayValue = getItemAtPositionPtr(i)->value;
		/*
		since standard abs(x) function doesn't work with integers,
		and we may not care at the moment to cast to <resultingT>, use this macro
		*/
		if (absX(arrayValue - testValue) < epsilon) nOfTimes++;
	}
	return nOfTimes;
}

//number of occurence of value within buffer, with difference always less then epsilon, devided by count
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::frequencyOfValue(T testValue, T epsilon)
{
	return (resultingT)occurenceOfValue(testValue, epsilon) / getCount();
}

//mean absolute deviation around calculated average of all
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::meanAbsoluteDeviationAroundAverage()
{
	return _meanAbsoluteDeviationAroundAverage(_tail, getCount(), items, _capacity, getItemValue);
}

//mean absolute deviation ardound medianaverage
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::meanAbsoluteDeviationAroundMedianAverage(uint8_t maxDistanceFromMedian)
{
	sortToValues(getCount());
	resultingT retVal = _meanAbsoluteDeviationAroundMedianAverage(_tail, getCount(), maxDistanceFromMedian, items, _capacity, getItemValue);
	sortToInsertSequence();
	return retVal;
}

//original, unchanged median value
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::median() {
	sortToValues(getCount());
	T retVal = _median(_tail, getCount(), items, _capacity, getItemValue);
	sortToInsertSequence();
	return retVal;
}

//shortcut to average of median and all points in range +-length/4
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::medianAverage() {
	return medianAverage(getCount() / 4);
}

//average of median and -+points at distance
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::medianAverage(uint8_t maxDistanceFromMedian) {
	sortToValues(getCount());
	resultingT retVal = _medianAverage(_tail, getCount(), maxDistanceFromMedian, items, _capacity, getItemValue);
	sortToInsertSequence();
	return retVal;
}

//if items in buffer are type of occurence, of no important value
//then measure average interval (at least 2 items to make any sense)
//intervals are written to .value field, and original .value is lost
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::medianInterval() {

	uint8_t length = getCount();
	if (length < 2)	return T();

	intervalsToValues();		//will not run if already done
	sortToValues(length - 1);	//the last one does not cointain interval

	//check all, but ignore last one, it should be 0!
	T retVal = _median(_tail, length - 1, items, _capacity, getItemValue);

	sortToInsertSequence();
	return retVal;
}

template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::medianAverageInterval(uint8_t maxDistanceFromMedian) {

	uint8_t length = getCount();
	if (length < 2)	return resultingT();

	intervalsToValues();	//will not run if already done
	sortToValues(length - 1);

	//check all, but ignore last one, it should be 0!
	resultingT retVal = _medianAverage(_tail, length - 1, maxDistanceFromMedian, items, _capacity, getItemValue);

	sortToInsertSequence();
	return retVal;
}

template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::medianRateOfChange() {
	if (getCount() < 2)	return resultingT();
	return 1 / (resultingT)medianInterval();
}

template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::medianAverageRateOfChange(uint8_t maxDistanceFromMedian) {
	if (getCount() < 2)	return resultingT();
	return 1 / medianAverageInterval(maxDistanceFromMedian);
}

template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::average() {
	//average does not shuffle order of items
	return _average(_tail, getCount(), items, _capacity, getItemValue);
}

template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::averageInterval() {

	uint8_t length = getCount();
	if (length < 2)	return resultingT();

	intervalsToValues();

	//check all, but ignore last one, it does not cointain interval
	return _average(_tail, length - 1, items, _capacity, getItemValue);
}

template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::averageRateOfChange() {
	if (getCount() < 2)	return resultingT();
	return 1 / averageInterval();
}


//---------------static median and average functions------------------
/*
NOTE: some averaging parts need ABS(x) function; however if <resultinF> is unsigned int, abs will throw
compilation error. So custom absX is provided, since user _may_ want to have median in other values
*/

//helper function to get actual position of item in array, regarding tail (integer overflow calculations)
template<typename T, typename timeT, typename resultingT>
uint8_t qmedianbuffer<T, timeT, resultingT>::getTruePos(uint8_t posSeek, uint8_t tail, uint8_t capacity){
	return (posSeek + tail) % capacity;
}


//standard average function
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::_average(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)){

	/*
	this will average numbers, trying to avoid overflow;
	note that medianaverage function can also do the same job, but this one is left here to make it easier to follow process
	*/
	resultingT avg{};

	for (uint8_t i = 0; i < len; i++){
		resultingT itemValue = (resultingT)getSortValueFunc(arr[getTruePos(i, tail, arrCapacity)]);
#if EXPECT_BIG_NUMBERS
		avg = (itemValue - avg) / (i + 1) + avg; //simple approach to try to avoid overflow with big numbers; use double type if needed more precision
	}
#else		
		avg = avg + itemValue; //use double type, or any bigger type, big enough to hold sum of array, since it will owerflow
	}
	avg = avg / len;
#endif		
	return avg;
}

//mean absolute deviation around average
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::_meanAbsoluteDeviationAroundAverage(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate))
{
	resultingT avg = _average(tail, len, arr, arrCapacity, getSortValueFunc);
	resultingT mada = 0;

	for (uint8_t i = 0; i < len; i++){
		resultingT arrayValue = (resultingT)getSortValueFunc(arr[getTruePos(i, tail, arrCapacity)]);
#if EXPECT_BIG_NUMBERS
		mada = (absX(arrayValue - avg) - mada) / (i + 1) + mada;
	}
#else
		mada = absX(arrayValue - avg) + mada;
	}
	mada = mada / len;
#endif	
	return mada;
}

//pick median in previously sorted array; always original numeric value, no averaging at any time
template<typename T, typename timeT, typename resultingT>
T qmedianbuffer<T, timeT, resultingT>::_median(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)){
	if (len == 0) {
		return T();
	}
	if (len == 1) {
		return getSortValueFunc(arr[getTruePos(0, tail, arrCapacity)]);
	}
	// else, always pick at least one original value
	return getSortValueFunc(arr[getTruePos(len / 2, tail, arrCapacity)]);
}

//pick median, and average with surrounding numbers with max distance of it
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::_medianAverage(uint8_t tail, uint8_t len, uint8_t maxDistanceFromMedian, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)){

	/*
	median is in the middle of sorted array
	start at the -maxdistance, go over median, and finish at +maxdistance
	average numbers and return; if len is even, then median is middle of two middle numbers
	that means that in case of len = 6, distance = 1, evaluated array items are => | 0, 1, (1, 1), 1, 0 |; total = 4
	this function can be used for averaging if needed, where max distance = len/2
	*/

	if (len == 0) {
		return resultingT();
	}
	if (len == 1) {
		return (resultingT)getSortValueFunc(arr[getTruePos(0, tail, arrCapacity)]);
	}

	uint8_t evenNumCorrection = 0;
	if (len % 2 == 0) evenNumCorrection = 1;

	if (maxDistanceFromMedian > len / 2) maxDistanceFromMedian = len / 2;
	uint8_t startpos = len / 2 - maxDistanceFromMedian - evenNumCorrection; //so middle is found, start earlier
	uint8_t total = 1 + 2 * maxDistanceFromMedian + evenNumCorrection; //so middle is found, it is one more

	resultingT avgN{}, mPosition;

	for (uint8_t i = 0; i < total; i++){
		mPosition = (resultingT)getSortValueFunc(arr[getTruePos(startpos + i, tail, arrCapacity)]); //position up
#if EXPECT_BIG_NUMBERS			
		avgN = (mPosition - avgN) / (i + 1) + avgN; //simple approach to try to avoid overflow with big numbers; use double type if needed more precision
	}
#else
		avgN = mPosition + avgN;
	}
	//and finaly
	avgN = avgN / total;
#endif

	return avgN;
}

//pick median, and average with surrounding numbers with max distance of it
template<typename T, typename timeT, typename resultingT>
resultingT qmedianbuffer<T, timeT, resultingT>::_meanAbsoluteDeviationAroundMedianAverage(uint8_t tail, uint8_t len, uint8_t maxDistanceFromMedian, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)){

	/*
	this is actually simple thing - average of numbers around median!
	median is in the middle of sorted array, if len is even, then median is middle of two middle numbers
	start at the -maxdistance, go over median, and finish at +maxdistance
	average numbers and return
	that means that in case of len = 6, distance = 1, evaluated array items are => | 0, 1, (1, 1), 1, 0 |; total = 4
	*/

	if (len < 2) {
		return resultingT();
	}

	//get median (average using same distance) first
	resultingT med = _medianAverage(tail, len, maxDistanceFromMedian, arr, arrCapacity, getSortValueFunc); //max dist must be 0 to get real median

	//then average all around it at max distance
	uint8_t evenNumCorrection = 0;
	if (len % 2 == 0) evenNumCorrection = 1;

	if (maxDistanceFromMedian > len / 2) maxDistanceFromMedian = len / 2;
	uint8_t startpos = len / 2 - maxDistanceFromMedian - evenNumCorrection; //so middle is found, start earlier
	uint8_t total = 1 + 2 * maxDistanceFromMedian + evenNumCorrection; //so middle is found, it is one more

	resultingT avgMAD{};

	for (uint8_t i = 0; i < total; i++){
		resultingT mPosition = (resultingT)getSortValueFunc(arr[getTruePos(startpos + i, tail, arrCapacity)]); //position up
#if EXPECT_BIG_NUMBERS //simple approach to try to avoid overflow with big numbers; use double type if needed more precision
		avgMAD = (absX(mPosition - med) - avgMAD) / (i + 1) + avgMAD;
	}
#else
		avgMAD = absX(mPosition - med) + avgMAD;
	}
	//and finaly
	avgMAD = avgMAD / total;
#endif

	return avgMAD;
}


//---------------static select and sort functions------------------

//standard insertionSort algorithm, done in one pass
template<typename T, typename timeT, typename resultingT>
void qmedianbuffer<T, timeT, resultingT>::sort(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)) {

	int j; //needs to be signed since in while loop, it will become -1 to exit while
	itemQ tmp;
	for (uint8_t i = 1; i < len; i++)
	{
		tmp = arr[getTruePos(i, tail, arrCapacity)];
		j = i - 1;

		while (j >= 0 && getSortValueFunc(arr[getTruePos(j, tail, arrCapacity)]) > getSortValueFunc(tmp))
		{
			arr[getTruePos(j + 1, tail, arrCapacity)] = arr[getTruePos(j, tail, arrCapacity)];
			j = j - 1;
		}
		arr[getTruePos(j + 1, tail, arrCapacity)] = tmp;
	}
}
#endif
