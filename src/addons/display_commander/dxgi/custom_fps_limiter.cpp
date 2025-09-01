#include "custom_fps_limiter.hpp"
#include "../addon.hpp"
#include <algorithm>
#include "../../utils/timing.hpp"

namespace dxgi::fps_limiter {
    HANDLE m_timer_handle = nullptr;

CustomFpsLimiter::CustomFpsLimiter()
    : last_time_point_ns(0)
{
}

void CustomFpsLimiter::LimitFrameRate(double fps)
{
    LONGLONG wait_target_ns = last_time_point_ns + (utils::SEC_TO_NS / fps);
    utils::wait_until_ns(wait_target_ns, m_timer_handle);
    LONGLONG end_time_ns = utils::get_now_ns();
    late_amount_ns = end_time_ns - wait_target_ns;
    last_time_point_ns = end_time_ns;
}
} // namespace dxgi::fps_limiter
