#include"Test.hlsli"

Texture2D<float32_t4> gTexture : register(t0);//SRV reigster=>t
SamplerState gSampler : register(s0);//Sampler register=>s

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    float32_t4 textureColor = gTexture.Sample(gSampler, uv);
    
    //位置セット（ｘｙｚｗ）かカラーセット（rgba）でアクセス出来る
    output.color = textureColor; //non effect  float32_t4(uv.x, uv.y, 0.0f, 1.0f)
    return output;
}