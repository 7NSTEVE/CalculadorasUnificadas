#include "Snapshot.hpp"

namespace casioemu
{
	std::uint64_t StateHash(const void *data, std::size_t size)
	{
		// FNV-1a de 64 bits. No es criptografico y no pretende serlo: aqui solo hace
		// falta cazar un volcado truncado o de otra ROM, no resistir a un atacante.
		std::uint64_t hash = 1469598103934665603ull;
		const unsigned char *bytes = static_cast<const unsigned char *>(data);
		for (std::size_t ix = 0; ix != size; ++ix)
		{
			hash ^= bytes[ix];
			hash *= 1099511628211ull;
		}
		return hash;
	}
}
