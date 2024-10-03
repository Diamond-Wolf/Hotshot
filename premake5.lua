newoption {
	trigger = "build-editor",
	description = "Build with internal editor"
}

newoption {
	trigger = "texmap",
	description = "Select texmapper to use",
	allowed = {
		{"descent1", "Use D1 texmapper in both games"},
		{"descent2", "Use D2 texmapper in both games"},
		{"original", "Use each game's original texmapper"},
		{"swap", "Use the other game's texmapper"}
	},
	default = "original"
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
	cppdialect "C++14"
	kind "WindowedApp"
	location "./build"
	includedirs "."

	buildoptions { "-funsigned-char", "-Wno-dangling-else", "-Wno-invalid-source-encoding", "-Wno-writable-strings", "-Wno-parentheses", "-Wno-parentheses-equality" }

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
		"main_shared/*.cpp",
		"main_shared/*.h",
		"mem/*.cpp",
		"mem/*.h",
		"misc/*.c",
		"misc/*.cpp",
		"misc/*.h",
		"platform/*.cpp",
		"platform/*.h",
		"platform/fluidsynth/*.cpp",
		"platform/fluidsynth/*.h",
		"platform/openal/*.cpp",
		"platform/openal/*.h",
		"platform/sdl/*.cpp",
		"platform/sdl/*.h",
		"texmap/*.h",
		"vecmat/*.cpp",
		"vecmat/*.h"
	}
	
	-- Game-specific overrides
	removefiles {
		"2d/font.cpp",
		"2d/scale.cpp",
		"2d/scale_blend.cpp",
		"2d/font_d2.cpp",
		"2d/scale_d2.cpp"
	}
	
	filter { "options:build-editor" }
		files {
			"ui/*.cpp",
			"ui/*.h"
		}

	filter { "system:windows" }
		files {
			"platform/win32/**.cpp",
			"platform/win32/**.h"
	 	}
		
		defines {
			"WIN32",
			"_WIN32"
		}

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
		openal .. "/lib",
		fluidsynth .. "/lib"
	}
	
	includedirs {
		sdl2 .. "/include/SDL2",
		openal .. "/include",
		fluidsynth .. "/include",
	}
	
	links {
		"fluidsynth",
		"openal",
		"sdl2",
		"sdl2main"
	}
	
	defines {
		"USE_SDL",
		"USE_OPENAL",
		"USE_FLUIDSYNTH"
	}
	
	texmapList1 = {
		"texmap/ntmap.cpp",
		"texmap/scanline.cpp",
		"texmap/tmapflat.cpp"
	}

	texmapList2 = {
		"texmap/ntmap_d2.cpp",
		"texmap/scanline_d2.cpp",
		"texmap/tmapflat_d2.cpp"
	}

project "Hotshot-1"

	defines {
		"BUILD_DESCENT1"
	}

	files {
		"2d/font.cpp",
		"2d/scale.cpp",
		"main_d1/*.cpp",
		"main_d1/*.h"
	}
	
	removefiles {
		"main_d1/effects.cpp",
		"main_d1/wall.cpp",
		"main_d1/wall.h"
	}
	
	filter { "options:build-editor" }
		files {
			"main_d1/editor/*.cpp",
			"main_d1/editor/*.h"
		}
		
	filter { "options:texmap=descent1 or texmap=original" }
		files {texmapList1}
		
	filter { "options:texmap=descent2 or texmap=swap" }
		files {texmapList2}
		

project "Hotshot-2"

	defines {
		"BUILD_DESCENT2"
	}

	files {
		"2d/font_d2.cpp",
		"2d/scale_d2.cpp",
		"main_d2/*.cpp",
		"main_d2/*.h",
		"mve/*.cpp",
		"mve/*.h"
	}

	removefiles {
		"main_d2/i_main.cpp",
		"main_d2/wall.cpp",
		"main_d2/wall.h"
	}

	filter { "options:build-editor" }
		files {
			"main_d2/editor/*.cpp",
			"main_d2/editor/*.h"
		}
		
	filter { "options:texmap=descent2 or texmap=original" }
		files {texmapList2}
		
	filter { "options:texmap=descent1 or texmap=swap" }
		files {texmapList1}