#pragma once

extern "C"
{
#include <graphics/graphics.h>
}

#include <cstdlib>

namespace MosaicExample
{
    inline void * graphicsMalloc(gp_size_t size, void * data)
    {
        (void)data;

        return std::malloc(size);
    }

    inline void * graphicsRealloc(void * pointer, gp_size_t size, void * data)
    {
        (void)data;

        return std::realloc(pointer, size);
    }

    inline void graphicsFree(void * pointer, void * data)
    {
        (void)data;

        std::free(pointer);
    }

    inline gp_graphics_t * createGraphics()
    {
        gp_graphics_t * graphics = nullptr;

        if(gp_graphics_create(&graphics, &graphicsMalloc, &graphicsRealloc, &graphicsFree, nullptr,
               GP_CAPACITY_DEFAULT, GP_CAPACITY_DEFAULT, GP_CAPACITY_DEFAULT) == GP_FAILURE)
        {
            return nullptr;
        }

        return graphics;
    }
}
