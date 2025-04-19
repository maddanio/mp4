#pragma once

#include <cstdint>
#include <type_traits>
#include <cstdlib>

// euclid's algorithm
template<typename count_t>
constexpr count_t gcd(count_t a, count_t b)
{
    while (b != 0)
    {
        auto h = a % b;
        a = b;
        b = h;
    }
    return a;
}

constexpr uint32_t common_base(uint32_t base_v, uint32_t other_base_v)
{
    auto d = gcd(base_v, other_base_v);
    return base_v * (other_base_v / d);
}

template<typename count_t> count_t change_timebase(
    count_t count,
    uint32_t from_timebase,
    uint32_t to_timebase
)
{
    auto common = gcd(from_timebase, to_timebase);
    from_timebase /= common;
    to_timebase /= common;
    return (count * to_timebase) / from_timebase;
}

template<uint32_t base_v, typename count_t = int64_t>
class fixed_point_t
{
public:
    typedef fixed_point_t<base_v, count_t> this_t;

    explicit fixed_point_t(float value)
    : _count{static_cast<count_t>(value * base_v)}
    {
    }

    template<uint32_t other_base_v, typename other_count_t>
    explicit fixed_point_t(fixed_point_t<other_base_v, other_count_t> other)
    : _count(change_timebase(other.count(), other_base_v, base_v))
    {
    }

    static this_t with_count(count_t count)
    {
        this_t result;
        result._count = count;
        return result;
    }

    static this_t from_timebase(count_t count, uint32_t from_timebase)
    {
        return with_count(change_timebase(count, from_timebase, base_v));
    }

    static constexpr uint32_t base()
    {
        return base_v;
    }

    count_t in_timebase(uint32_t to_timebase) const
    {
        return change_timebase(count(), base_v, to_timebase);
    }

    fixed_point_t()
    : _count{0}
    {
    }

    void set_count(count_t new_count)
    {
        _count = new_count;
    }

    explicit operator float() const
    {
        return to_float();
    }

    float to_float() const
    {
        return float(_count) / base_v;
    }

    template<uint32_t other_base_v>
    this_t& operator=(const fixed_point_t<other_base_v>& other)
    {
        _count = (other.count() * base_v) / other_base_v;
        return *this;
    }

    this_t& operator+=(const this_t& other)
    {
        _count += other._count;
        return *this;
    }

    this_t& operator-=(const this_t& other)
    {
        _count -= other._count;
        return *this;
    }

    this_t& operator*=(float f)
    {
        _count *= f;
        return *this;
    }

    this_t operator*(float f)
    {
        auto result = *this;
        result *= f;
        return result;
    }

    this_t before() const
    {
        return with_count(count() - 1);
    }

    this_t after() const
    {
        return with_count(count() + 1);
    }

    this_t operator-() const
    {
        return with_count(-_count);
    }

    template<uint32_t other_base_v>
    auto
    operator+(const fixed_point_t<other_base_v>& other) const
    {
        return do_math_op(other, [](auto a, auto b){return a + b;});
    }

    template<uint32_t other_base_v>
    auto
    operator-(const fixed_point_t<other_base_v>& other) const
    {
        return do_math_op(other, [](auto a, auto b){return a - b;});
    }

    template<uint32_t other_base_v>
    auto
    operator/(const fixed_point_t<other_base_v>& other) const
    {
        return with_count((other_base_v * count()) / other.count());
    }

    template<uint32_t other_base_v>
    bool operator>(const fixed_point_t<other_base_v> & other) const
    {
        return do_cmp_op(other, [](auto a, auto b){return a > b;});
    }

    template<uint32_t other_base_v>
    bool operator>=(const fixed_point_t<other_base_v> & other) const
    {
        return do_cmp_op(other, [](auto a, auto b){return a >= b;});
    }

    template<uint32_t other_base_v>
    bool operator<(const fixed_point_t<other_base_v> & other) const
    {
        return do_cmp_op(other, [](auto a, auto b){return a < b;});
    }

    template<uint32_t other_base_v>
    bool operator<=(const fixed_point_t<other_base_v> & other) const
    {
        return do_cmp_op(other, [](auto a, auto b){return a <= b;});
    }

    template<uint32_t other_base_v>
    bool operator==(const fixed_point_t<other_base_v> & other) const
    {
        return do_cmp_op(other, [](auto a, auto b){return a == b;});
    }

    template<uint32_t other_base_v>
    bool operator!=(const fixed_point_t<other_base_v> & other) const
    {
        return do_cmp_op(other, [](auto a, auto b){return a != b;});
    }

    count_t count() const
    {
        return _count;
    }

    count_t to_timebase(uint32_t timebase) const
    {
        return (_count * timebase) / base_v;
    }

private:

    template<uint32_t other_base_v, typename math_op_fun_t>
    fixed_point_t<common_base(base_v, other_base_v)>
    do_math_op(const fixed_point_t<other_base_v>& other, math_op_fun_t f) const
    {
        constexpr int64_t new_base_v = common_base(base_v, other_base_v);
        constexpr int64_t m1 = new_base_v / base_v;
        constexpr int64_t m2 = new_base_v / other_base_v;
        return fixed_point_t<new_base_v>::with_count(
            f(count() * m1, other.count() * m2)
        );
    }

    template<uint32_t other_base_v, typename cmp_op_fun_t>
    bool
    do_cmp_op(const fixed_point_t<other_base_v>& other, cmp_op_fun_t f) const
    {
        constexpr int64_t d = gcd(base_v, other_base_v);
        constexpr int64_t m1 = other_base_v / d;
        constexpr int64_t m2 = base_v / d;
        return f(count() * m1, other.count() * m2);
    }

    count_t _count{0};
};

template<uint32_t base_v> fixed_point_t<base_v> abs(fixed_point_t<base_v> x)
{
    return fixed_point_t<base_v>::with_count(std::abs(x.count()));
}

template<typename in_count_t>
class runtime_fixed_point_t
{
public:
    using count_t = in_count_t;
    using base_t = std::make_unsigned_t<count_t>;

    static runtime_fixed_point_t from_float(float value, base_t base)
    {
        return runtime_fixed_point_t{count_t(value * base), base};
    }

    template<typename other_count_t>
    runtime_fixed_point_t(runtime_fixed_point_t<other_count_t> other)
    : _count{other.count()}
    , _base{other.base()}
    {
    }

    runtime_fixed_point_t(count_t count, base_t base)
    : _count{count}
    , _base{base}
    {
    }

    template<uint32_t base_v> explicit runtime_fixed_point_t(fixed_point_t<base_v> f)
    : runtime_fixed_point_t{count_t(f.count()), base_t(base_v)}
    {
    }

    template<typename out_count_t, uint32_t base_v> explicit operator fixed_point_t<base_v, out_count_t>() const
    {
        return fixed_point_t<base_v, out_count_t>::with_count(out_count_t(rebased_count(base_t(base_v))));
    }

    template<typename out_count_t> auto with_count_type() const
    {
        using out_t = runtime_fixed_point_t<out_count_t>;
        return out_t{
            static_cast<typename out_t::count_t>(_count),
            static_cast<typename out_t::base_t>(_base),
        };
    }

    base_t base() const
    {
        return _base;
    }

    count_t count() const
    {
        return _count;
    }

    runtime_fixed_point_t in_timebase(base_t new_base) const
    {
        return {rebased_count(new_base), new_base};
    }

    count_t rebased_count(base_t new_base) const
    {
        auto divisor = gcd(new_base, base());
        return (_count * (new_base / divisor)) / (base() / divisor);
    }

    explicit operator float() const
    {
        return to_float();
    }

    float to_float() const
    {
        return float(count()) / base();
    }

    template<typename out_count_t>
    runtime_fixed_point_t& operator+=(runtime_fixed_point_t<out_count_t> other)
    {
        return *this = *this + other;
    }

    template<typename out_count_t>
    runtime_fixed_point_t& operator-=(runtime_fixed_point_t<out_count_t> other)
    {
        return *this = *this - other;
    }

    template<typename out_count_t>
    runtime_fixed_point_t operator+(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return {
            rebased_count(new_base) + other.rebased_count(new_base),
            new_base,
        };
    }

    template<typename out_count_t>
    runtime_fixed_point_t operator-(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return {
            rebased_count(new_base) - other.rebased_count(new_base),
            new_base,
        };
    }

    template<typename out_count_t>
    bool operator>(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return rebased_count(new_base) > other.rebased_count(new_base);
    }

    template<typename out_count_t>
    bool operator>=(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return rebased_count(new_base) >= other.rebased_count(new_base);
    }

    template<typename out_count_t>
    bool operator<(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return rebased_count(new_base) < other.rebased_count(new_base);
    }

    template<typename out_count_t>
    bool operator<=(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return rebased_count(new_base) <= other.rebased_count(new_base);
    }

    template<typename out_count_t>
    bool operator==(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return rebased_count(new_base) == other.rebased_count(new_base);
    }

    template<typename out_count_t>
    bool operator!=(runtime_fixed_point_t<out_count_t> other) const
    {
        auto new_base = common_base(base(), other.base());
        return rebased_count(new_base) != other.rebased_count(new_base);
    }

private:
    count_t _count;
    base_t _base;
};

template<uint32_t base_v, typename count_t>
runtime_fixed_point_t(fixed_point_t<base_v, count_t>) -> runtime_fixed_point_t<count_t>;

template<typename in_count_t> runtime_fixed_point_t<in_count_t> abs(runtime_fixed_point_t<in_count_t> x)
{
    return runtime_fixed_point_t<in_count_t>{
        abs(x.count()),
        x.base()
    };
}

template<typename ostream_t, uint32_t base_v, typename count_t>
ostream_t& operator<<(ostream_t& os, fixed_point_t<base_v, count_t> value)
{
    return os << value.count() << "/" << base_v << "(" << value.to_float() << ")";
}

template<typename ostream_t, typename count_t>
ostream_t& operator<<(ostream_t& os, runtime_fixed_point_t<count_t> value)
{
    return os << value.count() << "/" << value.base() << "(" << float{value} << ")";
}
