// src/conversation.cpp
#include "core/conversation.h"
#include <stdexcept>
#include <utility>

Conversation::Conversation() = default;

Conversation::~Conversation() { delete[] data_; }

Conversation::Conversation(const Conversation& other)
    : data_(other.size_ ? new Message[other.size_] : nullptr),
      size_(other.size_),
      capacity_(other.size_) {
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

void Conversation::swap(Conversation& a, Conversation& b) noexcept {
    std::swap(a.data_, b.data_);
    std::swap(a.size_, b.size_);
    std::swap(a.capacity_, b.capacity_);
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this != &other) {
        Conversation tmp(other);
        swap(*this, tmp);
    }
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void Conversation::grow(std::size_t new_capacity) {
    Message* new_data = new Message[new_capacity];
    for (std::size_t i = 0; i < size_; ++i) {
        new_data[i] = std::move(data_[i]);
    }
    delete[] data_;
    data_ = new_data;
    capacity_ = new_capacity;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        std::size_t new_capacity = (capacity_ == 0) ? 1 : capacity_ * 2;
        grow(new_capacity);
    }
    data_[size_++] = std::move(m);
}

std::size_t Conversation::size() const noexcept { return size_; }

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index out of range");
    }
    return data_[i];
}

const Message* Conversation::begin() const noexcept { return data_; }
const Message* Conversation::end()   const noexcept { return data_ + size_; }
