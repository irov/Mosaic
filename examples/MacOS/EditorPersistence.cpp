#include "EditorPersistence.hpp"
#include "Charconv.hpp"

#include <limits>
#include <utility>

namespace MosaicExample
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        void appendUnsigned(Mosaic::String & output, uint64_t value)
        {
            Mosaic::Array<char, 32> buffer;
            Mosaic::Detail::ToCharsResult conversion = Mosaic::Detail::toChars(buffer.data(), buffer.data() + buffer.size(), value);

            if(conversion.ec != std::errc{})
            {
                return;
            }

            output.append(buffer.data(), conversion.ptr);
        }
        //////////////////////////////////////////////////////////////////////////
        void appendQuoted(Mosaic::String & output, Mosaic::StringView value)
        {
            output.push_back('"');
            for(char character : value)
            {
                if(character == '"' || character == '\\')
                {
                    output.push_back('\\');
                }

                output.push_back(character);
            }
            output.push_back('"');
        }
        //////////////////////////////////////////////////////////////////////////
        void skipWhitespace(Mosaic::StringView input, size_t & offset) noexcept
        {
            while(offset != input.size())
            {
                char character = input[offset];

                if(character != ' ' && character != '\t' && character != '\r' && character != '\n')
                {
                    break;
                }

                ++offset;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseUnsigned(Mosaic::StringView input, size_t & offset, uint64_t * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            Detail::skipWhitespace(input, offset);
            const char * begin = input.data() + offset;
            const char * end = input.data() + input.size();
            uint64_t parsedValue = 0;
            Mosaic::Detail::FromCharsResult conversion = Mosaic::Detail::fromChars(begin, end, parsedValue);

            if(conversion.ec != std::errc{})
            {
                return false;
            }

            if(conversion.ptr == begin)
            {
                return false;
            }

            offset = static_cast<size_t>(conversion.ptr - input.data());

            *_out = parsedValue;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseQuoted(Mosaic::StringView input, size_t & offset, Mosaic::String * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            Detail::skipWhitespace(input, offset);

            if(offset == input.size())
            {
                return false;
            }

            if(input[offset] != '"')
            {
                return false;
            }

            ++offset;
            Mosaic::String parsedValue;
            while(offset != input.size())
            {
                char character = input[offset++];

                if(character == '"')
                {

                    *_out = std::move(parsedValue);

                    return true;
                }

                if(character == '\\')
                {
                    if(offset == input.size())
                    {
                        return false;
                    }

                    parsedValue.push_back(input[offset++]);
                    continue;
                }

                parsedValue.push_back(character);
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::ByteSpan bytes(Mosaic::StringView value) noexcept
        {
            const std::byte * data = reinterpret_cast<const std::byte *>(value.data());
            Mosaic::ByteSpan result(data, value.size());

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    EditorPersistence::EditorPersistence(Mosaic::PlatformAdapter & platform, Mosaic::String path) : m_platform(platform), m_path(std::move(path))
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void EditorPersistence::begin(uint32_t version)
    {
        m_version = version;
        m_records.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    void EditorPersistence::write(Mosaic::StringView section, Mosaic::Id id, Mosaic::StringView key, Mosaic::StringView value)
    {
        Record record;
        record.section.assign(section);
        record.id = id;
        record.key.assign(key);
        record.value.assign(value);
        m_records.emplace_back(std::move(record));
    }
    //////////////////////////////////////////////////////////////////////////
    void EditorPersistence::end()
    {
        Mosaic::String contents;
        Detail::appendUnsigned(contents, m_version);
        contents.push_back('\n');
        for(const Record & record : m_records)
        {
            Detail::appendQuoted(contents, record.section);
            contents.push_back(' ');
            Detail::appendUnsigned(contents, record.id);
            contents.push_back(' ');
            Detail::appendQuoted(contents, record.key);
            contents.push_back(' ');
            Detail::appendQuoted(contents, record.value);
            contents.push_back('\n');
        }
        Mosaic::ByteSpan data = Detail::bytes(contents);
        bool written = m_platform.writeFile(m_path, data);

        if(written == false)
        {
            m_status = "Could not write the layout file through the platform backend";

            return;
        }

        m_status = "Layout saved to ~/Library/Application Support/Mosaic";
    }
    //////////////////////////////////////////////////////////////////////////
    uint32_t EditorPersistence::version() const noexcept
    {
        return m_version;
    }
    //////////////////////////////////////////////////////////////////////////
    void EditorPersistence::forEach(void (*visitor)(Mosaic::StringView, Mosaic::Id, Mosaic::StringView, Mosaic::StringView, void *), void * data) const
    {
        for(const Record & record : m_records)
        {
            visitor(record.section, record.id, record.key, record.value, data);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool EditorPersistence::load()
    {
        Mosaic::ByteVector fileData;
        bool read = m_platform.readFile(m_path, &fileData);

        if(read == false)
        {
            m_status = "No saved layout exists yet";

            return false;
        }

        if(fileData.empty() == true)
        {
            m_status = "The saved layout header is invalid";

            return false;
        }

        const char * text = reinterpret_cast<const char *>(fileData.data());
        Mosaic::StringView contents(text, fileData.size());
        size_t offset = 0;
        uint64_t loadedVersion = 0;
        if(Detail::parseUnsigned(contents, offset, &loadedVersion) == false)
        {
            m_status = "The saved layout header is invalid";

            return false;
        }

        if(loadedVersion > std::numeric_limits<uint32_t>::max())
        {
            m_status = "The saved layout header is invalid";

            return false;
        }

        RecordVector loaded;
        while(offset != contents.size())
        {
            Detail::skipWhitespace(contents, offset);

            if(offset == contents.size())
            {
                break;
            }

            Record record;
            uint64_t id = 0;
            bool sectionParsed = Detail::parseQuoted(contents, offset, &record.section);
            bool idParsed = Detail::parseUnsigned(contents, offset, &id);
            bool keyParsed = Detail::parseQuoted(contents, offset, &record.key);
            bool valueParsed = Detail::parseQuoted(contents, offset, &record.value);

            if(sectionParsed == false)
            {
                m_status = "The saved layout contents are invalid";

                return false;
            }

            if(idParsed == false)
            {
                m_status = "The saved layout contents are invalid";

                return false;
            }

            if(keyParsed == false)
            {
                m_status = "The saved layout contents are invalid";

                return false;
            }

            if(valueParsed == false)
            {
                m_status = "The saved layout contents are invalid";

                return false;
            }

            record.id = id;
            loaded.emplace_back(std::move(record));
        }
        m_version = static_cast<uint32_t>(loadedVersion);
        m_records = std::move(loaded);
        m_status = "Layout loaded from disk";

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::StringView EditorPersistence::status() const noexcept
    {
        return m_status;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace MosaicExample
