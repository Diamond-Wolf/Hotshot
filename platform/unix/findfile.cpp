/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <sys/types.h>
//#include <dirent.h>

#include <string>
#include <filesystem>
#include <cstring>

#include "platform/mono.h"
#include "platform/findfile.h"
#include "platform/posixstub.h"
#include "platform/platform_filesys.h"

namespace fs = std::filesystem;

char searchStr[13];
//DIR *currentDir;
//fs::path currentPath;
fs::directory_iterator currentDir;
fs::directory_iterator end;

int	FileFindFirst(const char* search_str, FILEFINDSTRUCT* ffstruct) {
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	char dir[CHOCOLATE_MAX_FILE_PATH_SIZE];
	char temp_search_path[CHOCOLATE_MAX_FILE_PATH_SIZE];
	char *separator_pos;
#else
	char dir[256];
#endif
	char full_search_str[CHOCOLATE_MAX_FILE_PATH_SIZE];
	const char *search;

	//Make sure the current search buffer is clear
	memset(searchStr, 0, 13);
	memset(dir, 0, 256);

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
	get_full_file_path(dir, "");
	get_platform_localized_interior_path(temp_search_path, search_str);
	memset(full_search_str, 0, CHOCOLATE_MAX_FILE_PATH_SIZE);
	snprintf(full_search_str, CHOCOLATE_MAX_FILE_PATH_SIZE, "%s%s", dir, temp_search_path);
#else
	snprintf(full_search_str, CHOCOLATE_MAX_FILE_PATH_SIZE, "%s", search_str);
#endif

	//Open the directory for searching
	_splitpath(full_search_str, NULL, dir, NULL, NULL);
	if (strlen(dir) == 0) //godawful hack
	{
#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
		strncpy(dir, full_search_str, CHOCOLATE_MAX_FILE_PATH_SIZE);
		separator_pos = strrchr(dir, PLATFORM_PATH_SEPARATOR);
		if (separator_pos != NULL && separator_pos - dir > 0)
		{
			dir[separator_pos - dir + 1] = '\0';
		}
#else
		dir[0] = '.';
#endif
	}

	search = strrchr(search_str, '*');
	strncpy(searchStr, search+2, 12);

	if (!(fs::exists(dir) && fs::is_directory(dir))) {
		printf("Checking non-directory %s for %s", dir, search_str);
		return 1;
	}

	currentDir = fs::directory_iterator(dir);

	return FileFindNext(ffstruct);

}

/*inline std::string AsLower(const std::string& s) {
	std::string s2 = s;
	for (char& c : s2)
		c = std::tolower(c);

	return s2;
}*/

inline bool CompareExt(const fs::path& path, const std::string& comp) {
	
	std::string check = path.extension().string();
	if (check.length() == 0)
		return false;

	if (check.c_str()[0] == '.')
		check = check.substr(1);

	auto cl = check.length();

	if (cl != comp.length())
		return false;

	return !_strnicmp(check.c_str(), comp.c_str(), cl);

}

int	FileFindNext(FILEFINDSTRUCT* ffstruct) {
	
	while (currentDir != end) {

		auto& entry = *currentDir;
		currentDir++;

		auto path = entry.path();

		if (path.extension().string().length() == 0)
			continue;

		if (CompareExt(path, searchStr)) {
			strncpy(ffstruct->name, path.filename().c_str(), FF_PATHSIZE);
			ffstruct->size = static_cast<uint32_t>(entry.file_size());
			ffstruct->type = FF_TYPE_FILE;
			return 0;
		}
	};

	return 1;

}

int	FileFindClose(void) {}
