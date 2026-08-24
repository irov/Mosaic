#pragma once

#include <Mosaic/Mosaic.hpp>

namespace MosaicExample
{
    class EditorPersistence final : public Mosaic::Serializer, public Mosaic::Deserializer
    {
    public:
        EditorPersistence(Mosaic::PlatformAdapter & platform, Mosaic::String path);

        void begin(uint32_t version) override;
        void write(Mosaic::StringView section, Mosaic::Id id, Mosaic::StringView key, Mosaic::StringView value) override;
        void end() override;

        [[nodiscard]] uint32_t version() const noexcept override;
        void forEach(void (*visitor)(Mosaic::StringView section, Mosaic::Id id, Mosaic::StringView key, Mosaic::StringView value, void * data), void * data) const override;

        [[nodiscard]] bool load();
        [[nodiscard]] Mosaic::StringView status() const noexcept;

    private:
        struct Record
        {
            Mosaic::String section;
            Mosaic::Id id = Mosaic::InvalidId;
            Mosaic::String key;
            Mosaic::String value;
        };

        using RecordVector = Mosaic::Vector<Record>;

        Mosaic::PlatformAdapter & m_platform;
        Mosaic::String m_path;
        Mosaic::String m_status = "Layout has not been saved yet";
        RecordVector m_records;
        uint32_t m_version = 1;
    };
} // namespace MosaicExample
