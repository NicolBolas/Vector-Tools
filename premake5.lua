--Main Premake5 files

workspace "vector_tools"
	configurations { "Debug", "Release" }

project "vector tools"
	kind "ConsoleApp"
	language "C++"
	targetdir "bin"
	targetname "vector_tools"
	
	includedirs { "include" }
	
	files { "src/*.cpp", "src/*.hpp", "src/*.h" }
	files { "include/vector_tools/*.hpp", "include/vector_tools/*.h" }
	
	vpaths { Headers = {"**.h", "**.hpp"}, Source = "**.cpp" }

	flags {"C++14"}

	filter "configurations:Debug"
		defines { "DEBUG", "_DEBUG", "MEMORY_DEBUGGING" }
		symbols "On"
		targetsuffix "D"
		objdir("Debug")

	filter "configurations:Release"
		defines { "NDEBUG", "RELEASE" }
		optimize "On"
		objdir("Release")
		warnings "Extra"
		editandcontinue "Off"
