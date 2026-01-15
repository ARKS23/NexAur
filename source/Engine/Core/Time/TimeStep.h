//
// Created by ASUS on 2026/1/14.
//

#pragma once

namespace NexAur {
    class TimeStep {
    public:
        explicit TimeStep(const double seconds = 0.0f) : m_seconds(seconds) {}

    public:
        double GetSeconds() const { return m_seconds; }
        double GetMilliseconds() const { return m_seconds * 1000.0f; }
        operator double() const { return m_seconds; }
        operator float() const { return static_cast<float>(m_seconds); } // 隐式类型转换,大量三方库只支持到float
        void reset() { m_seconds = 0.0f; }

        // 标量运算 + 数学运算
        TimeStep operator*(double scale) const { return TimeStep(m_seconds * scale); }
        TimeStep operator/(double scale) const { return TimeStep(m_seconds / scale); }
        TimeStep operator+(const TimeStep &other) const { return TimeStep(m_seconds + other.m_seconds); }
        TimeStep operator-(const TimeStep &other) const { return TimeStep(m_seconds - other.m_seconds); }

        // 自身运算
        TimeStep& operator+=(const TimeStep &other) { m_seconds += other.m_seconds; return *this; }
        TimeStep& operator-=(const TimeStep &other) { m_seconds -= other.m_seconds; return *this; }

        // 比较运算
        bool operator>(const TimeStep &other) const { return m_seconds > other.m_seconds; }
        bool operator<(const TimeStep &other) const { return m_seconds < other.m_seconds; }
        bool operator>=(const TimeStep &other) const { return m_seconds >= other.m_seconds; }
        bool operator<=(const TimeStep &other) const { return m_seconds <= other.m_seconds; }

    private:
        double m_seconds ;  // 始终存储秒
    };
} // namespace NexAur