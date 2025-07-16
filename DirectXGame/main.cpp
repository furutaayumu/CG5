#include "GameScene.h"
#include "IndexBuffer.h"
#include "KamataEngine.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include <Windows.h>
#include "WorldTransformEx.h"
// #include<d3dcompiler.h>
// グローバル関数
using namespace KamataEngine;

// 関数プロトタイプ宣言-----------------------------
// PipelineStateObjectの生成
void SetupPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps);

// RenderTextureResourceの生成
ID3D12Resource* CreateRenderTextureResource(ID3D12Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format, const FLOAT* clearColor);

// DepthStencilTextureResourceの生成
ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

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

	// デスクリプタレンジ
	D3D12_DESCRIPTOR_RANGE srvDescRange[1]{};
	// t0レジスタを利用可能にする
	srvDescRange[0].BaseShaderRegister = 0;
	srvDescRange[0].NumDescriptors = 1;
	srvDescRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvDescRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

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
		Vector2 texcoord;
	};
	// 頂点データの準備　★00_07で追加
	VertexData vertices[] = {
	    {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // 左下
	    {{-1.0f, 1.0f, 0.0f, 1.0f},  {0.0f, 0.0f}}, // 左上
	    {{1.0f, -1.0f, 0.0f, 1.0f},  {1.0f, 1.0f}}, // 右下
	    {{1.0f, 1.0f, 0.0f, 1.0f},   {1.0f, 0.0f}}, // 右上
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

	// Resource生成、Heap生成、View生成で再利用される変数の準備-------------------
	ID3D12Device* device = dxCommon->GetDevice();

	// RenderTexture関係　★00_09追加
	// 0.RenderTextureResourceの作成
	// 画面クリア色　※分かりやすいように赤とする
	const FLOAT kRenderTargetClearColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
	ID3D12Resource* renderTextureResource = CreateRenderTextureResource(device, WinApp::kWindowWidth, WinApp::kWindowHeight, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, kRenderTargetClearColor);

	// 0.DepthStencilTextureResourceの作成-----------------
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, WinApp::kWindowWidth, WinApp::kWindowHeight);

	// 1.DSV用のDescriptorHeapの作成
	ID3D12DescriptorHeap* dsvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc{};
	dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;   // Heap Type
	dsvDescriptorHeapDesc.NumDescriptors = 1;                      // Heap Typeの個数
	dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // DSVはShaderで触らないとする
	HRESULT hr;
	hr = device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap));
	assert(SUCCEEDED(hr));

	// CPU側からみたHANDLEを取得しておく
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandleCPU = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	
	// 2.DSV用のViewの生成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;                // 基本的にResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2DTexture

	// DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView(depthStencilResource, &dsvDesc, dsvHandleCPU);

	// 1.SRV用のDescriptorHeapの作成----------------------------------------
	ID3D12DescriptorHeap* srvDescriptorHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC srvDescriptorHeapDesc = {};
	srvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;     // SRV
	srvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // PixelShaderから見える
	srvDescriptorHeapDesc.NumDescriptors = 1;

	hr = device->CreateDescriptorHeap(&srvDescriptorHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap));
	assert(SUCCEEDED(hr));

	// CPU側からみたHANDLE、GPU側からみたHANDLEを取得しておく
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	// 2.SRV(Shader Resource View)の作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;                           // RenderTargetResourceと同じにする
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // RGBA値をそのままShaderに対応させる
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;                      // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;                                            // MipLevelは１しかない

	device->CreateShaderResourceView(
	    renderTextureResource, // Viewと関連付けたいリソース
	    &srvDesc,              // SRVの詳細情報（Desc:Description、構成内容の記述）
	    srvHandleCPU           // SRV用ディスクリプターヒープのCPU Handle
	);

	// 1.RTV用のDescriptorHeapを作成する-----------------------------------
	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // RTV
	rtvDescriptorHeapDesc.NumDescriptors = 1;                    // Descriptorの個数は１

	hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	assert(SUCCEEDED(hr));

	// CPU側からみたHANDLEを取得しておく
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleCPU = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	// 2.RTV用のViewの生成
	device->CreateRenderTargetView(
	    renderTextureResource, // Viewと関連付けたいリソース
	    nullptr,               // RTVの詳細情報(Desc:Description、構成内容の記述)
	                           // ※RTVの場合nullptrするとDirectX12が自動で推測してくれる
	    rtvHandleCPU           // RTV用ディスクリプタヒープのCPU Handle
	);

	//アプリで利用する3Dモデル=================================================
	//被写体の準備
	Model* model = Model::CreateFromOBJ("terrain");

	WorldTransformEx worldTransform;//WorldTransformExのインスタンス生成
	worldTransform.Initialize();
	worldTransform.scale_ = Vector3(1.0f, 1.0f, 1.0f);

	//カメラの準備
	Camera camera;
	camera.Initialize();
	camera.translation_ = Vector3(0.0f, 1.0f, 0.0f);

	// メインループ
	while (true) {
		// エンジンの更新
		if (KamataEngine::Update()) {
			break;
		}

		//World変換行列の定数バッファへの転送
		worldTransform.rotation_.y += 0.005f;//適当な回転角度(ラジアン)
		worldTransform.UpdateMatrix();//UpdateMatrixメンバ関数の呼び出し	

		//cameraの更新と定数バッファへの転送
		camera.UpdateMatrix();

		// ゲームシーンの更新
		gameScene->Update();

		// TransitionBarrierをSRV⇒RTVに設定する
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;                       // TranslationBarrierの設定
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;                            // フラグはNoneにしておく
		barrier.Transition.pResource = renderTextureResource;                        // バリアを張る対象のリソース
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // 遷移前
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;          // 遷移後
		commandList->ResourceBarrier(1, &barrier);                                   // バリアを張る

		// 描画先のRTVとDSVを設定する
		commandList->OMSetRenderTargets(1, &rtvHandleCPU, false, &dsvHandleCPU);

		// Viewportの設定
		D3D12_VIEWPORT viewport{};
		viewport.Width = WinApp::kWindowWidth;
		viewport.Height = WinApp::kWindowHeight;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f; // 深度の最小値
		viewport.MaxDepth = 1.0f; // 深度の最大値

		commandList->RSSetViewports(1, &viewport);

		// Scissorの設定
		D3D12_RECT scissorRect{};
		// 基本的にビューポートと同じ矩形が構成されるようにする
		scissorRect.left = 0;
		scissorRect.right = WinApp::kWindowWidth;
		scissorRect.top = 0;
		scissorRect.bottom = WinApp::kWindowHeight;

		commandList->RSSetScissorRects(1, &scissorRect);

		// 全画面クリア
		commandList->ClearRenderTargetView(rtvHandleCPU, kRenderTargetClearColor, 0, nullptr);
		// 指定した深度で画面全体をクリアする
		commandList->ClearDepthStencilView(dsvHandleCPU, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		Model::PreDraw(commandList);
		model->Draw(worldTransform, camera);
		Model::PostDraw();

		// TransitionBarrierをもとに戻し、PixelShaderが扱えるようにする
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;                      // TranslationBarrierの設定
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;                           // フラグはNONEにしておく
		barrier.Transition.pResource = renderTextureResource;                       // バリアを張る対象のリソース
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;        // 還移前
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // 還移後
		commandList->ResourceBarrier(1, &barrier);

		// 描画開始
		dxCommon->PreDraw();

		// コマンドを詰む
		commandList->SetGraphicsRootSignature(rs.Get());                          // RootSignatureの設定
		commandList->SetPipelineState(pipelineState.Get());                       // PSOの設定をする
		commandList->IASetVertexBuffers(0, 1, vb.GetView());                      // VBVの設定をする
		commandList->IASetIndexBuffer(ib.GetView());                              // ★IBVを設定する
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // トポロジの設定

		// 使用するディスクリプタヒープの設定　★00_09で追加
		commandList->SetDescriptorHeaps(srvDescriptorHeap->GetDesc().NumDescriptors, &srvDescriptorHeap);

		// SRVのDescriptorTableの先頭を設定　※０はrootParameter[0]である　★00_09で追加
		commandList->SetGraphicsRootDescriptorTable(0, srvHandleGPU);

		// 頂点数、インデックス数、インデックスの開始位置、インデックスのオフセット
		// commandList->DrawInstanced(3, 1, 0, 0);
		// 画面を覆うポリゴンの描画
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

	// 解放
	delete model;

	renderTextureResource->Release();
	srvDescriptorHeap->Release();
	rtvDescriptorHeap->Release();

	depthStencilResource->Release();
	dsvDescriptorHeap->Release();

	// エンジンの終了処理
	KamataEngine::Finalize();

	return 0;
}

void SetupPipelineState(PipelineState& pipelineState, RootSignature& rs, Shader& vs, Shader& ps) {
	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

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

// RenderTextureResourceの生成
ID3D12Resource* CreateRenderTextureResource(ID3D12Device* device, uint32_t width, uint32_t height, DXGI_FORMAT format, const FLOAT* clearColor) {
	// 1.生成するRenderTextureのDescの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(width);                             // RenderTextureの幅
	resourceDesc.Height = UINT(height);                           // Textureの高さ
	resourceDesc.MipLevels = 1;                                   // mipmapの数
	resourceDesc.DepthOrArraySize = 1;                            // 奥行or配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;        // TextureのFormat
	resourceDesc.SampleDesc.Count = 1;                            // サンプリングカウント１固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // Textureの時限数。普段使っているのは２次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // RenderTargetとして使う通知
	// 2.利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 3.ClearValueの用意
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = format; // ページ６（一応資料にはclearFormatってある）
	clearValue.Color[0] = clearColor[0];
	clearValue.Color[1] = clearColor[1];
	clearValue.Color[2] = clearColor[2];
	clearValue.Color[3] = clearColor[3];

	// 4.RenderTextureResourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
	    &heapProperties,                            // Heapの設定
	    D3D12_HEAP_FLAG_NONE,                       // Heapの特殊な設定
	    &resourceDesc,                              // Resourceの設定
	    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // PixelShaderでアクセス出来るようにする
	    &clearValue,                                // Clear最適値
	    IID_PPV_ARGS(&resource)                     // 作成するResourceポインタへのポインタ
	);
	assert(SUCCEEDED(hr));
	return resource;
}

ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	// 1.生成するDepthStencilTextureのDesc
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;                  // Textureの幅
	resourceDesc.Height = height;                // Textureの高さ
	resourceDesc.MipLevels = 1;                  // mipmapの数DepthStencilなので１つでいい
	resourceDesc.DepthOrArraySize = 1;           // Textureの配列数DepthStencilは１つでいい
	resourceDesc.Format = DXGI_FORMAT_D32_FLOAT; // DepthStencilとして利用可能なフォーマット
	// ※KamataEngineと合わせる
	resourceDesc.SampleDesc.Count = 1;                            // サンプリングカウント１固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;  // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 2.利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VARM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;      // 1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT; // Zバッファ形式、resourceと合わせる　※KamataEngineと合わせた

	// 3.Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
	    &heapProperties,                  // Heapの設定
	    D3D12_HEAP_FLAG_NONE,             // Heapの特殊な設定　★後で変更？
	    &resourceDesc,                    // Resourceの設定
	    D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込み状態にしておく
	    &depthClearValue,                 // Clear最適値
	    IID_PPV_ARGS(&resource)           // 作成するResourceポインタへのポインタ
	);
	assert(SUCCEEDED(hr));

	return resource;
}
