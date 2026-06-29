#pragma once

#include <string>
#include <string_view>
#include <vector>

class Path {
    std::string storage_;
    char delimiter_;
    std::vector<std::string_view> parts_;

    void build_parts() {
        parts_.clear();
        std::string_view sv{storage_};
        size_t pos = 0;
        while (true) {
            size_t next = sv.find(delimiter_, pos);
            if (next == std::string_view::npos) {
                parts_.push_back(sv.substr(pos));
                break;
            }
            parts_.push_back(sv.substr(pos, next - pos));
            pos = next + 1;
        }
    }

public:
    Path(std::string path, char delimiter)
        : storage_(std::move(path)), delimiter_(delimiter) {
        build_parts();
    }

    Path(const Path& other)
        : storage_(other.storage_), delimiter_(other.delimiter_) {
        build_parts();
    }

    Path& operator=(const Path& other) {
        if (this != &other) {
            storage_ = other.storage_;
            delimiter_ = other.delimiter_;
            build_parts();
        }
        return *this;
    }

    // Must rebuild parts after move since string_views would point into the
    // moved-from string's (now-invalid) buffer.
    Path(Path&& other) noexcept
        : storage_(std::move(other.storage_)), delimiter_(other.delimiter_) {
        build_parts();
    }

    Path& operator=(Path&& other) noexcept {
        if (this != &other) {
            storage_ = std::move(other.storage_);
            delimiter_ = other.delimiter_;
            build_parts();
        }
        return *this;
    }

    const std::string& str() const { return storage_; }
    std::string_view sv() const { return storage_; }
    char delimiter() const { return delimiter_; }
    const std::vector<std::string_view>& parts() const { return parts_; }
};
