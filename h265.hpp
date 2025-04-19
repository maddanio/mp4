#pragma once

#include "from_bits.hpp"

#include <cstdint>


inline uint8_t h265_nalu_type(const char* nalu)
{
    return game_on::from_bits<uint8_t>(nalu, 1, 6);
}

inline const char* h265_nalu_type_str(uint8_t t)
{
    switch(t)
    {
        case 0: return "TRAIL_N";//  Coded slice seg. of a non-TSA ,non-STSA trailing picture  VCL
        case 1: return "TRAIL_R";//  Coded slice seg. of a non-TSA, non-STSA trailing picture VCL
        case 2: return "TSA_N";//    Coded slice segment of a TSA picture            VCL
        case 3: return "TSA_R";//    Coded slice segment of a TSA pictur             VCL
        case 4: return "STSA_N";//   Coded slice segment of an STSA picture          VCL
        case 5: return "STSA_R";//   Coded slice segment of an STSA picture          VCL
        case 6: return "RADL_N";//   Coded slice segment of an RADL picture          VCL
        case 7: return "RADL_R";//   Coded slice segment of an RADL picture          VCL
        case 8: return "RASL_N";//   Coded slice segment of an RASL picture          VCL
        case 9: return "RASL_R";//   Coded slice segment of an RASL picture          VCL
        case 10: return "RSV_VCL_N10"; //Reserved N VCL          VCL
        case 12: return "RSV_VCL_N12"; //Reserved N VCL          VCL
        case 14: return "RSV_VCL_N14"; //Reserved N VCL          VCL
        case 11: return "RSV_VCL_N11"; //Reserved R VCL          VCL
        case 13: return "RSV_VCL_N13"; //Reserved R VCL          VCL
        case 15: return "RSV_VCL_N15"; //Reserved R VCL          VCL
        case 16: return "BLA W TFD"; // Coded slice segment of a BLA picture           VCL
        case 17: return "BLA W DLP"; // Coded slice segment of a BLA picture           VCL
        case 18: return "BLA N LP"; //  Coded slice segment of a BLA picture           VCL
        case 19: return "IDR W LP"; //  Coded slice segment of an IDR picture          VCL
        case 20: return "IDR N LP"; //  Coded slice segment of an IDR picture          VCL
        case 21: return "CRA_NUT"; //  Coded slice segment of a CRA picture            VCL
        case 22: return "RSV_RAP_VCL22"; // Reserved RAP           VCL
        case 23: return "RSV_RAP_VCL23"; // Reserved RAP           VCL
        case 24: return "RSV_NVCL24"; // Reserved VCL                   VCL
        case 25: return "RSV_NVCL25"; // Reserved VCL                   VCL
        case 26: return "RSV_NVCL26"; // Reserved VCL                   VCL
        case 27: return "RSV_NVCL27"; // Reserved VCL                   VCL
        case 28: return "RSV_NVCL28"; // Reserved VCL                   VCL
        case 29: return "RSV_NVCL29"; // Reserved VCL                   VCL
        case 30: return "RSV_NVCL30"; // Reserved VCL                   VCL
        case 31: return "RSV_NVCL31"; // Reserved VCL                   VCL
        case 32: return "VPS NUT"; //  Video parameter set                         non-VCL
        case 33: return "SPS NUT"; //  Sequence parameter set                      non-VCL
        case 34: return "PPS NUT"; //  Picture parameter set                       non-VCL
        case 35: return "AUD NUT"; //  Access unit delimiter                       non-VCL
        case 36: return "EOS NUT"; //  End of sequence                             non-VCL
        case 37: return "EOB NUT"; //  End of bitsteam                             non-VCL
        case 38: return "FD  NUT"; //  Filler data                                 non-VCL
        case 39: return "PREFIX_SEI_NUT"; //  Prefix Supplemental enhancement information (SEI)  non-VCL
        case 40: return "SUFIX_SEI_NUT"; //  Suffix Supplemental enhancement information (SEI)  non-VCL
        case 41: return "RSV_NVCL41"; // Reserved                      non-VCL
        case 42: return "RSV_NVCL42"; // Reserved                      non-VCL
        case 43: return "RSV_NVCL43"; // Reserved                      non-VCL
        case 44: return "RSV_NVCL44"; // Reserved                      non-VCL
        case 45: return "RSV_NVCL45"; // Reserved                      non-VCL
        case 46: return "RSV_NVCL46"; // Reserved                      non-VCL
        case 47: return "RSV_NVCL47"; // Reserved                      non-VCL
        case 48: return "STAP"; //                                 RTP
        case 49: return "FU"; //                                 RTP
        case 50: return "UNSPEC50"; // Unspecified                   non-VCL
        case 51: return "UNSPEC51"; // Unspecified                   non-VCL
        case 52: return "UNSPEC52"; // Unspecified                   non-VCL
        case 53: return "UNSPEC53"; // Unspecified                   non-VCL
        case 54: return "UNSPEC54"; // Unspecified                   non-VCL
        case 55: return "UNSPEC55"; // Unspecified                   non-VCL
        case 56: return "UNSPEC56"; // Unspecified                   non-VCL
        case 57: return "UNSPEC57"; // Unspecified                   non-VCL
        case 58: return "UNSPEC58"; // Unspecified                   non-VCL
        case 59: return "UNSPEC59"; // Unspecified                   non-VCL
        case 61: return "UNSPEC61"; // Unspecified                   non-VCL
        case 62: return "UNSPEC62"; // Unspecified                   non-VCL
        case 63: return "UNSPEC63"; // Unspecified                   non-VCL
        case 64: return "UNSPEC64"; // Unspecified                   non-VCL
    }
    return "??";
}
