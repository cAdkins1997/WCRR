#pragma once
#include "../common.h"


namespace pakl
{
    enum SectionType : u8
    {
        Scene, Meshes, Textures
    };

    enum EntryType : u8
    {

    };

    struct Section
    {
        SectionType type;
        u64 offset, size;
        u32 flags, entryCount, metaDataSize;
    };

    struct Entry
    {
        EntryType type;
        u64 offset, size;
        u32 nameIndex, ID;
    };

    struct Header
    {
        u8 identifier[12];
        u64 timestamp;
        u32 version;
        u32 flags;
        u32 sectionCount;

        std::vector<Section> sections;
        std::vector<Entry> entries;
        std::vector<u8> data;
    };
}