#include"Test.hlsli"
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t2 uv = input.texcoord;
    
    //位置セット（ｘｙｚｗ）かカラーセット（rgba）でアクセス出来る
    output.color = float32_t4(uv.x, uv.y, 0.0f, 1.0f);
    return output;
}