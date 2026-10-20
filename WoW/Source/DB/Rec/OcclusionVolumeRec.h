#ifndef WOW_SOURCE_DB_REC_OCCLUSIONVOLUMEREC_H
#define WOW_SOURCE_DB_REC_OCCLUSIONVOLUMEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class OcclusionVolumeRec
{
public:
    static const int32_t s_numColumns = 4;
    static const int32_t s_rowSize = 16;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id = -1;
    /* 0004 */ int32_t m_name = -1; // just padding, in editor this is used as comment to make dbc file more readable
    /* 0008 */ int32_t m_mapID = -1;
    /* 000C */ int32_t m_lightID = -1;

    OcclusionVolumeRec();
    ~OcclusionVolumeRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
