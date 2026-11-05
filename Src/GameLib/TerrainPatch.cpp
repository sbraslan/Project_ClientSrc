// TerrainPatch.cpp: implementation of the CTerrainPatch class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TerrainPatch.h"

// Diagnostic is absent in normal builds and reads ONLY the CPU source
// before existing DX9 hardware/software vertex buffer upload.
#if defined(M2_DX11_PATCH_CAPTURE) && defined(_WIN64) && !defined(WORLD_EDITOR)
#include "../EterLib/Renderer/DX11LegacyPatchSnapshot.h"
#include <array>
#include <atomic>
#include <vector>

namespace
{
    void TryCaptureOriginalPatch(const HardwareTransformPatch_SSourceVertex* source)
    {
        char flag[2]{};
        const DWORD flagLength = ::GetEnvironmentVariableA(
            "METIN2_DX11_PATCH_CAPTURE_ONCE", flag,
            static_cast<DWORD>(sizeof(flag)));
        const bool enabled = flagLength == 1 && flag[0] == '1';

        // One entry per instrumented process, even when opt-in flag is absent.
        static std::atomic<bool> logged{false};
        if (!logged.exchange(true))
            TraceError("[DX11 PATCH DIAG] hook reached | envLength=%lu | enabled=%u",
                       static_cast<unsigned long>(flagLength), enabled ? 1u : 0u);
        if (!enabled)
            return;

        static std::atomic<bool> attempted{false};
        if (attempted.exchange(true))
            return;

        // Fixed, validated vertex count: 17x17, each original position+normal.
        static_assert(CTerrainPatch::TERRAIN_VERTEX_COUNT ==
                      m2::render::DX11LegacyPatchSnapshot::kVertexCount,
                      "Legacy patch vertex count changed");
        constexpr char filename[] = "dx11_terrain_patch_capture.bin";

        try
        {
            if (!source)
            {
                TraceError("[DX11 PATCH CAPTURE] missing source; DX9 unchanged");
                return;
            }

            std::array<m2::render::LegacyPatchVertex,
                       m2::render::DX11LegacyPatchSnapshot::kVertexCount> vertices{};
            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                vertices[i].position[0] = source[i].kPosition.x;
                vertices[i].position[1] = source[i].kPosition.y;
                vertices[i].position[2] = source[i].kPosition.z;
                vertices[i].normal[0] = source[i].kNormal.x;
                vertices[i].normal[1] = source[i].kNormal.y;
                vertices[i].normal[2] = source[i].kNormal.z;
            }

            std::vector<std::uint8_t> encoded;
            if (!m2::render::DX11LegacyPatchSnapshot::Encode(
                    vertices.data(), vertices.size(), encoded))
            {
                TraceError("[DX11 PATCH CAPTURE] validation failed; DX9 unchanged");
                return;
            }

            HANDLE handle = ::CreateFileA(
                filename, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (handle == INVALID_HANDLE_VALUE)
            {
                TraceError("[DX11 PATCH CAPTURE] cannot create %s; Win32=%lu",
                           filename, ::GetLastError());
                return;
            }

            DWORD written = 0;
            const bool writtenOk = ::WriteFile(
                handle, encoded.data(), static_cast<DWORD>(encoded.size()),
                &written, nullptr) != FALSE && written == encoded.size();
            const bool closedOk = ::CloseHandle(handle) != FALSE;
            if (!writtenOk || !closedOk)
            {
                ::DeleteFileA(filename);
                TraceError("[DX11 PATCH CAPTURE] write failed; DX9 unchanged");
                return;
            }

            TraceError("[DX11 PATCH CAPTURE] created %s (%u bytes, vertices=%u)",
                       filename, static_cast<unsigned>(written),
                       static_cast<unsigned>(vertices.size()));
        }
        catch (...)
        {
            TraceError("[DX11 PATCH CAPTURE] exception; DX9 unchanged");
        }
    }
}
#endif

// Second, strictly independent, compile + process gated capture path for
// exactly two REAL spatially adjacent source patches. No default DX9 effect.
// Does NOT overwrite the previously validated single-patch snapshot.
#if defined(M2_DX11_ADJACENT_CAPTURE) && defined(_WIN64) && !defined(WORLD_EDITOR)
#include "../EterLib/Renderer/DX11LegacyPatchSnapshot.h"
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <utility>
#include <vector>

namespace
{
    struct DX11AdjacentPatchCandidate
    {
        float worldX = 0.0f;
        float worldY = 0.0f;
        std::vector<std::uint8_t> encoded;
    };

    bool DX11WriteNewBinary(const char* name,
                            const std::vector<std::uint8_t>& data)
    {
        HANDLE handle = ::CreateFileA(
            name, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return false;
        DWORD written = 0;
        const bool correct = ::WriteFile(
            handle, data.data(), static_cast<DWORD>(data.size()),
            &written, nullptr) != FALSE && written == data.size();
        const bool closed = ::CloseHandle(handle) != FALSE;
        if (!correct || !closed)
        {
            // Our own newly created file only; existing captures are untouched.
            ::DeleteFileA(name);
            return false;
        }
        return true;
    }

    void TryCaptureAdjacentPatches(
        const HardwareTransformPatch_SSourceVertex* source)
    {
        char enabledBuffer[2]{};
        const DWORD envLength = ::GetEnvironmentVariableA(
            "METIN2_DX11_ADJACENT_CAPTURE_ONCE", enabledBuffer,
            static_cast<DWORD>(sizeof(enabledBuffer)));
        const bool enabled = envLength == 1 && enabledBuffer[0] == '1';

        static std::atomic<bool> logOnce{false};
        if (!logOnce.exchange(true))
            TraceError("[DX11 ADJACENT DIAG] hook reached | envLength=%lu | enabled=%u",
                       static_cast<unsigned long>(envLength),
                       enabled ? 1u : 0u);
        if (!enabled || !source)
            return;

        // 17x17 source layout is shared with the verified single-patch codec.
        static_assert(CTerrainPatch::TERRAIN_VERTEX_COUNT ==
                      m2::render::DX11LegacyPatchSnapshot::kVertexCount,
                      "Unexpected original terrain patch dimensions");
        try
        {
            std::array<m2::render::LegacyPatchVertex,
                       m2::render::DX11LegacyPatchSnapshot::kVertexCount> vertices{};
            for (std::size_t i = 0; i < vertices.size(); ++i)
            {
                vertices[i].position[0] = source[i].kPosition.x;
                vertices[i].position[1] = source[i].kPosition.y;
                vertices[i].position[2] = source[i].kPosition.z;
                vertices[i].normal[0] = source[i].kNormal.x;
                vertices[i].normal[1] = source[i].kNormal.y;
                vertices[i].normal[2] = source[i].kNormal.z;
            }
            std::vector<std::uint8_t> encoded;
            if (!m2::render::DX11LegacyPatchSnapshot::Encode(
                    vertices.data(), vertices.size(), encoded))
                return;

            static std::mutex candidateMutex;
            static std::vector<DX11AdjacentPatchCandidate> candidates;
            static bool finished = false;
            const std::lock_guard<std::mutex> lock(candidateMutex);
            if (finished)
                return;

            constexpr double patchSpan = 3200.0;
            constexpr double originTolerance = 0.05;
            const double x = vertices[0].position[0];
            const double y = vertices[0].position[1];
            for (const auto& prior : candidates)
            {
                const double dx = x - prior.worldX;
                const double dy = y - prior.worldY;
                const bool adjacent =
                    (std::abs(std::abs(dx) - patchSpan) <= originTolerance &&
                     std::abs(dy) <= originTolerance) ||
                    (std::abs(std::abs(dy) - patchSpan) <= originTolerance &&
                     std::abs(dx) <= originTolerance);
                if (!adjacent)
                    continue;

                finished = true; // One pair attempt per process.
                constexpr char first[] = "dx11_terrain_adjacent_A.bin";
                constexpr char second[] = "dx11_terrain_adjacent_B.bin";

                // A previous independent capture must never be deleted.
                if (::GetFileAttributesA(first) != INVALID_FILE_ATTRIBUTES ||
                    ::GetFileAttributesA(second) != INVALID_FILE_ATTRIBUTES)
                {
                    TraceError("[DX11 ADJACENT CAPTURE] output exists; no overwrite");
                    return;
                }
                if (!DX11WriteNewBinary(first, prior.encoded))
                {
                    TraceError("[DX11 ADJACENT CAPTURE] first write failed; DX9 unchanged");
                    return;
                }
                if (!DX11WriteNewBinary(second, encoded))
                {
                    // Only delete first if we created it in this attempt.
                    ::DeleteFileA(first);
                    TraceError("[DX11 ADJACENT CAPTURE] second write failed; DX9 unchanged");
                    return;
                }

                TraceError("[DX11 ADJACENT CAPTURE] created %s + %s | A origin=(%.0f, %.0f) | B origin=(%.0f, %.0f) | delta=(%.0f, %.0f) | bytes=%u each",
                           first, second,
                           static_cast<double>(prior.worldX),
                           static_cast<double>(prior.worldY),
                           x, y, dx, dy,
                           static_cast<unsigned>(encoded.size()));
                return;
            }

            // Bounded in-memory search; repeated origins do not accumulate.
            for (const auto& prior : candidates)
                if (std::abs(x - prior.worldX) <= originTolerance &&
                    std::abs(y - prior.worldY) <= originTolerance)
                    return;

            constexpr std::size_t kMaxCandidates = 256;
            if (candidates.size() >= kMaxCandidates)
            {
                finished = true;
                TraceError("[DX11 ADJACENT CAPTURE] no neighboring pair in %u candidates",
                           static_cast<unsigned>(kMaxCandidates));
                return;
            }
            DX11AdjacentPatchCandidate candidate;
            candidate.worldX = static_cast<float>(x);
            candidate.worldY = static_cast<float>(y);
            candidate.encoded.swap(encoded);
            candidates.push_back(std::move(candidate));
        }
        catch (...)
        {
            TraceError("[DX11 ADJACENT CAPTURE] exception; DX9 unchanged");
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTerrainPatch::SSoftwareTransformPatch::SSoftwareTransformPatch()
{
	__Initialize();
}

CTerrainPatch::SSoftwareTransformPatch::~SSoftwareTransformPatch()
{
	Destroy();
}

void CTerrainPatch::SSoftwareTransformPatch::Create()
{
	assert(nullptr == m_akTerrainVertex);
	m_akTerrainVertex = new SoftwareTransformPatch_SSourceVertex[TERRAIN_VERTEX_COUNT];
}

void CTerrainPatch::SSoftwareTransformPatch::Destroy()
{
	delete[] m_akTerrainVertex;

	__Initialize();
}

void CTerrainPatch::SSoftwareTransformPatch::__Initialize()
{
	m_akTerrainVertex = nullptr;
}

bool CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE = TRUE;

void CTerrainPatch::Clear()
{
	m_kHT.m_kVB.Destroy();
	m_kST.Destroy();

	m_WaterVertexBuffer.Destroy();
	ClearID();
	SetUse(false);

	m_bWaterExist = false;
	m_bNeedUpdate = true;

	m_dwWaterPriCount = 0;
	m_byType = PATCH_TYPE_PLAIN;

	m_fMinX = m_fMaxX = m_fMinY = m_fMaxY = m_fMinZ = m_fMaxZ = 0.0f;

	m_dwVersion = 0;
}

void CTerrainPatch::BuildWaterVertexBuffer(SWaterVertex * akSrcVertex, uint32_t uWaterVertexCount)
{
	CGraphicVertexBuffer & rkVB = m_WaterVertexBuffer;

	if (!rkVB.Create(uWaterVertexCount, D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DUSAGE_WRITEONLY, D3DPOOL_MANAGED))
		return;

	SWaterVertex * akDstWaterVertex;
	if (rkVB.Lock((void **) &akDstWaterVertex))
	{
		uint32_t uVBSize = sizeof(SWaterVertex) * uWaterVertexCount;
		memcpy(akDstWaterVertex, akSrcVertex, uVBSize);
		m_dwWaterPriCount = uWaterVertexCount / 3;

		rkVB.Unlock();
	}
}

void CTerrainPatch::BuildTerrainVertexBuffer(HardwareTransformPatch_SSourceVertex * akSrcVertex)
{
#if defined(M2_DX11_PATCH_CAPTURE) && defined(_WIN64) && !defined(WORLD_EDITOR)
    TryCaptureOriginalPatch(akSrcVertex);
#endif
#if defined(M2_DX11_ADJACENT_CAPTURE) && defined(_WIN64) && !defined(WORLD_EDITOR)
    TryCaptureAdjacentPatches(akSrcVertex);
#endif
	if (SOFTWARE_TRANSFORM_PATCH_ENABLE)
		__BuildSoftwareTerrainVertexBuffer(akSrcVertex);
	else
		__BuildHardwareTerrainVertexBuffer(akSrcVertex);
}

void CTerrainPatch::__BuildSoftwareTerrainVertexBuffer(HardwareTransformPatch_SSourceVertex * akSrcVertex)
{
	//uint32_t dwVBSize=sizeof(HardwareTransformPatch_SSourceVertex)*TERRAIN_VERTEX_COUNT;

	m_kST.Create();

	SoftwareTransformPatch_SSourceVertex * akDstVertex = SoftwareTransformPatch_GetTerrainVertexDataPtr();
	for (uint32_t uIndex = 0; uIndex != TERRAIN_VERTEX_COUNT; ++uIndex)
	{
		*((HardwareTransformPatch_SSourceVertex *) (akDstVertex + uIndex)) = *(akSrcVertex + uIndex);
		akDstVertex[uIndex].dwDiffuse = 0xFFFFFFFF;
	}
}

void CTerrainPatch::__BuildHardwareTerrainVertexBuffer(HardwareTransformPatch_SSourceVertex * akSrcVertex)
{
	CGraphicVertexBuffer & rkVB = m_kHT.m_kVB;
	if (!rkVB.Create(TERRAIN_VERTEX_COUNT, D3DFVF_XYZ | D3DFVF_NORMAL, D3DUSAGE_WRITEONLY, D3DPOOL_MANAGED))
		return;

	HardwareTransformPatch_SSourceVertex * akDstVertex;
	if (rkVB.Lock((void **) &akDstVertex))
	{
		uint32_t uVBSize = sizeof(HardwareTransformPatch_SSourceVertex) * TERRAIN_VERTEX_COUNT;

		memcpy(akDstVertex, akSrcVertex, uVBSize);
		rkVB.Unlock();
	}
}

void CTerrainPatch::SoftwareTransformPatch_UpdateTerrainLighting(uint32_t dwVersion, const D3DLIGHT9 & c_rkLight,
																 const D3DMATERIAL9 & c_rkMtrl)
{
	if (m_dwVersion == dwVersion)
		return;

	m_dwVersion = dwVersion;

	SoftwareTransformPatch_SSourceVertex * akSrcVertex = SoftwareTransformPatch_GetTerrainVertexDataPtr();
	if (!akSrcVertex)
		return;

	D3DXVECTOR3 kLightDir = c_rkLight.Direction;


	uint32_t dwDot;
	uint32_t dwAmbientR = (c_rkMtrl.Ambient.r * c_rkLight.Ambient.r + c_rkMtrl.Emissive.r) * 255.0f;
	uint32_t dwAmbientG = (c_rkMtrl.Ambient.g * c_rkLight.Ambient.g + c_rkMtrl.Emissive.g) * 255.0f;
	uint32_t dwAmbientB = (c_rkMtrl.Ambient.b * c_rkLight.Ambient.b + c_rkMtrl.Emissive.b) * 255.0f;
	uint32_t dwDiffuseR = (c_rkMtrl.Diffuse.r * c_rkLight.Diffuse.r) * 255.0f;
	uint32_t dwDiffuseG = (c_rkMtrl.Diffuse.g * c_rkLight.Diffuse.g) * 255.0f;
	uint32_t dwDiffuseB = (c_rkMtrl.Diffuse.b * c_rkLight.Diffuse.b) * 255.0f;

	if (dwDiffuseR > 255 - dwAmbientR)
		dwDiffuseR = 255 - dwAmbientR;

	if (dwDiffuseG + dwAmbientG > 255)
		dwDiffuseG = 255 - dwAmbientG;

	if (dwDiffuseB + dwAmbientB > 255)
		dwDiffuseB = 255 - dwAmbientB;

	for (uint32_t uIndex = 0; uIndex != CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
	{
		float fDot = D3DXVec3Dot(&akSrcVertex[uIndex].kNormal, &kLightDir);

		const float N = 0xffffff;
		const int S = 24;
		if (fDot < 0.0f)
			dwDot = (N * -fDot);
		else
			dwDot = (N * +fDot);

		akSrcVertex[uIndex].dwDiffuse = (0xff000000) | (((dwDiffuseR * dwDot >> S) + dwAmbientR) << 16) |
			(((dwDiffuseG * dwDot >> S) + dwAmbientG) << 8) | ((dwDiffuseB * dwDot >> S) + dwAmbientB);
	}
}

uint32_t CTerrainPatch::GetWaterFaceCount()
{
	return m_dwWaterPriCount;
}

CTerrainPatchProxy::CTerrainPatchProxy()
{
	Clear();
}

CTerrainPatchProxy::~CTerrainPatchProxy()
{
	Clear();
}

void CTerrainPatchProxy::SetCenterPosition(const D3DXVECTOR3 & c_rv3Center)
{
	m_v3Center = c_rv3Center;
}

bool CTerrainPatchProxy::IsIn(const D3DXVECTOR3 & c_rv3Target, float fRadius)
{
	float dx = m_v3Center.x - c_rv3Target.x;
	float dy = m_v3Center.y - c_rv3Target.y;
	float fDist = dx * dx + dy * dy;
	float fCheck = fRadius * fRadius;

	if (fDist < fCheck)
		return true;

	return false;
}

void CTerrainPatchProxy::SoftwareTransformPatch_UpdateTerrainLighting(uint32_t dwVersion, const D3DLIGHT9 & c_rkLight,
																	  const D3DMATERIAL9 & c_rkMtrl)
{
	if (m_pTerrainPatch)
		m_pTerrainPatch->SoftwareTransformPatch_UpdateTerrainLighting(dwVersion, c_rkLight, c_rkMtrl);
}

CGraphicVertexBuffer * CTerrainPatchProxy::HardwareTransformPatch_GetVertexBufferPtr()
{
	if (m_pTerrainPatch)
		return m_pTerrainPatch->HardwareTransformPatch_GetVertexBufferPtr();

	return nullptr;
}

SoftwareTransformPatch_SSourceVertex * CTerrainPatchProxy::SoftwareTransformPatch_GetTerrainVertexDataPtr()
{
	if (m_pTerrainPatch)
		return m_pTerrainPatch->SoftwareTransformPatch_GetTerrainVertexDataPtr();

	return nullptr;
}

uint32_t CTerrainPatchProxy::GetWaterFaceCount()
{
	if (m_pTerrainPatch)
		return m_pTerrainPatch->GetWaterFaceCount();

	return 0;
}

void CTerrainPatchProxy::Clear()
{
	m_bUsed = false;
	m_sPatchNum = 0;
	m_byTerrainNum = 0xFF;

	m_pTerrainPatch = nullptr;
}
