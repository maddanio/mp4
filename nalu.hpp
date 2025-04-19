#pragma once

#include "h264.hpp"
#include "h265.hpp"

namespace game_on
{
    enum class nalu_kind_t {
        h264,
        h265,
    };

    inline const uint8_t nalu_type(nalu_kind_t kind, const char* nalu)
    {
        switch(kind)
        {
        case nalu_kind_t::h264: return h264_nalu_type(nalu);
        case nalu_kind_t::h265: return h265_nalu_type(nalu);
        }
        return 0;
    }

    inline const char* nalu_type_str(nalu_kind_t kind, uint8_t t)
    {
        switch(kind)
        {
        case nalu_kind_t::h264: return h264_nalu_type_str(t);
        case nalu_kind_t::h265: return h265_nalu_type_str(t);
        }
        return 0;
    }

    inline const bool nalu_is_vcl(nalu_kind_t kind, uint8_t t)
    {
        switch(kind)
        {
        case nalu_kind_t::h264: return t >= 1 && t <= 5;
        case nalu_kind_t::h265: return t >= 0 && t <= 28;
        }
        return false;
    }

    inline const bool nalu_is_keyframe(nalu_kind_t kind, uint8_t t)
    {
        switch(kind)
        {
        case nalu_kind_t::h264:
            return t == 5;
        case nalu_kind_t::h265:
            return t >= 16 && t <= 21;
        }
    }

    inline const bool nalu_is_parameter_set(nalu_kind_t kind, uint8_t t)
    {
        switch(kind)
        {
        case nalu_kind_t::h264: return t >= 7 && t <= 8;
        case nalu_kind_t::h265: return t >= 32 && t <= 34;
        }
        return false;
    }
}
