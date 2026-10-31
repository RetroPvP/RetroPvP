#include "Smb1RomProfile.h"
#include "Smb1ProfileData.h"
#include <algorithm>
#include <cctype>

namespace RetroPvP {

bool IsSupportedSmb1Rom(const std::string& sha1)
{
	std::string normalized = sha1;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char value) {
		return static_cast<char>(std::tolower(value));
	});
	return normalized == Smb1ProfileData::Sha1;
}

}
