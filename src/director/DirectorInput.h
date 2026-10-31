#pragma once

#include "../Libretro/libretro.h"

namespace RetroPvP {

struct DirectorInput {
	bool spawnHeld = false;
	bool actionAHeld = false;
	bool actionBHeld = false;
	bool up = false;
	bool down = false;
	bool slower = false;
	bool faster = false;
	bool previousTargetHeld = false;
	bool nextTargetHeld = false;
	bool takeoverHeld = false;
};

DirectorInput ReadDirectorInput(retro_input_state_t getInputState, unsigned port);

}
