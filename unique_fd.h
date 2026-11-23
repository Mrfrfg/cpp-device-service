#pragma once

#include <unistd.h>

class UniqueFd
{
public:
    explicit UniqueFd(int fd = -1) noexcept
        : fd_(fd)
    {
    }

    ~UniqueFd()
    {
        reset();
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept
        : fd_(other.release())
    {
    }

    UniqueFd& operator=(UniqueFd&& other) noexcept
    {
        if (this != &other) {
            reset(other.release());
        }

        return *this;
    }

    int get() const noexcept
    {
        return fd_;
    }

    explicit operator bool() const noexcept
    {
        return fd_ != -1;
    }

    int release() noexcept
    {
        const int old_fd = fd_;
        fd_ = -1;
        return old_fd;
    }

    void reset(int new_fd = -1) noexcept
    {
        if (fd_ != -1) {
            close(fd_);
        }

        fd_ = new_fd;
    }

private:
    int fd_;
};
