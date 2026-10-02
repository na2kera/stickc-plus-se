#pragma once
#include <M5GFX.h>
#include "model.hpp"
namespace mini {
void drawScreen(lgfx::LGFXBase& canvas, const App& app, uint32_t now, bool boardReady, bool storageError);
}
