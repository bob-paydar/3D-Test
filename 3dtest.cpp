// Main.cpp — 3D Test v6.4.2
// Borderless fullscreen via 'F', ESC exits, F1 help.
// In‑scene white FPS billboard.
// Skybox + starfield background. Glassy colorful cube (50% alpha).
#include <windows.h>
#include <wrl.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <gdiplus.h>
#include <string>
#include <chrono>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "Gdiplus.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace Gdiplus;

static const wchar_t* kBaseTitle = L"3D Test v6.4 — Programmer: Bob Paydar - F1: Help";
constexpr UINT kFpsTexW = 512u;
constexpr UINT kFpsTexH = 256u;

HINSTANCE g_hInst{};
HWND      g_hWnd{};

// D3D
ComPtr<IDXGISwapChain>      g_swapChain;
ComPtr<ID3D11Device>        g_device;
ComPtr<ID3D11DeviceContext> g_ctx;

ComPtr<ID3D11RenderTargetView> g_rtv;
ComPtr<ID3D11Texture2D>        g_dsTex;
ComPtr<ID3D11DepthStencilView> g_dsv;

struct RtTex { ComPtr<ID3D11Texture2D> tex; ComPtr<ID3D11RenderTargetView> rtv; ComPtr<ID3D11ShaderResourceView> srv; };
RtTex g_rtBG;

// Shaders and states
ComPtr<ID3D11VertexShader>  g_vs3D;
ComPtr<ID3D11PixelShader>   g_psGlass;
ComPtr<ID3D11VertexShader>  g_vsFSQ;
ComPtr<ID3D11PixelShader>   g_psStars;
ComPtr<ID3D11PixelShader>   g_psCopy;
ComPtr<ID3D11InputLayout>   g_inputLayout3D;
ComPtr<ID3D11InputLayout>   g_inputLayoutBill;
ComPtr<ID3D11VertexShader>  g_vsSky;
ComPtr<ID3D11PixelShader>   g_psSky;
ComPtr<ID3D11Buffer>        g_cbSky;

ComPtr<ID3D11VertexShader>  g_vsBill;
ComPtr<ID3D11PixelShader>   g_psBill;
ComPtr<ID3D11Buffer>        g_vbBill, g_ibBill, g_cbBill;
ComPtr<ID3D11ShaderResourceView> g_srvFps;
ComPtr<ID3D11Texture2D>     g_texFps;

ComPtr<ID3D11Buffer>        g_vbCube, g_ibCube, g_cb3D, g_cbFSQ;
ComPtr<ID3D11SamplerState>  g_sampler;
ComPtr<ID3D11BlendState>    g_alphaBlend, g_addBlend;
ComPtr<ID3D11RasterizerState> g_rsNoCull;
ComPtr<ID3D11ShaderResourceView> g_srvText[6];

UINT g_width = 1280, g_height = 720;
double g_fps = 0.0; int g_frameCount = 0;

// Borderless fullscreen state
bool g_isBorderless = false;
RECT g_windowedRect{};
LONG_PTR g_styleSaved = 0, g_exStyleSaved = 0;

// ---------- Data types ----------
struct VSConstants3D {
    XMMATRIX mvp;
    XMMATRIX model;
    XMFLOAT3 lightDir; float time;
    XMFLOAT3 eyePos;  float invWidth;
    float    invHeight; XMFLOAT3 pad;
};
struct CBSky { XMMATRIX vpNoTrans; float time; float pad[3]; };
struct CBBill { XMMATRIX mvp; XMFLOAT4 tint; };
struct V3D { XMFLOAT3 pos; XMFLOAT3 normal; XMFLOAT2 uv; UINT face; };
struct VBill { XMFLOAT3 pos; XMFLOAT2 uv; };
struct CBFSQ { float time; float aspect; float pad[2]; };

// ---------- Geometry ----------
static const V3D kCubeVerts[] =
{
    {{-1,-1, 1},{0,0,1},{0,1},0}, {{-1, 1, 1},{0,0,1},{0,0},0}, {{ 1, 1, 1},{0,0,1},{1,0},0}, {{ 1,-1, 1},{0,0,1},{1,1},0},
    {{ 1,-1,-1},{0,0,-1},{0,1},1},{{ 1, 1,-1},{0,0,-1},{0,0},1},{{-1, 1,-1},{0,0,-1},{1,0},1},{{-1,-1,-1},{0,0,-1},{1,1},1},
    {{-1,-1,-1},{-1,0,0},{0,1},2},{{-1, 1,-1},{-1,0,0},{0,0},2},{{-1, 1, 1},{-1,0,0},{1,0},2},{{-1,-1, 1},{-1,0,0},{1,1},2},
    {{ 1,-1, 1},{ 1,0,0},{0,1},3},{{ 1, 1, 1},{ 1,0,0},{0,0},3},{{  1, 1,-1},{ 1,0,0},{1,0},3},{{ 1,-1,-1},{ 1,0,0},{1,1},3},
    {{-1, 1, 1},{0,1,0},{0,1},4},{{-1, 1,-1},{0,1,0},{0,0},4},{{  1, 1,-1},{0,1,0},{1,0},4},{{ 1, 1, 1},{0,1,0},{1,1},4},
    {{-1,-1,-1},{0,-1,0},{0,1},5},{{-1,-1, 1},{0,-1,0},{0,0},5},{{ 1,-1, 1},{0,-1,0},{1,0},5},{{ 1,-1,-1},{0,-1,0},{1,1},5},
};
constexpr UINT kCubeIdxCount = 36u;
static const uint16_t kCubeIdx[] =
{
    0,1,2, 0,2,3,    4,5,6, 4,6,7,
    8,9,10, 8,10,11, 12,13,14, 12,14,15,
    16,17,18, 16,18,19, 20,21,22, 20,22,23
};

static const VBill kBillVerts[] =
{
    {{-0.5f,-0.2f,0}, {0,1}},
    {{-0.5f, 0.2f,0}, {0,0}},
    {{ 0.5f, 0.2f,0}, {1,0}},
    {{ 0.5f,-0.2f,0}, {1,1}},
};
static const uint16_t kBillIdx[] = { 0,1,2, 0,2,3 };

// ---------- HLSL ----------
static const char* kHLSL_VS_3D = R"(
cbuffer CB : register(b0)
{
    float4x4 mvp;
    float4x4 model;
    float3   lightDir; float time;
    float3   eyePos;  float invWidth;
    float    invHeight; float3 pad;
}
struct VSIn  { float3 pos:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint face:BLENDINDICES; };
struct VSOut { float4 pos:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint face:BLENDINDICES; float3 wpos:TEXCOORD1; };
VSOut main(VSIn v)
{
    VSOut o;
    float4 wp = mul(float4(v.pos,1), model);
    o.pos = mul(wp, mvp);
    o.n   = mul(float4(v.n,0), model).xyz;
    o.uv  = v.uv;
    o.face= v.face;
    o.wpos= wp.xyz;
    return o;
}
)";
static const char* kHLSL_PS_GLASS = R"(
Texture2D BG    : register(t0);
Texture2D Tex   : register(t1);
SamplerState Samp : register(s0);
cbuffer CB : register(b0)
{
    float4x4 mvp;
    float4x4 model;
    float3   lightDir; float time;
    float3   eyePos;  float invWidth;
    float    invHeight; float3 _pad;
}
struct PSIn { float4 pos:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint face:BLENDINDICES; float3 wpos:TEXCOORD1; };
float3 faceTint(uint face, float2 uv)
{
    float h = (face / 6.0) + (uv.x - uv.y) * 0.1 + 0.55;
    h = frac(h);
    float3 k = float3(1.0, 2.0/3.0, 1.0/3.0);
    float3 p = abs(frac(float3(h,h,h) + k) * 6.0 - 3.0);
    return saturate(float3(1.0,1.0,1.0) - p) * 0.9 + 0.1;
}
float4 main(PSIn i) : SV_TARGET
{
    float3 N = normalize(i.n);
    float3 V = normalize(eyePos - i.wpos);
    float  NdotV = saturate(dot(N, V));
    float  fres = pow(1.0 - NdotV, 5.0);
    float  reflectivity = lerp(0.20, 0.70, fres);

    float2 suv = i.pos.xy * float2(invWidth, invHeight) * 0.5 + 0.5;
    float2 refrOff = N.xy * 0.06;
    float2 reflOff = -N.xy * 0.04;
    float3 refrCol = BG.Sample(Samp, suv + refrOff).rgb;
    float3 reflCol = BG.Sample(Samp, suv + reflOff).rgb;

    float3 tint = faceTint(i.face, i.uv);
    refrCol *= tint;
    reflCol = lerp(reflCol, refrCol, 0.25);

    float3 L = normalize(-lightDir);
    float  NdotL = max(dot(N, L), 0.0);
    float  shade = (0.35 + 0.65 * NdotL);

    float3 baseCol = lerp(refrCol, reflCol, reflectivity) * shade;

    float a0 = Tex.Sample(Samp, i.uv).a;
    float2 px = float2(invWidth, invHeight) * 1.5;
    float glow = (
        Tex.Sample(Samp, i.uv + float2( 0, -px.y)).a +
        Tex.Sample(Samp, i.uv + float2( 0,  px.y)).a +
        Tex.Sample(Samp, i.uv + float2( px.x, 0)).a +
        Tex.Sample(Samp, i.uv + float2(-px.x, 0)).a +
        Tex.Sample(Samp, i.uv + float2( px.x, -px.y)).a +
        Tex.Sample(Samp, i.uv + float2(-px.x, -px.y)).a +
        Tex.Sample(Samp, i.uv + float2( px.x,  px.y)).a +
        Tex.Sample(Samp, i.uv + float2(-px.x,  px.y)).a
    ) * 0.12;
    float3 glowColor = float3(1.0, 1.0, 1.0); // white text/glow
    float3 textEmission = glowColor * (glow * 1.7 + a0 * 2.2);

    float3 color = saturate(baseCol + textEmission);
    return float4(color, 0.5);
}
)";
static const char* kHLSL_VS_FSQ = R"(
cbuffer CBFSQ : register(b0) { float time; float aspect; float2 _pad; }
struct VSOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
VSOut main(uint vid:SV_VertexID)
{
    float2 p = float2((vid==2)? 3.0 : -1.0, (vid==1)? 3.0 : -1.0);
    VSOut o; o.pos = float4(p, 0, 1); o.uv = p*0.5 + 0.5; return o;
}
)";
static const char* kHLSL_PS_STARS = R"(
cbuffer CBFSQ : register(b0) { float time; float aspect; float2 _pad; }
float hash21(float2 p){ p=frac(p*float2(123.34,456.21)); p+=dot(p,p+34.345); return frac(p.x*p.y); }
float3 starLayer(float2 uv, float t, float density, float speed)
{
    float2 p = uv; p.x *= aspect; p += float2(t*speed, t*speed*0.3);
    float3 col = 0;
    float2 g = floor(p*density);
    float2 f = frac(p*density);
    [unroll] for(int j=-1;j<=1;++j)
    [unroll] for(int i=-1;i<=1;++i)
    {
        float2 o = float2(i,j);
        float n = hash21(g+o);
        float2 star = frac(sin(n*float2(12.7,78.3))*43758.5453);
        float2 d = (o+star - f);
        float dist = dot(d,d);
        float tw = 0.5 + 0.5*sin(t*6.283*(0.2+hash21(g+o)));
        float s = exp(-dist*80.0)*tw;
        col += s.xxx;
    }
    return col * float3(0.85, 0.9, 1.0);
}
float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET
{
    float t = time*0.05;
    float3 a = starLayer(uv, t, 24.0, 0.03);
    float3 b = starLayer(uv+0.13, -t*1.4, 36.0, 0.06);
    float3 c = starLayer(uv*1.7+0.07, t*0.6, 18.0, 0.02);
    float3 col = saturate(a*0.9 + b*0.7 + c*0.5);
    return float4(col,1);
}
)";
static const char* kHLSL_PS_COPY = R"(
Texture2D Src : register(t0);
SamplerState Samp : register(s0);
float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET
{ return Src.Sample(Samp, uv); }
)";
static const char* kHLSL_VS_SKY = R"(
cbuffer CBSky : register(b0)
{
    float4x4 vpNoTrans;
    float time;
};
struct VSIn { float3 pos:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint face:BLENDINDICES; };
struct VSOut{ float4 pos:SV_POSITION; float3 dir:TEXCOORD0; };
VSOut main(VSIn v)
{
    VSOut o;
    float3 p = v.pos * 50.0;
    o.pos = mul(float4(p,1), vpNoTrans);
    o.dir = normalize(v.pos);
    return o;
}
)";
static const char* kHLSL_PS_SKY = R"(
cbuffer CBSky : register(b0)
{
    float4x4 vpNoTrans;
    float time;
};
float hash31(float3 p){ return frac(sin(dot(p,float3(12.9898,78.233,37.719)))*43758.5453); }
float noise(float3 p)
{
    float3 i=floor(p); float3 f=frac(p);
    float n=0.0;
    [unroll] for(int z=0;z<=1;++z)
    [unroll] for(int y=0;y<=1;++y)
    [unroll] for(int x=0;x<=1;++x)
    {
        float3 o=float3(x,y,z);
        float3 r=f-o;
        float w=(1-abs(r.x))*(1-abs(r.y))*(1-abs(r.z));
        n += hash31(i+o)*w;
    }
    return n;
}
float fbm(float3 p)
{
    float a=0.0, amp=0.5;
    for(int i=0;i<5;++i){ a+=noise(p)*amp; p*=2.02; amp*=0.55; }
    return a;
}
float3 palette(float t)
{
    float3 a=float3(0.08,0.05,0.10);
    float3 b=float3(0.2,0.3,0.6);
    float3 c=float3(0.6,0.2,0.8);
    float3 d=float3(0.9,0.9,0.6);
    return a + b*t + c*t*t + d*t*(1-t);
}
struct PSIn{ float4 pos:SV_POSITION; float3 dir:TEXCOORD0; };
float4 main(PSIn i) : SV_TARGET
{
    float3 d = normalize(i.dir);
    float t = time*0.03;
    float n = fbm(d*3.0 + t);
    float m = fbm(d*6.0 - t*1.3);
    float v = saturate(n*0.6 + m*0.4);
    float3 col = palette(v) * 0.8;
    return float4(col, 0.8);
}
)";
static const char* kHLSL_VS_BILL = R"(
cbuffer CBBill : register(b0)
{
    float4x4 mvp;
    float4   tint;
};
struct VSIn  { float3 pos:POSITION; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
VSOut main(VSIn v)
{
    VSOut o; o.pos = mul(float4(v.pos,1), mvp); o.uv = v.uv; return o;
}
)";
static const char* kHLSL_PS_BILL = R"(
Texture2D Tex : register(t0);
SamplerState Samp: register(s0);
cbuffer CBBill : register(b0)
{
    float4x4 mvp;
    float4   tint;
};
float4 main(float4 pos:SV_POSITION, float2 uv:TEXCOORD0) : SV_TARGET
{
    float4 c = Tex.Sample(Samp, uv);
    c.rgb = saturate(c.rgb * tint.rgb + tint.rgb * c.a * 0.2);
    return float4(c.rgb, c.a);
}
)";

// ---------- Helpers ----------
static void CreateRtTex(UINT w, UINT h, RtTex& out)
{
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g_device->CreateTexture2D(&td, nullptr, out.tex.GetAddressOf()))) throw std::runtime_error("Create RT tex failed");
    if (FAILED(g_device->CreateRenderTargetView(out.tex.Get(), nullptr, out.rtv.GetAddressOf()))) throw std::runtime_error("Create RT RTV failed");
    if (FAILED(g_device->CreateShaderResourceView(out.tex.Get(), nullptr, out.srv.GetAddressOf()))) throw std::runtime_error("Create RT SRV failed");
}
static void CreateRenderTargets(UINT width, UINT height)
{
    g_rtv.Reset(); g_dsv.Reset(); g_dsTex.Reset();
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)back.GetAddressOf()))) throw std::runtime_error("GetBuffer failed");
    if (FAILED(g_device->CreateRenderTargetView(back.Get(), nullptr, g_rtv.GetAddressOf()))) throw std::runtime_error("Create RTV failed");

    D3D11_TEXTURE2D_DESC dsd{};
    dsd.Width = width; dsd.Height = height; dsd.MipLevels = 1; dsd.ArraySize = 1;
    dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; dsd.SampleDesc.Count = 1;
    dsd.Usage = D3D11_USAGE_DEFAULT; dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(g_device->CreateTexture2D(&dsd, nullptr, g_dsTex.GetAddressOf()))) throw std::runtime_error("Create depth tex failed");
    if (FAILED(g_device->CreateDepthStencilView(g_dsTex.Get(), nullptr, g_dsv.GetAddressOf()))) throw std::runtime_error("Create DSV failed");

    CreateRtTex(width, height, g_rtBG);

    g_ctx->OMSetRenderTargets(1, g_rtv.GetAddressOf(), g_dsv.Get());
    D3D11_VIEWPORT vp{}; vp.Width = (float)width; vp.Height = (float)height; vp.MaxDepth = 1; g_ctx->RSSetViewports(1, &vp);
}
static void Resize(UINT w, UINT h)
{
    if (!g_swapChain) return;
    g_width = w; g_height = h; if (!w || !h) return;
    g_ctx->OMSetRenderTargets(0, nullptr, nullptr);
    g_rtv.Reset(); g_dsv.Reset(); g_dsTex.Reset();
    DXGI_SWAP_CHAIN_DESC d{}; g_swapChain->GetDesc(&d);
    if (FAILED(g_swapChain->ResizeBuffers(d.BufferCount, w, h, d.BufferDesc.Format, d.Flags))) return;
    CreateRenderTargets(w, h);
}

// FPS texture (GDI+), **white**
static void CreateOrResizeFpsTexture(UINT w, UINT h)
{
    if (g_texFps)
    {
        D3D11_TEXTURE2D_DESC td{}; g_texFps->GetDesc(&td);
        if (td.Width == w && td.Height == h) return;
        g_texFps.Reset(); g_srvFps.Reset();
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DYNAMIC; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_device->CreateTexture2D(&td, nullptr, g_texFps.GetAddressOf()))) throw std::runtime_error("Create FPS tex failed");
    if (FAILED(g_device->CreateShaderResourceView(g_texFps.Get(), nullptr, g_srvFps.GetAddressOf()))) throw std::runtime_error("Create FPS SRV failed");
}
static void UpdateFpsTexture(double fps)
{
    CreateOrResizeFpsTexture(kFpsTexW, kFpsTexH);
    Bitmap bmp(kFpsTexW, kFpsTexH, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.Clear(Color(0, 0, 0, 0));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    wchar_t buf[64];
    swprintf_s(buf, L"FPS: %.1f", fps);

    // White glow layers
    for (int r = 7; r >= 2; --r)
    {
        SolidBrush b(Color(20 + r * 10, 255, 255, 255));
        FontFamily ff(L"Segoe UI");
        Font font(&ff, 80.0f + r * 1.0f, FontStyleBold, UnitPixel);
        RectF layout(0, 0, (REAL)kFpsTexW, (REAL)kFpsTexH);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(buf, -1, &font, layout, &sf, &b);
    }
    // White core

    {
        SolidBrush bCore(Color(255, 255, 255, 255));
        FontFamily ff(L"Segoe UI");
        Font font(&ff, 84.0f, FontStyleBold, UnitPixel);
        RectF layout(0, 0, (REAL)kFpsTexW, (REAL)kFpsTexH);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(buf, -1, &font, layout, &sf, &bCore);
    }

    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(g_ctx->Map(g_texFps.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
    {
        BitmapData bd{}; Rect lockR(0, 0, kFpsTexW, kFpsTexH);
        bmp.LockBits(&lockR, ImageLockModeRead, PixelFormat32bppARGB, &bd);
        BYTE* dst = static_cast<BYTE*>(m.pData);
        BYTE* src = static_cast<BYTE*>(bd.Scan0);
        for (UINT y = 0; y < kFpsTexH; ++y)
        {
            std::memcpy(dst + static_cast<size_t>(y) * static_cast<size_t>(m.RowPitch),
                src + static_cast<size_t>(y) * static_cast<size_t>(bd.Stride),
                static_cast<size_t>(kFpsTexW) * 4u);
        }
        bmp.UnlockBits(&bd);
        g_ctx->Unmap(g_texFps.Get(), 0);
    }
}

static ComPtr<ID3D11ShaderResourceView> CreateTextSRV(UINT sizePx, const wchar_t* text, COLORREF textColor)
{
    Bitmap bmp(sizePx, sizePx, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.Clear(Color(0, 0, 0, 0));
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    // Outer glow
    SolidBrush glowBrush(Color(64, GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)));
    for (int r = 8; r >= 2; --r)
    {
        FontFamily ff(L"Segoe UI");
        REAL sz = (REAL)(sizePx * (0.18f + (8 - r) * 0.002f));
        Font font(&ff, sz, FontStyleBold, UnitPixel);
        RectF layout(0, 0, (REAL)sizePx, (REAL)sizePx);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(text, -1, &font, layout, &sf, &glowBrush);
    }
    // Core text
    SolidBrush coreBrush(Color(220, GetRValue(textColor), GetGValue(textColor), GetBValue(textColor)));
    {
        FontFamily ff(L"Segoe UI");
        Font font(&ff, (REAL)(sizePx * 0.18f), FontStyleBold, UnitPixel);
        RectF layout(0, 0, (REAL)sizePx, (REAL)sizePx);
        StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(text, -1, &font, layout, &sf, &coreBrush);
    }

    // Upload to immutable texture
    BitmapData bd{};
    Rect lockR(0, 0, sizePx, sizePx);
    bmp.LockBits(&lockR, ImageLockModeRead, PixelFormat32bppARGB, &bd);
    D3D11_TEXTURE2D_DESC td{};
    td.Width = sizePx; td.Height = sizePx; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA srd{}; srd.pSysMem = bd.Scan0; srd.SysMemPitch = (UINT)bd.Stride;

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = g_device->CreateTexture2D(&td, &srd, tex.GetAddressOf());
    bmp.UnlockBits(&bd);
    if (FAILED(hr)) throw std::runtime_error("Create text texture failed");

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = g_device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Create text SRV failed");

    return srv;
}

// ---------- Rendering helpers ----------
static void DrawStarsToRT(ID3D11RenderTargetView* rtv, float timeSec)
{
    g_ctx->IASetInputLayout(g_inputLayoutBill.Get());
    g_ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    g_ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_ctx->VSSetShader(g_vsFSQ.Get(), nullptr, 0);
    g_ctx->PSSetShader(g_psStars.Get(), nullptr, 0);

    CBFSQ cb{ timeSec, (g_height ? (float)g_width / g_height : 1.0f), {0,0} };
    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(g_ctx->Map(g_cbFSQ.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
    {
        std::memcpy(m.pData, &cb, sizeof(cb));
        g_ctx->Unmap(g_cbFSQ.Get(), 0);
    }

    ID3D11Buffer* cbs[] = { g_cbFSQ.Get() };
    g_ctx->VSSetConstantBuffers(0, 1, cbs);
    g_ctx->PSSetConstantBuffers(0, 1, cbs);

    g_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    g_ctx->Draw(3, 0);
}

static void DrawSkyboxToRT(ID3D11RenderTargetView* rtv, const XMMATRIX& vpNoTrans, float timeSec)
{
    float blendF[4] = { 0,0,0,0 };
    g_ctx->OMSetBlendState(g_addBlend.Get(), blendF, 0xFFFFFFFF);

    CBSky cb{}; cb.vpNoTrans = XMMatrixTranspose(vpNoTrans); cb.time = timeSec;
    g_ctx->UpdateSubresource(g_cbSky.Get(), 0, nullptr, &cb, 0, 0);

    g_ctx->IASetInputLayout(g_inputLayout3D.Get());
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(V3D), offset = 0;
    g_ctx->IASetVertexBuffers(0, 1, g_vbCube.GetAddressOf(), &stride, &offset);
    g_ctx->IASetIndexBuffer(g_ibCube.Get(), DXGI_FORMAT_R16_UINT, 0);

    g_ctx->VSSetShader(g_vsSky.Get(), nullptr, 0);
    g_ctx->PSSetShader(g_psSky.Get(), nullptr, 0);

    ID3D11Buffer* cbs[] = { g_cbSky.Get() };
    g_ctx->VSSetConstantBuffers(0, 1, cbs);
    g_ctx->PSSetConstantBuffers(0, 1, cbs);

    g_ctx->OMSetRenderTargets(1, &rtv, nullptr);
    g_ctx->DrawIndexed(kCubeIdxCount, 0, 0);

    g_ctx->OMSetBlendState(nullptr, blendF, 0xFFFFFFFF);
}

static void BlitSRVToBackbuffer(ID3D11ShaderResourceView* srv)
{
    g_ctx->IASetInputLayout(g_inputLayoutBill.Get());
    g_ctx->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    g_ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_ctx->VSSetShader(g_vsFSQ.Get(), nullptr, 0);
    g_ctx->PSSetShader(g_psCopy.Get(), nullptr, 0);
    g_ctx->PSSetShaderResources(0, 1, &srv);
    g_ctx->PSSetSamplers(0, 1, g_sampler.GetAddressOf());

    g_ctx->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);
    g_ctx->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    g_ctx->PSSetShaderResources(0, 1, &nullSRV);
}

static void DrawBillboardFPS(const XMMATRIX& V, const XMMATRIX& P)
{
    XMMATRIX T = XMMatrixTranslation(0.0f, 1.4f, 0.0f);
    XMMATRIX R = XMMatrixTranspose(V); R.r[3] = XMVectorSet(0, 0, 0, 1);
    XMMATRIX S = XMMatrixScaling(1.8f, 0.7f, 1.0f);
    XMMATRIX M = S * R * T;
    XMMATRIX MVP = M * V * P;

    CBBill cb{};
    cb.mvp = XMMatrixTranspose(MVP);
    cb.tint = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // white billboard modulation
    g_ctx->UpdateSubresource(g_cbBill.Get(), 0, nullptr, &cb, 0, 0);

    g_ctx->IASetInputLayout(g_inputLayoutBill.Get());
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(VBill), offset = 0;
    g_ctx->IASetVertexBuffers(0, 1, g_vbBill.GetAddressOf(), &stride, &offset);
    g_ctx->IASetIndexBuffer(g_ibBill.Get(), DXGI_FORMAT_R16_UINT, 0);

    g_ctx->VSSetShader(g_vsBill.Get(), nullptr, 0);
    g_ctx->PSSetShader(g_psBill.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { g_cbBill.Get() };
    g_ctx->VSSetConstantBuffers(0, 1, cbs);
    g_ctx->PSSetConstantBuffers(0, 1, cbs);
    g_ctx->PSSetSamplers(0, 1, g_sampler.GetAddressOf());
    ID3D11ShaderResourceView* srv = g_srvFps.Get();
    g_ctx->PSSetShaderResources(0, 1, &srv);

    float blendF[4] = { 0,0,0,0 };
    g_ctx->OMSetBlendState(g_alphaBlend.Get(), blendF, 0xFFFFFFFF);
    g_ctx->DrawIndexed(6, 0, 0);
    g_ctx->OMSetBlendState(nullptr, blendF, 0xFFFFFFFF);
    ID3D11ShaderResourceView* nullSRV = nullptr;
    g_ctx->PSSetShaderResources(0, 1, &nullSRV);
}

// ---------- Render ----------
static void RenderFrame(float dt, float timeSec)
{
    float aspect = (g_height ? (float)g_width / (float)g_height : 1.0f);
    XMVECTOR eye = XMVectorSet(0, 0.8f, -4.2f, 1);
    XMVECTOR at = XMVectorSet(0, 0.0f, 0.0f, 1);
    XMVECTOR up = XMVectorSet(0, 1.0f, 0.0f, 0);
    XMMATRIX V = XMMatrixLookAtLH(eye, at, up);
    XMMATRIX P = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.01f, 100.0f);
    XMMATRIX Vrot = V; Vrot.r[3] = XMVectorSet(0, 0, 0, 1);
    XMMATRIX VPnoT = Vrot * P;

    const float clear[4] = { 0,0,0,1 };
    g_ctx->ClearRenderTargetView(g_rtBG.rtv.Get(), clear);
    DrawStarsToRT(g_rtBG.rtv.Get(), timeSec);
    DrawSkyboxToRT(g_rtBG.rtv.Get(), VPnoT, timeSec);

    g_ctx->ClearRenderTargetView(g_rtv.Get(), clear);
    g_ctx->ClearDepthStencilView(g_dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    BlitSRVToBackbuffer(g_rtBG.srv.Get());

    static float angle = 0.0f; angle += dt * XM_PIDIV4;
    XMMATRIX M = XMMatrixRotationY(angle) * XMMatrixRotationX(angle * 0.6f);
    XMMATRIX MVP = M * V * P;
    XMFLOAT3 lightDir = XMFLOAT3(std::cos(timeSec * 0.8f), -0.6f, std::sin(timeSec * 0.8f));

    VSConstants3D cb{};
    cb.mvp = XMMatrixTranspose(MVP);
    cb.model = XMMatrixTranspose(M);
    cb.lightDir = lightDir;
    cb.time = timeSec;
    XMStoreFloat3(&cb.eyePos, eye);
    cb.invWidth = 1.0f / (float)g_width;
    cb.invHeight = 1.0f / (float)g_height;
    g_ctx->UpdateSubresource(g_cb3D.Get(), 0, nullptr, &cb, 0, 0);

    g_ctx->OMSetRenderTargets(1, g_rtv.GetAddressOf(), g_dsv.Get());
    g_ctx->IASetInputLayout(g_inputLayout3D.Get());
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(V3D), offset = 0;
    g_ctx->IASetVertexBuffers(0, 1, g_vbCube.GetAddressOf(), &stride, &offset);
    g_ctx->IASetIndexBuffer(g_ibCube.Get(), DXGI_FORMAT_R16_UINT, 0);

    ID3D11Buffer* cbs[] = { g_cb3D.Get() };
    g_ctx->VSSetShader(g_vs3D.Get(), nullptr, 0);
    g_ctx->PSSetShader(g_psGlass.Get(), nullptr, 0);
    g_ctx->VSSetConstantBuffers(0, 1, cbs);
    g_ctx->PSSetConstantBuffers(0, 1, cbs);
    g_ctx->PSSetSamplers(0, 1, g_sampler.GetAddressOf());

    float blendF[4] = { 0,0,0,0 };
    g_ctx->OMSetBlendState(g_alphaBlend.Get(), blendF, 0xFFFFFFFF);
    for (int face = 0, idxBase = 0; face < 6; ++face, idxBase += 6)
    {
        ID3D11ShaderResourceView* srvs[2] = { g_rtBG.srv.Get(), g_srvText[face].Get() };
        g_ctx->PSSetShaderResources(0, 2, srvs);
        g_ctx->DrawIndexed(6, idxBase, 0);
    }
    g_ctx->OMSetBlendState(nullptr, blendF, 0xFFFFFFFF);
    ID3D11ShaderResourceView* null2[2] = { nullptr, nullptr };
    g_ctx->PSSetShaderResources(0, 2, null2);

    DrawBillboardFPS(V, P);

    g_swapChain->Present(1, 0);
}

// ---------- Borderless Fullscreen Helpers ----------
static void EnterBorderless()
{
    if (g_isBorderless) return;
    g_isBorderless = true;

    // Save windowed placement/styles
    GetWindowRect(g_hWnd, &g_windowedRect);
    g_styleSaved = GetWindowLongPtrW(g_hWnd, GWL_STYLE);
    g_exStyleSaved = GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE);

    // Find target monitor
    HMONITOR mon = MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{}; mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    // Switch to borderless
    SetWindowLongPtrW(g_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE, g_exStyleSaved & ~(WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE));
    SetWindowPos(g_hWnd, HWND_TOPMOST,
        mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    // Resize swapchain to match new size
    UINT w = mi.rcMonitor.right - mi.rcMonitor.left;
    UINT h = mi.rcMonitor.bottom - mi.rcMonitor.top;
    Resize(w, h);
}

static void ExitBorderless()
{
    if (!g_isBorderless) return;
    g_isBorderless = false;

    // Restore styles and size
    SetWindowLongPtrW(g_hWnd, GWL_STYLE, g_styleSaved);
    SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE, g_exStyleSaved);
    SetWindowPos(g_hWnd, HWND_NOTOPMOST,
        g_windowedRect.left, g_windowedRect.top,
        g_windowedRect.right - g_windowedRect.left,
        g_windowedRect.bottom - g_windowedRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    Resize(g_windowedRect.right - g_windowedRect.left, g_windowedRect.bottom - g_windowedRect.top);
}

static void ToggleBorderless()
{
    if (g_isBorderless) ExitBorderless();
    else EnterBorderless();
}

// ---------- Win32 ----------
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (g_swapChain) { Resize(LOWORD(lParam), HIWORD(lParam)); }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SYSKEYDOWN:
        // Ignore Alt+Enter; we control fullscreen via 'F' (borderless)
        return 0;
    case WM_KEYDOWN:
        if (wParam == 'F' || wParam == 'f') { ToggleBorderless(); return 0; }
        if (wParam == VK_ESCAPE) { if (g_isBorderless) { ExitBorderless(); return 0; } }
        if (wParam == VK_F1)
        {
            MessageBoxW(hWnd,
                L"Display Modes Help:\r\n\r\n"
                L"• F: Toggle borderless fullscreen on/off.\r\n"
                L"• ESC: Exit borderless fullscreen and return to windowed mode.\r\n\r\n"
                L"Tip: Windowed mode can be resized to test performance.",
                L"3D Test — Help",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void InitD3D(HWND hWnd, UINT w, UINT h)
{
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = w; sd.BufferDesc.Height = h; sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL fl{};
    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION, &sd, g_swapChain.GetAddressOf(),
        g_device.GetAddressOf(), &fl, g_ctx.GetAddressOf())))
        throw std::runtime_error("D3D11CreateDeviceAndSwapChain failed");

    CreateRenderTargets(w, h);

    // Compile shaders and create resources
    ComPtr<ID3DBlob> vs3D, psGlass, vsFSQ, psStars, psCopy, vsSky, psSky, vsBill, psBill, err;
    auto CC = [&](LPCSTR src, LPCSTR entry, LPCSTR prof, ComPtr<ID3DBlob>& out) {
        err.Reset();
        HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, prof, 0, 0, out.GetAddressOf(), err.GetAddressOf());
        if (FAILED(hr)) { if (err) MessageBoxA(hWnd, (const char*)err->GetBufferPointer(), "HLSL Error", MB_ICONERROR); throw std::runtime_error("Shader compile failed"); }
        };
    CC(kHLSL_VS_3D, "main", "vs_5_0", vs3D);
    CC(kHLSL_PS_GLASS, "main", "ps_5_0", psGlass);
    CC(kHLSL_VS_FSQ, "main", "vs_5_0", vsFSQ);
    CC(kHLSL_PS_STARS, "main", "ps_5_0", psStars);
    CC(kHLSL_PS_COPY, "main", "ps_5_0", psCopy);
    CC(kHLSL_VS_SKY, "main", "vs_5_0", vsSky);
    CC(kHLSL_PS_SKY, "main", "ps_5_0", psSky);
    CC(kHLSL_VS_BILL, "main", "vs_5_0", vsBill);
    CC(kHLSL_PS_BILL, "main", "ps_5_0", psBill);

    g_device->CreateVertexShader(vs3D->GetBufferPointer(), vs3D->GetBufferSize(), nullptr, g_vs3D.GetAddressOf());
    g_device->CreatePixelShader(psGlass->GetBufferPointer(), psGlass->GetBufferSize(), nullptr, g_psGlass.GetAddressOf());
    g_device->CreateVertexShader(vsFSQ->GetBufferPointer(), vsFSQ->GetBufferSize(), nullptr, g_vsFSQ.GetAddressOf());
    g_device->CreatePixelShader(psStars->GetBufferPointer(), psStars->GetBufferSize(), nullptr, g_psStars.GetAddressOf());
    g_device->CreatePixelShader(psCopy->GetBufferPointer(), psCopy->GetBufferSize(), nullptr, g_psCopy.GetAddressOf());
    g_device->CreateVertexShader(vsSky->GetBufferPointer(), vsSky->GetBufferSize(), nullptr, g_vsSky.GetAddressOf());
    g_device->CreatePixelShader(psSky->GetBufferPointer(), psSky->GetBufferSize(), nullptr, g_psSky.GetAddressOf());
    g_device->CreateVertexShader(vsBill->GetBufferPointer(), vsBill->GetBufferSize(), nullptr, g_vsBill.GetAddressOf());
    g_device->CreatePixelShader(psBill->GetBufferPointer(), psBill->GetBufferSize(), nullptr, g_psBill.GetAddressOf());

    constexpr UINT kInputElems = 4u;
    D3D11_INPUT_ELEMENT_DESC ild3D[kInputElems] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(V3D,pos),   D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",  0,DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(V3D,normal),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,    0, (UINT)offsetof(V3D,uv),    D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BLENDINDICES",0,DXGI_FORMAT_R32_UINT,    0, (UINT)offsetof(V3D,face),  D3D11_INPUT_PER_VERTEX_DATA,0}
    };
    if (FAILED(g_device->CreateInputLayout(ild3D, kInputElems, vs3D->GetBufferPointer(), vs3D->GetBufferSize(), g_inputLayout3D.GetAddressOf())))
        throw std::runtime_error("Create input layout failed");
    // Billboard input layout (POSITION float3, TEXCOORD float2)
    {
        constexpr UINT kBillElems = 2u;
        D3D11_INPUT_ELEMENT_DESC ildBill[kBillElems] = {
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(VBill,pos), D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,    0, (UINT)offsetof(VBill,uv),  D3D11_INPUT_PER_VERTEX_DATA,0}
        };
        if (FAILED(g_device->CreateInputLayout(ildBill, kBillElems, vsBill->GetBufferPointer(), vsBill->GetBufferSize(), g_inputLayoutBill.GetAddressOf())))
            throw std::runtime_error("Create billboard input layout failed");
    }


    // Cube
    D3D11_BUFFER_DESC bd{}; bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.ByteWidth = sizeof(kCubeVerts); bd.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA srd{ kCubeVerts };
    if (FAILED(g_device->CreateBuffer(&bd, &srd, g_vbCube.GetAddressOf()))) throw std::runtime_error("Create VB cube failed");
    D3D11_BUFFER_DESC id{}; id.BindFlags = D3D11_BIND_INDEX_BUFFER; id.ByteWidth = sizeof(kCubeIdx); id.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA srdI{ kCubeIdx };
    if (FAILED(g_device->CreateBuffer(&id, &srdI, g_ibCube.GetAddressOf()))) throw std::runtime_error("Create IB cube failed");

    // Billboard
    D3D11_BUFFER_DESC bdB{}; bdB.BindFlags = D3D11_BIND_VERTEX_BUFFER; bdB.ByteWidth = sizeof(kBillVerts); bdB.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA srdB{ kBillVerts };
    if (FAILED(g_device->CreateBuffer(&bdB, &srdB, g_vbBill.GetAddressOf()))) throw std::runtime_error("Create VB bill failed");
    D3D11_BUFFER_DESC idB{}; idB.BindFlags = D3D11_BIND_INDEX_BUFFER; idB.ByteWidth = sizeof(kBillIdx); idB.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA srdIB{ kBillIdx };
    if (FAILED(g_device->CreateBuffer(&idB, &srdIB, g_ibBill.GetAddressOf()))) throw std::runtime_error("Create IB bill failed");

    // CBs
    D3D11_BUFFER_DESC cbd{}; cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbd.ByteWidth = sizeof(VSConstants3D); cbd.Usage = D3D11_USAGE_DEFAULT;
    if (FAILED(g_device->CreateBuffer(&cbd, nullptr, g_cb3D.GetAddressOf()))) throw std::runtime_error("Create CB3D failed");
    D3D11_BUFFER_DESC cbdFSQ{}; cbdFSQ.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbdFSQ.ByteWidth = sizeof(CBFSQ);
    cbdFSQ.Usage = D3D11_USAGE_DYNAMIC; cbdFSQ.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_device->CreateBuffer(&cbdFSQ, nullptr, g_cbFSQ.GetAddressOf()))) throw std::runtime_error("Create CBFSQ failed");
    D3D11_BUFFER_DESC cbdSky{}; cbdSky.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbdSky.ByteWidth = sizeof(CBSky); cbdSky.Usage = D3D11_USAGE_DEFAULT;
    if (FAILED(g_device->CreateBuffer(&cbdSky, nullptr, g_cbSky.GetAddressOf()))) throw std::runtime_error("Create CBSky failed");
    D3D11_BUFFER_DESC cbdBill{}; cbdBill.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbdBill.ByteWidth = sizeof(CBBill); cbdBill.Usage = D3D11_USAGE_DEFAULT;
    if (FAILED(g_device->CreateBuffer(&cbdBill, nullptr, g_cbBill.GetAddressOf()))) throw std::runtime_error("Create CBBill failed");

    // States
    D3D11_SAMPLER_DESC sdS{}; sdS.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; sdS.AddressU = sdS.AddressV = sdS.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(g_device->CreateSamplerState(&sdS, g_sampler.GetAddressOf()))) throw std::runtime_error("Create sampler failed");
    D3D11_BLEND_DESC bdAlpha{}; bdAlpha.RenderTarget[0].BlendEnable = TRUE;
    bdAlpha.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; bdAlpha.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA; bdAlpha.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bdAlpha.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bdAlpha.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO; bdAlpha.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; bdAlpha.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(g_device->CreateBlendState(&bdAlpha, g_alphaBlend.GetAddressOf()))) throw std::runtime_error("Create blend failed");
    D3D11_BLEND_DESC bdAdd{}; bdAdd.RenderTarget[0].BlendEnable = TRUE;
    bdAdd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; bdAdd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE; bdAdd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bdAdd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; bdAdd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE; bdAdd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD; bdAdd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(g_device->CreateBlendState(&bdAdd, g_addBlend.GetAddressOf()))) throw std::runtime_error("Create additive blend failed");
    D3D11_RASTERIZER_DESC rs{}; rs.CullMode = D3D11_CULL_NONE; rs.FillMode = D3D11_FILL_SOLID;
    if (FAILED(g_device->CreateRasterizerState(&rs, g_rsNoCull.GetAddressOf()))) throw std::runtime_error("Create rasterizer failed");
    g_ctx->RSSetState(g_rsNoCull.Get());

    // Text faces: "BOB"
    GdiplusStartupInput si; ULONG_PTR token{};
    if (GdiplusStartup(&token, &si, nullptr) != Ok) throw std::runtime_error("GDI+ startup failed");
    COLORREF colors[6] = { RGB(255,120,120), RGB(255,180,90), RGB(255,255,120), RGB(120,255,140), RGB(120,200,255), RGB(200,140,255) };
    for (int f = 0; f < 6; ++f) { auto tmp = CreateTextSRV(512, L"BOB", colors[f]); g_srvText[f].Attach(tmp.Detach()); }
    UpdateFpsTexture(0.0);
    GdiplusShutdown(token);
}

static HWND CreateMainWindow(HINSTANCE hInst, UINT w, UINT h)
{
    const wchar_t* cls = L"D3D11_3DTEST_V6_4_2";
    WNDCLASSW wc{}; wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = cls;
    RegisterClassW(&wc);
    RECT r{ 0,0,(LONG)w,(LONG)h }; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    return CreateWindowW(cls, kBaseTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, hInst, nullptr);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    g_hInst = hInstance;
    g_hWnd = CreateMainWindow(hInstance, g_width, g_height);
    if (!g_hWnd) return -1;
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    try
    {
        InitD3D(g_hWnd, g_width, g_height);
    }
    catch (const std::exception& e)
    {
        wchar_t buf[512]; swprintf_s(buf, L"Initialization failed:\r\n%S", e.what());
        MessageBoxW(g_hWnd, buf, L"Error", MB_ICONERROR);
        return -1;
    }

    auto tPrev = std::chrono::steady_clock::now();
    auto tFps = tPrev; g_frameCount = 0; g_fps = 0.0;

    MSG msg{};
    for (;;)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return (int)msg.wParam;
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }

        auto tNow = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(tNow - tPrev).count();
        tPrev = tNow;
        float timeSec = std::chrono::duration<float>(tNow.time_since_epoch()).count();

        g_frameCount++;
        double elapsed = std::chrono::duration<double>(tNow - tFps).count();
        if (elapsed >= 0.25)
        {
            g_fps = g_frameCount / elapsed;
            g_frameCount = 0; tFps = tNow;
            GdiplusStartupInput si; ULONG_PTR token{};
            if (GdiplusStartup(&token, &si, nullptr) == Ok) { UpdateFpsTexture(g_fps); GdiplusShutdown(token); }
        }

        RenderFrame(dt, timeSec);
    }
}
