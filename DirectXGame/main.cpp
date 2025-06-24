#include "GameScene.h"
#include "IndexBuffer.h"
#include "KamataEngine.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include <Windows.h>
// #include<d3dcompiler.h>
// グローバル関数
using namespace KamataEngine;

// 関数プロトタイプ宣言
void SetupPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps);

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	////エンジンの初期化
	KamataEngine::Initialize(L"LE3D_15_フルタアユム");

	// DirectXCommonインスタンスの取得
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// DirectXCommonクラスが管理している、ウインドウの幅と高さの値の取得
	int32_t w = dxCommon->GetBackBufferWidth();
	int32_t h = dxCommon->GetBackBufferHeight();
	DebugText::GetInstance()->ConsolePrintf(std::format("width:{},height:{}\n", w, h).c_str());

	// DirectXCommonクラスが管理している、コマンドリストの取得
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

	// ゲームシーンのインスタンス生成
	GameScene* gameScene = new GameScene();
	// ゲームシーンの初期化
	gameScene->Initialize();

	// RootSignature作成-------------------
	RootSignature rs;
	rs.Create();

	// 頂点シェーダの読み込みとコンパイル
	Shader vs;
	vs.LoadDxc(L"Resources/shaders/TestVS.hlsl", L"vs_6_0"); // 変更
	assert(vs.GetDxcBlob() != nullptr);

	// ピクセルシェーダの読み込みとコンパイル
	Shader ps;
	ps.LoadDxc(L"Resources/shaders/TestPS.hlsl", L"ps_6_0"); // 変更
	assert(ps.GetDxcBlob() != nullptr);

	// PipelineState作成------------------
	PipelineState pipelineState;
	SetupPipelineState(pipelineState, rs, vs, ps);

	// リソースの確保含め、頂点情報を柔軟に対応できるようにVertexData構造体を新たに作成する
	// Vector4→VertexDataに変更して利用する
	struct VertexData {
		Vector4 position;
	};
	// 頂点データの準備　★00_07で追加
	VertexData vertices[] = {
	    {-1.0f, -1.0f, 0.0f, 1.0f}, // 左下
	    {-1.0f, 1.0f,  0.0f, 1.0f}, // 左上
	    {1.0f,  -1.0f, 0.0f, 1.0f}, // 右下
	    {1.0f,  1.0f,  0.0f, 1.0f}, // 右上
	};

	// 頂点インデックスデータの準備---------★00_07で追加
	uint16_t indices[] = {
	    0, 1, 2, // 下にある三角形
	    2, 1, 3  // 上にある三角形
	};

	// VertexBuffer(VertexResource,VertexResourceView)の生成
	VertexBuffer vb;
	// vb.Create(sizeof(Vector4) * 3, sizeof(Vector4));
	vb.Create(sizeof(vertices), sizeof(vertices[0])); // ★00_07変更

	// 頂点リソースにデータを書き込む ★00_07追加
	VertexData* pGpuVertices = nullptr;
	vb.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pGpuVertices));

	for (int i = 0; i < _countof(vertices); ++i) {
		pGpuVertices[i] = vertices[i];
	}

	// IndexBuffer(IndexResource,IndexResourceView)の生成
	IndexBuffer ib;
	ib.Create(sizeof(indices), sizeof(indices[0]));
	// 頂点インデックスリソースにデータを書き込む
	uint16_t* pGpuIndices = nullptr;
	ib.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pGpuIndices));

	for (int i = 0; i < _countof(indices); ++i) {
		pGpuIndices[i] = indices[i];
	}

	// メインループ
	while (true) {
		// エンジンの更新
		if (KamataEngine::Update()) {
			break;
		}
		// ゲームシーンの更新
		gameScene->Update();
		// 描画開始
		dxCommon->PreDraw();

		// コマンドを詰む
		commandList->SetGraphicsRootSignature(rs.Get());     // RootSignatureの設定
		commandList->SetPipelineState(pipelineState.Get());  // PSOの設定をする
		commandList->IASetVertexBuffers(0, 1, vb.GetView()); // VBVの設定をする
		commandList->IASetIndexBuffer(ib.GetView());         // ★IBVを設定する
		// トポロジの設定
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// 頂点数、インデックス数、インデックスの開始位置、インデックスのオフセット
		// commandList->DrawInstanced(3, 1, 0, 0);
		commandList->DrawIndexedInstanced(_countof(indices), 1, 0, 0, 0);

		// 描画処理
		gameScene->Draw();

		// 描画終了
		dxCommon->PostDraw();
	}

	// ゲームシーンの開放
	delete gameScene;

	// nullptrの代入
	gameScene = nullptr;

	// エンジンの終了処理
	KamataEngine::Finalize();

	return 0;
}

void SetupPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps) {
	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState(今回は不透明)
	D3D12_BLEND_DESC blendDesc{};
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面(反時計回り)をカリングする
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 塗りつぶしモードをソリッドにする(ワイヤーフレームならD3D12_FILE_MODE_WIREFRAME)
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// PSO(PipelineStateObject)の生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPiplineStateDesc{};
	graphicsPiplineStateDesc.pRootSignature = rs.Get();                                                    // RootSignature
	graphicsPiplineStateDesc.InputLayout = inputLayoutDesc;                                                // InputLayout
	graphicsPiplineStateDesc.VS = {vs.GetDxcBlob()->GetBufferPointer(), vs.GetDxcBlob()->GetBufferSize()}; // VertexShader
	graphicsPiplineStateDesc.PS = {ps.GetDxcBlob()->GetBufferPointer(), ps.GetDxcBlob()->GetBufferSize()}; // PixelShader
	graphicsPiplineStateDesc.BlendState = blendDesc;                                                       // BlendState
	graphicsPiplineStateDesc.RasterizerState = rasterizerDesc;                                             // RasterizerState

	// 書き込むRTVの情報
	graphicsPiplineStateDesc.NumRenderTargets = 1; // 1つのRTVに書き込む　※２つ同時にしようと思えば出来る
	graphicsPiplineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ(形状)のタイプ。三角形
	graphicsPiplineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（今気にしなくていい）
	graphicsPiplineStateDesc.SampleDesc.Count = 1;
	graphicsPiplineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 準備は整った。PSOを生成する
	pipelineState.Create(graphicsPiplineStateDesc);
}
