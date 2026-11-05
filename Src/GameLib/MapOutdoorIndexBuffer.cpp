#include "StdAfx.h"
#include "MapOutdoor.h"

// Completely dormant unless BOTH a dedicated x64 compile switch and a
// one-shot runtime environment variable are explicitly enabled. Captures
// CPU-side LOD indices before existing DX9 WRITEONLY buffer upload.
#if defined(M2_DX11_LOD_CAPTURE) && defined(_WIN64) && !defined(WORLD_EDITOR)
#include "../EterLib/Renderer/DX11LegacyIndexSnapshot.h"
#include <array>
#include <atomic>
#include <vector>

namespace
{
    void TryCaptureOriginalTerrainLOD(
        const std::array<m2::render::LegacyIndexView, 3>& original)
    {
        char enabled[2]{};
        // Only the exact value "1" enables capture; no configuration file.
        const DWORD envLength = ::GetEnvironmentVariableA(
            "METIN2_DX11_LOD_CAPTURE_ONCE", enabled,
            static_cast<DWORD>(sizeof(enabled)));
        const bool isEnabled = (envLength == 1 && enabled[0] == '1');

        // Diagnostics compiled ONLY into the explicit x64 capture build.
        // Even when the runtime flag is missing, log one entry per process
        // to distinguish "map never loaded" from "flag was not inherited".
        // Never print the environment value, only its length and validity.
        static std::atomic<bool> diagnosticLogged{false};
        if (!diagnosticLogged.exchange(true))
        {
            TraceError(
                "[DX11 LOD DIAG] hook reached | envLength=%lu | enabled=%u | LOD0=%u LOD1=%u LOD2=%u",
                static_cast<unsigned long>(envLength),
                isEnabled ? 1u : 0u,
                static_cast<unsigned>(original[0].count),
                static_cast<unsigned>(original[1].count),
                static_cast<unsigned>(original[2].count));
        }

        if (!isEnabled)
            return;

        // SetIndexBuffer is allowed to be called more than once; only one
        // capture attempt per process. Failed captures never affect DX9.
        static std::atomic<bool> attempted{false};
        if (attempted.exchange(true))
            return;

        constexpr char filename[] = "dx11_terrain_lod_capture.bin";

        try
        {
            std::vector<std::uint8_t> bytes;
            if (!m2::render::DX11LegacyIndexSnapshot::Encode(original, bytes))
            {
                TraceError("[DX11 LOD CAPTURE] validation failed; DX9 unchanged");
                return;
            }

            // CREATE_NEW avoids overwriting an existing diagnostic file.
            HANDLE file = ::CreateFileA(
                filename, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                TraceError("[DX11 LOD CAPTURE] cannot create %s; Win32=%lu",
                           filename, ::GetLastError());
                return;
            }

            DWORD written = 0;
            const bool successfulWrite =
                bytes.size() <= MAXDWORD &&
                ::WriteFile(file, bytes.data(),
                            static_cast<DWORD>(bytes.size()),
                            &written, nullptr) != FALSE &&
                static_cast<std::size_t>(written) == bytes.size();
            const bool successfulClose = ::CloseHandle(file) != FALSE;

            if (!successfulWrite || !successfulClose)
            {
                ::DeleteFileA(filename);
                TraceError("[DX11 LOD CAPTURE] write failed; DX9 unchanged");
                return;
            }

            TraceError("[DX11 LOD CAPTURE] created %s (%u bytes, LOD0=%u LOD1=%u LOD2=%u)",
                       filename, static_cast<unsigned>(written),
                       static_cast<unsigned>(original[0].count),
                       static_cast<unsigned>(original[1].count),
                       static_cast<unsigned>(original[2].count));
        }
        catch (...)
        {
            // Diagnostics are best-effort: never make the original DX9 map
            // loader fail because snapshot allocation or I/O failed.
            TraceError("[DX11 LOD CAPTURE] exception; DX9 unchanged");
        }
    }
}
#endif

void CMapOutdoor::SetIndexBuffer()
{
	void * pIndices;
	long x, y;

	uint32_t dwIndexNum = TERRAIN_PATCHSIZE * TERRAIN_PATCHSIZE * 4;

#ifdef WORLD_EDITOR
	m_pwIndices = new uint16_t[dwIndexNum];
	if (!m_pwIndices)
		TraceError("CMapOutdoor::SetIndexBuffer() IndexBuffer is nullptr");

	std::fill_n(m_pwIndices, dwIndexNum, uint16_t{});
	if (!m_IndexBuffer.Create(dwIndexNum, D3DFMT_INDEX16))
		TraceError("CMapOutdoor::SetIndexBuffer() IndexBuffer Create Error");

	uint16_t count = 0;
	uint16_t count2 = 0;
	long ry = 0;

	uint8_t ucNumLineWarp = TERRAIN_PATCHSIZE + 1;

	for (y = 0; y < TERRAIN_PATCHSIZE; y++)
	{
		if (ry % 2 == 0)
		{
			m_pwIndices[count++] = count2;
			m_pwIndices[count++] = count2 + ucNumLineWarp;
		}
		else
		{
			m_pwIndices[count++] = count2 + ucNumLineWarp;
			m_pwIndices[count++] = count2;
		}

		for (x = 0; x < TERRAIN_PATCHSIZE; x++)
		{
			if (ry % 2 == 1)
			{
				m_pwIndices[count++] = (uint16_t)(count2 + ucNumLineWarp - 1);
				m_pwIndices[count++] = (uint16_t)(count2 - 1);
				count2 -= (int16_t) 1;
			}
			else
			{
				m_pwIndices[count++] = (uint16_t)(count2 + 1);
				m_pwIndices[count++] = (uint16_t)(count2 + ucNumLineWarp + 1);
				count2 += (int16_t) 1;
			}
		}

		if (y < TERRAIN_PATCHSIZE - 1)
		{
			m_pwIndices[count++] = (uint16_t)(count2 + ucNumLineWarp);
			m_pwIndices[count++] = (uint16_t)(count2 + ucNumLineWarp);
			count2 += ucNumLineWarp;
		}
		ry++;
	}

	m_wNumIndices = count;
	if (!m_IndexBuffer.Lock((void **) &pIndices))
		TraceError("CMapOutdoor::SetIndexBuffer() IndexBuffer Unlock Error");
	memcpy(pIndices, m_pwIndices, count * sizeof(uint16_t));
	m_IndexBuffer.Unlock();

	delete[] m_pwIndices;
	m_pwIndices = nullptr;
#else
	uint16_t count[TERRAINPATCH_LODMAX], count2[TERRAINPATCH_LODMAX];
	uint8_t uci;
	for (uci = 0; uci < TERRAINPATCH_LODMAX; ++uci)
	{
		m_pwaIndices[uci] = new uint16_t[dwIndexNum];
		memset(m_pwaIndices[uci], 0, sizeof(uint16_t) * dwIndexNum);
		count[uci] = 0;
		count2[uci] = 0;
		if (!m_IndexBuffer[uci].Create(dwIndexNum, D3DFMT_INDEX16))
			TraceError("CMapOutdoor::SetIndexBuffer() IndexBuffer Create Error");
	}

	uint8_t ucNumLineWarp = TERRAIN_PATCHSIZE + 1;

	for (y = 0; y < TERRAIN_PATCHSIZE; y++)
	{
		if (y % 2 == 0)
		{
			m_pwaIndices[0][(count[0])++] = count2[0];
			m_pwaIndices[0][(count[0])++] = count2[0] + ucNumLineWarp;
		}
		else
		{
			m_pwaIndices[0][(count[0])++] = count2[0] + ucNumLineWarp;
			m_pwaIndices[0][(count[0])++] = count2[0];
		}

		for (x = 0; x < TERRAIN_PATCHSIZE; x++)
		{
			if (y % 2 == 0)
			{
				m_pwaIndices[0][(count[0])++] = (uint16_t)(count2[0] + 1);
				m_pwaIndices[0][(count[0])++] = (uint16_t)(count2[0] + ucNumLineWarp + 1);
				count2[0] += (int16_t) 1;
			}
			else
			{
				m_pwaIndices[0][(count[0])++] = (uint16_t)(count2[0] + ucNumLineWarp - 1);
				m_pwaIndices[0][(count[0])++] = (uint16_t)(count2[0] - 1);
				count2[0] -= (int16_t) 1;
			}

			if (0 == x % 2)
			{
				if (0 == y)
				{
					if (0 == x)
						ADDLvl1TL(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
					else if ((TERRAIN_PATCHSIZE - 2) == x)
						ADDLvl1TR(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
					else
						ADDLvl1T(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
				}
				else if ((TERRAIN_PATCHSIZE - 2) == y)
				{
					if (0 == x)
						ADDLvl1BL(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
					else if ((TERRAIN_PATCHSIZE - 2) == x)
						ADDLvl1BR(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
					else
						ADDLvl1B(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
				}
				else if (0 == y % 2)
				{
					if (0 == x)
						ADDLvl1L(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
					else if ((TERRAIN_PATCHSIZE - 2) == x)
						ADDLvl1R(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
					else
						ADDLvl1M(m_pwaIndices[1], count[1], count2[1], ucNumLineWarp);
				}
				count2[1] += 2;
			}

			if (0 == x % 4)
			{
				if (0 == y)
				{
					if (0 == x)
						ADDLvl2TL(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
					else if ((TERRAIN_PATCHSIZE - 4) == x)
						ADDLvl2TR(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
					else
						ADDLvl2T(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
				}
				else if ((TERRAIN_PATCHSIZE - 4) == y)
				{
					if (0 == x)
						ADDLvl2BL(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
					else if ((TERRAIN_PATCHSIZE - 4) == x)
						ADDLvl2BR(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
					else
						ADDLvl2B(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
				}
				else if (0 == y % 4)
				{
					if (0 == x)
						ADDLvl2L(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
					else if ((TERRAIN_PATCHSIZE - 4) == x)
						ADDLvl2R(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
					else
						ADDLvl2M(m_pwaIndices[2], count[2], count2[2], ucNumLineWarp);
				}
				count2[2] += 4;
			}
		}

		if (y < TERRAIN_PATCHSIZE - 1)
		{
			m_pwaIndices[0][(count[0])++] = (uint16_t)(count2[0] + ucNumLineWarp);
			m_pwaIndices[0][(count[0])++] = (uint16_t)(count2[0] + ucNumLineWarp);
			count2[0] += ucNumLineWarp;
			if (0 == y % 2)
				count2[1] += 2;
			if (0 == y % 4)
				count2[2] += 4;
		}
	}

#if defined(M2_DX11_LOD_CAPTURE) && defined(_WIN64) && !defined(WORLD_EDITOR)
	// Capture the original three index arrays BEFORE the normal GPU upload
	// and delete[]. Never change index order, counts or rendering state.
	static_assert(TERRAINPATCH_LODMAX == 3, "Snapshot format expects 3 LODs");
	const std::array<m2::render::LegacyIndexView, 3> originalLOD{{
		{m_pwaIndices[0], count[0]},
		{m_pwaIndices[1], count[1]},
		{m_pwaIndices[2], count[2]}
	}};
	TryCaptureOriginalTerrainLOD(originalLOD);
#endif

	for (uci = 0; uci < TERRAINPATCH_LODMAX; ++uci)
	{
		m_wNumIndices[uci] = count[uci];
		if (!m_IndexBuffer[uci].Lock((void **) &pIndices))
			TraceError("CMapOutdoor::SetIndexBuffer() IndexBuffer Unlock Error");
		memcpy(pIndices, m_pwaIndices[uci], count[uci] * sizeof(uint16_t));
		m_IndexBuffer[uci].Unlock();
		delete[] m_pwaIndices[uci];
		m_pwaIndices[uci] = nullptr;
	}
#endif
}

void CMapOutdoor::ADDLvl1TL(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
	pIndices[rwCount++] = c_rwCurCount + 1;
	pIndices[rwCount++] = c_rwCurCount;
}

void CMapOutdoor::ADDLvl1T(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 1;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
}

void CMapOutdoor::ADDLvl1TR(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 1;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
}

void CMapOutdoor::ADDLvl1L(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
}

void CMapOutdoor::ADDLvl1R(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
}

void CMapOutdoor::ADDLvl1BL(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
}

void CMapOutdoor::ADDLvl1B(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
}

void CMapOutdoor::ADDLvl1BR(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 1;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp + 1;
}

void CMapOutdoor::ADDLvl1M(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + 2;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 2;
}

void CMapOutdoor::ADDLvl2TL(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	ADDLvl1TL(pIndices, rwCount, c_rwCurCount, c_rucNumLineWarp);
	ADDLvl1T(pIndices, rwCount, c_rwCurCount + 2, c_rucNumLineWarp);
	ADDLvl1L(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2, c_rucNumLineWarp);
	ADDLvl1M(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2 + 2, c_rucNumLineWarp);
}

void CMapOutdoor::ADDLvl2T(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	ADDLvl1T(pIndices, rwCount, c_rwCurCount, c_rucNumLineWarp);
	ADDLvl1T(pIndices, rwCount, c_rwCurCount + 2, c_rucNumLineWarp);

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4 + 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4 + 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
}

void CMapOutdoor::ADDLvl2TR(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	ADDLvl1T(pIndices, rwCount, c_rwCurCount, c_rucNumLineWarp);
	ADDLvl1TR(pIndices, rwCount, c_rwCurCount + 2, c_rucNumLineWarp);
	ADDLvl1M(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2, c_rucNumLineWarp);
	ADDLvl1R(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2 + 2, c_rucNumLineWarp);
}

void CMapOutdoor::ADDLvl2L(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	ADDLvl1L(pIndices, rwCount, c_rwCurCount, c_rucNumLineWarp);
	ADDLvl1L(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2, c_rucNumLineWarp);

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 4;

	pIndices[rwCount++] = c_rwCurCount + 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4 + 4;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4 + 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4 + 2;
}

void CMapOutdoor::ADDLvl2R(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	ADDLvl1R(pIndices, rwCount, c_rwCurCount + 2, c_rucNumLineWarp);
	ADDLvl1R(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2 + 2, c_rucNumLineWarp);

	pIndices[rwCount++] = c_rwCurCount + 2;
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;

	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4 + 2;
}

void CMapOutdoor::ADDLvl2BL(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	ADDLvl1L(pIndices, rwCount, c_rwCurCount, c_rucNumLineWarp);
	ADDLvl1M(pIndices, rwCount, c_rwCurCount + 2, c_rucNumLineWarp);
	ADDLvl1BL(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2, c_rucNumLineWarp);
	ADDLvl1B(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2 + 2, c_rucNumLineWarp);
}

void CMapOutdoor::ADDLvl2B(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount;

	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + 4;

	pIndices[rwCount++] = c_rwCurCount + 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 2;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 2 + 4;

	ADDLvl1B(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2, c_rucNumLineWarp);
	ADDLvl1B(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2 + 2, c_rucNumLineWarp);
}

void CMapOutdoor::ADDLvl2BR(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	ADDLvl1M(pIndices, rwCount, c_rwCurCount, c_rucNumLineWarp);
	ADDLvl1R(pIndices, rwCount, c_rwCurCount + 2, c_rucNumLineWarp);
	ADDLvl1B(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2, c_rucNumLineWarp);
	ADDLvl1BR(pIndices, rwCount, c_rwCurCount + c_rucNumLineWarp * 2 + 2, c_rucNumLineWarp);
}

void CMapOutdoor::ADDLvl2M(uint16_t * pIndices, uint16_t & rwCount, const uint16_t & c_rwCurCount, const uint8_t & c_rucNumLineWarp)
{
	pIndices[rwCount++] = c_rwCurCount;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4;
	pIndices[rwCount++] = c_rwCurCount + 4;

	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4;
	pIndices[rwCount++] = c_rwCurCount + c_rucNumLineWarp * 4 + 4;
	pIndices[rwCount++] = c_rwCurCount + 4;
}

