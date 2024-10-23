newoption {
	trigger = "build-editor",
	description = "Build with internal editor (untested)"
}

newoption {
	trigger = "build-network",
	description = "Build with network support (untested)"
}

newoption {
	trigger = "sdl2-path",
	value = "path",
	description = "Path to SDL2 version, including trailing slash"
}

newoption {
	trigger = "openal-path",
	value = "path",
	description = "Path to OpenAL version, including trailing slash"
}

newoption {
	trigger = "fluidsynth-path",
	value = "path",
	description = "Path to FluidSynth version, including trailing slash"
}

workspace "Hotshot"
	configurations {"Debug", "Release"}
	language "C++"
	cppdialect "C++20"
	kind "WindowedApp"
	location "./build"
	includedirs "."

	unsignedchar "On"

	filter { "action:gmake2" }
		buildoptions { "-Wno-dangling-else", "-Wno-invalid-source-encoding", "-Wno-writable-strings", "-Wno-parentheses", "-Wno-parentheses-equality" }

	filter { "configurations:Debug" }
		symbols "On"
		defines "DEBUG"

	filter { "configurations:Release" }
		optimize "Full"
		defines "NDEBUG"

	--Don't name non-bundles ".app", Mac isn't smart enough to realize it's just a file
	filter { "system:macosx" }
		targetextension ""

	filter {}

	files {
		"2d/*.h",
		"2d/*.cpp",
		"3d/*.h",
		"3d/*.cpp",
		"cfile/*.cpp",
		"cfile/*.h",
		"fix/*.cpp",
		"fix/*.h",
		"iff/*.cpp",
		"iff/*.h",
		"main/*.cpp",
		"main/*.h",
		"mem/*.cpp",
		"mem/*.h",
		"misc/*.c",
		"misc/*.cpp",
		"misc/*.h",
		"mve/*.cpp",
		"mve/*.h",
		"platform/*.cpp",
		"platform/*.h",
		"platform/fluidsynth/*.cpp",
		"platform/fluidsynth/*.h",
		"platform/openal/*.cpp",
		"platform/openal/*.h",
		"platform/sdl/*.cpp",
		"platform/sdl/*.h",
		"texmap/*.cpp",
		"texmap/*.h",
		"vecmat/*.cpp",
		"vecmat/*.h"
	}
	
	filter { "options:build-editor" }
		files {
			"ui/*.cpp",
			"ui/*.h"
		}
		defines "EDITOR"
		
	filter { "options:build-network" }
		defines "NETWORK"

	filter { "system:windows" }
		files {
			"platform/win32/**.cpp",
			"platform/win32/**.h"
	 	}
		
		defines {
			"WIN32",
			"_WIN32"
		}

		architecture "x86_64" --isn't this supposed to set the architecture to x86_64? Not "default" which is just regular x86?

	filter { "system:bsd or linux or macosx" }
		files {
			"platform/unix/**.cpp",
			"platform/unix/**.h"
		}
		
		defines {
			"UNIX",
			"_UNIX"
		}
		
	filter {}
	
	--sdl2 = os.findlib("sdl2")
	--openal = os.findlib("openal")
	--fluidsynth = os.findlib("fluidsynth")
	
	sdl2 = _OPTIONS["sdl2-path"]
	openal = _OPTIONS["openal-path"]
	fluidsynth = _OPTIONS["fluidsynth-path"]
	
	libdirs {
		sdl2 .. "/lib",
		sdl2 .. "/lib/x64",
		openal .. "/lib",
		openal .. "/libs/Win64",
		fluidsynth .. "/lib"
	}
	
	includedirs {
		sdl2 .. "/include",
		sdl2 .. "/include/SDL2",
		openal .. "/include",
		fluidsynth .. "/include",
	}
	
	links {
		"sdl2",
		"sdl2main"
	}
	
	filter { "system:not windows" }
		links {
			"fluidsynth",
			"openal",
		}
	
	filter { "system:windows" }

		links "OpenAL32"

		--is it bad when you have to tell the engine to override itself to use the
		--right file, rather than something simple like specifying the right file?
		--AND IT DOESNT EVEN WORK
		--[[premake.override(premake.tools.msc, "getLibraryExtensions", 
			function(oldfn)
				local extensions = oldfn()
				extensions["a"] = true
				extensions["lib"] = false
				return extensions
			end
		)
			
		links "libfluidsynth"
		]]

	filter {}
	
	defines {
		"USE_SDL",
		"USE_OPENAL",
		"USE_FLUIDSYNTH"
	}

project "Hotshot"

	defines "BUILD_DESCENT2"

	filter { "options:build-editor" }
		files {
			"main/editor/*.cpp",
			"main/editor/*.h"
		}