#pragma once


class CustomWindow : public QWidget {
public:
    int targetX;
    int targetY;
    int targetWidth;
    int targetHeight;
    float targetOpacity;
    bool targetDecorations = true;
    // QLabel* testLabel;
    bool isVisible = true;

    bool _lastDecorations = true;

    int cutoffX;
    int cutoffY;

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
    void _setDecorations(bool hasDecorations);
    void setTargetMove(int x, int y);
    void setTargetSize(int w, int h);
    void updateThings();
    void paintEvent(QPaintEvent* paintEvent) override;
    void setIcon(QImage* image);
    ~CustomWindow();
};
