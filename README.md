# ECE 309 — Project 2: The Conversation Loop

`miniharness` is a small C++ program that manages a conversation between a user and a model.

It stores the conversation history, reads the model response in chunks, checks for the stop sentinel `<|end_conversation|>`, and ends the conversation when:

- the stop sentinel is found
- the maximum number of turns is reached
- the user presses Ctrl-D

Example:

```text
$ ./build/miniharness --script scripts/greeting.script
you> hello
assistant> I am doing well, thank you! How can I help you?
you> how are you
assistant> I can definitely do that for you. Anything else?
you> bye
assistant> Goodbye!
[conversation ended: stop sentinel after 3 turns]


##What I Implemented
Message
Files:
- include/core/message.h
Message stores:
- the role of the message
- the message text
The possible roles are:
- System
- User
- Assistant
Conversation
Files:
- include/core/conversation.h
- src/conversation.cpp
Conversation stores a dynamic array of Message objects.
Since std::vector is not allowed for this part of the project, the class manages its own array using dynamic memory.
When the array becomes full, its capacity doubles:
0 → 1 → 2 → 4 → 8 → 16 → ...
This keeps the average cost of append() at O(1).
The class also implements the Rule of Five:
- destructor
- copy constructor
- copy assignment operator
- move constructor
- move assignment operator
Copies create their own independent array, while moves transfer ownership of the existing array.
More details are explained in:
docs/design-log-p2.md
SentinelScanner
Files:
- include/core/sentinel_scanner.h
- src/sentinel_scanner.cpp
SentinelScanner checks the model output for:
<|end_conversation|>
The sentinel may be split between multiple chunks, so the scanner keeps a small amount of text between calls to feed().
The scanner makes sure the sentinel itself is never printed to the user.
It only keeps at most:
sentinel.size() - 1
characters between calls, so the stored data stays small even when processing a long stream.
Tests
File:
- tests/p2/test_p2.cpp
The tests check the main parts of the project, including:
- empty conversation behavior
- bounds checking
- copy behavior
- move behavior
- array growth
- sentinel detection
- sentinel detection across chunk boundaries
- turn limits
- stopping on the sentinel
- transcript replay


##Provided Starter Code
The following parts were provided and were not part of my main implementation:
- include/model/
- include/harness/
- src/model_client.cpp
- src/scripted_client.cpp
- src/replay_client.cpp
- src/harness.cpp
- src/main.cpp
These files provide the model clients, conversation harness, and command-line interface.
Build

##To build the project:
cmake -S . -B build
cmake --build build
This creates:
./build/miniharness
./build/test_p2
Run the Tests
./build/test_p2
Run the Program
./build/miniharness --script scripts/greeting.script
To save the conversation:
./build/miniharness --script scripts/greeting.script --save transcript.txt
The user can also press Ctrl-D to end the conversation early.
Command-Line Options
--script <path>
Chooses the script file used by ScriptedModelClient.
Example:
./build/miniharness --script scripts/greeting.script
--save <path>
Saves the conversation transcript to a file.
Example:
./build/miniharness --save transcript.txt
--max-turns <N>
Sets the maximum number of conversation turns.
Example:
./build/miniharness --max-turns 10
The default maximum is 20 turns.


##Testing
The project is compiled with warning and sanitizer options including:
-Wall
-Wextra
-Wpedantic
-fsanitize=address,undefined
The test suite passes without reported memory leaks or sanitizer errors.
```