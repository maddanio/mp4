#pragma once

#include <cstring>
#include <iostream>
#include <functional>
#include <fstream>
#include <stdint.h>
#include <vector>
#include <list>
#include <stdexcept>
#include <cassert>
#include <optional>

#include "nalu.hpp"

#include <boost/assert.hpp>

#include "fixed_point.hpp"

// https://developer.apple.com/library/mac/documentation/QuickTime/QTFF/QTFFChap2/qtff2.html#//apple_ref/doc/uid/TP40000939-CH204-33303
// what we want:
// - sps and pps NALUs (from the avcC atom) to put into CMVideoFormatDescriptionCreateFromH264ParameterSets
// -- see also http://stackoverflow.com/questions/25078364/cmvideoformatdescriptioncreatefromh264parametersets-issues
// -- in the avcC sps seems to be after 6 bytes with 2 byte size prefix and pps after that again with 2 byte size prefix
// - the mdat atom by extracing its NALUs and putting them into CMSampleBuffers
// -- they should just be 4-byte size prefixed, and thats the way (ahaha) we like them (ahaha)

// ---------------------- file reading utils --------------------------

namespace my_remux::mp4
{
    struct Box
    {
        size_t content_offset;
        size_t content_length;
        size_t header_length;

        size_t endOffset() const
        {
            return content_offset + content_length;
        }
        size_t totalLength() const
        {
            return content_length + header_length;
        }
        size_t headerOffset() const
        {
            return content_offset - header_length;
        }
        bool operator<(const Box& other)
        {
            return headerOffset() < other.headerOffset();
        }
    };

    struct NALU : public Box
    {
        NALU(const Box& box, uint8_t flags, game_on::nalu_kind_t kind)
        : Box(box)
        , kind{kind}
        , type(game_on::nalu_type(kind, (const char*)&flags))
        {}
        game_on::nalu_kind_t kind;
        uint8_t type;

        std::string describeType() const
        {
            return game_on::nalu_type_str(kind, type);
        }

        bool isVCL() const
        {
            return game_on::nalu_is_vcl(kind, type);
        }
        
        bool is_keyframe() const
        {
            return game_on::nalu_is_keyframe(kind, type);
        }

        bool is_parameter_set() const
        {
            return game_on::nalu_is_parameter_set(kind, type);
        }
    };

    struct MP4Atom : public Box
    {
        uint32_t type;

        MP4Atom(size_t c_offset, size_t c_length, size_t h_length, uint32_t type)
        : Box{c_offset, c_length, h_length}
        , type(type)
        {}

        std::string typeString() const
        {
            return std::string(reinterpret_cast<const char*>(&type), 4);
        }

        bool isType(const char* t) const
        {
            return 0 == strncmp((const char*)&type, t, 4);
        }

        bool isContainer() const
        {
            return isType("moov") ||
                   isType("moof") ||
                   isType("trak") ||
                   isType("traf") ||
                   isType("mfra") ||
                   isType("mvex") ||
                   isType("mdia") ||
                   isType("minf") ||
                   isType("stbl") ||
                   isType("stsd") ||
                   isType("file") || // pseudo atom
                   isType("dinf") ||
                   isType("dref") ||
                   isType("udta") ||
                   isType("meta") ||
                   isType("ilst") ||
                   isType("edts") ||
                   isType("avc1");
        }

        size_t childOffset() const
        {
            if (isType("stsd") || isType("dref"))
            {
                return 8; // uint32_t version_flags, uint32_t numChildren
            }
            else if (isType("meta"))
            {
                return 4; // uint32_t version_flags
            }
            else if (isType("avc1"))
            {
                return 78; // 6 bytes pad & 2 bytes index
                           // version(2), revision(2), vendor(4),
                           // temp quality(4), spat quality(4),
                           // width(2), height(2), hppi(4), vppi(4)
                           // data size(4, always 0)
                           // frame count(2)
                           // compressor name(4)
                           // depth(2)
                           // color table id (2)
                           // color table (if any)
                           // ...right?
                           // actually taken from gdcl code, could not find it in any spec yet...
            }
            else
            {
                return 0;
            }
        }
    };

    struct fullbox_header_t
    {
        uint8_t version;
        uint32_t flags;
    };

    struct hev1_t
    {
    };

    struct hvc1_t
    {
    };

    struct avcC_t
    {
        int naluLengthFieldSize = 4;
        std::vector<char> sps;
        std::vector<char> pps;
    };

    struct avc1_t
    {
        uint16_t width = 0;
        uint16_t height = 0;
        avcC_t avcC;
    };

    struct stsd_t
    {
        avc1_t avc1;
    };

    struct tts_t
    {
        uint32_t count;
        int32_t duration;
    };

    struct stss_t
    {
        std::vector<uint32_t> keyframe_indices;
    };

    struct stc_t
    {
        uint32_t first_chunk;
        uint32_t samples_per_chunk;
        uint32_t sample_description_index;
    };

    struct stbl_t
    {
        stsd_t stsd;
        std::vector<tts_t> stts; // compressed dts
        stss_t stss; // keyframe indices
        std::vector<tts_t> ctts; // dts -> pts mapping
        std::vector<stc_t> stsc; // samples -> chunks mapping
        std::vector<uint32_t> stsz; // sample sizes
        std::vector<uint64_t> co64; // chunk offsets
    };

    struct mvhd_t
    {
        uint64_t creation_time = 0;
        uint64_t modification_time = 0;
        uint32_t time_scale = 90000;
        uint64_t duration = 0;
        fixed_point_t<0x00010000, int32_t> preferred_rate{1.0f};
        fixed_point_t<0x000100, int16_t> preferred_volume{1.0f};
        int32_t preview_time{0};
        int32_t preview_duration{0};
        int32_t poster_time{0};
        int32_t selection_time{0};
        int32_t selection_duration{0};
        int32_t current_time{0};
        uint32_t next_track_id{0xff};
    };

    struct matrix_t
    {
        fixed_point_t<0x10000, int32_t> a{1.0f};
        fixed_point_t<0x10000, int32_t> b{0.0f};
        fixed_point_t<0x10000, int32_t> c{0.0f};
        fixed_point_t<0x10000, int32_t> d{1.0f};
        fixed_point_t<0x10000, int32_t> tx{0.0f};
        fixed_point_t<0x10000, int32_t> ty{0.0f};
        fixed_point_t<0x40000000, int32_t> u{0.0f};
        fixed_point_t<0x40000000, int32_t> v{0.0f};
        fixed_point_t<0x40000000, int32_t> w{1.0f};
    };

    struct tkhd_t
    {
        uint64_t creation_time = 0;
        uint64_t modification_time = 0;
        uint32_t track_id = 1;
        uint64_t duration = 0;
        uint16_t group = 0;
        uint16_t volume = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        matrix_t display_matrix = {};
    };

    struct mdhd_t
    {
        uint64_t creation_time = 0;
        uint64_t modification_time = 0;
        uint32_t time_scale = 90000;
        uint64_t duration = 0;
        uint16_t language = 0;
    };

    struct mfhd_t
    {
        uint32_t sequence_number;
    };

    struct tfhd_t
    {
        uint32_t track_ID;
        std::optional<uint64_t> base_data_offset;
        std::optional<uint32_t> sample_description_index;
        std::optional<uint32_t> default_sample_duration;
        std::optional<uint32_t> default_sample_size;
        std::optional<uint32_t> default_sample_flags;
    };

    struct trun_t
    {
        struct sample_t
        {
            std::optional<uint32_t> duration;
            std::optional<uint32_t> size;
            std::optional<uint32_t> flags;
            std::optional<int32_t> composition_time_offset;            
        };
        uint32_t sample_count;
        std::optional<int32_t> data_offset;
        std::optional<uint32_t> first_sample_flags;
        std::vector<sample_t> samples;
    };

    struct tfdt_t
    {
        uint64_t base_media_decode_time;
    };

    struct traf_t
    {
        tfhd_t tfhd;
        std::optional<tfdt_t> tfdt;
        std::vector<trun_t> trun;
    };

    struct moof_t
    {
        mfhd_t mfhd;
        std::vector<traf_t> traf;
    };

    struct edit_t
    {
        uint64_t duration;
        uint64_t start_offset;
        fixed_point_t<0x10000, int32_t> rate{1.0f};
    };

    struct edts_t
    {
        std::vector<edit_t> elst;
    };

    struct hdlr_t
    {
        /*
        char hdlr[4];
        char hdlr_type[4];
        */
        std::string description;
    };

    struct vmhd_t
    {
    };

    struct url_t
    {
    };

    struct dref_t
    {
        url_t url;
    };

    struct dinf_t
    {
        dref_t dref;
    };

    struct minf_t
    {
        vmhd_t vmhd;
        dinf_t dinf;
        stbl_t stbl;
    };

    struct mdia_t
    {
        mdhd_t mdhd;
        hdlr_t hdlr;
        minf_t minf;
    };

    struct trak_t
    {
        tkhd_t tkhd;
        edts_t edts;
        mdia_t mdia;
    };

    struct moov_t
    {
        mvhd_t mvhd;
        trak_t trak;
    };

    struct udta_t
    {
        std::string name;
    };

    struct ftyp_t
    {
    };

    inline uint32_t to_host(uint32_t v)
    {
        char* p = reinterpret_cast<char*>(&v);
        std::swap(p[0], p[3]);
        std::swap(p[1], p[2]);
        return v;
    }

    inline int32_t to_host(int32_t v)
    {
        char* p = reinterpret_cast<char*>(&v);
        std::swap(p[0], p[3]);
        std::swap(p[1], p[2]);
        return v;
    }

    inline uint16_t to_host(uint16_t v)
    {
        char* p = reinterpret_cast<char*>(&v);
        std::swap(p[0], p[1]);
        return v;
    }

    template<typename T>
    inline T read_to_host(std::istream& file)
    {
        T x;
        file.read((char*)&x, sizeof(x));
        return to_host(x);
    }

    template<typename T>
    inline T read_to_host(const char* p)
    {
        T x = *reinterpret_cast<const T*>(p);;
        return to_host(x);
    }

    inline uint64_t to_host(uint64_t v)
    {
        uint32_t hi = to_host(uint32_t(v >> 32));
        uint32_t lo = to_host(uint32_t(v & 0xffffffff));
        char* p = reinterpret_cast<char*>(&v);
        char* p_hi = reinterpret_cast<char*>(&hi);
        char* p_lo = reinterpret_cast<char*>(&lo);
        std::copy(p_hi, p_hi + 4, p);
        std::copy(p_lo, p_lo + 4, p + 4);
        return v;
    }

    template<>
    inline uint64_t read_to_host<uint64_t>(std::istream& file)
    {
        uint64_t hi = read_to_host<uint32_t>(file);
        uint64_t lo = read_to_host<uint32_t>(file);
        return (hi << 32) + lo;
    }

    template<typename T>
    inline T read(std::istream& file)
    {
        T x;
        file.read((char*)&x, sizeof(x));
        return x;
    }

    template<typename T, typename output_iter_t>
    inline void copy_number(T x, output_iter_t o)
    {
        x = to_host(x);
        auto p = reinterpret_cast<const char*>(&x);
        std::copy(p, p + sizeof(T), o);
    }

    template<typename T>
    inline void put_number(T x, std::vector<char>& v)
    {
        copy_number(x, std::back_inserter(v));
    }

    inline void put_fourcc(const char* type, std::vector<char>& v)
    {
        std::copy(type, type + 4, std::back_inserter(v));
    }

    inline void put_fullbox_header(fullbox_header_t h, std::vector<char>& v)
    {
        v.push_back(h.version);
        v.push_back((h.flags >> 16) & 0xff);
        v.push_back((h.flags >> 8) & 0xff);
        v.push_back((h.flags >> 0) & 0xff);
    }

    inline size_t begin_atom(std::vector<char>& out, const char* type)
    {
        auto start_offset = out.size();
        put_number(uint32_t{0}, out);
        put_fourcc(type, out);
        return start_offset;
    }

    inline size_t finish_atom(std::vector<char>& out, size_t start_offset)
    {
        uint32_t size = uint32_t(out.size() - start_offset);
        copy_number(size, out.data() + start_offset);
        return size;
    }

    inline size_t fileLength(std::istream& file)
    {
        file.seekg(0, std::ios_base::end);
        return file.tellg();
    }

    inline Box readSimpleBoxAtOffset(std::istream& file, size_t offset, size_t lengthFieldSize)
    {
        file.seekg(offset);
        size_t content_length;
        switch(lengthFieldSize)
        {
            case 2: content_length = read_to_host<uint16_t>(file); break;
            case 4: content_length = read_to_host<uint32_t>(file); break;
            default: throw std::runtime_error("readSimpleBoxAtOffset: box must have 2 or 4 byte length field");
        }
        return Box{offset + lengthFieldSize, content_length, lengthFieldSize};
    }

    inline NALU readNALUAtOffset(std::istream& file, size_t offset, size_t lengthFieldSize, game_on::nalu_kind_t kind)
    {
        Box box = readSimpleBoxAtOffset(file, offset, lengthFieldSize);
        uint8_t flags = file.get();
        return NALU(box, flags, kind);
    }

    inline std::vector<char> readBox(std::istream& file, const Box& box)
    {
        std::vector<char> data(box.totalLength());
        file.seekg(box.headerOffset());
        file.read(&data[0], data.size());
        return data;
    }

    inline std::vector<char> readBoxContent(std::istream& file, const Box& box)
    {
        std::vector<char> data(box.content_length);
        file.seekg(box.content_offset);
        file.read(&data[0], data.size());
        return data;
    }

    inline MP4Atom readAtomAtOffset(std::istream& file, size_t offset)
    {
        file.seekg(offset);
        uint32_t length = read_to_host<uint32_t>(file);
        uint32_t type = read<uint32_t>(file);
        if (length == 1)
        {
            uint64_t length = read_to_host<uint64_t>(file);
            size_t content_offset = offset + 16;
            return MP4Atom(content_offset, length - 16, 16, type);
        }
        else
        {
            size_t content_offset = offset + 8;
            return MP4Atom(content_offset, length - 8, 8, type);
        }
    }

    inline fullbox_header_t read_fullbox_header(std::istream& file)
    {
        uint8_t bytes[4] = {
            read<uint8_t>(file),
            read<uint8_t>(file),
            read<uint8_t>(file),
            read<uint8_t>(file),
        };
        fullbox_header_t header{
            bytes[0],
            (uint32_t(bytes[1]) << 16) + (uint32_t(bytes[2]) << 8) + (uint32_t(bytes[3]) << 0),
        };
        return header;
    }

    inline std::vector<int32_t> read_tts(std::istream& file, const MP4Atom& atom)
    {
        std::vector<int32_t> tts;
        file.seekg(atom.content_offset + 4);
        auto entry_count = read_to_host<uint32_t>(file);
        for (int32_t i = 0; i < entry_count; ++i)
        {
            auto sample_count = read_to_host<int32_t>(file);
            auto sample_value = read_to_host<int32_t>(file);
            for (int32_t j = 0; j < sample_count; ++j)
                tts.push_back(sample_value);
        }
        return tts;
    }

    inline std::vector<uint32_t> read_stco(std::istream& file, const MP4Atom& atom)
    {
        std::vector<uint32_t> stco;
        file.seekg(atom.content_offset + 4);
        auto entry_count = read_to_host<uint32_t>(file);        
        for (int32_t i = 0; i < entry_count; ++i)
            stco.push_back(read_to_host<uint32_t>(file));
        return stco;
    }

    inline std::vector<uint64_t> read_co64(std::istream& file, const MP4Atom& atom)
    {
        std::vector<uint64_t> co64;
        file.seekg(atom.content_offset + 4);
        auto entry_count = read_to_host<uint32_t>(file);        
        for (int32_t i = 0; i < entry_count; ++i)
            co64.push_back(read_to_host<uint64_t>(file));
        return co64;
    }

    inline std::vector<stc_t> read_stsc(std::istream& file, const MP4Atom& atom)
    {
        std::vector<stc_t> stsc;
        file.seekg(atom.content_offset + 4);
        auto entry_count = read_to_host<uint32_t>(file);
        for (int32_t i = 0; i < entry_count; ++i)
        {
            stsc.push_back({
                read_to_host<uint32_t>(file),
                read_to_host<uint32_t>(file),
                read_to_host<uint32_t>(file)
            });
        }
        return stsc;
    }


    inline std::vector<int32_t> read_stts(std::istream& file, const MP4Atom& atom)
    {
        return read_tts(file, atom);
    }

    inline std::vector<uint32_t> read_stsz(std::istream& file, const MP4Atom& atom)
    {
        std::vector<uint32_t> stsz;
        file.seekg(atom.content_offset + 4);
        auto common_size = read_to_host<uint32_t>(file);
        auto entry_count = read_to_host<int32_t>(file);
        if (common_size == 0)
            for (int32_t i = 0; i < entry_count; ++i)
                stsz.push_back(read_to_host<uint32_t>(file));
        else
            for (int32_t i = 0; i < entry_count; ++i)
                stsz.push_back(common_size);
        return stsz;
    }

    inline std::vector<edit_t> read_elst(std::istream& file, const MP4Atom& atom)
    {
        std::vector<edit_t> edits;
        file.seekg(atom.content_offset);
        auto header = read_fullbox_header(file);
        file.seekg(atom.content_offset + 4);
        auto entry_count = read_to_host<int32_t>(file);
        for (int32_t i = 0; i < entry_count; ++i)
            if (header.version == 0)
                edits.push_back({
                    read_to_host<uint32_t>(file),
                    read_to_host<uint32_t>(file),
                    fixed_point_t<0x10000, int32_t>::with_count(read_to_host<uint32_t>(file))
                });
            else
                edits.push_back({
                    read_to_host<uint64_t>(file),
                    read_to_host<uint64_t>(file),
                    fixed_point_t<0x10000, int32_t>::with_count(read_to_host<uint32_t>(file))
                });
        return edits;
    }

    inline std::vector<int32_t> read_ctts(std::istream& file, const MP4Atom& atom)
    {
        return read_tts(file, atom);
    }

    inline avcC_t read_avcC(std::istream& file, const MP4Atom& atom)
    {
        file.seekg(atom.content_offset + 4);
        auto naluLengthFieldSize = file.get() & 3 + 1;
        file.seekg(atom.content_offset + 5);
        size_t elementCount = file.get() & 0x1f;
        BOOST_ASSERT(elementCount == 1);
        auto sps = readSimpleBoxAtOffset(file, atom.content_offset + 6, 2);
        file.seekg(sps.endOffset());
        auto ppsCount = file.get();
        BOOST_ASSERT(ppsCount == 1);
        auto pps = readSimpleBoxAtOffset(file, sps.endOffset() + 1, 2);
        auto spsData = readBoxContent(file, sps);
        auto ppsData = readBoxContent(file, pps);
        return {
            naluLengthFieldSize,
            //elementCount,
            spsData,
            ppsData
        };
    }

    inline mvhd_t read_mvhd(std::istream& file, const MP4Atom& atom)
    {
        mvhd_t mvhd;
        file.seekg(atom.content_offset);
        auto header = read_fullbox_header(file);
        if (header.version == 0)
        {
            mvhd.creation_time = read_to_host<uint32_t>(file);
            mvhd.modification_time = read_to_host<uint32_t>(file);
            mvhd.time_scale = read_to_host<uint32_t>(file);
            mvhd.duration = read_to_host<uint32_t>(file);
        }
        else
        {
            mvhd.creation_time = read_to_host<uint64_t>(file);
            mvhd.modification_time = read_to_host<uint64_t>(file);
            mvhd.time_scale = read_to_host<uint32_t>(file);
            mvhd.duration = read_to_host<uint64_t>(file);            
        }
        mvhd.preferred_rate = mvhd.preferred_rate.with_count(read_to_host<int32_t>(file));
        mvhd.preferred_volume = mvhd.preferred_volume.with_count(read_to_host<int16_t>(file));
        mvhd.preview_time = read_to_host<int32_t>(file);
        mvhd.preview_duration = read_to_host<int32_t>(file);
        mvhd.poster_time = read_to_host<int32_t>(file);
        mvhd.selection_time = read_to_host<int32_t>(file);
        mvhd.selection_duration = read_to_host<int32_t>(file);
        mvhd.current_time = read_to_host<int32_t>(file);
        mvhd.next_track_id = read_to_host<int32_t>(file);
        return mvhd;
    }

    inline mdhd_t read_mdhd(std::istream& file, const MP4Atom& atom)
    {
        mdhd_t mdhd;
        file.seekg(atom.content_offset);
        auto header = read_fullbox_header(file);
        if (header.version == 0)
        {
            mdhd.creation_time = read_to_host<uint32_t>(file);
            mdhd.modification_time = read_to_host<uint32_t>(file);
            mdhd.time_scale = read_to_host<uint32_t>(file);
            mdhd.duration = read_to_host<uint32_t>(file);
        }
        else
        {
            mdhd.creation_time = read_to_host<uint64_t>(file);
            mdhd.modification_time = read_to_host<uint64_t>(file);
            mdhd.time_scale = read_to_host<uint32_t>(file);
            mdhd.duration = read_to_host<uint64_t>(file);
        }
        mdhd.language = read_to_host<uint16_t>(file);
        return mdhd;
    }

    inline mfhd_t read_mfhd(std::istream& file, const MP4Atom& atom)
    {
        file.seekg(atom.content_offset);
        read_fullbox_header(file);
        return mfhd_t{
            read_to_host<uint32_t>(file)
        };
    }

    inline tfdt_t read_tfdt(std::istream& file, const MP4Atom& atom)
    {
        file.seekg(atom.content_offset);
        auto header = read_fullbox_header(file);
        if (header.version == 0)
            return tfdt_t{read_to_host<uint32_t>(file)};
        else
            return tfdt_t{read_to_host<uint64_t>(file)};
    }

    inline tfhd_t read_tfhd(std::istream& file, const MP4Atom& atom)
    {
        file.seekg(atom.content_offset);
        auto header = read_fullbox_header(file);
        tfhd_t tfhd{
            read_to_host<uint32_t>(file)
        };
        if (header.flags & 0x1)
            tfhd.base_data_offset = read_to_host<uint64_t>(file);
        if (header.flags & 0x2)
            tfhd.sample_description_index = read_to_host<uint32_t>(file);
        if (header.flags & 0x8)
            tfhd.default_sample_duration = read_to_host<uint32_t>(file);
        if (header.flags & 0x10)
            tfhd.default_sample_size = read_to_host<uint32_t>(file);
        if (header.flags & 0x20)
            tfhd.default_sample_flags = read_to_host<uint32_t>(file);
        return tfhd;
    }

    inline trun_t read_trun(std::istream& file, const MP4Atom& atom)
    {
        file.seekg(atom.content_offset);
        trun_t trun;
        auto header = read_fullbox_header(file);
        trun.sample_count = read_to_host<uint32_t>(file);
        if (header.flags & 0x1)
            trun.data_offset = read_to_host<int32_t>(file);
        if (header.flags & 0x4)
            trun.first_sample_flags = read_to_host<uint32_t>(file);
        for (uint32_t i = 0; i < trun.sample_count; ++i)
        {
            trun_t::sample_t sample;
            if (header.flags & 0x100)
                sample.duration = read_to_host<uint32_t>(file);
            if (header.flags & 0x200)
                sample.size = read_to_host<uint32_t>(file);
            if (header.flags & 0x400)
                sample.flags = read_to_host<uint32_t>(file);
            if (header.flags & 0x800)
                sample.composition_time_offset = read_to_host<int32_t>(file);
            trun.samples.push_back(sample);
        }
        return trun;
    }

    inline traf_t read_traf(std::istream& file, const MP4Atom& atom)
    {
        auto tfhd_atom = readAtomAtOffset(file, atom.content_offset);
        traf_t traf{
            read_tfhd(file, tfhd_atom),
        };
        auto offset = tfhd_atom.endOffset();
        while (offset < atom.endOffset())
        {
            auto atom = readAtomAtOffset(file, offset);
            if (atom.isType("tfdt"))
            {
                if (traf.tfdt)
                    throw std::runtime_error{"multiple tfdt atoms in traf atom"};
                if (!traf.trun.empty())
                    throw std::runtime_error{"tfdt has to occur before trun"};
                traf.tfdt = read_tfdt(file, atom);
            }
            else if (atom.isType("trun"))
            {
                traf.trun.push_back(read_trun(file, atom));
            }
            offset = atom.endOffset();
        }
        return traf;
    }

    inline moof_t read_moof(std::istream& file, const MP4Atom& atom)
    {
        auto mfhd_atom = readAtomAtOffset(file, atom.content_offset);
        moof_t moof{
            read_mfhd(file, mfhd_atom),
        };
        auto offset = mfhd_atom.endOffset();
        while (offset < atom.endOffset())
        {
            auto atom = readAtomAtOffset(file, offset);
            if (atom.isType("traf"))
                moof.traf.push_back(read_traf(file, atom));
            else
                throw std::runtime_error{
                    "unexpected atom type '" + atom.typeString() + "' in moof after header"
                };
            offset = atom.endOffset();
        }
        return moof;
    }

    inline void write_matrix(std::vector<char>& out, const matrix_t& matrix)
    {
        put_number(matrix.a.count(), out);
        put_number(matrix.b.count(), out);
        put_number(matrix.u.count(), out);
        put_number(matrix.c.count(), out);
        put_number(matrix.d.count(), out);
        put_number(matrix.v.count(), out);
        put_number(matrix.tx.count(), out);
        put_number(matrix.ty.count(), out);
        put_number(matrix.w.count(), out);
    }

    inline size_t write_avcC(std::vector<char>& out, const avcC_t& avcc)
    {
        auto start_offset = begin_atom(out, "avcC");
        out.push_back(0x1); // version
        out.push_back(avcc.sps[1]); // profile
        out.push_back(avcc.sps[2]); // constraints
        out.push_back(avcc.sps[3]); // level
        out.push_back(avcc.naluLengthFieldSize - 1);
        out.push_back(1); // num sps nalus
        put_number(uint16_t(avcc.sps.size()), out);
        out.insert(out.end(), avcc.sps.begin(), avcc.sps.end());
        out.push_back(1); // num pps nalus
        put_number(uint16_t(avcc.pps.size()), out);
        out.insert(out.end(), avcc.pps.begin(), avcc.pps.end());
        return finish_atom(out, start_offset);
    }

    inline size_t write_avc1(std::vector<char>& out, const avc1_t& avc1)
    {
        auto start_offset = begin_atom(out, "avc1");
        put_number(uint32_t{0}, out); // reserved
        put_number(uint16_t{0}, out); // reserved
        put_number(uint16_t{1}, out); // data reference index
        put_number(uint16_t{0}, out); // codec stream version
        put_number(uint16_t{0}, out); // codec stream revision
        put_number(uint32_t{0}, out); // reserved
        put_number(uint32_t{0}, out); // reserved
        put_number(uint32_t{0}, out); // reserved
        put_number(avc1.width, out);
        put_number(avc1.height, out);
        put_number(uint32_t{0x00480000}, out); // horizontal resolution: 72dpi
        put_number(uint32_t{0x00480000}, out); // vecrtical resolution: 72dpi
        put_number(uint32_t{0}, out); // data size
        put_number(uint16_t{1}, out); // frame count
        for (size_t i = 0; i < 32; ++i)
            out.push_back(0); // deprecated compressor name?
        put_number(uint16_t{0x18}, out); // reserved
        put_number(uint16_t{0xffff}, out); // reserved
        write_avcC(out, avc1.avcC);
        return finish_atom(out, start_offset);        
    }

    inline size_t write_stsd(std::vector<char>& out, const stsd_t& stsd)
    {
        auto start_offset = begin_atom(out, "stsd");
        put_fullbox_header({0, 0}, out);
        put_number(uint32_t{1}, out); // entry count
        write_avc1(out, stsd.avc1);
        return finish_atom(out, start_offset);
    }

    inline std::vector<tts_t> compress_to_tts(const std::vector<int32_t> values)
    {
        std::vector<tts_t> result;
        if(!values.empty())
        {
            uint32_t i = 0, start_index = 0;
            for (; i < values.size(); ++i)
            {
                if (i > start_index && values[i] != values[start_index])
                {
                    result.push_back({i - start_index, values[start_index]});
                    start_index = i;
                }
            }
            result.push_back({i - start_index, values[start_index]});
        }
        return result;
    }

    inline size_t write_tts(
        std::vector<char>& out,
        const char* tag,
        const std::vector<tts_t>& entries,
        uint8_t version = 0
    )
    {
        auto start_offset = begin_atom(out, tag);
        put_fullbox_header({0, 0}, out);
        put_number(uint32_t(entries.size()), out);
        for (auto&& tts : entries)
        {
            put_number(tts.count, out);
            put_number(tts.duration, out);
        }
        return finish_atom(out, start_offset);
    }

    inline size_t write_stss(std::vector<char>& out, const stss_t& stss)
    {
        auto start_offset = begin_atom(out, "stss");
        put_fullbox_header({0, 0}, out);
        put_number(uint32_t(stss.keyframe_indices.size()), out);
        for (auto keyframe_index : stss.keyframe_indices)
            put_number(keyframe_index, out);
        return finish_atom(out, start_offset);        
    }

    inline size_t write_stsc(std::vector<char>& out, const std::vector<stc_t>& stsc)
    {
        auto start_offset = begin_atom(out, "stsc");
        put_fullbox_header({0, 0}, out);
        put_number(uint32_t(stsc.size()), out);
        for (auto&& entry : stsc)
        {
            put_number(entry.first_chunk, out);
            put_number(entry.samples_per_chunk, out);
            put_number(entry.sample_description_index, out);
        }
        return finish_atom(out, start_offset);        
    }

    inline size_t write_stsz(std::vector<char>& out, const std::vector<uint32_t>& stsz)
    {
        auto start_offset = begin_atom(out, "stsz");
        put_fullbox_header({0, 0}, out);
        put_number(uint32_t(0), out); // store sizes per sample
        put_number(uint32_t(stsz.size()), out);
        for (auto&& entry : stsz)
            put_number(entry, out);
        return finish_atom(out, start_offset);
    }

    inline size_t write_co64(std::vector<char>& out, const std::vector<uint64_t>& co64)
    {
        auto start_offset = begin_atom(out, "co64");
        put_fullbox_header({0, 0}, out);
        put_number(uint32_t(co64.size()), out);
        for (auto&& entry : co64)
        {
            put_number(entry, out);
        }
        return finish_atom(out, start_offset);        
    }

    inline size_t write_stbl(std::vector<char>& out, const stbl_t& stbl)
    {
        auto start_offset = begin_atom(out, "stbl");
        write_stsd(out, stbl.stsd);
        write_tts(out, "stts", stbl.stts);
        write_stss(out, stbl.stss);
        write_tts(out, "ctts", stbl.ctts, 1);
        write_stsc(out, stbl.stsc);
        write_stsz(out, stbl.stsz);
        write_co64(out, stbl.co64);
        return finish_atom(out, start_offset);
    }

    inline size_t write_mvhd(std::vector<char>& out, const mvhd_t& mvhd)
    {
        auto start_offset = begin_atom(out, "mvhd");
        put_fullbox_header({1, 0}, out); // version 1: more precision
        put_number(mvhd.creation_time, out);
        put_number(mvhd.modification_time, out);
        put_number(mvhd.time_scale, out);
        put_number(mvhd.duration, out);
        put_number(mvhd.preferred_rate.count(), out);
        put_number(mvhd.preferred_volume.count(), out);
        put_number(uint16_t(0), out); // reserved
        put_number(uint32_t(0), out); // reserved
        put_number(uint32_t(0), out); // reserved
        write_matrix(out, matrix_t{});
        put_number(mvhd.preview_time, out);
        put_number(mvhd.preview_duration, out);
        put_number(mvhd.poster_time, out);
        put_number(mvhd.selection_time, out);
        put_number(mvhd.selection_duration, out);
        put_number(mvhd.current_time, out);
        put_number(mvhd.next_track_id, out);        
        return finish_atom(out, start_offset);
    }

    inline size_t write_mdhd(std::vector<char>& out, const mdhd_t& mdhd)
    {
        auto start_offset = begin_atom(out, "mdhd");
        put_fullbox_header({1, 0}, out); // version 1: more precision
        put_number(mdhd.creation_time, out);
        put_number(mdhd.modification_time, out);
        put_number(mdhd.time_scale, out);
        put_number(mdhd.duration, out);
        put_number(mdhd.language, out);
        put_number(uint16_t(0), out); // reserved: quality
        return finish_atom(out, start_offset);
    }

    inline size_t write_hdlr(std::vector<char>& out, const hdlr_t& hdlr)
    {
        // taken from avformat
        auto start_offset = begin_atom(out, "hdlr");
        put_fullbox_header({0, 0}, out);
        put_number(uint32_t(0), out);
        put_fourcc("mdir", out);
        put_fourcc("appl", out);
        put_number(uint32_t(0), out);
        put_number(uint32_t(0), out);
        out.insert(out.end(), hdlr.description.begin(), hdlr.description.end());
        put_number(uint8_t(0), out);
        return finish_atom(out, start_offset);
    }

    inline size_t write_vmhd(std::vector<char>& out, const vmhd_t& vmhd)
    {
        // taken from avformat
        auto start_offset = begin_atom(out, "vmhd");
        put_fullbox_header({0, 1}, out);
        put_number(uint64_t(0), out); // reserved: graphics mode = copy
        return finish_atom(out, start_offset);
    }

    inline size_t write_url(std::vector<char>& out, const url_t& url)
    {
        auto start_offset = begin_atom(out, "url ");
        put_fullbox_header({0, 1}, out);
        return finish_atom(out, start_offset);
    }

    inline size_t write_dref(std::vector<char>& out, const dref_t& dref)
    {
        auto start_offset = begin_atom(out, "dref");
        put_fullbox_header({0, 1}, out);
        put_number(uint32_t(1), out); // entry count
        write_url(out, dref.url);
        return finish_atom(out, start_offset);
    }

    inline size_t write_dinf(std::vector<char>& out, const dinf_t& dinf)
    {
        auto start_offset = begin_atom(out, "dinf");
        write_dref(out, dinf.dref);
        return finish_atom(out, start_offset);        
    }

    inline size_t write_minf(std::vector<char>& out, const minf_t& minf)
    {
        auto start_offset = begin_atom(out, "minf");
        write_vmhd(out, minf.vmhd);
        write_dinf(out, minf.dinf);
        write_stbl(out, minf.stbl);
        return finish_atom(out, start_offset);
    }

    inline size_t write_mdia(std::vector<char>& out, const mdia_t& mdia)
    {
        auto start_offset = begin_atom(out, "mdia");
        write_mdhd(out, mdia.mdhd);
        write_hdlr(out, mdia.hdlr);
        write_minf(out, mdia.minf);
        return finish_atom(out, start_offset);
    }

    inline size_t write_elst(std::vector<char>& out, const std::vector<edit_t>& elst)
    {
        auto start_offset = begin_atom(out, "elst");
        put_fullbox_header({1, 0}, out); // version 1: more precision
        put_number(uint32_t(elst.size()), out);
        for (auto&& edit : elst)
        {
            put_number(edit.duration, out);
            put_number(edit.start_offset, out);
            put_number(edit.rate.count(), out);
        }
        return finish_atom(out, start_offset);
    }

    inline size_t write_edts(std::vector<char>& out, const edts_t& edts)
    {
        auto start_offset = begin_atom(out, "edts");
        write_elst(out, edts.elst);
        return finish_atom(out, start_offset);
    }

    inline size_t write_tkhd(std::vector<char>& out, const tkhd_t& tkhd)
    {
        auto start_offset = begin_atom(out, "tkhd");
        put_fullbox_header({1, 0}, out); // version 1: more precision
        put_number(tkhd.creation_time, out);
        put_number(tkhd.modification_time, out);
        put_number(tkhd.track_id, out);
        put_number(uint32_t{0}, out); // reserved
        put_number(tkhd.duration, out);
        put_number(uint32_t{0}, out); // reserved
        put_number(uint32_t{0}, out); // reserved
        put_number(uint16_t{0}, out); // layer
        put_number(tkhd.group, out);
        put_number(tkhd.volume, out);
        put_number(uint16_t{0}, out); // version
        write_matrix(out, tkhd.display_matrix);
        put_number(uint32_t{tkhd.width * 0x1000}, out); // version
        put_number(uint32_t{tkhd.height * 0x1000}, out); // version
        return finish_atom(out, start_offset);
    }

    inline size_t write_trak(std::vector<char>& out, const trak_t& trak)
    {
        auto start_offset = begin_atom(out, "trak");
        write_tkhd(out, trak.tkhd);
        write_edts(out, trak.edts);
        write_mdia(out, trak.mdia);
        return finish_atom(out, start_offset);
    }

    inline size_t write_moov(std::vector<char>& out, const moov_t& moov)
    {
        auto start_offset = begin_atom(out, "moov");
        write_mvhd(out, moov.mvhd);
        write_trak(out, moov.trak);
        return finish_atom(out, start_offset);
    }

    inline size_t write_ftyp(std::vector<char>& out, const ftyp_t& ftyp)
    {
        auto start_offset = begin_atom(out, "ftyp");
        put_fourcc("isom", out); // major
        put_number(uint32_t(0x200), out); // minor
        put_fourcc("isom", out);
        put_fourcc("iso2", out);
        put_fourcc("avc1", out);
        put_fourcc("mp41", out);
        return finish_atom(out, start_offset);
    }

    inline size_t write_mfhd(std::vector<char>& out, const mfhd_t& mfhd)
    {
        auto start_offset = begin_atom(out, "mfhd");
        put_fullbox_header({0, 0}, out);
        put_number(mfhd.sequence_number, out);
        return finish_atom(out, start_offset);
    }

    inline size_t write_tfhd(std::vector<char>& out, const tfhd_t& tfhd)
    {
        auto start_offset = begin_atom(out, "tfhd");
        uint32_t flags = 0;
        if (tfhd.base_data_offset)
            flags |= 0x1;
        if (tfhd.sample_description_index)
            flags |= 0x2;
        if (tfhd.default_sample_duration)
            flags |= 0x8;
        if (tfhd.default_sample_size)
            flags |= 0x10;
        if (tfhd.default_sample_flags)
            flags |= 0x20;
        put_fullbox_header({1, flags}, out);
        put_number(tfhd.track_ID, out);
        if (tfhd.base_data_offset)
            put_number(*tfhd.base_data_offset, out);
        if (tfhd.sample_description_index)
            put_number(*tfhd.sample_description_index, out);
        if (tfhd.default_sample_duration)
            put_number(*tfhd.default_sample_duration, out);
        if (tfhd.default_sample_size)
            put_number(*tfhd.default_sample_size, out);
        if (tfhd.default_sample_flags)
            put_number(*tfhd.default_sample_flags, out);
        return finish_atom(out, start_offset);
    }

    inline size_t write_tfdt(std::vector<char>& out, const tfdt_t& tfdt)
    {
        auto start_offset = begin_atom(out, "tfdt");
        put_fullbox_header({1, 0}, out);
        put_number(tfdt.base_media_decode_time, out);
        return finish_atom(out, start_offset);
    }

    inline size_t write_trun(std::vector<char>& out, const trun_t& trun)
    {
        auto start_offset = begin_atom(out, "trun");
        uint32_t flags = 0;
        if (trun.data_offset)
            flags |= 0x1;
        if (trun.first_sample_flags)
            flags |= 0x4;
        if (!trun.samples.empty())
        {
            auto& sample = trun.samples.front();
            if (sample.duration)
                flags |= 0x100;
            if (sample.size)
                flags |= 0x200;
            if (sample.flags)
                flags |= 0x400;
            if (sample.composition_time_offset)
                flags |= 0x800;
        }
        put_fullbox_header({1, flags}, out);
        put_number(uint32_t(trun.samples.size()), out);
        if (trun.data_offset)
            put_number(*trun.data_offset, out);
        if (trun.first_sample_flags)
            put_number(*trun.first_sample_flags, out);
        for (auto& sample : trun.samples)
        {
            if (sample.duration)
                put_number(*sample.duration, out);
            if (sample.size)
                put_number(*sample.size, out);
            if (sample.flags)
                put_number(*sample.flags, out);
            if (sample.composition_time_offset)
                put_number(*sample.composition_time_offset, out);
        }
        return finish_atom(out, start_offset);
    }

    inline size_t write_traf(std::vector<char>& out, const traf_t& traf)
    {
        auto start_offset = begin_atom(out, "traf");
        write_tfhd(out, traf.tfhd);
        if (traf.tfdt)
            write_tfdt(out, *traf.tfdt);
        for (auto& trun : traf.trun)
            write_trun(out, trun);
        return finish_atom(out, start_offset);
    }

    inline size_t write_moof(std::vector<char>& out, const moof_t& moof)
    {
        auto start_offset = begin_atom(out, "moof");
        write_mfhd(out, moof.mfhd);
        for (auto& traf : moof.traf)
            write_traf(out, traf);
        return finish_atom(out, start_offset);
    }

#if 0
    struct NALUFrame
    {
        std::vector<NALU> nalus;
        void clear()
        {
            nalus.clear();
        }
        bool consume(const NALU& nalu)
        {
            BOOST_ASSERT(nalus.empty() || nalu.headerOffset() == nalus.back().endOffset());
            if (isNewFrame(nalu))
            {
                return false;
            }
            else
            {
                if (nalu.isVCL())
                {
                    nalus.push_back(nalu);
                }
                return true;
            }
        }
        bool isNewFrame(const NALU& nalu) const
        {
            if (nalus.empty())
            {
                return false;
            }
            const NALU& lastNalu = nalus.back();
            if (!lastNalu.isVCL())
            {
                return false;
            }
            else if (nalu.isVCL())
            {
                return true;
            }
            else if ((lastNalu.idc != nalu.idc) && (lastNalu.idc == 0 || nalu.idc == 0))
            {
                return true;
            }
            else if (nalu.type >= 1 && nalu.type <= 5)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        Box asBox() const
        {
            return Box{nalus.front().headerOffset(), totalLength(), 0};
        }
        size_t totalLength() const
        {
            size_t n = 0;
            for (const NALU& nalu : nalus)
            {
                n += nalu.totalLength();
            }
            return n;
        }
    };
#endif

    // ---------------------- file scanning utils --------------------------

    inline void dumpAtoms(std::istream& file, size_t offset, size_t end, const size_t indent = 1)
    {
        while(offset < end)
        {
            MP4Atom atom = readAtomAtOffset(file, offset);
            for (size_t i = 0; i < indent; ++i)
            {
                std::cout << " ";
            }
            std::cout << "- '" << atom.typeString() << "' @" << std::hex << offset
                      << " length: " << atom.content_length
                      << " end: " << atom.endOffset()
                      << std::endl;
            if (atom.isContainer())
            {
                dumpAtoms(file, atom.content_offset + atom.childOffset(), atom.endOffset(), indent + 1);
            }
            offset = atom.endOffset();
        }
        if (offset > end)
        {
            //std::cout << "warning: file truncated" << std::endl;
        }
    }

    inline void iterateAtoms(std::istream& file, size_t offset, size_t end, std::function<void(MP4Atom)> f)
    {
        while(offset < end)
        {
            MP4Atom atom = readAtomAtOffset(file, offset);
            f(atom);
            if (atom.isContainer())
            {
                iterateAtoms(file, atom.content_offset + atom.childOffset(), atom.endOffset(), f);
            }
            offset = atom.endOffset();
        }
    }

    template<typename T>
    struct TreeNode
    {
        TreeNode(const T& data, TreeNode* parent = NULL, size_t depth = 0)
        : data(data)
        , parent(parent)
        , depth(depth)
        {}
        TreeNode& add_child(const T& data)
        {
            return *children.insert(children.end(), TreeNode(data, this, depth + 1));
        }
        bool isRoot() const
        {
            return parent == NULL;
        }
        template<typename F>
        const TreeNode* find(F predicate) const
        {
            if (predicate(*this))
            {
                return this;
            }
            else
            {
                for (const TreeNode& child : children)
                {
                    if (const TreeNode* found = child.find(predicate))
                    {
                        return found;
                    }
                }
            }
            return NULL;
        }
        const TreeNode* find_type(const std::string& type) const
        {
            return find([&type](const TreeNode<MP4Atom>& node)
                {
                    return node.data.typeString() == type;
                });
        }

        T data;
        std::list<TreeNode> children;
        TreeNode* parent;
        size_t depth;
    };

    struct AtomWalker
    {
        AtomWalker(std::istream& file, std::optional<size_t> file_size = std::nullopt)
        : file(file)
        , offset(0)
        , tree(MP4Atom{0, file_size.value_or(fileLength(file)), 0, *reinterpret_cast<const uint32_t*>("file")})
        , currentNode(&tree)
        {
        }
        MP4Atom at(const std::string& type)
        {
            if (auto found = find(type))
            {
                return *found;
            }
            else
            {
                throw std::out_of_range{"atom not found"};
            }
        }
        std::optional<MP4Atom> find(const std::string& type)
        {
            if (const TreeNode<MP4Atom>* found = tree.find_type(type))
            {
                return found->data;
            }
            while (next())
            {
                if (top().typeString() == type)
                {
                    return top();
                }
            }
            return std::nullopt;
        }
        const TreeNode<MP4Atom>& root() {return tree;}
        bool next()
        {
            while (!currentNode->isRoot() && offset >= top().endOffset())
            {
                currentNode = currentNode->parent;
            }
            if (offset >= top().endOffset())
            {
                return false;
            }
            // top must be a container... :)
            MP4Atom atom = readAtomAtOffset(file, offset);
            currentNode = &currentNode->add_child(atom);
            if (top().isContainer())
            {
                offset = top().content_offset + top().childOffset();
            }
            else
            {
                offset = top().endOffset();
            }
            return true;
        }
        const MP4Atom& top()
        {
            return currentNode->data;
        }
        void printPath() const
        {
            bool first = true;
            for (const TreeNode<MP4Atom>* node = currentNode; node != NULL; node = node->parent)
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    std::cout << ".";
                }
                std::cout << node->data.typeString();
            }
            std::cout << std::endl;
        }
        std::istream& file;
        size_t offset;
        TreeNode<MP4Atom> tree;
        TreeNode<MP4Atom>* currentNode;
    };

    static inline void dumpAtoms(std::istream& file)
    {
        file.seekg(0, std::ios_base::end);
        size_t length = file.tellg();
        dumpAtoms(file, 0, length);
    }

    static inline void iterateAtoms(std::istream& file, std::function<void(MP4Atom)> f)
    {
        file.seekg(0, std::ios_base::end);
        size_t length = file.tellg();
        iterateAtoms(file, 0, length, f);
    }
}
// todo:
// scan for mdat and remember moov if encountered
// on request locate moov (may be after mdat) and extract sps/pps
// on request extract NALUs from mdat (i.e. getNextMdatNALUSize/getNextMdatNALUDataInto until done)
