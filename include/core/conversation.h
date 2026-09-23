// include/core/conversation.h
#pragma once
#include "core/message.h"
#include <cstddef>

// A growable array of Message
class Conversation {
public:
    // Empty conversation: size() == 0, no allocation yet.
    Conversation();

    // Releases all owned Message storage. No effect if already empty
    ~Conversation();

    // Deep copy: allocates its own buffer and copies every Message.
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Steals other's buffer — no per-element copying. Afterward, other
    // must be left valid and empty (safe to destroy or reassign).
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Appends m, growing the backing array if needed.
    void append(Message m);

    // Number of messages currently stored.
    std::size_t size() const noexcept;

    // Bounds-checked access. Throws std::out_of_range if i >= size().
    const Message& at(std::size_t i) const;

    // Range-for iteration, oldest message first. begin() == end() when
    // size() == 0.
    const Message* begin() const noexcept;
    const Message* end()   const noexcept;

private:
    void grow(std::size_t new_capacity);
    static void swap(Conversation& a, Conversation& b) noexcept;

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};
