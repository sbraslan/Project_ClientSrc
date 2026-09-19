#pragma once

#include <d3d9/d3dx9.h>

#include "../EterLib/TextFileLoader.h"

#include "Type.h"
#include "EffectElementBase.h"

class CLightData : public CEffectElementBase
{
	friend class CLightInstance;

public:
	CLightData();
	~CLightData() override;

	void GetRange(float fTime, float & rRange);
	float GetDuration() const;

	BOOL isLoop() const { return m_bLoopFlag; }

	int GetLoopCount() const { return m_iLoopCount; }

	void InitializeLight(D3DLIGHT9 & light);

protected:
	void OnClear() override;
	bool OnIsData() override;

	BOOL OnLoadScript(CTextFileLoader & rTextFileLoader) override;

protected:
	float m_fMaxRange{};
	float m_fDuration{};
	TTimeEventTableFloat m_TimeEventTableRange;

	D3DXCOLOR m_cAmbient;
	D3DXCOLOR m_cDiffuse;

	BOOL m_bLoopFlag{};
	int m_iLoopCount{};

	float m_fAttenuation0{};
	float m_fAttenuation1{};
	float m_fAttenuation2{};

public:
	static void DestroySystem();

	static CLightData * New();
	static void Delete(CLightData * pkData);

	static CDynamicPool<CLightData> ms_kPool;
};
