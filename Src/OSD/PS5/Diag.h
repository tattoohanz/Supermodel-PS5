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
 * Diag.h
 *
 * PS5 diagnostics. When /data/supermodel/diag-dump.txt exists, 15, 30 and
 * 45 s into a session (Main.cpp) the renderer dumps
 * each stage of the picture - the two tile layers, the 3D scene, the frame
 * that is presented - to /data/supermodel/Screenshots/ as PNGs, and logs a
 * one-line summary of each ("diag: ..."), so a picture missing on the TV can
 * be traced to the stage that loses it.
 */

#ifndef INCLUDED_PS5_DIAG_H
#define INCLUDED_PS5_DIAG_H

// The tag of the dump due in this frame (such as "15s"), or nullptr.
const char *PS5DiagDumpTag();

#endif  // INCLUDED_PS5_DIAG_H
