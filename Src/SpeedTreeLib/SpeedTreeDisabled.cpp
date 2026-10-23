#include "StdAfx.h"

#if !defined(_WIN64)
#error "SpeedTreeDisabled.cpp is only for the native Win64 migration build."
#endif

#include "CSpeedTreeDirectX.h"
#include "SpeedTreeWrapper.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

unsigned int CSpeedTreeWrapper::m_unNumWrappersActive = 0;
bool CSpeedTreeWrapper::ms_bSelfShadowOn = true;
LPDIRECT3DVERTEXSHADER9 CSpeedTreeWrapper::ms_lpBranchVertexShader = nullptr;
LPDIRECT3DVERTEXSHADER9 CSpeedTreeWrapper::ms_lpLeafVertexShader = nullptr;

CSpeedTreeWrapper::CSpeedTreeWrapper()
    : m_pSpeedTree(nullptr),
      m_pTextureInfo(nullptr),
      m_bIsInstance(false),
      m_pInstanceOf(nullptr),
      m_pGeometryCache(nullptr),
      m_pBranchVertexBuffer(nullptr),
      m_unBranchVertexCount(0),
      m_pBranchIndexBuffer(nullptr),
      m_pBranchIndexCounts(nullptr),
      m_pFrondVertexBuffer(nullptr),
      m_unFrondVertexCount(0),
      m_unNumFrondLods(0),
      m_pFrondIndexBuffers(nullptr),
      m_pFrondIndexCounts(nullptr),
      m_usNumLeafLods(0),
      m_pLeafVertexBuffer(nullptr),
      m_pLeavesUpdatedByCpu(nullptr)
{
    std::fill(std::begin(m_afPos), std::end(m_afPos), 0.0f);
    std::fill(std::begin(m_afBoundingBox), std::end(m_afBoundingBox), 0.0f);
    ++m_unNumWrappersActive;
}

CSpeedTreeWrapper::~CSpeedTreeWrapper()
{
    if (m_unNumWrappersActive > 0)
        --m_unNumWrappersActive;
}

uint32_t CSpeedTreeWrapper::GetCollisionObjectCount()
{
    return 0;
}

void CSpeedTreeWrapper::GetCollisionObject(
    uint32_t,
    CSpeedTreeRT::ECollisionObjectType&,
    float*,
    float*)
{
}

void CSpeedTreeWrapper::OnUpdateCollisionData(const CStaticCollisionDataVector*)
{
}

bool CSpeedTreeWrapper::GetBoundingSphere(D3DXVECTOR3& center, float& radius)
{
    center = D3DXVECTOR3(m_afPos[0], m_afPos[1], m_afPos[2]);
    radius = 0.0f;
    return true;
}

void CSpeedTreeWrapper::SetPosition(float x, float y, float z)
{
    m_afPos[0] = x;
    m_afPos[1] = y;
    m_afPos[2] = z;
    CGraphicObjectInstance::SetPosition(x, y, z);
}

void CSpeedTreeWrapper::CalculateBBox()
{
    m_afBoundingBox[0] = m_afBoundingBox[3] = m_afPos[0];
    m_afBoundingBox[1] = m_afBoundingBox[4] = m_afPos[1];
    m_afBoundingBox[2] = m_afBoundingBox[5] = m_afPos[2];
}

void CSpeedTreeWrapper::OnRender() {}
void CSpeedTreeWrapper::OnRenderPCBlocker() {}

bool CSpeedTreeWrapper::LoadTree(
    const char*,
    const uint8_t*,
    unsigned int,
    unsigned int,
    float,
    float)
{
    // Migration-only backend. Keep the tree object graph alive without calling
    // the unavailable 32-bit SpeedTreeRT runtime.
    CalculateBBox();
    return true;
}

void CSpeedTreeWrapper::SetupBranchForTreeType() const {}
void CSpeedTreeWrapper::SetupFrondForTreeType() const {}
void CSpeedTreeWrapper::SetupLeafForTreeType() const {}
void CSpeedTreeWrapper::EndLeafForTreeType() {}
void CSpeedTreeWrapper::RenderBranches() const {}
void CSpeedTreeWrapper::RenderFronds() const {}
void CSpeedTreeWrapper::RenderLeaves() const {}
void CSpeedTreeWrapper::RenderBillboards() const {}
void CSpeedTreeWrapper::Advance() {}
void CSpeedTreeWrapper::CleanUpMemory() {}

CSpeedTreeWrapper** CSpeedTreeWrapper::GetInstances(unsigned int& count)
{
    count = static_cast<unsigned int>(m_vInstances.size());
    return m_vInstances.empty() ? nullptr : m_vInstances.data();
}

CSpeedTreeWrapper* CSpeedTreeWrapper::MakeInstance()
{
    auto* instance = new CSpeedTreeWrapper;
    instance->m_bIsInstance = true;
    instance->m_pInstanceOf = this;
    std::copy(std::begin(m_afPos), std::end(m_afPos), std::begin(instance->m_afPos));
    std::copy(std::begin(m_afBoundingBox), std::end(m_afBoundingBox), std::begin(instance->m_afBoundingBox));
    m_vInstances.push_back(instance);
    return instance;
}

void CSpeedTreeWrapper::DeleteInstance(CSpeedTreeWrapper* instance)
{
    const auto it = std::find(m_vInstances.begin(), m_vInstances.end(), instance);
    if (it == m_vInstances.end())
        return;

    delete *it;
    m_vInstances.erase(it);
}

CSpeedTreeForest::CSpeedTreeForest()
    : m_fWindStrength(0.0f),
      m_fAccumTime(0.0f)
{
    std::fill(std::begin(m_afLighting), std::end(m_afLighting), 0.0f);
    std::fill(std::begin(m_afFog), std::end(m_afFog), 0.0f);

    m_afForestExtents[0] = m_afForestExtents[1] = m_afForestExtents[2] = FLT_MAX;
    m_afForestExtents[3] = m_afForestExtents[4] = m_afForestExtents[5] = -FLT_MAX;
}

CSpeedTreeForest::~CSpeedTreeForest()
{
    Clear();
}

void CSpeedTreeForest::ClearMainTree()
{
    Clear();
}

CSpeedTreeWrapper* CSpeedTreeForest::GetMainTree(uint32_t crc)
{
    const auto it = m_pMainTreeMap.find(crc);
    return it == m_pMainTreeMap.end() ? nullptr : it->second;
}

BOOL CSpeedTreeForest::GetMainTree(uint32_t crc, CSpeedTreeWrapper** mainTree, const char* fileName)
{
    if (!mainTree)
        return FALSE;

    if (auto* existing = GetMainTree(crc))
    {
        *mainTree = existing;
        return TRUE;
    }

    auto* tree = new CSpeedTreeWrapper;
    if (!tree->LoadTree(fileName))
    {
        delete tree;
        return FALSE;
    }

    m_pMainTreeMap.emplace(crc, tree);
    *mainTree = tree;
    return TRUE;
}

void CSpeedTreeForest::DeleteMainTree(uint32_t crc)
{
    const auto it = m_pMainTreeMap.find(crc);
    if (it == m_pMainTreeMap.end())
        return;

    auto* tree = it->second;
    unsigned int count = 0;
    while (tree->GetInstances(count) && count > 0)
    {
        auto** instances = tree->GetInstances(count);
        tree->DeleteInstance(instances[0]);
    }

    delete tree;
    m_pMainTreeMap.erase(it);
}

CSpeedTreeWrapper* CSpeedTreeForest::CreateInstance(
    float x,
    float y,
    float z,
    uint32_t treeCrc,
    const char* treeName)
{
    CSpeedTreeWrapper* mainTree = nullptr;
    if (!GetMainTree(treeCrc, &mainTree, treeName))
        return nullptr;

    auto* instance = mainTree->MakeInstance();
    if (!instance)
        return nullptr;

    instance->SetPosition(x, y, z);
    instance->CalculateBBox();
    AdjustExtents(x, y, z);
    return instance;
}

void CSpeedTreeForest::DeleteInstance(CSpeedTreeWrapper* instance)
{
    if (!instance)
        return;

    auto* parent = instance->InstanceOf();
    if (parent)
        parent->DeleteInstance(instance);
}

void CSpeedTreeForest::UpdateSystem(float currentTime)
{
    static float lastTime = currentTime;
    m_fAccumTime += currentTime - lastTime;
    lastTime = currentTime;
}

void CSpeedTreeForest::Clear()
{
    while (!m_pMainTreeMap.empty())
        DeleteMainTree(m_pMainTreeMap.begin()->first);
}

void CSpeedTreeForest::SetLight(const float* direction, const float* ambient, const float* diffuse)
{
    if (!direction || !ambient || !diffuse)
        return;

    m_afLighting[0] = direction[0];
    m_afLighting[1] = direction[1];
    m_afLighting[2] = direction[2];
    m_afLighting[3] = 1.0f;

    std::copy(ambient, ambient + 4, m_afLighting + 4);
    std::copy(diffuse, diffuse + 4, m_afLighting + 8);
}

void CSpeedTreeForest::SetFog(float fogNear, float fogFar)
{
    m_afFog[0] = fogNear;
    m_afFog[1] = fogFar;
    m_afFog[2] = (fogFar != fogNear) ? (1.0f / (fogFar - fogNear)) : 0.0f;
    m_afFog[3] = 0.0f;
}

void CSpeedTreeForest::SetWindStrength(float strength)
{
    m_fWindStrength = strength;
}

void CSpeedTreeForest::SetupWindMatrices(float)
{
}

void CSpeedTreeForest::AdjustExtents(float x, float y, float z)
{
    m_afForestExtents[0] = std::min(m_afForestExtents[0], x);
    m_afForestExtents[1] = std::min(m_afForestExtents[1], y);
    m_afForestExtents[2] = std::min(m_afForestExtents[2], z);
    m_afForestExtents[3] = std::max(m_afForestExtents[3], x);
    m_afForestExtents[4] = std::max(m_afForestExtents[4], y);
    m_afForestExtents[5] = std::max(m_afForestExtents[5], z);
}

CSpeedTreeDirectX::CSpeedTreeDirectX()
    : m_dwBranchVertexShader(nullptr),
      m_dwLeafVertexShader(nullptr)
{
}

CSpeedTreeDirectX::~CSpeedTreeDirectX() = default;

void CSpeedTreeDirectX::UploadWindMatrix(unsigned int, const float*) const
{
}

bool CSpeedTreeDirectX::SetRenderingDevice()
{
    return true;
}

void CSpeedTreeDirectX::Render(unsigned long)
{
}

void CSpeedTreeDirectX::UpdateCompundMatrix(
    const D3DXVECTOR3&,
    const D3DXMATRIX&,
    const D3DXMATRIX&)
{
}

bool CSpeedTreeDirectX::InitVertexShaders()
{
    return true;
}
