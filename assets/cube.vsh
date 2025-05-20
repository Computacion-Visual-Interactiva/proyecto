cbuffer Constants
{
    float4x4 g_WorldViewProj;
    float g_WingAngle;
};

struct VSInput
{
    float3 Pos : ATTRIB0;
    float2 TexCoord : ATTRIB1;
    float WingFlg : ATTRIB2;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;
};


static const float PIVOT_X = 0.02f;

void main(in VSInput IN,
          out PSInput OUT)
{
    float3 p = IN.Pos;

    if (abs(IN.WingFlg) > 0.5)
    {
        float pivotX = PIVOT_X * IN.WingFlg;
        p.x -= pivotX;
        
        float angle = g_WingAngle * IN.WingFlg;

        float s = sin(angle);
        float c = cos(angle);

        float3 r;
        r.x = p.x * c - p.y * s;
        r.y = p.x * s + p.y * c;
        r.z = p.z;
        p = r;
        p.x += pivotX;
    }

    OUT.Pos = mul(float4(p, 1.0), g_WorldViewProj);
    OUT.UV = IN.TexCoord;
}
