#pragma once

#include "DirectorInput.h"
#include <memory>

class Emulator;
class IConsole;

namespace RetroPvP {
	enum class SpawnResult {
	None,
	NotInGame,
	Paused,
	ExistingBullet,
	NoFreeSlot,
	Spawned,
	TargetChanged,
	TakeoverEnabled,
	TakeoverDisabled
	};

	struct DirectorStatus {
	bool hasTarget = false;
	bool controlled = false;
	unsigned slot = 0;
	unsigned enemyId = 0;
	const char* label = "";
	int screenX = 0;
	int screenY = 0;
	};

class BulletBillDirector {
private:
	bool _supportedRom = false;
	bool _postFrameEnemyControl = false;
	unsigned _postFrameEnemySlot = 0;
	bool _postFrameEnemySetDirection = false;
	uint8_t _postFrameEnemyDirection = 0;
	uint8_t _postFrameEnemySpeed = 0;
	DirectorStatus _status;

public:
	void Reset();
	bool IsEnabled() const { return _supportedRom; }
	const DirectorStatus& GetStatus() const { return _status; }
	void OnGameLoaded(Emulator* emulator, const std::shared_ptr<IConsole>& console);
	SpawnResult RunFrame(Emulator* emulator, const std::shared_ptr<IConsole>& console, const DirectorInput& input);
	void RunPostFrame(const std::shared_ptr<IConsole>& console);
};

}
