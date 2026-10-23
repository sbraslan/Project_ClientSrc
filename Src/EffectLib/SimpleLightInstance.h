#pragma once

#include "EffectElementBaseInstance.h"
#include "SimpleLightData.h"

class CLightInstance : public CEffectElementBaseInstance
{
public:
	friend class CLightData;

	CLightInstance();
	~CLightInstance() override;

protected:
	void OnSetDataPointer(CEffectElementBase * pElement) override;

	void OnInitialize() override;
	void OnDestroy() override;

	bool OnUpdate(float fElapsedTime) override;
	void OnRender() override;

	uint32_t m_LightID{};
	CLightData * m_pData{};
	uint32_t m_dwRangeIndex{};

	uint32_t m_iLoopCount{};

public:
	static void DestroySystem();

	static CLightInstance * New();
	static void Delete(CLightInstance * pkData);

	static CDynamicPool<CLightInstance> ms_kPool;
};
