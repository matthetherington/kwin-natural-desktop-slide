// SPDX-FileCopyrightText: 2026 Matt Hetherington
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <type_traits>

namespace NaturalSwipe
{
template<typename Member> struct MemberReturn;
template<typename Result, typename Owner, typename... Args>
struct MemberReturn<Result (Owner::*)(Args...)> { using Type = Result; };
template<typename Member> using MemberResult = typename MemberReturn<Member>::Type;

// KWin 6.7 painting has no failure result; 6.8 must propagate failures.
template<typename Draw> bool paintSucceeded(Draw draw)
{
    if constexpr (std::is_void_v<std::invoke_result_t<Draw>>) {
        draw();
        return true;
    } else {
        return draw();
    }
}
template<typename Result, typename Draw> Result paintResult(Draw draw)
{
    if constexpr (std::is_void_v<Result>) {
        draw();
    } else {
        return draw();
    }
}
template<typename Event> bool isAutoscroll(const Event &event)
{
    if constexpr (requires { event.autoscroll; }) return event.autoscroll;
    else return false; // Autoscroll events were introduced in KWin 6.8.
}
template<typename Result> bool outputConfigurationSucceeded(const Result &result)
{
    if constexpr (requires { result.has_value(); }) return result.has_value();
    else return result == Result::None;
}
}
