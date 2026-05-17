#pragma once

#include <atomic>
#include <cstddef>

class CustomWindow : public QWidget {
  public:
    int customId;
    int targetX;
    int targetY;
    int targetWidth;
    int targetHeight;
    float targetOpacity;
    bool targetDecorations = true;
    QString targetTitle;
    // QLabel* testLabel;
    bool isVisible = true;
    bool isClosing = false;
    bool hyprReady = false;

    bool _lastDecorations = true;

    int cutoffX = 0;
    int cutoffY = 0;

    int debugTextureUpdateCount = 0;
    int debugCopyCount = 0;
    int debugPaintCount = 0;
    bool debugLoggedMissingTexture = false;
    bool debugLoggedNullImage = false;
    bool textureDataIsUpsideDown = false;
    std::atomic_bool updateQueued = false;

#ifdef WITH_WINE
    ID3D11Resource* resource = nullptr;
    ID3D11Texture2D* texture = nullptr;
    D3D11_TEXTURE2D_DESC desc;

    ID3D11Texture2D* stagingTexture = nullptr;
    D3D11_TEXTURE2D_DESC stagingDesc;

    D3D11_MAPPED_SUBRESOURCE mapped;
#else
    GLuint glTextureId = -1;
    void* tempTexture = nullptr;
    bool textureUsesOpenGL = false;
    bool textureUsesVulkan = false;
    bool hasTexturePixels = false;
    void* unityVulkanTexture = nullptr;
    void* vulkanReadbackBuffer = nullptr;
    void* vulkanReadbackMemory = nullptr;
    void* vulkanReadbackMapped = nullptr;
    size_t vulkanReadbackSize = 0;
    bool vulkanReadbackHostCoherent = false;
    int vulkanReadbackWidth = 0;
    int vulkanReadbackHeight = 0;
    bool vulkanReadbackSwapRedBlue = false;
    unsigned long long vulkanReadbackSubmittedFrame = 0;
    bool vulkanReadbackPending = false;
#endif

    QImage* qtImage = nullptr;

    QPixmap iconPixmap;
    QIcon* iconIcon = nullptr;

    CustomWindow();

#ifdef WITH_WINE
    void setTexture(ID3D11Resource* resource);
#else
    void setTexture(GLuint textureId);
    void setVulkanTexture(void* texturePtr);
    void setTextureSize(int w, int h);
    void setTexturePixels(const void* pixels, int byteCount, int w, int h);
#endif
    bool copyTexture();

    void _setX11Decorations(bool hasDecorations);
    void setTargetMove(int x, int y);
    void setTargetSize(int w, int h);
    void updateThings();
    void paintEvent(QPaintEvent* paintEvent) override;
    void setIcon(QImage* image);
    void closeEvent(QCloseEvent* closeEvent) override;
    ~CustomWindow();
};

class ScreenSizeWindow : public QWidget {
  public:
    QScreen* actualScreen;

    void doTheStuff(QScreen* screen);

    int resizeCount = 0;
    void resizeEvent(QResizeEvent* event) override;
};

class Hyprctl {
  public:
    std::string socketPath;

    Hyprctl();
    void sendMessage(std::string message);
    bool sendMessageSync(std::string message);
    bool setProp(std::string window, std::string effect, std::string argument);
    void moveWindow(std::string window, int x, int y);
};
