#include "ModelInfo.hpp"

#include "../Emulator.hpp"
#include "SpriteInfo.hpp"
#include "ColourInfo.hpp"

namespace casioemux
{
    ModelInfo::ModelInfo(Emulator &_emulator, std::string _key)
        : emulator(_emulator), key(std::move(_key))
    {
    }

    ModelInfo::operator std::string()
    {
        auto it = emulator.model_strings.find(key);
        if (it != emulator.model_strings.end())
            return it->second;
        return {};
    }

    ModelInfo::operator int()
    {
        auto it = emulator.model_ints.find(key);
        if (it != emulator.model_ints.end())
            return it->second;
        return 0;
    }

    ModelInfo::operator SpriteInfo()
    {
        SpriteInfo s{};
        return s;
    }

    ModelInfo::operator ColourInfo()
    {
        ColourInfo c{20, 36, 58};
        return c;
    }
}
