//
// Created by vastrakai on 6/29/2024.
//

#include "D2D.hpp"

#include <winrt/base.h>
#include <spdlog/spdlog.h>

struct BlurCallbackData;
static winrt::com_ptr<ID2D1Factory3> d2dFactory = nullptr;
static winrt::com_ptr<ID2D1Device> d2dDevice = nullptr;

static winrt::com_ptr<ID2D1Effect> blurEffect = nullptr;
static winrt::com_ptr<ID2D1Bitmap1> sourceBitmap = nullptr;

static winrt::com_ptr<ID2D1DeviceContext> d2dDeviceContext = nullptr;
static winrt::com_ptr<ID2D1SolidColorBrush> brush = nullptr;

bool initD2D = false;
// used for optimized blur
static ID2D1Bitmap* cachedBitmap = nullptr;
static ID2D1ImageBrush* cachedBrush = nullptr;
static ID2D1RoundedRectangleGeometry* cachedClipRectGeo = nullptr;
static bool requestFlush = false;



float dpi = 0.0f;

void D2D::init(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice)
{
    if (initD2D) return;

    if (initD2D == false && SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory.put()))) {
        winrt::com_ptr<IDXGIDevice> dxgiDevice;
        pDevice->QueryInterface<IDXGIDevice>(dxgiDevice.put());
        if (dxgiDevice == nullptr) {
            return;
        }

        d2dFactory->CreateDevice(dxgiDevice.get(), d2dDevice.put());
        if (d2dDevice == nullptr) {
            return;
        }

        d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2dDeviceContext.put());
        if (d2dDeviceContext == nullptr) {
            return;
        }

        //Create blur
        d2dDeviceContext->CreateEffect(CLSID_D2D1GaussianBlur, blurEffect.put());
        if (blurEffect == nullptr) {
            return;
        }
    }

    initD2D = true;
}

void D2D::shutdown()
{
    if (!initD2D) {
        return;
    }

    d2dFactory = nullptr;
    d2dDevice = nullptr;
    d2dDeviceContext = nullptr;
    brush = nullptr;
    blurEffect = nullptr;
    sourceBitmap = nullptr;
    initD2D = false;
    spdlog::info("Shutdown D2D.");
}

void D2D::beginRender(IDXGISurface* surface, float fxdpi)
{
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                 D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));
    HRESULT hr = d2dDeviceContext->CreateBitmapFromDxgiSurface(surface, &bitmapProperties, sourceBitmap.put());

    if (FAILED(hr)) {
        spdlog::error("Failed to create bitmap from DXGI surface");
        return;
    }

    d2dDeviceContext->SetTarget(sourceBitmap.get());
    d2dDeviceContext->BeginDraw();
}

void D2D::ghostFrameCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    auto data = (GhostCallbackData*)cmd->UserCallbackData;
    if (!data)
        return;

    ImGuiIO& io = ImGui::GetIO();
    auto displaySize = io.DisplaySize;
    auto size = sourceBitmap->GetPixelSize();
    auto rect = D2D1::RectU(0, 0, size.width, size.height);
    auto destPoint = D2D1::Point2U(0, 0);

    // Static container for ghost frames and a reusable target bitmap.
    static std::vector<ID2D1Bitmap*> ghostBitmaps;
    static ID2D1Bitmap* targetBitmap = nullptr;
    static D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(sourceBitmap->GetPixelFormat());

    // Ensure the target bitmap exists.
    if (!targetBitmap) {
        d2dDeviceContext->CreateBitmap(size, props, &targetBitmap);
    }
    // Copy the current render target into our target bitmap.
    targetBitmap->CopyFromBitmap(&destPoint, sourceBitmap.get(), &rect);

    int maxFrames = data->maxFrames;
    // Remove the oldest frame if we exceed our maximum.
    while (ghostBitmaps.size() >= (size_t)maxFrames) {
        auto bmp = ghostBitmaps.front();
        ghostBitmaps.erase(ghostBitmaps.begin());
        bmp->Release();
    }
    // Create a new ghost bitmap for this frame.
    ID2D1Bitmap* newGhost = nullptr;
    d2dDeviceContext->CreateBitmap(size, props, &newGhost);
    newGhost->CopyFromBitmap(&destPoint, sourceBitmap.get(), &rect);
    ghostBitmaps.push_back(newGhost);

    // Compute weights for each stored frame using an exponential decay.
    // Newer frames have more weight; older ones fade out.
    const int numFrames = ghostBitmaps.size();
    float totalWeight = 0.0f;
    std::vector<float> weights(numFrames, 0.0f);
    // The decay factor can be tied to data->strength (e.g. higher intensity makes older frames fade faster).
    const float decay = data->strength; // For example, 0.9f
    for (int i = 0; i < numFrames; i++) {
        // Use index 0 for the oldest frame and index (numFrames - 1) for the most recent.
        float weight = pow(decay, float(numFrames - 1 - i));
        weights[i] = weight;
        totalWeight += weight;
    }
    // Normalize weights so their sum is 1.
    for (int i = 0; i < numFrames; i++) {
        weights[i] /= totalWeight;
    }

    // Save the current transform.
    D2D1_MATRIX_3X2_F originalTransform;
    d2dDeviceContext->GetTransform(&originalTransform);

    // Optionally, you could add a slight per-frame offset to simulate directional motion.
    // Here we allow a small horizontal offset that increases with the age of the ghost frame.
    const float offsetMultiplier = 2.0f; // tweak this value to control offset magnitude

    // Composite each ghost frame.
    for (int i = 0; i < numFrames; i++) {
        // Compute a small offset: older frames are shifted slightly more.
        float offsetX = -offsetMultiplier * (numFrames - 1 - i);
        float offsetY = 0.0f; // change if you want vertical offset as well

        // Apply a translation for this ghost frame.
        D2D1::Matrix3x2F translation = D2D1::Matrix3x2F::Translation(offsetX, offsetY);
        d2dDeviceContext->SetTransform(translation * originalTransform);

        // Draw the ghost bitmap with its computed normalized opacity.
        d2dDeviceContext->DrawBitmap(
            ghostBitmaps[i],
            D2D1::RectF(0, 0, displaySize.x, displaySize.y),
            weights[i],
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
        );
    }
    // Restore the original transform.
    d2dDeviceContext->SetTransform(originalTransform);

    // Optional: flush the context if needed.
    d2dDeviceContext->Flush();

    // Remove the callback data from our ghostCallbacks list.
    for (auto it = ghostCallbacks.begin(); it != ghostCallbacks.end(); ++it) {
        if (it->get() == data) {
            ghostCallbacks.erase(it);
            break;
        }
    }
}

void D2D::addGhostFrame(ImDrawList* drawList, int maxFrames, float strength)
{
    if (!initD2D)
        return;

    auto uniqueData = std::make_shared<GhostCallbackData>(strength, maxFrames);
    auto data = uniqueData.get();
    ghostCallbacks.push_back(uniqueData);
    drawList->AddCallback(ghostFrameCallback, data);
}


void D2D::endRender()
{
    // Call begin draw and end draw to flush the render target
    d2dDeviceContext->EndDraw();
    d2dDeviceContext->SetTarget(nullptr); // Unbind the render target
    sourceBitmap = nullptr;
    if (cachedBitmap != nullptr) {
        cachedBitmap->Release();
        cachedBitmap = nullptr;
    }
    if (cachedBrush != nullptr) {
        cachedBrush->Release();
        cachedBrush = nullptr;
    }
    if (cachedClipRectGeo != nullptr) {
        cachedClipRectGeo->Release();
        cachedClipRectGeo = nullptr;
    }

}




void D2D::blurCallback(const ImDrawList* parentList, const ImDrawCmd* cmd)
{
    // Grab our custom data, which has strength, rounding, etc.
    auto data = reinterpret_cast<BlurCallbackData*>(cmd->UserCallbackData);
    if (!data || !initD2D)
        return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 displaySize = io.DisplaySize;

    // 1) Figure out the clip rectangle to use.
    //    If the user didn't specify one, default to the entire screen area
    ImVec4 clipRect = data->clipRect.value_or(ImVec4(0, 0, displaySize.x, displaySize.y));

    // 2) Copy the current D2D render target into a temporary bitmap
    D2D1_SIZE_U srcSize = sourceBitmap->GetPixelSize();
    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(sourceBitmap->GetPixelFormat());

    // Create a temporary bitmap to hold the unblurred scene
    winrt::com_ptr<ID2D1Bitmap> tempBitmap;
    HRESULT hr = d2dDeviceContext->CreateBitmap(srcSize, props, tempBitmap.put());
    if (FAILED(hr) || !tempBitmap) {
        spdlog::error("Failed to create temporary scene bitmap for blur");
        return;
    }

    // Copy current scene into tempBitmap
    D2D1_POINT_2U destPoint = D2D1::Point2U(0, 0);
    D2D1_RECT_U   sourceRect = D2D1::RectU(0, 0, srcSize.width, srcSize.height);
    tempBitmap->CopyFromBitmap(&destPoint, sourceBitmap.get(), &sourceRect);

    // 3) Set up the built‐in GaussianBlur effect
    blurEffect->SetInput(0, tempBitmap.get());
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, data->strength);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
    blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY);

    // 4) Create a geometry for the clip region (rounded rectangle)
    D2D1_RECT_F   d2dClipRect = D2D1::RectF(clipRect.x, clipRect.y, clipRect.z, clipRect.w);
    D2D1_ROUNDED_RECT rrClip = D2D1::RoundedRect(d2dClipRect, data->rounding, data->rounding);

    winrt::com_ptr<ID2D1RoundedRectangleGeometry> clipGeometry;
    d2dFactory->CreateRoundedRectangleGeometry(rrClip, clipGeometry.put());

    // 5) Create an ImageBrush from the blur effect’s output
    winrt::com_ptr<ID2D1Image> blurredOutput;
    blurEffect->GetOutput(blurredOutput.put());

    D2D1_IMAGE_BRUSH_PROPERTIES brushProps =
        D2D1::ImageBrushProperties(D2D1::RectF(0, 0, float(srcSize.width), float(srcSize.height)));
    winrt::com_ptr<ID2D1ImageBrush> imageBrush;
    hr = d2dDeviceContext->CreateImageBrush(blurredOutput.get(), brushProps, imageBrush.put());
    if (FAILED(hr) || !imageBrush) {
        spdlog::error("Failed to create image brush for blur");
        return;
    }

    // 6) Fill the clip geometry with the blurred image
    //    This effectively draws the blurred scene only inside the chosen rectangle
    d2dDeviceContext->FillGeometry(clipGeometry.get(), imageBrush.get());

    // 7) Flush to ensure the blur draws
    d2dDeviceContext->Flush();

    // 8) Clean up
    //    Remove from your blurCallbacks so we don’t keep data around
    for (auto it = blurCallbacks.begin(); it != blurCallbacks.end(); ++it) {
        if (it->get() == data) {
            blurCallbacks.erase(it);
            break;
        }
    }
}

bool D2D::addBlur(ImDrawList* drawList, float strength, std::optional<ImVec4> clipRectOpt, float rounding)
{
    if (!initD2D)
        return false;
    if (strength <= 0.f)
        return false;

    // Create the shared callback data
    auto uniqueData = std::make_shared<BlurCallbackData>(strength, rounding, clipRectOpt);
    auto data = uniqueData.get();
    blurCallbacks.push_back(uniqueData);

    // We add a single callback that handles everything
    drawList->AddCallback(blurCallback, data);
    return true;
}

template <typename T>
void SafeRelease(T** ptr) {
    if (*ptr) {
        (*ptr)->Release();
        *ptr = nullptr;
    }
}
void D2D::blurCallbackOptimized(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    auto data = (BlurCallbackData*)cmd->UserCallbackData;
    if (!data)
        return;

    ImGuiIO& io = ImGui::GetIO();
    // Use the provided clip rectangle or fallback to the command’s clip rect.
    ImVec4 clipRect = data->clipRect.has_value() ? *data->clipRect : cmd->ClipRect;

    // Ensure our cached bitmap matches the current render target.
    D2D1_SIZE_U bitmapSize = sourceBitmap->GetPixelSize();
    if (!cachedBitmap ||
        cachedBitmap->GetPixelSize().width != bitmapSize.width ||
        cachedBitmap->GetPixelSize().height != bitmapSize.height)
    {
        SafeRelease(&cachedBitmap);
        D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(sourceBitmap->GetPixelFormat());
        d2dDeviceContext->CreateBitmap(bitmapSize, props, &cachedBitmap);
        auto destPoint = D2D1::Point2U(0, 0);
        auto rect = D2D1::RectU(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        cachedBitmap->CopyFromBitmap(&destPoint, sourceBitmap.get(), &rect);
    }

    // Recreate the clip geometry if needed.
    static ImVec4 cachedClipRect;
    static float cachedRounding = -1.0f;
    if (!cachedClipRectGeo || cachedClipRect != clipRect || cachedRounding != data->rounding)
    {
        SafeRelease(&cachedClipRectGeo);
        cachedClipRect = clipRect;
        cachedRounding = data->rounding;

        D2D1_RECT_F clipRectD2D = D2D1::RectF(clipRect.x, clipRect.y, clipRect.z, clipRect.w);
        D2D1_ROUNDED_RECT clipRectRounded = D2D1::RoundedRect(clipRectD2D, data->rounding, data->rounding);
        d2dFactory->CreateRoundedRectangleGeometry(clipRectRounded, &cachedClipRectGeo);
    }

    // Reuse (or create) the image brush that holds the blurred image.
    if (!cachedBrush)
    {
        ID2D1Image* outImage = nullptr;
        blurEffect->SetInput(0, cachedBitmap);
        blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, data->strength);
        blurEffect->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);
        blurEffect->GetOutput(&outImage);

        D2D1_IMAGE_BRUSH_PROPERTIES brushProps = D2D1::ImageBrushProperties(
            D2D1::RectF(0, 0, io.DisplaySize.x, io.DisplaySize.y));
        d2dDeviceContext->CreateImageBrush(outImage, brushProps, &cachedBrush);
        SafeRelease(&outImage);
    }

    // Save the current transform.
    D2D1_MATRIX_3X2_F originalTransform;
    d2dDeviceContext->GetTransform(&originalTransform);

    // --- Enhanced Motion Blur ---
    // We'll composite several samples of the blurred image with a horizontal offset.
    // Adjust these values to fine-tune the effect.
    const int numSamples = 5;                    // Number of samples for the motion blur
    float motionMagnitude = data->strength * 10.0f; // Scale factor for offset magnitude

    // Loop over several samples.
    for (int i = 0; i < numSamples; i++)
    {
        // Normalize sample index to [0, 1].
        float t = (float)i / (numSamples - 1);
        // Compute an offset that ranges from -motionMagnitude/2 to +motionMagnitude/2.
        float offsetX = (t - 0.5f) * motionMagnitude;
        // Weight peaks at the center (t=0.5) and falls off at the edges.
        float weight = 1.0f - fabs(t - 0.5f) * 2.0f;

        // Push a layer with the desired opacity.
        D2D1_LAYER_PARAMETERS layerParams = D2D1::LayerParameters(
            D2D1::RectF(clipRect.x, clipRect.y, clipRect.z, clipRect.w),
            nullptr,
            D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
            D2D1::Matrix3x2F::Identity(),
            weight,
            nullptr,
            D2D1_LAYER_OPTIONS_NONE);
        d2dDeviceContext->PushLayer(&layerParams, nullptr);

        // Set a translation transform for this sample.
        D2D1::Matrix3x2F translation = D2D1::Matrix3x2F::Translation(offsetX, 0.0f);
        d2dDeviceContext->SetTransform(translation * originalTransform);

        // Draw the blurred image using the cached clip geometry.
        d2dDeviceContext->FillGeometry(cachedClipRectGeo, cachedBrush);

        d2dDeviceContext->PopLayer();
    }

    // Restore the original transform.
    d2dDeviceContext->SetTransform(originalTransform);

    d2dDeviceContext->Flush();
}



bool D2D::addBlurOptimized(ImDrawList* drawList, float strength, std::optional<ImVec4> clipRect, float rounding)
{
    if (!initD2D) {
        return false;
    }

    if (strength == 0) {
        return false;
    }

    auto uniqueData = std::make_shared<BlurCallbackData>(strength, rounding, clipRect);
    auto data = uniqueData.get();
    blurCallbacks.push_back(uniqueData);
    drawList->AddCallback(blurCallbackOptimized, data);
    return true;
}