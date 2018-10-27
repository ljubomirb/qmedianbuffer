/* Small circular queue buffer with median. Ljubomir Blanusa, 2018
   Buffer returns:
   -1 item, same value
   -2 items, average of 2
   -3 items, average of 3
   ->4 and more (even), median, and then average of 2 middle items
   ->5 and more (uneven), median, and then average of 3 middle items

   Important: it is possible to add infinitely to buffer, however -
   - it is a good idea to reset it to have fresh values at hand!

   Implementation:
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

   DEFINES and VALUES:
   -T type of value used - integer or double or whatever
   -unsignedT type of time value - unsigned uint8_t, uint16_t, uint32_t
   
	NOTE: each data entry is size T + unsignedT + 1 byte
   -max length 255,
   */

#ifndef qmedianbuffer_h
#define qmedianbuffer_h
#if defined(ARDUINO)
#include "Arduino.h"
#else
#include <stdint.h>
#endif


//T is type of numeric data stored, unsignedT is any UNSIGNED numeric type, used for time calculations
template<typename T, typename unsignedT> //njak
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

	bool deleteOlderThanInterval(unsignedT currentTimeStamp, unsignedT interval);

	void clear();
	T median();
	T medianAverage();	

	bool isFull();
	bool isEmpty();
	uint8_t getCount();

private:
	//template<typename unsignedT>
	struct itemQ {
		uint8_t insertOrder{};
		T value{};
		unsignedT time{};	
	};

	static uint8_t getTruePos(uint8_t pos, uint8_t len, uint8_t capacity);
	static T _median(uint8_t head, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static T _medianAverage(uint8_t head, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static T quick_select(uint8_t k, uint8_t head, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));
	static void sort(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate));

	//itemQ items[_capacity]; //={1,200,2,6,9};
	itemQ* items;
	uint8_t _capacity{};
	uint8_t _head{};
	uint8_t _tail{};
	bool _isFull{};

	static T getItemValue(const itemQ &item);
	static T getItemInsertOrder(const itemQ &item);

	bool isInInsertSequence = true;	//items are in the same order as when being put
	void renumerateOldestToZeroIfNeeded();	//oldest item will have internal counter set to zero, others will increment
	void sortToInsertSequenceIfNeeded();
};

//------------------pop push peek-----------------

template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::push(T number, unsignedT currentTime) {

	sortToInsertSequenceIfNeeded();

	itemQ newitem;
	newitem.value = number;
	newitem.time = currentTime;
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

	sortToInsertSequenceIfNeeded();

	if (isEmpty()){
		return T(); //play stupid, do not throw error
	}
	itemQ item = items[_tail];
	_isFull = false; //it will for sure not be full, do not test
	_tail = (_tail + 1) % _capacity;
	return item.value;
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::peek() {
	sortToInsertSequenceIfNeeded();

	if (isEmpty()){
		return T(); //play stupid, do not throw error
	}
	itemQ item = items[_tail];
	return item.value;
}

template<typename T, typename unsignedT>
unsignedT qmedianbuffer<T, unsignedT>::peekTime() {

	sortToInsertSequenceIfNeeded();

	if (isEmpty()){
		return T(); //play stupid, do not throw error
	}
	itemQ item = items[_tail];
	return item.time; //todo: maybe rewrite two functions to 2+1 that returns item
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
void qmedianbuffer<T,unsignedT>::clear() {
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

//----------------shuffle to order-------------

//call this to prepare array to sort it back to the original order later
template<typename T, typename unsignedT>
void qmedianbuffer<T, unsignedT>::renumerateOldestToZeroIfNeeded() {
	if (isInInsertSequence){
		for (uint8_t i = 0; i < getCount(); i++){
			itemQ &item = items[getTruePos(i, _tail, _capacity)];
			item.insertOrder = i;
		}
		isInInsertSequence = false;
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


//------------------median prepare-------------

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::medianAverage() {
	//so we can sort it back to original later	
	//will remain in reshuffled state untill next put/peek/pop
	renumerateOldestToZeroIfNeeded();
	return _medianAverage(_tail, getCount(), items, _capacity, getItemValue);
}
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::median() {
	//so we can sort it back to original later	
	//will remain in reshuffled state untill next put/peek/pop
	renumerateOldestToZeroIfNeeded();
	return _median(_tail, getCount(), items, _capacity, getItemValue);
}

//---------------static median functions------------------

//helper function to get actual position of item in array, regarding tail (integer overflow calculations)
template<typename T, typename unsignedT>
uint8_t qmedianbuffer<T, unsignedT>::getTruePos(uint8_t posSeek, uint8_t tail, uint8_t capacity){
	uint8_t truePos = (posSeek + tail) % capacity;
	return truePos;
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::_median(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)) {
	if (len == 0) {
		return 0;
	}
	if (len == 1) {
		return arr[getTruePos(0, tail, arrCapacity)].value;
	}
	uint8_t pos = len / 2;
	return quick_select(pos, tail, len, arr, arrCapacity, getSortValueFunc);
}

template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::_medianAverage(uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)) {
	if (len == 0) {
		return 0;
	}
	if (len == 1) {
		return arr[getTruePos(0, tail, arrCapacity)].value;
	}
	uint8_t pos = len / 2;
	T median;
	if (len % 2 == 0) // Even number of elements, median is average of middle two
	{
		T medi = quick_select(pos, tail, len, arr, arrCapacity, getSortValueFunc);
		T mediminus = quick_select(pos - 1, tail, len, arr, arrCapacity, getSortValueFunc);
		T sum = medi + mediminus;
		median = sum / 2;
	}
	else // select middle element, -1, +1, and average of that
	{
		//NOTE: quick select will exit without rearanging all if it finds its goal
		T medi = quick_select(pos, tail, len, arr, arrCapacity, getSortValueFunc);
		T mediminus = quick_select(pos - 1, tail, len, arr, arrCapacity, getSortValueFunc);
		T mediplus = quick_select(pos + 1, tail, len, arr, arrCapacity, getSortValueFunc);
		T sum = medi + mediminus + mediplus; //to be cast to double, at least one operator must be double
		median = sum / 3;
	}
	return median;
}

//---------------static median and sort functions------------------

//Standard quick select, returns the k-th smallest item in array arr of length len
//that is - result is value of position in rearragned array (for k=2, len=5; =>3)
template<typename T, typename unsignedT>
T qmedianbuffer<T, unsignedT>::quick_select(uint8_t k, uint8_t tail, uint8_t len, itemQ *arr, uint8_t arrCapacity, T(*getSortValueFunc)(const itemQ &objToEvaluate)) {
	uint8_t left = 0, right = len - 1;
	uint8_t pos, i;
	itemQ pivot;
	itemQ temp;
	while (left < right) {
		pivot = arr[getTruePos(k, tail, arrCapacity)];
		//swap
		temp = arr[getTruePos(k, tail, arrCapacity)];
		arr[getTruePos(k, tail, arrCapacity)] = arr[getTruePos(right, tail, arrCapacity)], arr[getTruePos(right, tail, arrCapacity)] = temp;
		for (i = pos = left; i < right; i++) {
			if (getSortValueFunc(arr[getTruePos(i, tail, arrCapacity)]) < getSortValueFunc(pivot))
			{
				//swap
				temp = arr[getTruePos(i, tail, arrCapacity)];
				arr[getTruePos(i, tail, arrCapacity)] = arr[getTruePos(pos, tail, arrCapacity)], arr[getTruePos(pos, tail, arrCapacity)] = temp;
				pos++;
			}
		}
		//swap
		temp = arr[getTruePos(right, tail, arrCapacity)];
		arr[getTruePos(right, tail, arrCapacity)] = arr[getTruePos(pos, tail, arrCapacity)], arr[getTruePos(pos, tail, arrCapacity)] = temp;
		if (pos == k) break;
		if (pos < k) left = pos + 1;
		else right = pos - 1;
	}
	return getSortValueFunc(arr[getTruePos(k, tail, arrCapacity)]);
	//tested, using inline swap is 5 time slower
	//auto swap = [](itemQ a, itemQ b){itemQ temp = a; a = b; b = temp; };
}

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
