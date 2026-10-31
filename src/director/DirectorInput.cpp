#include "DirectorInput.h"

namespace RetroPvP {

DirectorInput ReadDirectorInput(retro_input_state_t getInputState, unsigned port)
{
	DirectorInput input;
	if(!getInputState) {
		return input;
	}

	auto pressed = [getInputState, port](unsigned button) {
		return getInputState(port, RETRO_DEVICE_JOYPAD, 0, button) != 0;
	};
	input.spawnHeld = pressed(RETRO_DEVICE_ID_JOYPAD_SELECT);
	input.actionAHeld = pressed(RETRO_DEVICE_ID_JOYPAD_A);
	input.actionBHeld = pressed(RETRO_DEVICE_ID_JOYPAD_B);
	input.up = pressed(RETRO_DEVICE_ID_JOYPAD_UP);
	input.down = pressed(RETRO_DEVICE_ID_JOYPAD_DOWN);
	input.faster = pressed(RETRO_DEVICE_ID_JOYPAD_LEFT);
	input.slower = pressed(RETRO_DEVICE_ID_JOYPAD_RIGHT);
	input.previousTargetHeld = pressed(RETRO_DEVICE_ID_JOYPAD_L);
	input.nextTargetHeld = pressed(RETRO_DEVICE_ID_JOYPAD_R);
	input.takeoverHeld = pressed(RETRO_DEVICE_ID_JOYPAD_X);
	return input;
}

}
