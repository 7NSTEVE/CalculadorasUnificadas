#include "ModelInfo.hpp"

#include "../Emulator.hpp"
#include "../Logger.hpp"
#include "ColourInfo.hpp"
#include "SpriteInfo.hpp"

namespace casioemu
{
	ModelInfo::ModelInfo(Emulator &_emulator, std::string _key)
		: emulator(_emulator), key(_key)
	{
	}

	ModelInfo::operator std::string()
	{
		auto iterator = emulator.model_strings.find(key);
		if (iterator == emulator.model_strings.end())
			PANIC("el modelo no define la cadena '%s'\n", key.c_str());
		return iterator->second;
	}

	ModelInfo::operator int()
	{
		auto iterator = emulator.model_ints.find(key);
		if (iterator == emulator.model_ints.end())
			PANIC("el modelo no define el entero '%s'\n", key.c_str());
		return iterator->second;
	}

	ModelInfo::operator SpriteInfo()
	{
		auto iterator = emulator.model_sprites.find(key);
		if (iterator == emulator.model_sprites.end())
			PANIC("el modelo no define el sprite '%s'\n", key.c_str());
		return iterator->second;
	}

	ModelInfo::operator ColourInfo()
	{
		auto iterator = emulator.model_colours.find(key);
		if (iterator == emulator.model_colours.end())
			PANIC("el modelo no define el color '%s'\n", key.c_str());
		return iterator->second;
	}
}
