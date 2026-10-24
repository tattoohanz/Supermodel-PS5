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
 * GLStubs.cpp
 *
 * PS5: Vulkan only. CModel3 holds a SuperAA pointer for the OpenGL renderers
 * and calls Draw() through it when one is attached; on the PS5 none ever is.
 */

#include "Graphics/SuperAA.h"

void SuperAA::Draw()
{
}
