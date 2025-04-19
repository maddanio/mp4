#pragma once
#include <cstdint>


inline const uint8_t h264_nalu_type(const char* nalu)
{
    return reinterpret_cast<const uint8_t*>(nalu)[0] & 0x1f;
}

inline const char* h264_nalu_type_str(uint8_t t)
{
    switch(t)
    {
            case 0: return "unspecified";
            case 1: return "p-frame";
            case 2: return "coded slice a";
            case 3: return "coded slice b";
            case 4: return "coded slice c";
            case 5: return "i-frame";
            case 6: return "sei";
            case 7: return "sps";
            case 8: return "pps";
            case 9: return "access unit delimiter";
            case 10: return "end of sequence";
            case 11: return "end of stream";
            case 12: return "filler";
            case 13: return "sps extension";
            case 14: return "prefix";
            case 15: return "subset sps";
            case 16: return "reserved";
            case 17: return "reserved";
            case 18: return "reserved";
            case 19: return "coded slice of an auxiliary coded picture without partitioning";
            case 20: return "coded slice extension";
            case 21: return "coded slice extension for depth view components";
            case 22: return "reserved";
            case 23: return "reserved";
            case 24: return "stap-a";
            case 25: return "stap-b";
            case 26: return "mtap16";
            case 27: return "mtap24";
            case 28: return "fu-a";
            case 29: return "fu-b";
            case 30: return "unspecified";
            case 31: return "unspecified";
            default: return "???";

    }
}
