#------------------------------------------------------------------------------
# Compiler flags
#------------------------------------------------------------------------------
set(DEBUG_GCC_FLAGS
	-g -Og
	-Wall -Wextra -Wconversion -Wno-unused-parameter
	-Wfatal-errors
	-DOM_DEBUG
	-DOM_PROFILER_ENABLED
)

set(RELEASE_GCC_FLAGS
	-O3
	-Wall -Wextra -Wconversion -Wno-unused-parameter
	-Wfatal-errors
)

set(RELWITHDEB_GCC_FLAGS
	-g -O3
	-Wall -Wextra -Wconversion -Wno-unused-parameter
	-Wfatal-errors
)

#------------------------------------------------------------------------------
# Platform-specific flags
#------------------------------------------------------------------------------
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CLANG_FLAGS
		-Wno-sign-conversion
		-Wno-implicit-int-float-conversion
		-Wno-nested-anon-types
	)
elseif (MSVC)
	# Disable iterator debug, improves performance of debug builds
	add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)

	# The sources are UTF-8 (without BOM) and contain Chinese comments / string
	# literals. Without this flag MSVC reads them using the system ANSI codepage
	# (e.g. 936/GBK), which corrupts the comments (a trailing multibyte sequence
	# can swallow the line break and break the following declaration) and would
	# also encode narrow string literals as GBK instead of UTF-8 (SFML expects
	# UTF-8). GCC/Clang already treat sources and literals as UTF-8, so this only
	# brings MSVC in line with the MinGW build.
	add_compile_options(/utf-8)
endif()

#------------------------------------------------------------------------------
# Additional setup
#------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)

include_directories(external)
