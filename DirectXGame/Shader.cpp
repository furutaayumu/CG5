#include "Shader.h"
#include "MiscUtility.h"
#include<cassert>
#include<d3dcompiler.h>
#include<dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
// シェーダコンパイル関数
// filePath:シェーダファイルのパス　例 L"Resources/shaders/TestVS.hlsl"
// shaderModel:シェーダモデル　　例　"vs_5.0"
void Shader::Load(const std::wstring& filePath, const std::wstring& shaderModel) {
	ID3DBlob* shaderBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	std::string mdShaderModel = MiscUtility::ConvertString(shaderModel);

	HRESULT hr = D3DCompileFromFile(
	    filePath.c_str(), // シェーダファイル名
	    nullptr,
	    D3D_COMPILE_STANDARD_FILE_INCLUDE,               // インクルード可能にする
	    "main", mdShaderModel.c_str(),                   // エントリーポイント名、シェーダモデル指定
	    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバック用設定
	    0, &shaderBlob, &errorBlob);
	// エラーが発生した場合、止める
	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
			errorBlob->Release();
		}
		assert(false);
	}
	// 生成したshaderBlobを返す
	blob_ = shaderBlob;
	// wstring=>string文字列変換
	std::string mbShaderModel = mbShaderModel; // ←考えて見ようの部分
	// Shaderのコンパイル
	 hr = D3DCompileFromFile(
	    filePath.c_str(), // シェーダーファイル名
	    nullptr,
	    D3D_COMPILE_STANDARD_FILE_INCLUDE,               // インクルード可能にする
	    "main", mbShaderModel.c_str(),                   // エントリーポイント名、シェーダモデル指定（←考えてみようの部分）
	    D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバック用設定
	    0, &shaderBlob, &errorBlob);
}

void Shader::LoadDxc(const std::wstring& filePath, const std::wstring& shaderModel) 
{
	//DXC(DirectX Shader Compiler)を初期化
	static IDxcUtils* dxcUtils = nullptr;
	static IDxcCompiler3* dxcCompiler = nullptr;
	static IDxcIncludeHandler* includeHandler = nullptr;

	HRESULT hr;

	//DXCの生成
	if (dxcUtils == nullptr)
	{
		hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
		assert(SUCCEEDED(hr));//うまくいかなかったときは起動できない
	}
	if (dxcCompiler == nullptr)
	{
		hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
		assert(SUCCEEDED(hr));//うまくいかなかったときは起動できない
	}
	if (includeHandler == nullptr)
	{
		hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
		assert(SUCCEEDED(hr));//うまくいかなかったときは起動できない
	}
	//1.hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容をDxcBufferに設定する
	DxcBuffer shaderSourceBuffer{};
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	//2.Compileする
	//compileに必要なコンパイルオプションの準備
	LPCWSTR arguments[] = {
	    filePath.c_str(), // コンパイル対象のhlslファイル名
	    L"-E",
	    L"main", // エントリーポイントの指定。基本的にmain以外にはしない
	    L"-T",
	    shaderModel.c_str(), // ShaderProfileの設定
	    L"-Zi",
	    L"-Qembed_debug", // デバック用の情報を埋め込む
	    L"-Od",           // 最適化を外しておく
	    L"-Zpr",//メモリレイアウトは行優先
	};
	//実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,//読み込んだファイル
		arguments,//コンパイルオプション
	    _countof(arguments),//コンパイルオプションの数
		includeHandler,//includeが含まれた諸々
	    IID_PPV_ARGS(&shaderResult)//コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));

	//3.警告・エラーが出てないか確認する
	IDxcBlobUtf8* shaderError = nullptr;
	IDxcBlobWide* nameBlob = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), &nameBlob);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		OutputDebugStringA(shaderError->GetStringPointer());
		assert(false);
	}

	//4.Compile結果を受け取る
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), &nameBlob);
	assert(SUCCEEDED(hr));
	// もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを取っておく
	dxcBlob_ = shaderBlob;
}

//コンパイル済みのシェーダーデータを返す　※未コンパイルの場合はnullptrとなる
ID3DBlob* Shader::GetBlob() 
{ 
	return blob_; 
}

IDxcBlob* Shader::GetDxcBlob() 
{  
	return dxcBlob_;
}

//コンストラクタ
Shader::Shader() {}

//デストラクタ
Shader::~Shader() 
{
	if (blob_ != nullptr)
	{
		blob_->Release();
		blob_ = nullptr;
	}
	if (dxcBlob_!=nullptr)
	{
		dxcBlob_->Release();
		dxcBlob_ = nullptr;
	}
}
