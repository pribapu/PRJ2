// tests/p2/test_p2.cpp
//


#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------
// Small helpers used by several tests below.
// ---------------------------------------------------------------------

// Writes a small script/transcript file so a test can hand it to
// ScriptedModelClient or ReplayModelClient.
void write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

const char* role_name(Role role) {
    if (role == Role::System) return "system";
    if (role == Role::User) return "user";
    return "assistant";
}

// Writes a Conversation to disk in the Appendix A transcript format.
void save_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path);
    bool first = true;
    for (const Message* m = conv.begin(); m != conv.end(); ++m) {
        if (!first) file << "---\n";
        first = false;
        file << "role: " << role_name(m->role()) << "\n";
        file << m->content() << "\n";
    }
}

// A simple InputSource that just returns a fixed list of lines, then EOF.
class FixedInput : public InputSource {
public:
    explicit FixedInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}

    std::string read_line() override {
        if (idx_ >= lines_.size()) {
            eof_ = true;
            return "";
        }
        return lines_[idx_++];
    }
    bool is_eof() const override { return eof_; }

private:
    std::vector<std::string> lines_;
    std::size_t idx_ = 0;
    bool eof_ = false;
};

// An OutputSink that just remembers everything written to it, so a test
// can check what would have been printed to the user.
class CapturingOutput : public OutputSink {
public:
    std::string captured;
    void write(std::string_view text) override { captured += text; }
};

// ---------------------------------------------------------------------
// Conversation tests
// ---------------------------------------------------------------------

void test_empty_conversation_bounds() {
    Conversation conv;
    assert(conv.size() == 0);
    assert(conv.begin() == conv.end());

    bool threw = false;
    try {
        conv.at(0);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw);
}

void test_system_message_stays_first() {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "hi there"));
    conv.append(Message(Role::User, "bye"));

    assert(conv.at(0).role() == Role::System);
    assert(conv.at(0).content() == "Be concise.");
    for (std::size_t i = 1; i < conv.size(); ++i) {
        assert(conv.at(i).role() != Role::System);
    }
}

void test_copy_is_deep() {
    Conversation original;
    original.append(Message(Role::User, "hi"));
    original.append(Message(Role::Assistant, "hello"));

    // Copy constructor: different buffer, same contents.
    Conversation copy(original);
    assert(copy.begin() != original.begin());
    assert(copy.size() == original.size());
    for (std::size_t i = 0; i < original.size(); ++i) {
        assert(copy.at(i).content() == original.at(i).content());
    }

    // Growing the copy must not affect the original.
    copy.append(Message(Role::User, "extra"));
    assert(copy.size() == 3);
    assert(original.size() == 2);

    // Copy assignment: also a different buffer.
    Conversation assigned;
    assigned = original;
    assert(assigned.begin() != original.begin());
    assert(assigned.size() == original.size());
}

void test_move_constructor_steals_pointer() {
    Conversation original;
    original.append(Message(Role::User, "hi"));
    original.append(Message(Role::Assistant, "hello"));
    const Message* original_ptr = original.begin();

    Conversation moved(std::move(original));
    assert(moved.begin() == original_ptr);
    assert(moved.size() == 2);

    // The moved-from object must be left empty and safe to reuse.
    assert(original.size() == 0);
    original.append(Message(Role::User, "reuse"));
    assert(original.size() == 1);
}

void test_move_assignment_steals_pointer() {
    Conversation a;
    a.append(Message(Role::User, "x"));
    const Message* a_ptr = a.begin();

    Conversation b;
    b.append(Message(Role::Assistant, "y"));
    b.append(Message(Role::Assistant, "z"));

    b = std::move(a);
    assert(b.begin() == a_ptr);
    assert(b.size() == 1);
    assert(b.at(0).content() == "x");
    assert(a.size() == 0);
}

void test_growth_and_reallocation() {
    // Capacity doubles: 0 -> 1 -> 2 -> 4 -> 8 ... so the backing buffer
    // should be reallocated (begin() changes) after the 1st, 2nd, 3rd,
    // and 5th appends, but NOT after the 4th.
    Conversation conv;

    conv.append(Message(Role::User, "a"));
    const Message* p1 = conv.begin();

    conv.append(Message(Role::User, "b"));
    const Message* p2 = conv.begin();
    assert(p2 != p1);

    conv.append(Message(Role::User, "c"));
    const Message* p3 = conv.begin();
    assert(p3 != p2);

    conv.append(Message(Role::User, "d"));
    const Message* p4 = conv.begin();
    assert(p4 == p3);  // capacity was 4, size went from 3 to 4: no growth yet

    conv.append(Message(Role::User, "e"));
    const Message* p5 = conv.begin();
    assert(p5 != p4);  // size==capacity (4==4): this append grows to 8

    // size() and at() should stay correct after many more appends/reallocations.
    for (int i = 0; i < 100; ++i) {
        conv.append(Message(Role::User, "extra" + std::to_string(i)));
    }
    assert(conv.size() == 105);
    assert(conv.at(0).content() == "a");
    assert(conv.at(4).content() == "e");
    assert(conv.at(104).content() == "extra99");
}

// ---------------------------------------------------------------------
// SentinelScanner tests
// ---------------------------------------------------------------------

void test_scanner_clean_text() {
    SentinelScanner scanner("<|end_conversation|>");
    const std::string text = "Hello, world! No sentinel here.";

    auto fed = scanner.feed(text);
    assert(!fed.sentinel_found);

    auto flushed = scanner.flush();
    assert(!flushed.sentinel_found);

    // Nothing is ever thrown away when there's no sentinel.
    assert(fed.safe_text + flushed.safe_text == text);
}

// This is the sample test from the spec: catch the sentinel no matter
// where the chunk boundary falls.
void test_scanner_catches_sentinel_at_every_boundary() {
    const std::string sentinel = "<|end_conversation|>";
    const std::string text = "Goodbye." + sentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(sentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert(out1.sentinel_found || out2.sentinel_found);
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

void test_scanner_no_false_alarm() {
    SentinelScanner scanner("<|end_conversation|>");
    // Shares the "<|end_" prefix with the real sentinel, then diverges.
    const std::string text = "Well, <|end_world|> not that one.";

    std::string safe_so_far;
    bool found = false;
    for (char c : text) {
        auto out = scanner.feed(std::string_view(&c, 1));
        safe_so_far += out.safe_text;
        if (out.sentinel_found) found = true;
    }
    auto flushed = scanner.flush();
    safe_so_far += flushed.safe_text;

    assert(!found);
    assert(safe_so_far == text);
}

// Feeds an adversarial stream one byte at a time and checks that the
// scanner never holds back more than sentinel.size() - 1 bytes. We can't

void test_scanner_bounded_memory() {
    const std::string sentinel = "<|end_conversation|>";
    const std::size_t max_held_back = sentinel.size() - 1;
    SentinelScanner scanner(sentinel);

    const std::string unit = "<|end_";  // never completes the real sentinel
    std::size_t total_fed = 0;
    std::size_t total_safe = 0;

    for (int rep = 0; rep < 2000; ++rep) {
        for (char c : unit) {
            auto out = scanner.feed(std::string_view(&c, 1));
            assert(!out.sentinel_found);
            total_fed += 1;
            total_safe += out.safe_text.size();
            assert(total_fed - total_safe <= max_held_back);
        }
    }

    auto flushed = scanner.flush();
    total_safe += flushed.safe_text.size();
    assert(total_fed == total_safe);
}

// ---------------------------------------------------------------------
// Harness / ModelClient tests
// ---------------------------------------------------------------------

void test_harness_turn_limit() {
    write_file("tmp_turn_limit.script",
        "role: assistant\n"
        "Hi there.\n"
        "---\n"
        "role: assistant\n"
        "Still going.\n");

    auto model = std::make_unique<ScriptedModelClient>("tmp_turn_limit.script");
    HarnessConfig cfg;
    cfg.max_turns = 1;
    Harness harness(std::move(model), cfg);

    FixedInput in({"hello", "hello"});
    CapturingOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::TurnLimit);
    std::remove("tmp_turn_limit.script");
}

void test_harness_sentinel_halt() {
    write_file("tmp_sentinel.script",
        "chunk: 5\n"
        "role: assistant\n"
        "Goodbye.<|end_conversation|>\n");

    auto model = std::make_unique<ScriptedModelClient>("tmp_sentinel.script");
    HarnessConfig cfg;
    cfg.max_turns = 5;
    Harness harness(std::move(model), cfg);

    FixedInput in({"bye"});
    CapturingOutput out;
    StopReason reason = harness.run(in, out);

    assert(reason.kind == StopReason::Kind::Sentinel);

    // The sentinel must never reach the printed output...
    assert(out.captured.find("<|end_conversation|>") == std::string::npos);
    assert(out.captured.find("Goodbye.") != std::string::npos);

    // ...but it must still be stored in the Conversation, so a saved
    // transcript replays the same stop.
    const Conversation& conv = harness.conversation();
    assert(conv.at(conv.size() - 1).content().find("<|end_conversation|>") !=
           std::string::npos);

    std::remove("tmp_sentinel.script");
}

void test_transcript_round_trip() {
    Conversation conv;
    conv.append(Message(Role::System, "Be concise."));
    conv.append(Message(Role::User, "hello"));
    conv.append(Message(Role::Assistant, "Hi! What can I do for you today?"));
    conv.append(Message(Role::User, "bye"));
    conv.append(Message(Role::Assistant, "Goodbye.<|end_conversation|>"));

    save_transcript(conv, "tmp_transcript.txt");

    ReplayModelClient replay("tmp_transcript.txt");
    assert(replay.system_message() == "Be concise.");

    Conversation unused_ctx;  // ReplayModelClient ignores its conv argument.
    Message reply1 = replay.generate(unused_ctx);
    assert(reply1.content() == "Hi! What can I do for you today?");

    Message reply2 = replay.generate(unused_ctx);
    assert(reply2.content() == "Goodbye.<|end_conversation|>");

    std::remove("tmp_transcript.txt");
}

// ---------------------------------------------------------------------

int main() {
    test_empty_conversation_bounds();
    std::cout << "test_empty_conversation_bounds passed\n";

    test_system_message_stays_first();
    std::cout << "test_system_message_stays_first passed\n";

    test_copy_is_deep();
    std::cout << "test_copy_is_deep passed\n";

    test_move_constructor_steals_pointer();
    std::cout << "test_move_constructor_steals_pointer passed\n";

    test_move_assignment_steals_pointer();
    std::cout << "test_move_assignment_steals_pointer passed\n";

    test_growth_and_reallocation();
    std::cout << "test_growth_and_reallocation passed\n";

    test_scanner_clean_text();
    std::cout << "test_scanner_clean_text passed\n";

    test_scanner_catches_sentinel_at_every_boundary();
    std::cout << "test_scanner_catches_sentinel_at_every_boundary passed\n";

    test_scanner_no_false_alarm();
    std::cout << "test_scanner_no_false_alarm passed\n";

    test_scanner_bounded_memory();
    std::cout << "test_scanner_bounded_memory passed\n";

    test_harness_turn_limit();
    std::cout << "test_harness_turn_limit passed\n";

    test_harness_sentinel_halt();
    std::cout << "test_harness_sentinel_halt passed\n";

    test_transcript_round_trip();
    std::cout << "test_transcript_round_trip passed\n";

    std::cout << "All tests passed!\n";
    return 0;
}
