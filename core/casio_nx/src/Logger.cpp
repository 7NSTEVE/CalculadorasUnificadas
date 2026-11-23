#include "Logger.hpp"

#include <cstdarg>
#include <cstdio>

namespace casioemu
{
	namespace logger
	{
		/**
		 * El original se apoyaba en readline para redibujar el prompt de la consola
		 * despues de cada mensaje. Aqui no hay consola interactiva: en el banco de
		 * pruebas esto va a stderr y en iOS no se usa, asi que basta con imprimir.
		 *
		 * Va a stderr y no a stdout a proposito: el banco de pruebas escribe
		 * resultados por stdout y no conviene mezclarlos.
		 */
		void Info(const char *format, ...)
		{
			va_list args;
			va_start(args, format);
			std::vfprintf(stderr, format, args);
			va_end(args);
		}
	}
}
