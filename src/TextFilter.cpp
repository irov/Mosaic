#include "Mosaic/Mosaic.hpp"

#include <cctype>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool containsIgnoringCase(StringView value, StringView pattern) noexcept
        {
            if(pattern.empty() == true)
            {
                return true;
            }

            if(pattern.size() > value.size())
            {
                return false;
            }

            for(size_t offset = 0; offset + pattern.size() <= value.size(); ++offset)
            {
                bool matches = true;

                for(size_t index = 0; index != pattern.size(); ++index)
                {
                    unsigned char valueCharacter = static_cast<unsigned char>(value[offset + index]);
                    unsigned char patternCharacter = static_cast<unsigned char>(pattern[index]);

                    if(std::tolower(valueCharacter) != std::tolower(patternCharacter))
                    {
                        matches = false;
                        break;
                    }
                }

                if(matches == true)
                {
                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        void appendFilterTerm(TextFilter * filter, StringView term)
        {
            bool exclude = term.front() == '-';

            if(exclude == true)
            {
                term.remove_prefix(1);
            }

            if(term.empty() == true)
            {
                return;
            }

            if(exclude == true)
            {
                filter->excludes.emplace_back(term);
            }
            else
            {
                filter->includes.emplace_back(term);
            }
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    void setTextFilter(TextFilter * filter, StringView expression)
    {
        if(filter == nullptr)
        {
            return;
        }

        filter->expression.assign(expression);
        filter->includes.clear();
        filter->excludes.clear();
        size_t begin = 0;

        while(begin <= expression.size())
        {
            size_t separator = expression.find(',', begin);
            size_t end = separator == StringView::npos ? expression.size() : separator;

            while(begin < end && std::isspace(static_cast<unsigned char>(expression[begin])) != 0)
            {
                ++begin;
            }

            while(end > begin && std::isspace(static_cast<unsigned char>(expression[end - 1])) != 0)
            {
                --end;
            }

            StringView term = expression.substr(begin, end - begin);

            if(term.empty() == false)
            {
                Detail::appendFilterTerm(filter, term);
            }

            if(separator == StringView::npos)
            {
                break;
            }

            begin = separator + 1;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void clearTextFilter(TextFilter * filter) noexcept
    {
        if(filter == nullptr)
        {
            return;
        }

        filter->expression.clear();
        filter->includes.clear();
        filter->excludes.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    bool textFilterActive(const TextFilter & filter) noexcept
    {
        auto returnedValue = filter.includes.empty() == false || filter.excludes.empty() == false;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool textFilterPasses(const TextFilter & filter, StringView value) noexcept
    {
        for(const String & term : filter.excludes)
        {
            if(Detail::containsIgnoringCase(value, term) == true)
            {
                return false;
            }
        }

        if(filter.includes.empty() == true)
        {
            return true;
        }

        for(const String & term : filter.includes)
        {
            if(Detail::containsIgnoringCase(value, term) == true)
            {
                return true;
            }
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool textFilterPasses(StringView value, StringView expression) noexcept
    {
        TextFilter filter;
        Mosaic::setTextFilter(&filter, expression);
        auto returnedValue = Mosaic::textFilterPasses(filter, value);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
