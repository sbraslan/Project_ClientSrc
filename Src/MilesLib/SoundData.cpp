#include "Stdafx.h"
#include "SoundData.h"

#include "../EterPack/EterPackManager.h"
#include "../EterBase/Timer.h"

bool CSoundData::ms_isSoundFile[SOUND_FILE_MAX_NUM];
CMappedFile CSoundData::ms_SoundFile[SOUND_FILE_MAX_NUM];

const char * CSoundData::GetFileName()
{
	return m_filename;
}

void CSoundData::Assign(const char * filename)
{
	assert(m_assigned == false);

	strncpy(m_filename, filename, sizeof(m_filename) - 1);
	m_assigned = true;
}

LPVOID CSoundData::Get()
{
	++m_iRefCount;
	m_dwAccessTime = ELTimer_GetMSec();

	if (!m_data)
		ReadFromDisk();

	if (m_flag & FLAG_DATA_SIZE)
		return ((S32 *) m_data + 1);
	else
		return (m_data);
}

ULONG CSoundData::GetSize()
{
	return m_size;
}

void CSoundData::Release()
{
	assert(m_iRefCount != 0);
	--m_iRefCount;
	m_dwAccessTime = ELTimer_GetMSec();
}

uint32_t CSoundData::GetPlayTime()
{
	return m_dwPlayTime;
}

void CSoundData::SetPlayTime(uint32_t dwPlayTime)
{
	m_dwPlayTime = dwPlayTime;
}

uint32_t CSoundData::GetAccessTime()
{
	return m_dwAccessTime;
}

bool CSoundData::ReadFromDisk()
{
	assert(m_assigned == true);

#if defined(_WIN64)
	CMappedFile file;
	LPCVOID mappedData = nullptr;
	if (!CEterPackManager::Instance().Get(file, m_filename, &mappedData) || !mappedData || file.Size() == 0)
		return false;

	const size_t rawSize = file.Size();
	if (rawSize > static_cast<size_t>(ULONG_MAX))
		return false;

	auto* allocation = static_cast<std::uint8_t*>(malloc(rawSize + sizeof(U32)));
	if (!allocation)
		return false;

	*reinterpret_cast<U32*>(allocation) = static_cast<U32>(rawSize);
	memcpy(allocation + sizeof(U32), mappedData, rawSize);

	m_data = allocation;
	m_size = static_cast<ULONG>(rawSize);
	m_flag |= FLAG_DATA_SIZE;
	return true;
#else

	assert(m_assigned == true);

	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);

	auto * s = (U32 *) AIL_file_read(m_filename, FILE_READ_WITH_SIZE);

	if (s == nullptr)
		return false;

	S32 type = AIL_file_type(s + 1, s[0]);
	AILSOUNDINFO info;

	switch (type)
	{
	case AILFILETYPE_PCM_WAV:
	{
		m_data = s;
		m_size = *((S32 *) s);
		m_flag |= FLAG_DATA_SIZE;
	}
	break;

	case AILFILETYPE_ADPCM_WAV: // 3D »ç¿îµå´Â decompress ÇØ¾ß ÇÔ.
	{
		AIL_WAV_info(s + 1, &info);
		AIL_decompress_ADPCM(&info, &m_data, &m_size);
		AIL_mem_free_lock(s);
	}
	break;

	case AILFILETYPE_MPEG_L3_AUDIO:
	{
		AIL_decompress_ASI(s + 1, *((S32 *) s), m_filename, &m_data, &m_size, nullptr);
		AIL_mem_free_lock(s);
	}
	break;

	default:
		assert(!"Unknown File Type");
		AIL_mem_free_lock(s);
		return false;
	}

	return true;

#endif
}

#if !defined(_WIN64)
bool CSoundData::isSlotIndex(uint32_t dwIndex)
{
	if (dwIndex >= SOUND_FILE_MAX_NUM)
		return false;

	if (!ms_isSoundFile[dwIndex])
		return false;

	return true;
}

int CSoundData::GetEmptySlotIndex()
{
	for (int i = 0; i < SOUND_FILE_MAX_NUM; ++i)
	{
		if (!ms_isSoundFile[i])
			return i;
	}

	return -1;
}

U32 AILCALLBACK CSoundData::open_callback(char const * filename, U32 * file_handle)
{
	int iIndex = GetEmptySlotIndex();

	if (-1 == iIndex)
		return 0;

	LPCVOID pMap;

	if (!CEterPackManager::Instance().Get(ms_SoundFile[iIndex], filename, &pMap))
		return 0;

	ms_isSoundFile[iIndex] = true;

	*file_handle = iIndex;
	return 1;
}

void AILCALLBACK CSoundData::close_callback(U32 file_handle)
{
	if (!isSlotIndex(file_handle))
		return;

	ms_SoundFile[file_handle].Destroy();
	ms_isSoundFile[file_handle] = false;
}

S32 AILCALLBACK CSoundData::seek_callback(U32 file_handle, S32 offset, U32 type)
{
	if (!isSlotIndex(file_handle))
		return 0;

	return ms_SoundFile[file_handle].Seek(offset, type);
}

U32 AILCALLBACK CSoundData::read_callback(U32 file_handle, void * buffer, U32 bytes)
{
	if (!isSlotIndex(file_handle))
		return 0;

	auto dwRealSize = std::min<size_t>(ms_SoundFile[file_handle].Size(), bytes);
	ms_SoundFile[file_handle].Read(buffer, dwRealSize);
	return dwRealSize;
}

void CSoundData::SetPackMode()
{
	// if sound data reads from pack file, the callbacks of the MSS should be changed.
	AIL_set_file_callbacks(open_callback, close_callback, seek_callback, read_callback);

	for (bool & i : ms_isSoundFile)
		i = false;
}


#else
void CSoundData::SetPackMode()
{
    // Native x64 reads raw sound bytes directly through CEterPackManager.
}
#endif

void CSoundData::Destroy()
{
	if (m_data)
	{
#if defined(_WIN64)
		free(m_data);
#else
		AIL_mem_free_lock(m_data);
#endif
		m_data = nullptr;
	}
}

CSoundData::CSoundData()
	: m_assigned(false), m_iRefCount(0), m_dwPlayTime(0), m_dwAccessTime(ELTimer_GetMSec()), m_size(0), m_data(nullptr), m_flag(0)
{
}

CSoundData::~CSoundData()
{
	Destroy();
}
