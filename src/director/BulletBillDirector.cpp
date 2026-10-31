#include "BulletBillDirector.h"
#include "Smb1RomProfile.h"
#include "Smb1ProfileData.h"
#include "Shared/Emulator.h"
#include "Shared/Interfaces/IConsole.h"
#include "NES/NesConsole.h"
#include "NES/NesMemoryManager.h"
#include <algorithm>

namespace RetroPvP {

void BulletBillDirector::Reset()
{
	_supportedRom = false;
	_postFrameEnemyControl = false;
	_status = {};
}

void BulletBillDirector::OnGameLoaded(Emulator* emulator, const std::shared_ptr<IConsole>& console)
{
	Reset();
	if(!emulator || !console || console->GetConsoleType() != ConsoleType::Nes) {
		return;
	}
	_supportedRom = IsSupportedSmb1Rom(emulator->GetHash(HashType::Sha1));
	_postFrameEnemyControl = false;
	if(_supportedRom) {
		auto nesConsole = std::dynamic_pointer_cast<NesConsole>(console);
		if(nesConsole && nesConsole->GetMemoryManager()) {
			uint8_t* ram = nesConsole->GetMemoryManager()->GetInternalRam();
			if(ram) {
				ram[Smb1ProfileData::DirectorState] = 0;
			}
		}
	}
}

static bool HasExistingBulletBill(uint8_t* ram)
{
	for(unsigned slot = 0; slot < Smb1ProfileData::EnemySlots; ++slot) {
		if(ram[Smb1ProfileData::EnemyFlag + slot] == 0) {
			continue;
		}
		const uint8_t enemyId = ram[Smb1ProfileData::EnemyId + slot];
		if(enemyId == Smb1ProfileData::CannonBulletBillId || enemyId == Smb1ProfileData::FrenzyBulletBillId) {
			return true;
		}
	}
	return false;
}

static int FindFreeBillSlot(uint8_t* ram)
{
	for(unsigned slot = 0; slot < Smb1ProfileData::ControllableSlots; ++slot) {
		if(ram[Smb1ProfileData::EnemyFlag + slot] == 0) {
			return static_cast<int>(slot);
		}
	}
	return -1;
}

static bool IsBillOffscreenLeft(uint8_t* ram, unsigned slot)
{
	if(ram[Smb1ProfileData::EnemyMovingDirection + slot] != Smb1ProfileData::LeftDirection) {
		return false;
	}

	const uint16_t enemyWorldX = static_cast<uint16_t>(
		(static_cast<uint16_t>(ram[Smb1ProfileData::EnemyPage + slot]) << 8) |
		ram[Smb1ProfileData::EnemyX + slot]);
	const uint16_t screenLeftWorldX = static_cast<uint16_t>(
		(static_cast<uint16_t>(ram[Smb1ProfileData::ScreenLeftPage]) << 8) |
		ram[Smb1ProfileData::ScreenLeftX]);
	const int16_t xFromScreenLeft = static_cast<int16_t>(static_cast<uint16_t>(enemyWorldX - screenLeftWorldX));
	return xFromScreenLeft < -16;
}

static uint8_t SetHorizontalSpeedDirection(uint8_t currentSpeed, bool movingLeft)
{
	const int signedSpeed = currentSpeed < 0x80
		? static_cast<int>(currentSpeed)
		: static_cast<int>(currentSpeed) - 0x100;
	const int magnitude = std::max(8, signedSpeed < 0 ? -signedSpeed : signedSpeed);
	const int directedSpeed = movingLeft ? -magnitude : magnitude;
	return static_cast<uint8_t>(directedSpeed < 0 ? directedSpeed + 0x100 : directedSpeed);
}

static bool GetEnemyScreenPosition(uint8_t* ram, unsigned slot, int& screenX, int& screenY)
{
	const uint16_t enemyWorldX = static_cast<uint16_t>(
		(static_cast<uint16_t>(ram[Smb1ProfileData::EnemyPage + slot]) << 8) |
		ram[Smb1ProfileData::EnemyX + slot]);
	const uint16_t screenLeftWorldX = static_cast<uint16_t>(
		(static_cast<uint16_t>(ram[Smb1ProfileData::ScreenLeftPage]) << 8) |
		ram[Smb1ProfileData::ScreenLeftX]);
	screenX = static_cast<int16_t>(static_cast<uint16_t>(enemyWorldX - screenLeftWorldX));
	screenY = ram[Smb1ProfileData::EnemyY + slot];
	return screenX >= -16 && screenX < 256 && screenY < 240;
}

static bool IsTakeoverEligible(uint8_t enemyId)
{
	if(enemyId == Smb1ProfileData::CannonBulletBillId || enemyId == Smb1ProfileData::FrenzyBulletBillId) {
		return true;
	}
	for(unsigned index = 0; index < Smb1ProfileData::HorizontalEnemyCount; ++index) {
		if(Smb1ProfileData::HorizontalEnemies[index].id == enemyId) {
			return true;
		}
	}
	return false;
}

static const char* GetEnemyLabel(uint8_t enemyId)
{
	if(enemyId == Smb1ProfileData::CannonBulletBillId || enemyId == Smb1ProfileData::FrenzyBulletBillId) {
		return "BULLET BILL";
	}
	for(unsigned index = 0; index < Smb1ProfileData::HorizontalEnemyCount; ++index) {
		if(Smb1ProfileData::HorizontalEnemies[index].id == enemyId) {
			return Smb1ProfileData::HorizontalEnemies[index].label;
		}
	}
	return "ENEMY";
}

static bool IsEligibleTarget(uint8_t* ram, unsigned slot)
{
	if(ram[Smb1ProfileData::EnemyFlag + slot] == 0 ||
		(ram[Smb1ProfileData::EnemyState + slot] & Smb1ProfileData::DefeatedStateMask) != 0 ||
		!IsTakeoverEligible(ram[Smb1ProfileData::EnemyId + slot])) {
		return false;
	}
	int screenX = 0;
	int screenY = 0;
	return GetEnemyScreenPosition(ram, slot, screenX, screenY);
}

static unsigned FindNextEligibleTarget(uint8_t* ram, unsigned currentSlotCode, bool next)
{
	for(unsigned offset = 1; offset <= Smb1ProfileData::EnemySlots; ++offset) {
		const int step = next ? 1 : -1;
		const int baseSlot = currentSlotCode == 0
			? (next ? -1 : 0)
			: static_cast<int>(currentSlotCode - 1);
		int slot = (baseSlot + step * static_cast<int>(offset) +
			static_cast<int>(Smb1ProfileData::EnemySlots) * 2) % static_cast<int>(Smb1ProfileData::EnemySlots);
		if(IsEligibleTarget(ram, static_cast<unsigned>(slot))) {
			return static_cast<unsigned>(slot) + 1;
		}
	}
	return 0;
}

static void RetireOffscreenBulletBills(uint8_t* ram)
{
	for(unsigned slot = 0; slot < Smb1ProfileData::EnemySlots; ++slot) {
		if(ram[Smb1ProfileData::EnemyFlag + slot] == 0) {
			continue;
		}
		const uint8_t enemyId = ram[Smb1ProfileData::EnemyId + slot];
		const bool isBulletBill =
			enemyId == Smb1ProfileData::CannonBulletBillId || enemyId == Smb1ProfileData::FrenzyBulletBillId;
		if(isBulletBill && IsBillOffscreenLeft(ram, slot)) {
			ram[Smb1ProfileData::EnemyFlag + slot] = 0;
			ram[Smb1ProfileData::EnemyId + slot] = 0;
		}
	}
}

static void SpawnBulletBill(uint8_t* ram, unsigned slot)
{
	uint8_t page = ram[Smb1ProfileData::ScreenRightPage];
	uint8_t x = ram[Smb1ProfileData::ScreenRightX];
	if(x < Smb1ProfileData::SpawnInset) {
		page = static_cast<uint8_t>(page - 1);
	}
	x = static_cast<uint8_t>(x - Smb1ProfileData::SpawnInset);

	const int playerY = static_cast<int>(ram[Smb1ProfileData::PlayerY]);
	const uint8_t y = static_cast<uint8_t>(std::clamp(
		playerY - 8,
		static_cast<int>(Smb1ProfileData::HeightMin),
		static_cast<int>(Smb1ProfileData::HeightMax)));

	ram[Smb1ProfileData::EnemyPage + slot] = page;
	ram[Smb1ProfileData::EnemyX + slot] = x;
	ram[Smb1ProfileData::EnemyYHigh + slot] = static_cast<uint8_t>(Smb1ProfileData::VerticalHighPosition);
	ram[Smb1ProfileData::EnemyY + slot] = y;
	ram[Smb1ProfileData::EnemyFlag + slot] = 1;
	ram[Smb1ProfileData::EnemyState + slot] = 0;
	ram[Smb1ProfileData::EnemyBoundingBox + slot] = static_cast<uint8_t>(Smb1ProfileData::BoundingBoxValue);
	ram[Smb1ProfileData::EnemyMovingDirection + slot] = static_cast<uint8_t>(Smb1ProfileData::LeftDirection);
	ram[Smb1ProfileData::EnemyXSpeed + slot] = static_cast<uint8_t>(Smb1ProfileData::SpeedDefault);
	ram[Smb1ProfileData::EnemyId + slot] = static_cast<uint8_t>(Smb1ProfileData::CannonBulletBillId);
}

SpawnResult BulletBillDirector::RunFrame(Emulator* emulator, const std::shared_ptr<IConsole>& console, const DirectorInput& input)
{
	if(!_supportedRom || !emulator || !console) {
		return SpawnResult::None;
	}
	auto nesConsole = std::dynamic_pointer_cast<NesConsole>(console);
	if(!nesConsole || !nesConsole->GetMemoryManager()) {
		return SpawnResult::None;
	}
	uint8_t* ram = nesConsole->GetMemoryManager()->GetInternalRam();
	if(!ram) {
		return SpawnResult::None;
	}
	RetireOffscreenBulletBills(ram);

	const unsigned directorStateAddress = Smb1ProfileData::DirectorState;
	const uint8_t storedState = ram[directorStateAddress];
	constexpr uint8_t SpawnHeldMask = 0x01;
	constexpr uint8_t SelectedSlotMask = 0x0E;
	constexpr unsigned SelectedSlotShift = 1;
	constexpr uint8_t TakeoverEnabledMask = 0x10;
	constexpr uint8_t PreviousTargetMask = 0x20;
	constexpr uint8_t NextTargetMask = 0x40;
	constexpr uint8_t PreviousTakeoverMask = 0x80;
	const bool wasSpawnHeld = (storedState & SpawnHeldMask) != 0;
	const bool spawnPressed = input.spawnHeld && !wasSpawnHeld;
	unsigned selectedSlotCode = (storedState & SelectedSlotMask) >> SelectedSlotShift;
	if(selectedSlotCode > Smb1ProfileData::EnemySlots) {
		selectedSlotCode = 0;
	}
	bool takeoverEnabled = (storedState & TakeoverEnabledMask) != 0;
	const bool previousTargetPressed = input.previousTargetHeld && (storedState & PreviousTargetMask) == 0;
	const bool nextTargetPressed = input.nextTargetHeld && (storedState & NextTargetMask) == 0;
	const bool takeoverPressed = input.takeoverHeld && (storedState & PreviousTakeoverMask) == 0;
	_postFrameEnemyControl = false;
	_postFrameEnemySetDirection = false;

	const bool inGame = ram[Smb1ProfileData::OperationMode] == Smb1ProfileData::GameModeValue;
	const bool paused = ram[Smb1ProfileData::PauseStatus] != 0;
	SpawnResult spawnResult = SpawnResult::None;
	if(selectedSlotCode != 0 && !IsEligibleTarget(ram, selectedSlotCode - 1)) {
		selectedSlotCode = 0;
		takeoverEnabled = false;
	}

	if(inGame && !paused && previousTargetPressed != nextTargetPressed) {
		const unsigned newSlotCode = FindNextEligibleTarget(ram, selectedSlotCode, nextTargetPressed);
		if(newSlotCode != selectedSlotCode) {
			selectedSlotCode = newSlotCode;
			spawnResult = SpawnResult::TargetChanged;
		}
	}

	if(takeoverPressed && selectedSlotCode != 0 && inGame && !paused) {
		takeoverEnabled = !takeoverEnabled;
		spawnResult = takeoverEnabled ? SpawnResult::TakeoverEnabled : SpawnResult::TakeoverDisabled;
	}

	if(spawnPressed) {
		if(!inGame) {
			spawnResult = SpawnResult::NotInGame;
		} else if(paused) {
			spawnResult = SpawnResult::Paused;
		} else if(HasExistingBulletBill(ram)) {
			spawnResult = SpawnResult::ExistingBullet;
		} else {
			const int freeSlot = FindFreeBillSlot(ram);
			if(freeSlot < 0) {
				spawnResult = SpawnResult::NoFreeSlot;
			} else {
				SpawnBulletBill(ram, static_cast<unsigned>(freeSlot));
				selectedSlotCode = static_cast<unsigned>(freeSlot) + 1;
				takeoverEnabled = true;
				spawnResult = SpawnResult::Spawned;
			}
		}
	}

	if(selectedSlotCode != 0 && takeoverEnabled && inGame && !paused) {
		const unsigned slot = selectedSlotCode - 1;
		const uint8_t enemyId = ram[Smb1ProfileData::EnemyId + slot];
		const bool isBill = enemyId == Smb1ProfileData::CannonBulletBillId ||
			enemyId == Smb1ProfileData::FrenzyBulletBillId;
		if(isBill) {
			if((nesConsole->GetFrameCount() % Smb1ProfileData::HeightStepFrames) == 0 && input.up != input.down) {
				const int delta = input.up ? -1 : 1;
				const int height = std::clamp(
					static_cast<int>(ram[Smb1ProfileData::EnemyY + slot]) + delta,
					static_cast<int>(Smb1ProfileData::HeightMin),
					static_cast<int>(Smb1ProfileData::HeightMax));
				ram[Smb1ProfileData::EnemyY + slot] = static_cast<uint8_t>(height);
			}
			unsigned speed = Smb1ProfileData::SpeedDefault;
			if(input.slower != input.faster) {
				speed = input.slower ? Smb1ProfileData::SpeedSlow : Smb1ProfileData::SpeedFast;
			}
			ram[Smb1ProfileData::EnemyXSpeed + slot] = static_cast<uint8_t>(speed);
		} else if(!isBill && IsTakeoverEligible(enemyId)) {
			const bool hasDirectionInput = input.faster != input.slower;
			if(hasDirectionInput) {
				const uint8_t currentSpeed = ram[Smb1ProfileData::EnemyXSpeed + slot];
				const bool movingLeft = input.faster;
				_postFrameEnemySetDirection = true;
				_postFrameEnemyDirection = static_cast<uint8_t>(
					movingLeft ? Smb1ProfileData::LeftDirection : Smb1ProfileData::RightDirection);
				ram[Smb1ProfileData::EnemyMovingDirection + slot] = _postFrameEnemyDirection;
				ram[Smb1ProfileData::EnemyXSpeed + slot] = SetHorizontalSpeedDirection(currentSpeed, movingLeft);
				_postFrameEnemyControl = true;
				_postFrameEnemySlot = slot;
				_postFrameEnemySpeed = ram[Smb1ProfileData::EnemyXSpeed + slot];
			}
		}
	}

	_status = {};
	if(selectedSlotCode != 0 && IsEligibleTarget(ram, selectedSlotCode - 1)) {
		_status.hasTarget = true;
		_status.controlled = takeoverEnabled;
		_status.slot = selectedSlotCode - 1;
		_status.enemyId = ram[Smb1ProfileData::EnemyId + _status.slot];
		_status.label = GetEnemyLabel(static_cast<uint8_t>(_status.enemyId));
		GetEnemyScreenPosition(ram, _status.slot, _status.screenX, _status.screenY);
	}

	// SMB1 labels $06C9 unused; storing selection, takeover and input edge state
	// in RAM keeps the director deterministic across savestates and netplay rollback.
	ram[directorStateAddress] = static_cast<uint8_t>(
		(input.spawnHeld ? SpawnHeldMask : 0) |
		(selectedSlotCode << SelectedSlotShift) |
		(takeoverEnabled ? TakeoverEnabledMask : 0) |
		(input.previousTargetHeld ? PreviousTargetMask : 0) |
		(input.nextTargetHeld ? NextTargetMask : 0) |
		(input.takeoverHeld ? PreviousTakeoverMask : 0));
	return spawnResult;
}

void BulletBillDirector::RunPostFrame(const std::shared_ptr<IConsole>& console)
{
	if(!_supportedRom || !_postFrameEnemyControl || !console) {
		return;
	}
	auto nesConsole = std::dynamic_pointer_cast<NesConsole>(console);
	if(!nesConsole || !nesConsole->GetMemoryManager()) {
		_postFrameEnemyControl = false;
		return;
	}
	uint8_t* ram = nesConsole->GetMemoryManager()->GetInternalRam();
	if(!ram) {
		_postFrameEnemyControl = false;
		return;
	}
	const unsigned slot = _postFrameEnemySlot;
	const uint8_t storedState = ram[Smb1ProfileData::DirectorState];
	const unsigned selectedSlotCode = (storedState >> 1) & 0x07;
	const bool takeoverEnabled = (storedState & 0x10) != 0;
	const bool stillSelected = selectedSlotCode == slot + 1 && takeoverEnabled;
	const uint8_t enemyId = ram[Smb1ProfileData::EnemyId + slot];
	const bool stillControlledEnemy = ram[Smb1ProfileData::EnemyFlag + slot] != 0 &&
		IsTakeoverEligible(enemyId) &&
		(ram[Smb1ProfileData::EnemyState + slot] & Smb1ProfileData::DefeatedStateMask) == 0;
	if(stillSelected && stillControlledEnemy &&
		enemyId != Smb1ProfileData::CannonBulletBillId && enemyId != Smb1ProfileData::FrenzyBulletBillId) {
		// SMB1 moves horizontal enemies from signed X speed and direction. Reapply
		// both after native movement/collision so a collision cannot erase P2's turn.
		if(_postFrameEnemySetDirection) {
			ram[Smb1ProfileData::EnemyMovingDirection + slot] = _postFrameEnemyDirection;
		}
		ram[Smb1ProfileData::EnemyXSpeed + slot] = _postFrameEnemySpeed;
		_status.hasTarget = true;
		_status.controlled = true;
		_status.slot = slot;
		_status.enemyId = ram[Smb1ProfileData::EnemyId + slot];
		_status.label = GetEnemyLabel(static_cast<uint8_t>(_status.enemyId));
		GetEnemyScreenPosition(ram, slot, _status.screenX, _status.screenY);
	}
	_postFrameEnemyControl = false;
}

}
