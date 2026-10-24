/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **/

/*
 * FileSystemPath.cpp
 *
 * PS5: everything the emulator writes lives under /data/supermodel/; the
 * title's own files (Games.xml, the assets) from its folder (PS5AppTitleDir).
 */

#include "OSD/FileSystemPath.h"
#include "PS5App.h"

#include <string>
#include <sys/stat.h>
#include <sys/types.h>

namespace FileSystemPath
{
  bool PathExists(std::string fileSystemPath)
  {
    struct stat pathInfo;
    return stat(fileSystemPath.c_str(), &pathInfo) == 0 && S_ISDIR(pathInfo.st_mode);
  }

  int MakeDir(std::string dir)
  {
    if (!PathExists(dir))
      return mkdir(dir.c_str(), 0777);
    return 0;
  }

  std::string GetPath(PathType pathType)
  {
    const char *name = "";
    switch (pathType)
    {
    case Analysis:    name = "Analysis"; break;
    case Config:      name = "Config"; break;
    case Log:         name = "Log"; break;
    case NVRAM:       name = "NVRAM"; break;
    case Saves:       name = "Saves"; break;
    case Screenshots: name = "Screenshots"; break;
    case Assets:      return std::string(PS5AppTitleDir()) + "/Assets/";
    }
    std::string path = std::string(PS5_APP_DIR) + "/" + name;
    MakeDir(path);
    return path + "/";
  }

  std::string ResolvePath(const std::string &path)
  {
    if (path.empty() || path[0] == '/')
      return path;
    return std::string(PS5_APP_DIR) + "/" + path;
  }

  std::string GetBasePath()
  {
    return std::string(PS5_APP_DIR) + "/";
  }

  std::string GetDefaultRomPath()
  {
    return PS5_APP_ROMS;
  }
}
