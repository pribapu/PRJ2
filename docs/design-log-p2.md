# Design Log — Project 2

## Growth Factor and Amortized Cost

The `Conversation` class doubles its capacity whenever it runs out of space.

For example, the capacity grows like this:

`0 → 1 → 2 → 4 → 8 → 16 → ...`

This means the array does not need to reallocate every time a new `Message` is added. Most calls to `append()` only place the new message into the next available spot.

When the array does need to grow, the existing messages have to be copied into a larger array. Even though some individual appends take longer because of this, the reallocations do not happen very often.

Across many appends, the total amount of copying stays proportional to the number of messages added. Because of this, the average, or amortized, cost of `append()` is `O(1)`.

I also tested the growth behavior by checking when the address returned by `begin()` changed.

## Rule of Five

`Conversation` manages its own dynamically allocated `Message` array, so it needs to correctly handle copying, moving, and cleanup.

The destructor uses:

```cpp
delete[] data_;
to free the allocated array.

The copy constructor creates a new array and copies the messages from the original object. This makes the copy independent from the original.

The copy assignment operator also creates an independent copy before replacing the current data.

The move constructor and move assignment operator transfer ownership of the array instead of copying every message. After the move, the original object's pointer, size, and capacity are reset so that it is still safe to destroy or reuse.
I tested the copy operations by making sure the copied object had a different array address from the original. I also tested the move operations by checking that the new object received the original array address.


##Sentinel Scanner Memory
The SentinelScanner keeps part of the incoming text in pending_ so it can detect a sentinel that is split across multiple chunks.
For example, if part of:
<|end_conversation|>
appears at the end of one chunk and the rest appears in the next chunk, the scanner still needs to recognize it.
The scanner only needs to keep at most:
sentinel_.size() - 1
characters between calls.
If the sentinel is found, pending_ is cleared.
If the sentinel is not found, the scanner returns the text that is definitely safe and keeps only the small ending section that could still become part of the sentinel.
Because of this, the amount of stored text does not continue growing as more input is processed. Its retained memory depends only on the sentinel length, not on the total amount of text in the stream.
I also tested this by feeding input one character at a time and checking that the amount of buffered text stayed within the expected limit.


##What I Would Change

The scanner currently uses:
pending_.find(sentinel_);
to search for the sentinel.
This is simple and works well for this project because the sentinel is short and pending_ stays small.
If this were being used with much larger strings or a very large amount of streaming data, I could use a more efficient search algorithm such as Knuth-Morris-Pratt, or KMP.
KMP keeps track of how much of the pattern has already matched, so it avoids checking the same characters multiple times.
For this assignment, the current approach is simpler and is enough for the required behavior.
```