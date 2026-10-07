#pragma once

#include <utility>

namespace lxe::util {

/// Runs a callable when the scope ends; for C APIs with paired init/shutdown calls.
template <class F>
class [[nodiscard]] ScopeExit
{
public:
    explicit ScopeExit(F on_exit) noexcept
    : on_exit_{std::move(on_exit)}
    {}

    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit(ScopeExit&&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;

    ~ScopeExit() { on_exit_(); }

private:
    F on_exit_;
};

} // namespace lxe::util
