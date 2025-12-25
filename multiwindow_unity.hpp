#pragma once


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

    bool _lastDecorations = true;

    int cutoffX = 0;
    int cutoffY = 0;

    ID3D11Resource* resource = nullptr;
    ID3D11Texture2D* texture = nullptr;
    D3D11_TEXTURE2D_DESC desc;

    ID3D11Texture2D* stagingTexture = nullptr;
    D3D11_TEXTURE2D_DESC stagingDesc;

    D3D11_MAPPED_SUBRESOURCE mapped;
    QImage* qtImage = nullptr;
    QMutex qtImageMutex;

    QPixmap iconPixmap;
    QIcon* iconIcon = nullptr;

    CustomWindow();
    void setTexture(ID3D11Resource* resource);
    void copyTexture();
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
