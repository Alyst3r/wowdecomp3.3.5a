#ifndef WOW_SOURCE_DB_REC_OCCLUSIONVOLUMEPOINTREC_H
#define WOW_SOURCE_DB_REC_OCCLUSIONVOLUMEPOINTREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class OcclusionVolumePointRec
{
public:
    static const int32_t s_numColumns = 5;
    static const int32_t s_rowSize = 20;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id = -1;
    /* 0004 */ int32_t m_occlusionID = -1;
    /* 0008 */ float m_positionX = 0.f;
    /* 000C */ float m_positionY = 0.f;
    /* 0010 */ float m_positionZ = 0.f;

    OcclusionVolumePointRec();
    ~OcclusionVolumePointRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
