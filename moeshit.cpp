/*so first warning, this code is really unprofessional and messy, i made this in like 1 month, a lot of video guides, a lot of time spended, and i used a little bit of copypasting, sorry*/
/*can't make assembly file work with this app so im using the direct shit*/
/*a lot of shit is pretty shitty made*/
/*from .kona only palette info to .kona with frame information and other shit*/
/*it's messy in here*/
/*made old functions work more properly*/
/*skin system error fixed: wrong json reading, wrong json writing*/
/*im too lazy to implement Saturation or any other effects that are from gdi+ version, i will add effects in patches*/
/*yep i made json shit again, i don't fucking want to use other libraries that are too fucking heavy or idk*/
/*current bug, if skins width and height was too small compared to default skin then when applied default after skin, default becomes crappy that's the bug. solution jsut change your size again, will fix in da patch maybw*/
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <dwmapi.h>
#include <new>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <map>
#include <intrin.h>
#include <mmsystem.h>
#include "menu.h""
#define MIN_FRAME_TIME_MS  16//60 FPS cap
#define SPEED_STEP_MS      20
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")
//consts
UINT g_uTileW = 640;
UINT g_uTileH = 480;
#define TILE_BPP            4
inline UINT TileSize() { return g_uTileW * g_uTileH * TILE_BPP; }
inline UINT TileIndexedSize() { return g_uTileW * g_uTileH; }
#define MAX_PALETTE_COLORS 256
#define RLE_MARKER 0xFF
extern "C" {
    int Konata_RLE_Decode_x64(const BYTE* src, int srcSize, BYTE* dst, int dstSize);
    void Konata_IndexedToBGRA_AVX2(const BYTE* src8, BYTE* dst32, int pixelCount, const DWORD* paletteLUT);
}//don't needs to be here exactly right now as i know
BOOL g_bAVX2Supported = FALSE;
void CheckAVX2Support() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    g_bAVX2Supported = (cpuInfo[1] & (1 << 5)) != 0;
}
struct SkinInfo {
    WCHAR name[64];
    WCHAR pngPath[MAX_PATH];
    WCHAR konaPath[MAX_PATH];
    int  frames;
    int  fps;
    int  tileW;
    int  tileH;
};
std::vector<SkinInfo> g_skins;
int g_currentSkin = 0;
struct Palette {
    DWORD colors[MAX_PALETTE_COLORS];
    int   count;
    void Init() { count = 0; memset(colors, 0, sizeof(colors)); }

    __forceinline BYTE FindClosest(DWORD color) const {
        BYTE bestIdx = 0;
        int bestDist = INT_MAX;
        BYTE cr = (color >> 16) & 0xFF;//RLE marker
        BYTE cg = (color >> 8) & 0xFF;//same here
        BYTE cb = color & 0xFF;//and here and everything that 0xFF
        for (int i = 0; i < count; i++) {
            BYTE pr = (colors[i] >> 16) & 0xFF;
            BYTE pg = (colors[i] >> 8) & 0xFF;
            BYTE pb = colors[i] & 0xFF;
            int dr = cr - pr, dg = cg - pg, db = cb - pb;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist) { bestDist = dist; bestIdx = i; if (dist == 0) break; }
        }
        return bestIdx;
    }
};
struct FastHSL {
    FLOAT saturation, gamma;
    BOOL invert, grayscale;
    BYTE gammaTable[256], invertTable[256], grayTable[256];

    void Init() {
        saturation = 1.0f; gamma = 1.0f; invert = FALSE; grayscale = FALSE;
        for (int i = 0; i < 256; i++) {
            gammaTable[i] = (BYTE)(255.0f * powf(i / 255.0f, 1.0f));
            invertTable[i] = 255 - i;
        }
        BuildGrayTable();
    }

    void BuildGrayTable() {
        for (int i = 0; i < 256; i++) {
            grayTable[i] = (BYTE)i;
        }
    }

    void UpdateGamma() {
        for (int i = 0; i < 256; i++)
            gammaTable[i] = (BYTE)(255.0f * powf(i / 255.0f, 1.0f / gamma));
    }
};
alignas(64) DWORD g_PaletteLUT[256];//specially for cache
//globals
ID2D1Factory* g_pD2DFactory = NULL;
ID2D1HwndRenderTarget* g_pRenderTarget = NULL;
ID2D1Bitmap* g_pTileBitmap = NULL;
//memory mapped files
BYTE* g_pHeaderData = NULL;//only header and table for other shit
BYTE* g_pCurrentTileData = NULL;
UINT g_uCurrentMappedTile = UINT_MAX;
HANDLE g_hFile = INVALID_HANDLE_VALUE;
HANDLE g_hMapping = NULL;
//boofers
alignas(64) BYTE* g_pIndexedBuffer = NULL;
alignas(64) BYTE* g_pTileBuffer = NULL;
alignas(64) BYTE* g_pDecompressBuffer = NULL;
DWORD* g_pTileOffsets = NULL;
Palette* g_pPalettes = NULL;
UINT g_uTotalTiles = 0;
UINT g_uFramesPerRow = 0;
FastHSL g_hsl;
//parametres
UINT_PTR g_animTimerId = 0;
FLOAT g_fScale = 1.0f;
UINT g_uCurrentFrame = 0;
UINT g_uTotalFrames = 0;
UINT g_uAnimSpeedMs = 100;
volatile LONG g_bRunning = 1;
BOOL g_bPlaying = TRUE;
//menu
BYTE g_menuMemory[sizeof(KonataMenu)];
KonataMenu* g_pMenu = NULL;
D2D1_BITMAP_INTERPOLATION_MODE g_eInterpMode = D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
HWND g_hwnd = NULL;

WCHAR g_szCurrentPngPath[MAX_PATH] = L"Konatathingy.png";
WCHAR g_szCurrentKonaPath[MAX_PATH] = L"konata.kona";
const UINT g_uSpacing = 20;

void NextSkin();
void PrevSkin();
void makinskin(int index);
void ScanSkinsFolder();

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))

__forceinline int RLE_DecodeFast(const BYTE* src, int srcSize, BYTE* dst, int dstSize)
{
    const BYTE* srcEnd = src + srcSize;//non asm
    BYTE* dstStart = dst;
    BYTE* dstEnd = dst + dstSize;
    while (src < srcEnd && dst < dstEnd) {
        BYTE code = *src++;
        if (code == RLE_MARKER && src < srcEnd) {
            BYTE length = *src++;
            BYTE value = *src++;
            if (dst + length > dstEnd) length = dstEnd - dst;
            memset(dst, value, length);
            dst += length;
        }
        else {
            if (dst + code > dstEnd) code = dstEnd - dst;
            if (src + code > srcEnd) code = srcEnd - src;
            memcpy(dst, src, code);
            src += code; dst += code;
        }
    }
    return dst - dstStart;
}
__forceinline void ConvertFromIndexedFast(const BYTE* src8, BYTE* dst32)
{
    DWORD* dst = (DWORD*)dst32;
    int pixelCount = g_uTileW * g_uTileH;
    for (int i = 0; i < pixelCount; i++) dst[i] = g_PaletteLUT[src8[i]];
}
int RLE_EncodeFast(const BYTE* src, int srcSize, BYTE* dst, int dstMaxSize)
{
    const BYTE* srcEnd = src + srcSize;
    BYTE* dstStart = dst;
    BYTE* dstEnd = dst + dstMaxSize;
    while (src < srcEnd && dst < dstEnd - 3) {
        BYTE runValue = *src;
        const BYTE* runStart = src;
        int runLength = 0;
        while (src < srcEnd && *src == runValue && runLength < 255) { runLength++; src++; }
        if (runLength >= 4) {
            *dst++ = RLE_MARKER; *dst++ = runLength; *dst++ = runValue;
        }
        else {
            src = runStart;
            const BYTE* literalStart = src;
            int literalLength = 0;
            while (src < srcEnd && literalLength < 127) {
                if (src + 3 < srcEnd && src[0] == src[1] && src[1] == src[2] && src[2] == src[3]) break;
                literalLength++; src++;
            }
            if (literalLength > 0) {
                *dst++ = literalLength;
                memcpy(dst, literalStart, literalLength);
                dst += literalLength;
            }
        }
    }
    return dst - dstStart;
}
int CollectUniqueColors(const BYTE* tileData, DWORD* uniqueColors, int maxColors)
{
    const DWORD* pixels = (const DWORD*)tileData;
    int pixelCount = g_uTileW * g_uTileH;
    std::map<DWORD, int> colorFreq;
    for (int i = 0; i < pixelCount; i++) {
        DWORD color = pixels[i];
        BYTE r = ((color >> 16) & 0xFF) & 0xF0;
        BYTE g = ((color >> 8) & 0xFF) & 0xF0;
        BYTE b = (color & 0xFF) & 0xF0;
        colorFreq[(color & 0xFF000000) | (r << 16) | (g << 8) | b]++;
    }
    std::vector<std::pair<DWORD, int>> sorted;
    for (auto& p : colorFreq) sorted.push_back(p);
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    int uniqueCount = MIN(maxColors, (int)sorted.size());
    for (int i = 0; i < uniqueCount; i++) uniqueColors[i] = sorted[i].first;
    return uniqueCount;
}
void ConvertToIndexed(const BYTE* src32, BYTE* dst8, Palette* palette)
{
    DWORD uniqueColors[MAX_PALETTE_COLORS];
    palette->count = CollectUniqueColors(src32, uniqueColors, MAX_PALETTE_COLORS);
    for (int i = 0; i < palette->count; i++) palette->colors[i] = uniqueColors[i];
    const DWORD* pixels = (const DWORD*)src32;
    int pixelCount = g_uTileW * g_uTileH;
    for (int i = 0; i < pixelCount; i++) dst8[i] = palette->FindClosest(pixels[i]);
}
void ApplyHSLToTile(BYTE* tileData)
{
    if (g_hsl.gamma == 1.0f && g_hsl.saturation == 1.0f && !g_hsl.invert && !g_hsl.grayscale) return;

    DWORD* pixels = (DWORD*)tileData;
    int pixelCount = g_uTileW * g_uTileH;

    for (int i = 0; i < pixelCount; i++)
    {
        DWORD p = pixels[i];
        BYTE a = p >> 24, r = p >> 16, g = p >> 8, b = p;

        if (g_hsl.grayscale) r = g = b = (r * 77 + g * 150 + b * 29) >> 8;
        if (g_hsl.gamma != 1.0f) { r = g_hsl.gammaTable[r]; g = g_hsl.gammaTable[g]; b = g_hsl.gammaTable[b]; }
        if (g_hsl.invert) { r ^= 0xFF; g ^= 0xFF; b ^= 0xFF; }  // вместо таблицы - XOR
        if (g_hsl.saturation != 1.0f) {
            int gray = (r * 77 + g * 150 + b * 29) >> 8;
            r = std::clamp(gray + (int)((r - gray) * g_hsl.saturation), 0, 255);
            g = std::clamp(gray + (int)((g - gray) * g_hsl.saturation), 0, 255);
            b = std::clamp(gray + (int)((b - gray) * g_hsl.saturation), 0, 255);
        }

        pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}
void StartAnimTimer(HWND hwnd)
{
    if (g_animTimerId) KillTimer(hwnd, g_animTimerId);
    g_animTimerId = SetTimer(hwnd, 2, g_uAnimSpeedMs, NULL);
}
void fancylogic(HWND hwnd)
{
    if (g_bPlaying && g_uTotalFrames > 0) {
        StartAnimTimer(hwnd);
    }
    MSG msg;
    while (g_bRunning)
    {
        //block getmessage
        if (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_bRunning = 0;
        }
    }
    if (g_animTimerId) {
        KillTimer(hwnd, g_animTimerId);
        g_animTimerId = 0;
    }
}
HRESULT ConvertPNGtoKona(const WCHAR* pszPng, const WCHAR* pszKona)
{
    if (GetFileAttributesW(pszPng) == INVALID_FILE_ATTRIBUTES) {
        WCHAR msg[512];
        swprintf(msg, 512, L"PNG file not found\n%s", pszPng);
        MessageBoxW(NULL, msg, L"ERROR", MB_ICONERROR);
        return E_FAIL;
    }
    //check existing .kona
    HANDLE hFile = CreateFileW(pszKona, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD magic, read;
        BYTE version;
        UINT framesPerRow, totalTiles;
        ReadFile(hFile, &magic, sizeof(DWORD), &read, NULL);
        ReadFile(hFile, &version, sizeof(BYTE), &read, NULL);
        ReadFile(hFile, &framesPerRow, sizeof(UINT), &read, NULL);
        ReadFile(hFile, &totalTiles, sizeof(UINT), &read, NULL);
        CloseHandle(hFile);
        if (magic == 0x414E4F4B && totalTiles > 0) {//0x414E4F4B=kona
            return S_OK;
        }
        DeleteFileW(pszKona);
    }//this is a messy one
    IWICImagingFactory* pFactory = NULL;
    IWICBitmapDecoder* pDecoder = NULL;
    IWICBitmapFrameDecode* pFrame = NULL;
    IWICFormatConverter* pConverter = NULL;
    FILE* fpOut = NULL;
    BYTE* pTileBuffer32 = NULL;
    BYTE* pTileBuffer8 = NULL;
    BYTE* pCompressed = NULL;
    Palette palette;
    DWORD* offsets = NULL;
    HRESULT hr = S_OK;
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) return hr;
    hr = pFactory->CreateDecoderFromFilename(pszPng, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
    if (FAILED(hr)) { pFactory->Release(); return hr; }
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) { pDecoder->Release(); pFactory->Release(); return hr; }
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) { pFrame->Release(); pDecoder->Release(); pFactory->Release(); return hr; }
    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release(); return hr; }
    UINT uPNGWidth = 0, uPNGHeight = 0;
    pConverter->GetSize(&uPNGWidth, &uPNGHeight);
    UINT uTilesPerRow = (uPNGWidth + g_uSpacing) / (g_uTileW + g_uSpacing);
    UINT uTilesPerCol = (uPNGHeight + g_uSpacing) / (g_uTileH + g_uSpacing);
    UINT uTotalTiles = uTilesPerRow * uTilesPerCol;
    fpOut = _wfopen(pszKona, L"wb");
    if (!fpOut) { pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release(); return E_FAIL; }
    DWORD magic = 0x414E4F4B;//0x414E4F4B=kona
    BYTE formatVersion = 3;
    fwrite(&magic, sizeof(DWORD), 1, fpOut);
    fwrite(&formatVersion, sizeof(BYTE), 1, fpOut);
    fwrite(&uTilesPerRow, sizeof(UINT), 1, fpOut);
    fwrite(&uTotalTiles, sizeof(UINT), 1, fpOut);
    offsets = (DWORD*)calloc(uTotalTiles, sizeof(DWORD));
    fwrite(offsets, sizeof(DWORD), uTotalTiles, fpOut);
    pTileBuffer32 = (BYTE*)malloc(TileSize());
    pTileBuffer8 = (BYTE*)malloc(TileIndexedSize());
    pCompressed = (BYTE*)malloc(TileIndexedSize() * 2);
    if (!pTileBuffer32 || !pTileBuffer8 || !pCompressed) {
        fclose(fpOut); DeleteFileW(pszKona);
        free(pCompressed); free(pTileBuffer8); free(pTileBuffer32); free(offsets);
        pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release();
        return E_OUTOFMEMORY;
    }
    UINT tileIndex = 0;
    for (UINT uTileRow = 0; uTileRow < uTilesPerCol; uTileRow++) {
        for (UINT uTileCol = 0; uTileCol < uTilesPerRow; uTileCol++) {
            UINT uSrcX = uTileCol * (g_uTileW + g_uSpacing);
            UINT uSrcY = uTileRow * (g_uTileH + g_uSpacing);
            if (uSrcX + g_uTileW > uPNGWidth || uSrcY + g_uTileH > uPNGHeight) {
                memset(pTileBuffer32, 0, TileSize());//0 filling for empty tiles 
            }
            else {
                for (UINT y = 0; y < g_uTileH; y++) {
                    WICRect rc = { (INT)uSrcX, (INT)(uSrcY + y), (INT)g_uTileW, 1 };
                    pConverter->CopyPixels(&rc, g_uTileW * TILE_BPP, g_uTileW * TILE_BPP, pTileBuffer32 + y * g_uTileW * TILE_BPP);
                }
            }
            palette.Init();
            ConvertToIndexed(pTileBuffer32, pTileBuffer8, &palette);
            offsets[tileIndex] = ftell(fpOut);
            fwrite(&palette.count, sizeof(int), 1, fpOut);
            fwrite(palette.colors, sizeof(DWORD), palette.count, fpOut);
            int compressedSize = RLE_EncodeFast(pTileBuffer8, TileIndexedSize(), pCompressed, TileIndexedSize() * 2);
            fwrite(&compressedSize, sizeof(int), 1, fpOut);
            fwrite(pCompressed, 1, compressedSize, fpOut);
            tileIndex++;
        }
    }
    fseek(fpOut, sizeof(DWORD) + sizeof(BYTE) + sizeof(UINT) * 2, SEEK_SET);
    fwrite(offsets, sizeof(DWORD), uTotalTiles, fpOut);
    free(pCompressed); free(pTileBuffer8); free(pTileBuffer32); free(offsets);
    fclose(fpOut);
    pConverter->Release(); pFrame->Release(); pDecoder->Release(); pFactory->Release();
    return S_OK;
}
HRESULT CreateTileBitmap(ID2D1RenderTarget* pRT)
{
    D2D1_SIZE_U uSize = D2D1::SizeU(g_uTileW, g_uTileH);
    D2D1_BITMAP_PROPERTIES bmpProps = {};
    bmpProps.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    return pRT->CreateBitmap(uSize, bmpProps, &g_pTileBitmap);
}
HRESULT InitD2D(HWND hwnd)
{
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &g_pD2DFactory);
    if (FAILED(hr)) return hr;
    RECT rc; GetClientRect(hwnd, &rc);
    D2D1_SIZE_U uSize = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    return g_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(hwnd, uSize), &g_pRenderTarget);
}

void UnmapCurrentTile()
{
    if (g_pCurrentTileData && g_pCurrentTileData != g_pHeaderData) {
        UnmapViewOfFile(g_pCurrentTileData);
    }
    g_pCurrentTileData = NULL;
    g_uCurrentMappedTile = UINT_MAX;
}
BYTE* MapTileData(UINT uTileIndex)//is map alive?
{
    if (!g_hMapping || !g_pHeaderData || !g_pTileOffsets) return NULL;
    if (uTileIndex >= g_uTotalTiles) return NULL;
    if (uTileIndex == g_uCurrentMappedTile && g_pCurrentTileData)
        return g_pCurrentTileData;
    //unmap old one
    if (g_pCurrentTileData && g_pCurrentTileData != g_pHeaderData) {
        UnmapViewOfFile(g_pCurrentTileData);
        g_pCurrentTileData = NULL;
    }
    g_uCurrentMappedTile = UINT_MAX;
    //mapp a new one
    DWORD offset = g_pTileOffsets[uTileIndex];
    DWORD size;
    if (uTileIndex + 1 < g_uTotalTiles) {
        size = g_pTileOffsets[uTileIndex + 1] - offset;
    }
    else {
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(g_hFile, &fileSize)) return NULL;
        size = (DWORD)(fileSize.QuadPart - offset);
    }
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    DWORD gran = si.dwAllocationGranularity;
    DWORD alignedOffset = offset - (offset % gran);
    DWORD extra = offset - alignedOffset;
    DWORD mapSize = size + extra;
    g_pCurrentTileData = (BYTE*)MapViewOfFile(g_hMapping, FILE_MAP_READ,
        0, alignedOffset, mapSize);
    if (g_pCurrentTileData) {
        g_uCurrentMappedTile = uTileIndex;
        return g_pCurrentTileData + extra;
    }
    return NULL;
}
//MMF and streaming
void BuildPaletteLUT(const Palette* palette)
{
    for (int i = 0; i < palette->count; i++)
        g_PaletteLUT[i] = palette->colors[i];
}
__forceinline HRESULT StreamTileKona(UINT uFrameIndex)
{
    if (!g_pTileBuffer || !g_pTileBitmap) {
        //MessageBoxW(g_hwnd, L"StreamTileKona: buffer or bitmap is NULL", L"ERROR", MB_ICONERROR); //you don't need this in the original shit
        return E_FAIL;
    }
    if (g_uTotalFrames == 0) {
        MessageBoxW(g_hwnd, L"StreamTileKona: total frames is 0", L"ERROR", MB_ICONERROR);
        return E_FAIL;
    }
    if (uFrameIndex >= g_uTotalFrames) uFrameIndex = 0;
    BYTE* pTileData = MapTileData(uFrameIndex);
    if (!pTileData) {
        WCHAR msg[256];
        swprintf(msg, 256, L"MapTileData failed for frame %d of %d", uFrameIndex, g_uTotalFrames);//old
        MessageBoxW(g_hwnd, msg, L"ERROR", MB_ICONERROR);
        return E_FAIL;
    }
    int paletteCount = *(int*)pTileData;
    if (paletteCount <= 0 || paletteCount > MAX_PALETTE_COLORS) {
        WCHAR msg[256];
        //swprintf(msg, 256, L"invalid palette count: %d (max %d)", paletteCount, MAX_PALETTE_COLORS);//this is pretty useless
        MessageBoxW(g_hwnd, msg, L"ERROR", MB_ICONERROR);
        return E_FAIL;
    }
    DWORD* paletteColors = (DWORD*)(pTileData + sizeof(int));
    for (int i = 0; i < paletteCount; i++)
        g_PaletteLUT[i] = paletteColors[i];
    BYTE* pCompressedStart = pTileData + sizeof(int) + paletteCount * sizeof(DWORD);
    int compressedSize = *(int*)pCompressedStart;

    int expectedSize = TileIndexedSize();
    if (compressedSize <= 0 || compressedSize > expectedSize * 2) {
        WCHAR msg[256];
        swprintf(msg, 256, L"invalid compressed size: %d (expected max %d)\npalette: %d colors",
            compressedSize, expectedSize * 2, paletteCount);
        MessageBoxW(g_hwnd, msg, L"ERROR", MB_ICONERROR);
        return E_FAIL;
    }
    int decodedBytes = RLE_DecodeFast(
        pCompressedStart + sizeof(int),
        compressedSize,
        g_pIndexedBuffer,
        expectedSize
    );

    if (decodedBytes != expectedSize) {
        WCHAR msg[256];
        swprintf(msg, 256, L"RLE decode size mismatch\ndecoded: %d bytes\nexpected: %d bytes\ncompressed: %d bytes\npalette: %d colors\nframe: %d",//old shit
            decodedBytes, expectedSize, compressedSize, paletteCount, uFrameIndex);
        MessageBoxW(g_hwnd, msg, L"WARNING", MB_ICONWARNING);
        //if decoded smaller then just make what's left to 0
        if (decodedBytes < expectedSize) {
            memset(g_pIndexedBuffer + decodedBytes, 0, expectedSize - decodedBytes);
        }
        //if bigger then it's error too but who cares?
    }
    // indexed to BGRA
    ConvertFromIndexedFast(g_pIndexedBuffer, g_pTileBuffer);
    ApplyHSLToTile(g_pTileBuffer);
    //bitmaploadin
    D2D1_RECT_U dstRect = D2D1::RectU(0, 0, g_uTileW, g_uTileH);
    HRESULT hr = g_pTileBitmap->CopyFromMemory(&dstRect, g_pTileBuffer, g_uTileW * TILE_BPP);
    if (FAILED(hr)) {
        WCHAR msg[256];
        swprintf(msg, 256, L"CopyFromMemory failed\nsize: %dx%d\npitch: %d bytes\nHRESULT: 0x%08X",//for debugging and old versions, also it's crappy
            g_uTileW, g_uTileH, g_uTileW * TILE_BPP, hr);
        MessageBoxW(g_hwnd, msg, L"ERROR", MB_ICONERROR);
        return E_FAIL;
    }
    return S_OK;
}
HRESULT InitKonataStreaming(void)//i need to add comments in here because im fucking lost in here
{
    HRESULT hr = ConvertPNGtoKona(g_szCurrentPngPath, g_szCurrentKonaPath);
    if (FAILED(hr)) return hr;
    g_hFile = CreateFileW(g_szCurrentKonaPath, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (g_hFile == INVALID_HANDLE_VALUE) return E_FAIL;
    LARGE_INTEGER fileSize;
    GetFileSizeEx(g_hFile, &fileSize);
    g_hMapping = CreateFileMappingW(g_hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!g_hMapping) { CloseHandle(g_hFile); return E_FAIL; }
    DWORD headerSize = sizeof(DWORD) + sizeof(BYTE) + sizeof(UINT) * 2;
    DWORD offsetsSize = 0;
    BYTE* pTemp = (BYTE*)MapViewOfFile(g_hMapping, FILE_MAP_READ, 0, 0, headerSize);
    if (!pTemp) { CloseHandle(g_hMapping); CloseHandle(g_hFile); return E_FAIL; }
    DWORD magic = *(DWORD*)(pTemp);
    BYTE formatVersion = *(BYTE*)(pTemp + sizeof(DWORD));
    g_uFramesPerRow = *(UINT*)(pTemp + sizeof(DWORD) + sizeof(BYTE));
    g_uTotalTiles = *(UINT*)(pTemp + sizeof(DWORD) + sizeof(BYTE) + sizeof(UINT));
    g_uTotalFrames = g_uTotalTiles;
    UnmapViewOfFile(pTemp);
    offsetsSize = g_uTotalTiles * sizeof(DWORD);
    DWORD totalHeaderSize = headerSize + offsetsSize;
    g_pHeaderData = (BYTE*)MapViewOfFile(g_hMapping, FILE_MAP_READ, 0, 0, totalHeaderSize);
    if (!g_pHeaderData) { CloseHandle(g_hMapping); CloseHandle(g_hFile); return E_FAIL; }
    g_pTileOffsets = (DWORD*)(g_pHeaderData + headerSize);
    g_pIndexedBuffer = (BYTE*)VirtualAlloc(NULL, TileIndexedSize(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_pTileBuffer = (BYTE*)VirtualAlloc(NULL, TileSize(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_pDecompressBuffer = (BYTE*)VirtualAlloc(NULL, TileIndexedSize() * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (g_pIndexedBuffer) VirtualLock(g_pIndexedBuffer, TileIndexedSize());
    if (g_pTileBuffer) VirtualLock(g_pTileBuffer, TileSize());
    if (g_pDecompressBuffer) VirtualLock(g_pDecompressBuffer, TileIndexedSize() * 2);
    StreamTileKona(0);
    return S_OK;
}
HRESULT loadspriteshit(void)
{
    if (!g_pRenderTarget) return E_POINTER;
    HRESULT hr = InitKonataStreaming();
    if (FAILED(hr)) {
        WCHAR msg[256];
        swprintf(msg, 256, L"InitKonataStreaming failed: 0x%08X", hr);
        MessageBoxW(g_hwnd, msg, L"DEBUG", MB_OK);
        return hr;
    }
    hr = CreateTileBitmap(g_pRenderTarget);
    if (FAILED(hr)) {
        WCHAR msg[256];
        swprintf(msg, 256, L"CreateTileBitmap failed: 0x%08X", hr);
        MessageBoxW(g_hwnd, msg, L"DEBUG", MB_OK);
    }
    return hr;
}
void ripandtear()
{
    if (g_pCurrentTileData && g_pCurrentTileData != g_pHeaderData) {
        UnmapViewOfFile(g_pCurrentTileData);
        g_pCurrentTileData = NULL;
    }
    g_uCurrentMappedTile = UINT_MAX;
    if (g_pHeaderData) {
        UnmapViewOfFile(g_pHeaderData);
        g_pHeaderData = NULL;
    }
    g_pTileOffsets = NULL;
    if (g_hMapping) {
        CloseHandle(g_hMapping);
        g_hMapping = NULL;
    }
    if (g_hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(g_hFile);
        g_hFile = INVALID_HANDLE_VALUE;
    }
    //boofers
    if (g_pDecompressBuffer) {
        VirtualFree(g_pDecompressBuffer, 0, MEM_RELEASE);
        g_pDecompressBuffer = NULL;
    }
    if (g_pTileBuffer) {
        VirtualFree(g_pTileBuffer, 0, MEM_RELEASE);
        g_pTileBuffer = NULL;
    }
    if (g_pIndexedBuffer) {
        VirtualFree(g_pIndexedBuffer, 0, MEM_RELEASE);
        g_pIndexedBuffer = NULL;
    }
    g_uTotalTiles = 0;
    g_uTotalFrames = 0;
    g_uFramesPerRow = 0;
}
void Cleanup(void)
{
    g_bRunning = 0;
    //timer killer
    if (g_animTimerId && g_hwnd && IsWindow(g_hwnd)) {
        KillTimer(g_hwnd, g_animTimerId);
        g_animTimerId = 0;
    }
    g_bPlaying = FALSE;
    if (g_pMenu) {
        g_pMenu->~KonataMenu();
        g_pMenu = NULL;
    }
    //D2D
    if (g_pTileBitmap) {
        g_pTileBitmap->Release();
        g_pTileBitmap = NULL;
    }
    if (g_pRenderTarget) {
        g_pRenderTarget->Release();
        g_pRenderTarget = NULL;
    }
    if (g_pD2DFactory) {
        g_pD2DFactory->Release();
        g_pD2DFactory = NULL;
    }
    ripandtear();
}
void NextFrame(void) { g_uCurrentFrame = (g_uCurrentFrame + 1) % g_uTotalFrames; StreamTileKona(g_uCurrentFrame); }
//callbacks
void __cdecl OnScalechange(void* pTarget, FLOAT fScale) {
    g_fScale = fScale;
    SetWindowPos(g_hwnd, NULL, 0, 0,
        (int)(g_uTileW * g_fScale),
        (int)(g_uTileH * g_fScale),
        SWP_NOMOVE | SWP_NOZORDER);
    if (g_pMenu) g_pMenu->UpdateScale(fScale);
    InvalidateRect(g_hwnd, NULL, FALSE);
    UpdateWindow(g_hwnd);
}
void __cdecl OnFpschange(void* pTarget, int nFps) {
    if (nFps > 0) {
        g_uAnimSpeedMs = 1000 / nFps;
        if (g_pMenu) g_pMenu->UpdateFps(nFps);
        if (g_bPlaying) {
            StartAnimTimer(g_hwnd);
        }
    }
}
void __cdecl OnQualitychange(void* pTarget, int nMode) {
    g_eInterpMode = (nMode == 0) ?
        D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR :
        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;
    if (g_pMenu) g_pMenu->UpdateQuality(nMode);
    InvalidateRect(g_hwnd, NULL, FALSE);
    UpdateWindow(g_hwnd);
}
void __cdecl OnMenuClose(void* pTarget) {
    g_bPlaying = TRUE;
}
void OnPaint(HWND hwnd)//im fucking lost in this shit
{
    if (!g_pRenderTarget || !g_pTileBitmap) return;
    g_pRenderTarget->BeginDraw();
    g_pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
    D2D1_SIZE_F rtSize = g_pRenderTarget->GetSize();
    D2D1_RECT_F destRect = D2D1::RectF(0.0f, 0.0f, rtSize.width, rtSize.height);
    g_pRenderTarget->DrawBitmap(g_pTileBitmap, destRect, 1.0f, g_eInterpMode, NULL);
    HRESULT hr = g_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        g_pRenderTarget->Release(); g_pRenderTarget = NULL;
        g_pTileBitmap->Release(); g_pTileBitmap = NULL;
        RECT rc; GetClientRect(hwnd, &rc);
        D2D1_SIZE_U uSize = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        g_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
            D2D1::HwndRenderTargetProperties(hwnd, uSize), &g_pRenderTarget);
        CreateTileBitmap(g_pRenderTarget);
        StreamTileKona(g_uCurrentFrame);
    }
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        g_hwnd = hwnd;
        g_hsl.Init();
        SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TOPMOST);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        { MARGINS margins = { -1 }; DwmExtendFrameIntoClientArea(hwnd, &margins); }
        if (FAILED(InitD2D(hwnd))) { MessageBox(hwnd, L"Direct2D init ERROR", L"ERROR", MB_ICONERROR); return -1; }
        if (FAILED(loadspriteshit())) { MessageBox(hwnd, L"failed to load sprite", L"ERROR", MB_ICONERROR); return -1; }
        g_pMenu = new (g_menuMemory) KonataMenu();
        if (g_pMenu) {
            g_pMenu->SetCallbacks(NULL, OnScalechange, OnFpschange, OnQualitychange, OnMenuClose);
            g_pMenu->UpdateScale(1.0f); g_pMenu->UpdateFps(10); g_pMenu->UpdateQuality(1);
        }
        SetWindowPos(hwnd, NULL, 100, 100, g_uTileW, g_uTileH, SWP_NOZORDER);
        return 0;

    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hwnd, &ps); OnPaint(hwnd); EndPaint(hwnd, &ps); } return 0;
    case WM_RBUTTONDOWN: {
        POINT pt;
        GetCursorPos(&pt);
        if (g_pMenu) {
            g_bPlaying = FALSE;
            g_pMenu->Show(hwnd, pt.x, pt.y);
            if (g_bPlaying) {
                StartAnimTimer(hwnd);
            }
        }
    }
                       return 0;
    case WM_TIMER:
        if (wParam == 2 && g_bPlaying) {
            NextFrame();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if (g_pMenu && g_pMenu->IsVisible()) return 0;
        switch (wParam) {
        case VK_SPACE:
            g_bPlaying = !g_bPlaying;
            if (g_bPlaying) StartAnimTimer(hwnd);
            else { KillTimer(hwnd, g_animTimerId); g_animTimerId = 0; }
            break;//now down below this shitty shit comes veeeeery veeeery messy in my opinion code
        case VK_LEFT: g_bPlaying = FALSE; g_uCurrentFrame = (g_uCurrentFrame > 0) ? g_uCurrentFrame - 1 : g_uTotalFrames - 1; StreamTileKona(g_uCurrentFrame); InvalidateRect(hwnd, NULL, FALSE); break;
        case VK_RIGHT: g_bPlaying = FALSE; NextFrame(); InvalidateRect(hwnd, NULL, FALSE); break;
        case VK_UP: if (g_uAnimSpeedMs > 16) { g_uAnimSpeedMs = MAX(16, g_uAnimSpeedMs - 20); if (g_pMenu) g_pMenu->UpdateFps(1000 / g_uAnimSpeedMs); } break;
        case VK_DOWN: g_uAnimSpeedMs = MIN(1000, g_uAnimSpeedMs + 20); if (g_pMenu) g_pMenu->UpdateFps(1000 / g_uAnimSpeedMs); break;
        case VK_ESCAPE: DestroyWindow(hwnd); break;
        case VK_OEM_4:  //[
            PrevSkin();
            break;
        case VK_OEM_6:  //]
            NextSkin();
            break;
        case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9':
            makinskin(wParam - '1');
            break;
        case '0':
            makinskin(9);//10 skin
            break;
        } return 0;
    case WM_LBUTTONDOWN: ReleaseCapture(); SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0); return 0;
    case WM_DESTROY: Cleanup(); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
static volatile LONG g_bLoadingSkin = 0;
void makinskin(int index)
{
    if (InterlockedExchange(&g_bLoadingSkin, 1) == 1) return;

    if (index < 0 || index >= (int)g_skins.size()) {
        InterlockedExchange(&g_bLoadingSkin, 0);
        return;
    }
    if (g_skins.empty()) {
        InterlockedExchange(&g_bLoadingSkin, 0);
        return;
    }
    SkinInfo& newSkin = g_skins[index];
    BOOL hasKona = (GetFileAttributesW(newSkin.konaPath) != INVALID_FILE_ATTRIBUTES);
    BOOL hasPng = (GetFileAttributesW(newSkin.pngPath) != INVALID_FILE_ATTRIBUTES);
    if (!hasKona && !hasPng) {
        WCHAR msg[256];
        swprintf(msg, 256, L"files not found for %s", newSkin.name);
        MessageBoxW(g_hwnd, msg, L"ERROR", MB_ICONWARNING);
        InterlockedExchange(&g_bLoadingSkin, 0);
        return;
    }
    BOOL wasPlaying = g_bPlaying;
    g_bPlaying = FALSE;
    if (g_animTimerId) {
        KillTimer(g_hwnd, g_animTimerId);
        g_animTimerId = 0;
    }
    ShowWindow(g_hwnd, SW_HIDE);
    if (g_pTileBitmap) {
        g_pTileBitmap->Release();
        g_pTileBitmap = NULL;
    }
    //create render target if size is changes
    if (g_pRenderTarget) {
        g_pRenderTarget->Release();
        g_pRenderTarget = NULL;
    }
    ripandtear();
    //new parametres
    wcscpy_s(g_szCurrentPngPath, MAX_PATH, newSkin.pngPath);
    wcscpy_s(g_szCurrentKonaPath, MAX_PATH, newSkin.konaPath);
    g_uTileW = newSkin.tileW;
    g_uTileH = newSkin.tileH;
    g_uTotalFrames = newSkin.frames;
    g_uAnimSpeedMs = 1000 / max(newSkin.fps, 1);
    g_uCurrentFrame = 0;
    g_currentSkin = index;
    //convert PNG if needed
    if (!hasKona && hasPng) {
        HRESULT hr = ConvertPNGtoKona(g_szCurrentPngPath, g_szCurrentKonaPath);
        if (FAILED(hr)) {
            WCHAR msg[256];
            swprintf(msg, 256, L"PNG convertation failed %s", g_szCurrentPngPath);//you don't want to see this but tou will not
            MessageBoxW(g_hwnd, msg, L"ERROR", MB_ICONERROR);
            InterlockedExchange(&g_bLoadingSkin, 0);
            ShowWindow(g_hwnd, SW_SHOW);
            return;
        }
    }
    //skin logic for render target if skin have other size
    D2D1_SIZE_U uSize = D2D1::SizeU(g_uTileW, g_uTileH);
    HRESULT hr = g_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(g_hwnd, uSize),
        &g_pRenderTarget);

    if (FAILED(hr)) {
        MessageBoxW(g_hwnd, L"can't create render target", L"ERROR", MB_ICONERROR);//basically useless if you're using original version
        InterlockedExchange(&g_bLoadingSkin, 0);
        ShowWindow(g_hwnd, SW_SHOW);
        return;
    }
    hr = CreateTileBitmap(g_pRenderTarget);
    if (FAILED(hr)) {
        MessageBoxW(g_hwnd, L"can't create bitmap file", L"ERROR", MB_ICONERROR);//you don't usually see this if you're using this version and not your customizable
        InterlockedExchange(&g_bLoadingSkin, 0);
        ShowWindow(g_hwnd, SW_SHOW);
        return;
    }
    hr = InitKonataStreaming();
    if (FAILED(hr)) {
        MessageBoxW(g_hwnd, L"can't init streaming itt", L"ERROR", MB_ICONERROR);//same here
        if (g_pTileBitmap) { g_pTileBitmap->Release(); g_pTileBitmap = NULL; }
        InterlockedExchange(&g_bLoadingSkin, 0);
        ShowWindow(g_hwnd, SW_SHOW);
        return;
    }
    hr = StreamTileKona(0);
    if (FAILED(hr)) {
        //MessageBoxW(g_hwnd, L"cannot load first frame", L"ERROR", MB_ICONERROR);//useless if you're using original code
        InterlockedExchange(&g_bLoadingSkin, 0);
        ShowWindow(g_hwnd, SW_SHOW);
        return;
    }
    SetWindowPos(g_hwnd, NULL, 0, 0,
        (int)(g_uTileW * g_fScale),
        (int)(g_uTileH * g_fScale),
        SWP_NOMOVE | SWP_NOZORDER);

    WCHAR title[256];
    swprintf(title, 256, L"Konata - %s [%d/%d]",
        newSkin.name, index + 1, (int)g_skins.size());
    SetWindowTextW(g_hwnd, title);
    ShowWindow(g_hwnd, SW_SHOW);
    InvalidateRect(g_hwnd, NULL, TRUE);
    UpdateWindow(g_hwnd);

    g_bPlaying = wasPlaying;
    if (g_bPlaying) {
        StartAnimTimer(g_hwnd);
    }
    InterlockedExchange(&g_bLoadingSkin, 0);
}
void NextSkin() {
    if (g_skins.empty()) return;
    int next = (g_currentSkin + 1) % (int)g_skins.size();
    makinskin(next);
}
void PrevSkin() {
    if (g_skins.empty()) return;
    int prev = (g_currentSkin + (int)g_skins.size() - 1) % (int)g_skins.size();
    makinskin(prev);
}
void ParseSkinJsonFile(const WCHAR* filePath) {//added pretty recently and it works idk fine?
    FILE* fp = _wfopen(filePath, L"rb");
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size <= 0 || size > 1024 * 1024) {  //1mb maximum
        fclose(fp);
        return;
    }
    char* json = (char*)malloc(size + 1);
    if (!json) {
        fclose(fp);
        return;
    }
    fread(json, 1, size, fp);
    json[size] = '\0';
    fclose(fp);
    //parser kinda lame but hey
    char* p = json;
    char* end = json + size;

    while (p && p < end && (p = strstr(p, "\"name\"")) != NULL) {
        SkinInfo skin = {};
        skin.frames = 24;
        skin.fps = 10;
        skin.tileW = 220;
        skin.tileH = 251;
        //name
        p = strchr(p, ':');
        if (p && (p = strchr(p, '"')) != NULL) {
            p++;
            char* e = strchr(p, '"');
            if (e) {
                *e = '\0';
                MultiByteToWideChar(CP_UTF8, 0, p, -1, skin.name, 64);
                p = e + 1;
            }
        }
        if (!p || p >= end) break;
        char* k = strstr(p, "\"kona_file\"");
        if (k && k < end && (k = strchr(k, ':')) != NULL && (k = strchr(k, '"')) != NULL) {
            k++;
            char* e = strchr(k, '"');
            if (e) {
                *e = '\0';
                MultiByteToWideChar(CP_UTF8, 0, k, -1, skin.konaPath, MAX_PATH);
            }
        }
        char* f = strstr(p, "\"frames\"");
        if (f && f < end && (f = strchr(f, ':')) != NULL) {
            skin.frames = atoi(f + 1);
        }
        char* fps_str = strstr(p, "\"fps\"");
        if (fps_str && fps_str < end && (fps_str = strchr(fps_str, ':')) != NULL) {
            skin.fps = atoi(fps_str + 1);
        }
        char* tw = strstr(p, "\"tile_width\"");
        if (!tw || tw >= end || tw > strchr(p, '}')) {
            tw = strstr(p, "\"tile_w\"");
        }
        if (tw && tw < end && (tw = strchr(tw, ':')) != NULL) {
            skin.tileW = atoi(tw + 1);
        }
        char* th = strstr(p, "\"tile_height\"");
        if (!th || th >= end || th > strchr(p, '}')) {
            th = strstr(p, "\"tile_h\"");
        }
        if (th && th < end && (th = strchr(th, ':')) != NULL) {
            skin.tileH = atoi(th + 1);
        }
        if (skin.konaPath[0] != L'\0' && skin.pngPath[0] == L'\0') {
            wcscpy_s(skin.pngPath, MAX_PATH, skin.konaPath);
            WCHAR* ext = wcsstr(skin.pngPath, L".kona");
            if (ext) wcscpy_s(ext, MAX_PATH - (ext - skin.pngPath), L".png");
        }

        //only valid skins
        if (skin.tileW > 0 && skin.tileH > 0 && skin.name[0] != L'\0') {
            g_skins.push_back(skin);
        }

        p = strchr(p, '}');
        if (p) p++; else break;
    }
    free(json);
}
void ScanSkinsFolder() {
    g_skins.clear();
    SkinInfo d = {};
    wcscpy_s(d.name, 64, L"KonaDef");
    wcscpy_s(d.pngPath, MAX_PATH, L"Konatathingy.png");//this is basically useless because you can use .kona without .png
    wcscpy_s(d.konaPath, MAX_PATH, L"konata.kona");
    d.frames = 182;
    d.fps = 10;
    d.tileW = 640;
    d.tileH = 480;
    g_skins.push_back(d);
    //odl shit
    FILE* fp = _wfopen(L"skins\\skins.json", L"rb");
    if (fp) {
        fclose(fp);
        ParseSkinJsonFile(L"skins\\skins.json");
    }
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(L"skins\\*.skin.json", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            WCHAR fullPath[MAX_PATH];
            swprintf(fullPath, MAX_PATH, L"skins\\%s", fd.cFileName);
            if (_wcsicmp(fd.cFileName, L"skins.json") != 0) {
                ParseSkinJsonFile(fullPath);
            }

        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    CheckAVX2Support();
    timeBeginPeriod(1);
    CoInitialize(NULL);
    ScanSkinsFolder();

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"konashit";
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"konashit", L"KonaKona", WS_POPUP,
        100, 100, g_uTileW, g_uTileH, NULL, NULL, hInstance, NULL);
    if (!hwnd) { CoUninitialize(); timeEndPeriod(1); return 0; }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    fancylogic(hwnd);

    Cleanup();
    CoUninitialize();
    timeEndPeriod(1);
    return 0;
}/*all of this shit uses somewhere like 26mb vram at peak with standart skin, 0io at peak, 0,50cpu at peak, still idk why i made .kona format*/
/*im too lazy right now to make code look pretty but anyways*/