newoption {
	trigger = "build-editor",
	description = "Build with internal editor (unmaintained)"
}

newoption {
	trigger = "build-network",
	description = "Build with network support (untested)"
}

newoption {
	trigger = "sdl-path",
	value = "path",
	description = "Path to SDL2 version, including trailing slash"
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

	--Assemble app bundle
	filter { "system:macosx" }
		targetextension ""
		--todo: Have clean remove the bundle, once I figure out what will let me do anything with clean
		filter { "system:macosx", "configurations:Debug" }
			postbuildcommands { "\"" .. _MAIN_SCRIPT_DIR .. "/platform/macos/bundle.sh\" \"" .. _MAIN_SCRIPT_DIR .. "\" Debug \"%{wks.location}\"" }
		filter { "system:macosx", "configurations:Release" }
			postbuildcommands { "\"" .. _MAIN_SCRIPT_DIR .. "/platform/macos/bundle.sh\" \"" .. _MAIN_SCRIPT_DIR .. "\" Release \"%{wks.location}\"" }
		
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
		"hqaudio/*.cpp",
		"hqaudio/*.h",
		"hqaudio/lib/*.cpp",
		"hqaudio/lib/*.h",
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
		"platform/tsfmidi/*.cpp",
		"platform/tsfmidi/*.h",
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
		
		linkoptions {
			"-rpath /usr/local/lib"
		}
		
	filter {}
	
	sdl = _OPTIONS["sdl-path"]
	
	libdirs {
		sdl,
		sdl .. "/lib",
		sdl .. "/lib/x64",
	}
	
	includedirs {
		sdl .. "/include",
		sdl .. "/include/SDL3",
	}
	
	links {
		"SDL3"
	}
	
	filter {}
	
	defines {
		"USE_SDL",
		"USE_TSFMIDI"
	}

project "Hotshot"

	defines "BUILD_DESCENT2"

	filter { "options:build-editor" }
		files {
			"main/editor/*.cpp",
			"main/editor/*.h"
		}
