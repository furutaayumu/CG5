#include "PipelineState.h"
#include"KamataEngine.h"

using namespace KamataEngine;

//PipelineStateを生成する
void PipelineState::Create(D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc)
{
	//クラス内で取得する為に追加
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	
	ID3D12PipelineState* graphicsPipeLineState = nullptr;
	HRESULT hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(
	&graphicsPipelineStateDesc,IID_PPV_ARGS(&graphicsPipeLineState));
	assert(SUCCEEDED(hr));

	//生成したPipelineStateを取っておく
	pipelineState_ = graphicsPipeLineState;
}

//生成したPipelineStateを探す
ID3D12PipelineState* PipelineState::Get()
{
	return pipelineState_;
}

//コンストラクタ
PipelineState::PipelineState() {}

//デストラクタ
PipelineState::~PipelineState()
{
	if (pipelineState_)
	{
		pipelineState_->Release();
		pipelineState_ = nullptr;
	}
}
