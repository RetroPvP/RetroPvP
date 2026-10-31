#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "libretro.h"
#include "Smb1ProfileData.h"

#include <cstdarg>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <string>
#include <vector>

struct retro_vfs_file_handle {
	std::FILE* file = nullptr;
	std::string path;
};

namespace {

using namespace RetroPvP::Smb1ProfileData;
constexpr size_t NesRamSize = 0x800;
constexpr unsigned MaxStartupFrames = 180;
constexpr unsigned MaxApproachFrames = 900;
constexpr unsigned MovementFrames = 18;
constexpr unsigned BillReplayFrames = 60;
constexpr uint8_t SelectedSlotMask = 0x0E;
constexpr unsigned SelectedSlotShift = 1;
constexpr uint8_t TakeoverEnabledMask = 0x10;
constexpr unsigned GoombaId = 6;

static_assert(EnemyFlag + EnemySlots <= NesRamSize);
static_assert(EnemyId + EnemySlots <= NesRamSize);
static_assert(EnemyState + EnemySlots <= NesRamSize);
static_assert(EnemyMovingDirection + EnemySlots <= NesRamSize);
static_assert(EnemyXSpeed + EnemySlots <= NesRamSize);
static_assert(EnemyPage + EnemySlots <= NesRamSize);
static_assert(EnemyX + EnemySlots <= NesRamSize);
static_assert(EnemyY + EnemySlots <= NesRamSize);
static_assert(ScreenRightX < NesRamSize);
static_assert(DirectorState < NesRamSize);

constexpr uint16_t Button(unsigned id)
{
	return static_cast<uint16_t>(1u << id);
}

constexpr uint16_t P1A = Button(RETRO_DEVICE_ID_JOYPAD_A);
constexpr uint16_t P1Start = Button(RETRO_DEVICE_ID_JOYPAD_START);
constexpr uint16_t P1Right = Button(RETRO_DEVICE_ID_JOYPAD_RIGHT);
constexpr uint16_t P2Spawn = Button(RETRO_DEVICE_ID_JOYPAD_SELECT);
constexpr uint16_t P2Left = Button(RETRO_DEVICE_ID_JOYPAD_LEFT);
constexpr uint16_t P2Right = Button(RETRO_DEVICE_ID_JOYPAD_RIGHT);
constexpr uint16_t P2Down = Button(RETRO_DEVICE_ID_JOYPAD_DOWN);
constexpr uint16_t P2NextTarget = Button(RETRO_DEVICE_ID_JOYPAD_R);
constexpr uint16_t P2Takeover = Button(RETRO_DEVICE_ID_JOYPAD_X);

std::string systemDirectory;
std::string saveDirectory;
uint16_t inputPorts[2] = {};
retro_game_info_ext gameInfoExt{};
std::string gameDirectory;
std::string gameName;
std::string gameExtension;

std::FILE* OpenUtf8Path(const char* path, const char* mode)
{
	const int pathLength = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
	if(pathLength <= 0) {
		return nullptr;
	}
	std::wstring widePath(static_cast<size_t>(pathLength), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath.data(), pathLength);
	const int modeLength = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
	if(modeLength <= 0) {
		return nullptr;
	}
	std::wstring wideMode(static_cast<size_t>(modeLength), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, mode, -1, wideMode.data(), modeLength);
	return _wfopen(widePath.c_str(), wideMode.c_str());
}

const char* RETRO_CALLCONV VfsGetPath(retro_vfs_file_handle* handle)
{
	return handle ? handle->path.c_str() : nullptr;
}

retro_vfs_file_handle* RETRO_CALLCONV VfsOpen(const char* path, unsigned access, unsigned)
{
	if(!path) {
		return nullptr;
	}
	const bool read = (access & RETRO_VFS_FILE_ACCESS_READ) != 0;
	const bool write = (access & RETRO_VFS_FILE_ACCESS_WRITE) != 0;
	const bool update = (access & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) != 0;
	const char* mode = read && write ? (update ? "r+b" : "w+b") :
		write ? (update ? "r+b" : "wb") : read ? "rb" : nullptr;
	if(!mode) {
		return nullptr;
	}
	std::FILE* file = OpenUtf8Path(path, mode);
	if(!file && write && update) {
		file = OpenUtf8Path(path, read ? "w+b" : "wb");
	}
	if(!file) {
		return nullptr;
	}
	auto* handle = new retro_vfs_file_handle();
	handle->file = file;
	handle->path = path;
	return handle;
}

int RETRO_CALLCONV VfsClose(retro_vfs_file_handle* handle)
{
	if(!handle) {
		return -1;
	}
	const int result = std::fclose(handle->file);
	delete handle;
	return result;
}

int64_t RETRO_CALLCONV VfsSize(retro_vfs_file_handle* handle)
{
	if(!handle) {
		return -1;
	}
	const long position = std::ftell(handle->file);
	if(position < 0 || std::fseek(handle->file, 0, SEEK_END) != 0) {
		return -1;
	}
	const long size = std::ftell(handle->file);
	if(std::fseek(handle->file, position, SEEK_SET) != 0) {
		return -1;
	}
	return size;
}

int64_t RETRO_CALLCONV VfsTell(retro_vfs_file_handle* handle)
{
	return handle ? std::ftell(handle->file) : -1;
}

int64_t RETRO_CALLCONV VfsSeek(retro_vfs_file_handle* handle, int64_t offset, int origin)
{
	if(!handle || offset > LONG_MAX || offset < LONG_MIN) {
		return -1;
	}
	const int whence = origin == RETRO_VFS_SEEK_POSITION_START ? SEEK_SET :
		origin == RETRO_VFS_SEEK_POSITION_CURRENT ? SEEK_CUR : SEEK_END;
	if(std::fseek(handle->file, static_cast<long>(offset), whence) != 0) {
		return -1;
	}
	return std::ftell(handle->file);
}

int64_t RETRO_CALLCONV VfsRead(retro_vfs_file_handle* handle, void* output, uint64_t length)
{
	return handle ? static_cast<int64_t>(std::fread(output, 1, static_cast<size_t>(length), handle->file)) : -1;
}

int64_t RETRO_CALLCONV VfsWrite(retro_vfs_file_handle* handle, const void* input, uint64_t length)
{
	return handle ? static_cast<int64_t>(std::fwrite(input, 1, static_cast<size_t>(length), handle->file)) : -1;
}

int RETRO_CALLCONV VfsFlush(retro_vfs_file_handle* handle)
{
	return handle ? std::fflush(handle->file) : -1;
}

int RETRO_CALLCONV VfsRemove(const char* path)
{
	return path ? std::remove(path) : -1;
}

int RETRO_CALLCONV VfsRename(const char* oldPath, const char* newPath)
{
	return oldPath && newPath ? std::rename(oldPath, newPath) : -1;
}

int64_t RETRO_CALLCONV VfsTruncate(retro_vfs_file_handle* handle, int64_t length)
{
	if(!handle || !handle->file || length < 0) {
		return -1;
	}
	return _chsize_s(_fileno(handle->file), static_cast<__int64>(length)) == 0 ? 0 : -1;
}

retro_vfs_interface vfsInterface = {
	VfsGetPath, VfsOpen, VfsClose, VfsSize, VfsTell, VfsSeek, VfsRead,
	VfsWrite, VfsFlush, VfsRemove, VfsRename, VfsTruncate,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

void RETRO_CALLCONV LogMessage(enum retro_log_level level, const char* format, ...)
{
	const char* tag = level == RETRO_LOG_ERROR ? "error" :
		level == RETRO_LOG_WARN ? "warn" : level == RETRO_LOG_INFO ? "info" : "debug";
	std::fprintf(stderr, "[Mesen %s] ", tag);
	va_list args;
	va_start(args, format);
	std::vfprintf(stderr, format, args);
	va_end(args);
}

bool RETRO_CALLCONV Environment(unsigned command, void* data)
{
	switch(command) {
		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
			reinterpret_cast<retro_log_callback*>(data)->log = LogMessage;
			return true;
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
			*static_cast<const char**>(data) = systemDirectory.c_str();
			return true;
		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
			*static_cast<const char**>(data) = saveDirectory.c_str();
			return true;
		case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
			auto* info = static_cast<retro_vfs_interface_info*>(data);
			if(info->required_interface_version != 2) {
				return false;
			}
			info->required_interface_version = 2;
			info->iface = &vfsInterface;
			return true;
		}
		case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
			*static_cast<const retro_game_info_ext**>(data) = &gameInfoExt;
			return true;
		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
			return *static_cast<retro_pixel_format*>(data) == RETRO_PIXEL_FORMAT_XRGB8888;
		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
			*static_cast<bool*>(data) = false;
			return true;
		case RETRO_ENVIRONMENT_GET_VARIABLE:
			return false;
		case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
		case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
		case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
		case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
		case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
		case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
		case RETRO_ENVIRONMENT_SET_GEOMETRY:
		case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
			return true;
		case RETRO_ENVIRONMENT_SET_MESSAGE:
			if(data) {
				const auto* message = static_cast<const retro_message*>(data);
				if(message->msg) {
					std::fprintf(stderr, "[Mesen message] %s\n", message->msg);
				}
			}
			return true;
		default:
			return false;
	}
}

void RETRO_CALLCONV VideoRefresh(const void*, unsigned, unsigned, size_t) {}
void RETRO_CALLCONV AudioSample(int16_t, int16_t) {}
size_t RETRO_CALLCONV AudioSampleBatch(const int16_t*, size_t frames) { return frames; }
void RETRO_CALLCONV InputPoll() {}

int16_t RETRO_CALLCONV InputState(unsigned port, unsigned device, unsigned, unsigned id)
{
	if(port >= 2 || device != RETRO_DEVICE_JOYPAD) {
		return 0;
	}
	if(id == RETRO_DEVICE_ID_JOYPAD_MASK) {
		return static_cast<int16_t>(inputPorts[port]);
	}
	return id < 16 && (inputPorts[port] & Button(id)) != 0 ? 1 : 0;
}

template<typename T>
bool LoadFunction(HMODULE module, const char* name, T& function)
{
	function = reinterpret_cast<T>(GetProcAddress(module, name));
	if(!function) {
		std::fprintf(stderr, "Missing Libretro entry point: %s\n", name);
		return false;
	}
	return true;
}

struct CoreApi {
	HMODULE module = nullptr;
	void (RETRO_CALLCONV *setEnvironment)(retro_environment_t) = nullptr;
	void (RETRO_CALLCONV *setVideoRefresh)(retro_video_refresh_t) = nullptr;
	void (RETRO_CALLCONV *setAudioSample)(retro_audio_sample_t) = nullptr;
	void (RETRO_CALLCONV *setAudioSampleBatch)(retro_audio_sample_batch_t) = nullptr;
	void (RETRO_CALLCONV *setInputPoll)(retro_input_poll_t) = nullptr;
	void (RETRO_CALLCONV *setInputState)(retro_input_state_t) = nullptr;
	void (RETRO_CALLCONV *setControllerPortDevice)(unsigned, unsigned) = nullptr;
	void (RETRO_CALLCONV *getSystemInfo)(retro_system_info*) = nullptr;
	void (RETRO_CALLCONV *getSystemAvInfo)(retro_system_av_info*) = nullptr;
	void (RETRO_CALLCONV *init)() = nullptr;
	void (RETRO_CALLCONV *deinit)() = nullptr;
	bool (RETRO_CALLCONV *loadGame)(const retro_game_info*) = nullptr;
	void (RETRO_CALLCONV *unloadGame)() = nullptr;
	void (RETRO_CALLCONV *run)() = nullptr;
	void* (RETRO_CALLCONV *getMemoryData)(unsigned) = nullptr;
	size_t (RETRO_CALLCONV *serializeSize)() = nullptr;
	bool (RETRO_CALLCONV *serialize)(void*, size_t) = nullptr;
	bool (RETRO_CALLCONV *unserialize)(const void*, size_t) = nullptr;

	bool Load(const char* path)
	{
		module = LoadLibraryA(path);
		if(!module) {
			std::fprintf(stderr, "Could not load core DLL (Windows error %lu): %s\n", GetLastError(), path);
			return false;
		}
		return LoadFunction(module, "retro_set_environment", setEnvironment) &&
			LoadFunction(module, "retro_set_video_refresh", setVideoRefresh) &&
			LoadFunction(module, "retro_set_audio_sample", setAudioSample) &&
			LoadFunction(module, "retro_set_audio_sample_batch", setAudioSampleBatch) &&
			LoadFunction(module, "retro_set_input_poll", setInputPoll) &&
			LoadFunction(module, "retro_set_input_state", setInputState) &&
			LoadFunction(module, "retro_set_controller_port_device", setControllerPortDevice) &&
			LoadFunction(module, "retro_get_system_info", getSystemInfo) &&
			LoadFunction(module, "retro_get_system_av_info", getSystemAvInfo) &&
			LoadFunction(module, "retro_init", init) &&
			LoadFunction(module, "retro_deinit", deinit) &&
			LoadFunction(module, "retro_load_game", loadGame) &&
			LoadFunction(module, "retro_unload_game", unloadGame) &&
			LoadFunction(module, "retro_run", run) &&
			LoadFunction(module, "retro_get_memory_data", getMemoryData) &&
			LoadFunction(module, "retro_serialize_size", serializeSize) &&
			LoadFunction(module, "retro_serialize", serialize) &&
			LoadFunction(module, "retro_unserialize", unserialize);
	}

	~CoreApi()
	{
		if(module) {
			FreeLibrary(module);
		}
	}
};

uint16_t WorldX(const uint8_t* ram, unsigned slot)
{
	return static_cast<uint16_t>((static_cast<uint16_t>(ram[EnemyPage + slot]) << 8) |
		ram[EnemyX + slot]);
}

int16_t DeltaX(uint16_t current, uint16_t previous)
{
	return static_cast<int16_t>(static_cast<uint16_t>(current - previous));
}

bool IsLiveVisibleEnemy(const uint8_t* ram, unsigned slot, unsigned enemyId)
{
	if(ram[EnemyFlag + slot] == 0 || ram[EnemyId + slot] != enemyId ||
		(ram[EnemyState + slot] & DefeatedStateMask) != 0) {
		return false;
	}
	const uint16_t scroll = static_cast<uint16_t>(
		(static_cast<uint16_t>(ram[ScreenLeftPage]) << 8) | ram[ScreenLeftX]);
	const int16_t screenX = DeltaX(WorldX(ram, slot), scroll);
	return screenX >= -16 && screenX < 256 && ram[EnemyY + slot] < 240;
}

int FindEnemy(const uint8_t* ram, unsigned enemyId)
{
	for(unsigned slot = 0; slot < EnemySlots; ++slot) {
		if(IsLiveVisibleEnemy(ram, slot, enemyId)) {
			return static_cast<int>(slot);
		}
	}
	return -1;
}

unsigned SelectedSlotCode(const uint8_t* ram)
{
	return (ram[DirectorState] & SelectedSlotMask) >> SelectedSlotShift;
}

void RunFrame(CoreApi& core, uint16_t p1 = 0, uint16_t p2 = 0)
{
	inputPorts[0] = p1;
	inputPorts[1] = p2;
	core.run();
}

bool SelectTarget(CoreApi& core, const uint8_t* ram, unsigned slot)
{
	for(unsigned attempt = 0; attempt < EnemySlots + 1; ++attempt) {
		RunFrame(core, 0, P2NextTarget);
		RunFrame(core);
		if(SelectedSlotCode(ram) == slot + 1) {
			return true;
		}
	}
	return false;
}

int Fail(const char* message);

int RunHorizontalEnemyFixtureCheck(CoreApi& core, uint8_t*& ram)
{
	int naturalGoomba = -1;
	for(unsigned frame = 0; frame < MaxApproachFrames; ++frame) {
		const uint16_t jump = frame % 36 < 4 ? P1A : 0;
		RunFrame(core, static_cast<uint16_t>(P1Right | jump));
		naturalGoomba = FindEnemy(ram, GoombaId);
		if(naturalGoomba >= 0) {
			break;
		}
	}
	if(naturalGoomba < 0) {
		return Fail("scripted SMB1 movement did not expose a natural Goomba for enemy fixtures");
	}
	const size_t stateSize = core.serializeSize();
	if(stateSize == 0) {
		return Fail("core reported an empty serialization size for enemy fixtures");
	}
	std::vector<uint8_t> baseline(stateSize, 0xA5);
	if(!core.serialize(baseline.data(), baseline.size())) {
		return Fail("could not checkpoint the naturally spawned Goomba fixture");
	}
	if(baseline.back() != 0) {
		return Fail("core did not clear the unused tail of the enemy-fixture checkpoint");
	}
	std::printf("Captured a native Goomba checkpoint in slot %d for controlled enemy fixtures.\n", naturalGoomba);

	for(unsigned index = 0; index < HorizontalEnemyCount; ++index) {
		if(!core.unserialize(baseline.data(), baseline.size())) {
			return Fail("could not restore the native Goomba checkpoint for an enemy fixture");
		}
		ram = static_cast<uint8_t*>(core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM));
		if(!ram) {
			return Fail("core did not expose system RAM after restoring the enemy-fixture checkpoint");
		}
		const int restoredGoomba = FindEnemy(ram, GoombaId);
		if(restoredGoomba < 0) {
			return Fail("the native Goomba was missing after restoring the enemy-fixture checkpoint");
		}
		const unsigned slot = static_cast<unsigned>(restoredGoomba);
		const auto& enemy = HorizontalEnemies[index];
		for(unsigned other = 0; other < EnemySlots; ++other) {
			if(other != slot) {
				ram[EnemyFlag + other] = 0;
				ram[EnemyId + other] = 0;
			}
		}
		const uint16_t screenLeft = static_cast<uint16_t>(
			(static_cast<uint16_t>(ram[ScreenLeftPage]) << 8) | ram[ScreenLeftX]);
		const uint16_t fixtureWorldX = static_cast<uint16_t>(screenLeft + 200);
		ram[EnemyPage + slot] = static_cast<uint8_t>(fixtureWorldX >> 8);
		ram[EnemyX + slot] = static_cast<uint8_t>(fixtureWorldX);
		ram[EnemyId + slot] = static_cast<uint8_t>(enemy.id);
		ram[EnemyMovingDirection + slot] = static_cast<uint8_t>(LeftDirection);
		ram[DirectorState] = 0;
		RunFrame(core);

		if(FindEnemy(ram, enemy.id) != static_cast<int>(slot)) {
			return Fail("the native SMB1 update did not keep the injected enemy fixture active");
		}
		if(!SelectTarget(core, ram, slot)) {
			return Fail("P2 could not select the horizontal enemy fixture");
		}
		RunFrame(core, 0, P2Takeover);
		RunFrame(core);
		if((ram[DirectorState] & TakeoverEnabledMask) == 0) {
			return Fail("P2 could not enable takeover for the horizontal enemy fixture");
		}

		const uint8_t initialDirection = ram[EnemyMovingDirection + slot];
		const uint8_t desiredDirection = initialDirection == LeftDirection ? RightDirection : LeftDirection;
		const uint16_t directionInput = desiredDirection == LeftDirection ? P2Left : P2Right;
		RunFrame(core, 0, directionInput);
		if(ram[EnemyMovingDirection + slot] != desiredDirection) {
			std::fprintf(stderr, "Observed %s direction after P2 input: %u.\n",
				enemy.label, static_cast<unsigned>(ram[EnemyMovingDirection + slot]));
			return Fail("P2 direction input did not reverse the horizontal enemy");
		}
		const uint16_t afterDirectionPress = WorldX(ram, slot);
		for(unsigned frame = 0; frame < MovementFrames; ++frame) {
			RunFrame(core);
		}
		if(FindEnemy(ram, enemy.id) != static_cast<int>(slot)) {
			return Fail("the horizontal enemy fixture left the screen during the movement check");
		}
		const int16_t delta = DeltaX(WorldX(ram, slot), afterDirectionPress);
		const bool movedAsDirected = desiredDirection == LeftDirection ? delta < 0 : delta > 0;
		if(!movedAsDirected) {
			std::fprintf(stderr, "Observed %s displacement after release: %d px.\n", enemy.label, delta);
			return Fail("the horizontal enemy did not move in P2's selected direction after input release");
		}
		if(SelectedSlotCode(ram) != slot + 1 || (ram[DirectorState] & TakeoverEnabledMask) == 0) {
			return Fail("the selected horizontal enemy lost P2 takeover during the movement check");
		}
		std::printf("%s fixture: P2 reversed direction and native movement continued %d px after release.\n",
			enemy.label, delta);
	}
	std::puts("PASS: horizontal enemy takeover fixtures.");
	return 0;
}

int Fail(const char* message)
{
	std::fprintf(stderr, "FAIL: %s\n", message);
	return 1;
}

struct BillState {
	unsigned slot = 0;
	uint8_t flag = 0;
	uint8_t id = 0;
	uint8_t state = 0;
	uint8_t direction = 0;
	uint8_t speed = 0;
	uint8_t page = 0;
	uint8_t x = 0;
	uint8_t y = 0;
	uint8_t directorState = 0;
};

bool operator==(const BillState& lhs, const BillState& rhs)
{
	return lhs.slot == rhs.slot && lhs.flag == rhs.flag && lhs.id == rhs.id &&
		lhs.state == rhs.state && lhs.direction == rhs.direction && lhs.speed == rhs.speed &&
		lhs.page == rhs.page && lhs.x == rhs.x && lhs.y == rhs.y &&
		lhs.directorState == rhs.directorState;
}

int FindLiveBill(const uint8_t* ram)
{
	for(unsigned slot = 0; slot < EnemySlots; ++slot) {
		if(ram[EnemyFlag + slot] != 0 &&
			ram[EnemyId + slot] == CannonBulletBillId &&
			(ram[EnemyState + slot] & DefeatedStateMask) == 0) {
			return static_cast<int>(slot);
		}
	}
	return -1;
}

BillState CaptureBillState(const uint8_t* ram, unsigned slot)
{
	return {
		slot,
		ram[EnemyFlag + slot],
		ram[EnemyId + slot],
		ram[EnemyState + slot],
		ram[EnemyMovingDirection + slot],
		ram[EnemyXSpeed + slot],
		ram[EnemyPage + slot],
		ram[EnemyX + slot],
		ram[EnemyY + slot],
		ram[DirectorState]
	};
}

int RunBillSerializationCheck(CoreApi& core, uint8_t*& ram)
{
	for(unsigned frame = 0; frame < 60; ++frame) {
		RunFrame(core, P1Right);
	}
	RunFrame(core, 0, P2Spawn);
	RunFrame(core);
	const int billSlot = FindLiveBill(ram);
	if(billSlot < 0) {
		return Fail("P2 could not spawn a native Bullet Bill for the serialization check");
	}
	const unsigned slot = static_cast<unsigned>(billSlot);
	if((ram[DirectorState] & TakeoverEnabledMask) == 0) {
		return Fail("spawned Bullet Bill was not active under P2 control");
	}
	std::printf("Spawned active Bullet Bill in enemy slot %u.\n", slot);
	RunFrame(core, 0, P2Right);
	if(ram[EnemyXSpeed + slot] != SpeedSlow) {
		return Fail("P2 slow-speed input did not change the active Bill speed before saving");
	}

	const size_t stateSize = core.serializeSize();
	if(stateSize == 0) {
		return Fail("core reported an empty serialization size");
	}
	std::vector<uint8_t> checkpoint(stateSize, 0xA5);
	if(!core.serialize(checkpoint.data(), checkpoint.size())) {
		return Fail("core could not serialize the active Bullet Bill state");
	}
	if(checkpoint.back() != 0) {
		return Fail("core did not clear the unused tail of its fixed-size serialization buffer");
	}
	const BillState before = CaptureBillState(ram, slot);

	auto runBranch = [&]() {
		for(unsigned frame = 0; frame < BillReplayFrames; ++frame) {
			const uint16_t input = frame < 8 ? P2Down : 0;
			RunFrame(core, 0, input);
		}
	};
	runBranch();
	const BillState firstReplay = CaptureBillState(ram, slot);
	if(FindLiveBill(ram) != billSlot) {
		return Fail("the Bill left its native enemy slot before the checkpoint replay finished");
	}
	if(SelectedSlotCode(ram) != slot + 1 || (ram[DirectorState] & TakeoverEnabledMask) == 0) {
		return Fail("P2 lost control of the Bill during the checkpoint replay");
	}
	const uint16_t beforeWorldX = static_cast<uint16_t>((static_cast<uint16_t>(before.page) << 8) | before.x);
	const int16_t branchDeltaX = DeltaX(WorldX(ram, slot), beforeWorldX);
	const int branchDeltaY = static_cast<int>(firstReplay.y) - static_cast<int>(before.y);
	if(branchDeltaY <= 0 && branchDeltaX == 0) {
		return Fail("the controlled Bill did not move/respond to P2 inputs during the checkpoint replay");
	}
	std::printf("Checkpoint replay changed Bill position by %d px and %d Y units.\n", branchDeltaX, branchDeltaY);

	if(!core.unserialize(checkpoint.data(), checkpoint.size())) {
		return Fail("core could not restore the active Bullet Bill checkpoint");
	}
	ram = static_cast<uint8_t*>(core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM));
	if(!ram || FindLiveBill(ram) != billSlot || !(CaptureBillState(ram, slot) == before)) {
		return Fail("restoring the checkpoint did not restore the active Bill position and controls");
	}
	runBranch();
	const BillState secondReplay = CaptureBillState(ram, slot);
	if(FindLiveBill(ram) != billSlot || SelectedSlotCode(ram) != slot + 1 ||
		(ram[DirectorState] & TakeoverEnabledMask) == 0 || !(secondReplay == firstReplay)) {
		return Fail("replaying identical inputs after restore produced a different Bill state");
	}
	std::printf("Serialize/unserialize restored active Bill state and deterministic replay (%u-byte state).\n",
		static_cast<unsigned>(stateSize));
	std::puts("PASS: active Bullet Bill core state restore.");
	return 0;
}

} // namespace

int main(int argc, char** argv)
{
	const bool billStateMode = argc == 4 && std::string(argv[3]) == "--bill-state";
	const bool enemyFixturesMode = argc == 4 && std::string(argv[3]) == "--enemy-fixtures";
	if((argc != 3 && !billStateMode && !enemyFixturesMode) ||
		!std::filesystem::exists(argv[1]) || !std::filesystem::exists(argv[2])) {
		std::fprintf(stderr, "Usage: smb1_headless.exe <mesen2_libretro.dll> <smb1.nes> [--bill-state|--enemy-fixtures]\n");
		return 2;
	}
	const auto dataRoot = std::filesystem::path(argv[1]).parent_path() / "headless-data";
	std::filesystem::create_directories(dataRoot / "system");
	std::filesystem::create_directories(dataRoot / "save");
	systemDirectory = (dataRoot / "system").string();
	saveDirectory = (dataRoot / "save").string();
	SetDllDirectoryA(std::filesystem::path(argv[1]).parent_path().string().c_str());

	CoreApi core;
	if(!core.Load(argv[1])) {
		return 3;
	}
	core.setEnvironment(Environment);
	core.setVideoRefresh(VideoRefresh);
	core.setAudioSample(AudioSample);
	core.setAudioSampleBatch(AudioSampleBatch);
	core.setInputPoll(InputPoll);
	core.setInputState(InputState);
	retro_system_info systemInfo{};
	core.getSystemInfo(&systemInfo);
	core.init();
	core.setControllerPortDevice(0, RETRO_DEVICE_JOYPAD);
	core.setControllerPortDevice(1, RETRO_DEVICE_JOYPAD);

	std::ifstream romStream(argv[2], std::ios::binary | std::ios::ate);
	if(!romStream) {
		core.deinit();
		return Fail("could not read the supplied ROM");
	}
	const std::streamsize romLength = romStream.tellg();
	if(romLength <= 0) {
		core.deinit();
		return Fail("supplied ROM is empty or unreadable");
	}
	std::vector<uint8_t> romData(static_cast<size_t>(romLength));
	romStream.seekg(0, std::ios::beg);
	if(!romStream.read(reinterpret_cast<char*>(romData.data()), romLength)) {
		core.deinit();
		return Fail("could not read all bytes from the supplied ROM");
	}
	const std::filesystem::path romPath(argv[2]);
	gameDirectory = romPath.parent_path().string();
	gameName = romPath.stem().string();
	gameExtension = romPath.extension().string();
	if(!gameExtension.empty() && gameExtension.front() == '.') {
		gameExtension.erase(gameExtension.begin());
	}
	gameInfoExt = {};
	gameInfoExt.full_path = argv[2];
	gameInfoExt.dir = gameDirectory.c_str();
	gameInfoExt.name = gameName.c_str();
	gameInfoExt.ext = gameExtension.c_str();
	gameInfoExt.data = systemInfo.need_fullpath ? nullptr : romData.data();
	gameInfoExt.size = systemInfo.need_fullpath ? 0 : romData.size();
	gameInfoExt.file_in_archive = false;
	gameInfoExt.persistent_data = true;

	retro_game_info game{};
	game.path = argv[2];
	game.data = systemInfo.need_fullpath ? nullptr : romData.data();
	game.size = systemInfo.need_fullpath ? 0 : romData.size();
	if(!core.loadGame(&game)) {
		core.deinit();
		return Fail("MesenCE could not load the supplied ROM");
	}
	retro_system_av_info avInfo{};
	core.getSystemAvInfo(&avInfo);
	auto* ram = static_cast<uint8_t*>(core.getMemoryData(RETRO_MEMORY_SYSTEM_RAM));
	if(!ram) {
		core.unloadGame();
		core.deinit();
		return Fail("core did not expose NES system RAM");
	}

	bool gameStarted = false;
	for(unsigned frame = 0; frame < MaxStartupFrames; ++frame) {
		const bool pressStart = frame == 45;
		RunFrame(core, pressStart ? P1Start : 0);
		if(ram[OperationMode] == GameModeValue && frame > 2) {
			gameStarted = true;
			std::printf("Started SMB1 gameplay after %u frames.\n", frame + 1);
			break;
		}
	}
	if(!gameStarted) {
		core.unloadGame();
		core.deinit();
		return Fail("SMB1 did not enter gameplay after the title-screen Start press");
	}
	if(billStateMode) {
		const int result = RunBillSerializationCheck(core, ram);
		core.unloadGame();
		core.deinit();
		return result;
	}
	if(enemyFixturesMode) {
		const int result = RunHorizontalEnemyFixtureCheck(core, ram);
		core.unloadGame();
		core.deinit();
		return result;
	}
	ram[DirectorState] = 0;

	int goombaSlot = -1;
	for(unsigned frame = 0; frame < MaxApproachFrames; ++frame) {
		const uint16_t jump = frame % 36 < 4 ? P1A : 0;
		RunFrame(core, static_cast<uint16_t>(P1Right | jump));
		goombaSlot = FindEnemy(ram, GoombaId);
		if(goombaSlot >= 0) {
			break;
		}
	}
	if(goombaSlot < 0) {
		core.unloadGame();
		core.deinit();
		return Fail("scripted SMB1 movement did not expose a live Goomba on screen");
	}
	const unsigned slot = static_cast<unsigned>(goombaSlot);
	std::printf("Found live Goomba in enemy slot %u.\n", slot);

	if(!SelectTarget(core, ram, slot)) {
		core.unloadGame();
		core.deinit();
		return Fail("could not select the visible Goomba through P2 input");
	}
	if((ram[DirectorState] & TakeoverEnabledMask) == 0) {
		RunFrame(core, 0, P2Takeover);
		RunFrame(core);
	}
	if((ram[DirectorState] & TakeoverEnabledMask) == 0) {
		core.unloadGame();
		core.deinit();
		return Fail("P2 takeover toggle did not enable Goomba control");
	}

	const uint8_t initialDirection = ram[EnemyMovingDirection + slot];
	const uint8_t desiredDirection = initialDirection == LeftDirection ? RightDirection : LeftDirection;
	const uint16_t directionInput = desiredDirection == LeftDirection ? P2Left : P2Right;
	RunFrame(core, 0, directionInput);
	if(ram[EnemyMovingDirection + slot] != desiredDirection) {
		core.unloadGame();
		core.deinit();
		return Fail("one P2 direction press did not change the Goomba direction");
	}
	const uint16_t afterDirectionPress = WorldX(ram, slot);
	for(unsigned frame = 0; frame < MovementFrames; ++frame) {
		RunFrame(core);
	}
	if(!IsLiveVisibleEnemy(ram, slot, GoombaId)) {
		core.unloadGame();
		core.deinit();
		return Fail("Goomba left the active screen before the release check");
	}
	const int16_t releasedDelta = DeltaX(WorldX(ram, slot), afterDirectionPress);
	const bool movedAsDirected = desiredDirection == LeftDirection ? releasedDelta < 0 : releasedDelta > 0;
	if(!movedAsDirected) {
		core.unloadGame();
		core.deinit();
		std::fprintf(stderr, "Observed Goomba displacement after release: %d px.\n", releasedDelta);
		return Fail("Goomba did not keep moving in the chosen direction after releasing P2 input");
	}
	std::printf("Released direction input; native Goomba movement continued (%d px).\n", releasedDelta);

	if((ram[DirectorState] & TakeoverEnabledMask) != 0) {
		RunFrame(core, 0, P2Takeover);
		RunFrame(core);
	}
	if((ram[DirectorState] & TakeoverEnabledMask) != 0) {
		core.unloadGame();
		core.deinit();
		return Fail("P2 takeover toggle did not disable Goomba control");
	}
	const uint16_t afterDeselect = WorldX(ram, slot);
	for(unsigned frame = 0; frame < MovementFrames; ++frame) {
		RunFrame(core);
	}
	if(!IsLiveVisibleEnemy(ram, slot, GoombaId)) {
		core.unloadGame();
		core.deinit();
		return Fail("Goomba left the active screen before the deselection check");
	}
	const int16_t deselectedDelta = DeltaX(WorldX(ram, slot), afterDeselect);
	const bool movedAfterDeselect = desiredDirection == LeftDirection ? deselectedDelta < 0 : deselectedDelta > 0;
	if(!movedAfterDeselect) {
		core.unloadGame();
		core.deinit();
		std::fprintf(stderr, "Observed Goomba displacement after deselection: %d px.\n", deselectedDelta);
		return Fail("Goomba stopped moving after P2 deselected takeover");
	}
	std::printf("Disabled takeover; native Goomba movement continued (%d px).\n", deselectedDelta);
	std::puts("PASS: SMB1 Goomba direction tap, release, and deselection.");

	core.unloadGame();
	core.deinit();
	return 0;
}
