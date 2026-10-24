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
 * PS5App.h
 *
 * The PS5 title's console glue: the jailbreak request (for /data), the log in
 * /data/supermodel/, the imports the console left unresolved, and toasts.
 */

#ifndef INCLUDED_PS5APP_H
#define INCLUDED_PS5APP_H

#define PS5_APP_TITLE_ID "PPSA97374"
#define PS5_APP_DIR      "/data/supermodel"
#define PS5_APP_LOG      PS5_APP_DIR "/Supermodel.log"
#define PS5_APP_ROMS     PS5_APP_DIR "/roms"

// Jailbreak, the /data/supermodel/ folders, the log there (stderr and stdout),
// the unresolved-import report. False if the log could not be opened (a toast
// says why).
bool PS5AppInit();

// A system notification (toast).
void PS5Notify(const char *message);

// The system shows the title's launch picture over VideoOut until the app
// takes it down: after the first frame the app presents (the menu's, or the
// game's when it starts straight away). Logs what that was.
void PS5HideSplashOnce(const char *what);

// Seconds since PS5AppInit, for log timestamps.
double PS5AppSeconds();

// The title's own folder, for Games.xml and the assets: /app0 while the app
// is still in its sandbox, /data/homebrew/PPSA97374 once the jailbreak has
// moved its root (where /app0 no longer exists). Set by PS5AppInit.
const char *PS5AppTitleDir();

#endif  // INCLUDED_PS5APP_H
