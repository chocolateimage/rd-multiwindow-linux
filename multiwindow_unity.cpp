#ifdef WITH_WINE

#include <windef.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <d3d11.h>
#include <d3d11_4.h>
#include "unity/IUnityGraphics.h"
#include "unity/IUnityGraphicsD3D11.h"
#undef _WIN32
#undef WIN32
#undef __WIN32__
#undef WIN64
#undef _WIN64
#undef __WIN64__
#undef WINAPI_FAMILY
#undef __NT__
#undef interface
#include <QtWidgets>

#else

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
typedef void* HANDLE;
typedef void* HWND;
typedef bool BOOL;
typedef char* LPSTR;
#define WINAPI
#include "unity/IUnityGraphics.h"
#include <GL/glew.h>
#include <QtCore>
#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtGui/QScreen>
#include <QtGui/QCloseEvent>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>
#endif

#include <QDBusMessage>
#include <QDBusConnection>
#include <bitset>
#include <unistd.h>
#include <thread>
#include <xcb/xcb.h>
#include "multiwindow_unity.hpp"

#define SAFE_RELEASE(a) if (a) { a->Release(); a = NULL; }
#define SAFE_FREE(a) if (a) { free(a); a = NULL; }
#define SAFE_DELETE(a) if (a) { delete a; a = NULL; }

QString kwinFile = "/tmp/multiwindow_unity_kwin.js";
void* MAIN_WINDOW = (void*)0x12345;
#ifdef WITH_WINE
HWND main_window_handle;
#else
xcb_window_t main_window_handle;
#endif
int main_window_x = 0;
int main_window_y = 0;
int main_window_width = 800;
int main_window_height = 600;
bool createdApplication = false;
bool appReady = false;
bool useWayland = false;
// Need to use this hack instead of using actual "availableGeometry".
// Maximizing invisible windows (ScreenSizeWindow) is a more reliable
// method of getting the actual screen size without the taskbar.
QMap<QScreen*, QRect> screenGeometries;

std::vector<CustomWindow*> allCustomWindows;
std::vector<WId> waylandWindowOrder;

xcb_connection_t* globalXcbConnection;

std::string boolToStr(bool value) {
    return value ? "true" : "false";
}

char* createString(const char* string) {
#ifdef WITH_WINE
    return (char*)string;
#else
    char* address = (char*)malloc(strlen(string) + 1);
    strcpy(address, string);
    return address;
#endif
}

class CustomApplication : public QApplication {
public:
#ifdef WITH_WINE
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
#endif

    CustomApplication(int &argc, char** argv) : QApplication(argc, argv) {
        this->setQuitOnLastWindowClosed(false);
    }

    bool createKWinFile() {
        QFile file(kwinFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    
        QTextStream textStream(&file);
        textStream << "const appPid = " << this->applicationPid();
        textStream << R"""(
// puts your hands up in the sky. puts your hands up in the sky. puts your puts your. PUT YOUR HANDS UP HIGH
// https://youtu.be/eBC9r5WMNng

const mainWindow = workspace.stackingOrder.find((win) => win.pid == appPid);
let lastOrder = "";
const lastConstraints = [];

function decode(caption) {
    if (!caption.endsWith("\u200B") && !caption.endsWith("\u200C")) {
        return null;
    }
    let encoded = "";
    let msg = [];
    for (const letter of caption) {
        if (letter != "\u200B" && letter != "\u200C") continue;
        encoded += letter == "\u200B" ? "0" : 1;
        if (encoded.length == 24) {
            let num = parseInt(encoded.substring(1), 2);
            if (encoded[0] == "1") {
                num = -num;
            }
            msg.push(num);
            encoded = "";
        }
    }
    return msg;
}

function findWindow(id) {
    for (const win of workspace.windowList()) {
        if (win.destroyed) continue;
        if (win.pid != appPid) continue;
        const msg = decode(win.caption);
        if (msg == null) continue;
        if (msg[5] == id) {
            return win;
        }
    }
    return null;
}

workspace.windowAdded.connect((win) => {
    if (win.pid != appPid) return;
    const frameX = win.frameGeometry.x - win.clientGeometry.x;
    const frameY = win.frameGeometry.y - win.clientGeometry.y;
    const frameW = win.frameGeometry.width - win.clientGeometry.width;
    const frameH = win.frameGeometry.height - win.clientGeometry.height;

    function updateWindow() {
        const msg = decode(win.caption);
        if (msg == null) return;
        const noBorder = msg[4] == 0;
        if (!noBorder) {
            msg[0] += frameX;
            msg[1] += frameY;
            msg[2] += frameW;
            msg[3] += frameH;
        }
        win.frameGeometry = {
            x: msg[0],
            y: msg[1],
            width: msg[2],
            height: msg[3]
        };
        win.skipsCloseAnimation = true;
        win.skipTaskbar = true;
        win.keepAbove = true;
        win.noBorder = noBorder; // "The decision whether a window has a border or not belongs to the window manager." But Rhythm Doctor says otherwise.
        if (win.active) {
            workspace.activeWindow = mainWindow;
        }
        const arrangementCount = msg[6];
        if (arrangementCount > 0) {
            const orderStr = JSON.stringify(msg.slice(6, 6 + arrangementCount + 1));
            if (orderStr != lastOrder) {
                lastOrder = orderStr;
                for (const constraint of lastConstraints) {
                    const bWinReal = findWindow(constraint[0]);
                    const aWinReal = findWindow(constraint[1]);
                    if (bWinReal != null && aWinReal != null) {
                        workspace.unconstrain(bWinReal, aWinReal);
                    }
                }
                lastConstraints.length = 0;
                for (let i = 1; i < arrangementCount; i++) {
                    const bWin = msg[i + 6];
                    const aWin = msg[i + 7];
                    const bWinReal = findWindow(bWin);
                    const aWinReal = findWindow(aWin);
                    if (bWinReal != null && aWinReal != null) {
                        workspace.constrain(bWinReal, aWinReal);
                        lastConstraints.push([bWin, aWin]);
                    }
                }
            }
        }
    }

    win.captionChanged.connect(() => {
        updateWindow();
    });

    win.minimizedChanged.connect(() => {
        win.minimized = false;
    });

    win.interactiveMoveResizeFinished.connect(() => {
        updateWindow();
    });

    updateWindow();
})
    )""";

        return true;
    }

    void removeKWinFile() {
        QFile file(kwinFile);
        file.remove();
    }

    void addKWinScript() {
        if (!createKWinFile()) return;

        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Scripting"),
            QString(),
            QStringLiteral("loadScript")
        );

        QList<QVariant> arguments;
        arguments << QVariant(kwinFile);
        message.setArguments(arguments);

        QDBusMessage reply = QDBusConnection::sessionBus().call(message);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qInfo() << "KWin load script error" << reply.errorMessage();
            QMessageBox::critical(nullptr, "RD Window Dance", "Could not load the KWin script. Please create an issue on GitHub with the logs from the Wine output.");
            removeKWinFile();
            return;
        }

        auto id = reply.arguments().constFirst().toInt();

        message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QString("/Scripting/Script") + QString::number(id),
            QString(),
            QStringLiteral("run")
        );

        reply = QDBusConnection::sessionBus().call(message);

        if (reply.type() == QDBusMessage::ErrorMessage) {
            qInfo() << "KWin run script error" << reply.errorMessage();
            QMessageBox::critical(nullptr, "RD Window Dance", "Could not run the KWin script. Please create an issue on GitHub with the logs from the Wine output.");
        }

        removeKWinFile();
    }

    void startRunning() {
        if (useWayland) {
            addKWinScript();
        }

        appReady = true;

        for (auto screen : this->screens()) {
            screenGeometries[screen] = screen->availableGeometry(); // Temporary until we get actual sizes later (or use Wayland, there these are not used).

            if (!useWayland) {
                ScreenSizeWindow* screenSizeWindow = new ScreenSizeWindow();
                screenSizeWindow->doTheStuff(screen);
            }
        }

        this->exec();
    }
};

CustomApplication* app;

void blockWithError(QString error) {
    qCritical() << "[ERROR]" << error;
    QMetaObject::invokeMethod(app, [error]() {
        QMessageBox::critical(nullptr, "RD Window Dance", error);
    }, Qt::BlockingQueuedConnection);
}

xcb_atom_t getAtom(xcb_connection_t *connection, const char* name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, strlen(name), name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, NULL);
    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

#ifdef WITH_WINE
bool WINAPI CustomGetWindowRect(
    HWND   hWnd,
    LPRECT lpRect
);

void writeJump(void* memory, void* function) {
    DWORD oldProtect;
    VirtualProtect(memory, 12, PAGE_EXECUTE_READWRITE, &oldProtect);

    uint8_t* p = (uint8_t*)memory;
    p[0] = 0x48;
    p[1] = 0xB8;
    *(uint64_t*)(p + 2) = (uint64_t)function;
    p[10] = 0xFF;
    p[11] = 0xE0;

    VirtualProtect(memory, 12, oldProtect, &oldProtect);
}

void hookIntoDLL() {
    HMODULE user32 = GetModuleHandleA("user32.dll");
    void* target = (void*)GetProcAddress(user32, "GetWindowRect");

    writeJump(target, (void*)CustomGetWindowRect);
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);

    RECT rect;
    bool testReturn = GetWindowRect((HWND)0x987, &rect);
    if (!testReturn || rect.left != 123 || rect.top != 456 || rect.right != 789 || rect.bottom != 987) {
        std::cerr << "Error hooking GetWindowRect! Incorrect return values." << std::endl;
    }
}
#endif

void checkForKWinWayland() {
    if (qgetenv("XDG_SESSION_DESKTOP").toLower() != "kde") {
        return;
    }

    if (qgetenv("XDG_SESSION_TYPE").toLower() != "wayland") {
        return;
    }

    useWayland = true;
}

void createApplication() {
    if (createdApplication) return;
    createdApplication = true;
#ifdef KWIN_WAYLAND
    qInfo() << "Compiled with Wayland capabilities. Checking if it is supported.";
    checkForKWinWayland();
#endif
#ifdef WITH_WINE
    hookIntoDLL();
#endif

    qInfo() << "Using KWin Wayland:" << useWayland;
    
    if (!useWayland) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }

    std::thread([] {
        int argc = 0;
        app = new CustomApplication(argc, {});
        app->startRunning();
    }).detach();
}

#ifdef WITH_WINE
BOOL CALLBACK findMainWindowHandleCallback(HWND hwnd, LPARAM lParam) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    if (rect.bottom > 100) {
        main_window_handle = hwnd;
        return FALSE;
    }
    return TRUE;
}

void findMainWindowHandle() {
    EnumThreadWindows(GetCurrentThreadId(), findMainWindowHandleCallback, 0);
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // fprintf(stderr, "[multiwindow_unity] DllMain();\n");
    }

    return TRUE;
}
#else
void findMainWindowHandle() {
    int currentPid = getpid();

    xcb_connection_t* connection = xcb_connect(NULL, NULL);
    globalXcbConnection = connection;
    xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(connection));
    xcb_screen_t* screen = screens.data;
    xcb_window_t root = screen->root;

    xcb_atom_t clientListAtom = getAtom(connection, "_NET_CLIENT_LIST");
    xcb_atom_t pidAtom = getAtom(connection, "_NET_WM_PID");

    xcb_get_property_cookie_t cookie = xcb_get_property(
        connection,
        0,
        root,
        clientListAtom,
        XCB_ATOM_WINDOW,
        0,
        1024
    );

    xcb_get_property_reply_t* reply = xcb_get_property_reply(connection, cookie, NULL);
    int length = xcb_get_property_value_length(reply) / sizeof(xcb_window_t);

    xcb_window_t* windows = (xcb_window_t*)xcb_get_property_value(reply);
    for (int i = 0; i < length; i++) {
        xcb_window_t window = windows[i];
        xcb_get_property_cookie_t pidCookie = xcb_get_property(
            connection,
            0,
            window,
            pidAtom,
            XCB_ATOM_CARDINAL,
            0,
            1
        );
        xcb_get_property_reply_t *pidReply = xcb_get_property_reply(connection, pidCookie, NULL);
        if (pidReply == nullptr) continue;

        if (xcb_get_property_value_length(pidReply) != 4) continue;

        int pid = *(int*)xcb_get_property_value(pidReply);
        free(pidReply);
        if (pid == currentPid) {
            main_window_handle = window;
            break;
        }
    }

    free(reply);
}
#endif

struct MotifWmHints {
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t  input_mode;
    uint32_t status;
};

// ---- Start of CustomWindow ---- 

CustomWindow::CustomWindow() {
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlag(Qt::WindowStaysOnTopHint); // Does not work on Wayland, have to use JS hack above.
    setWindowFlag(Qt::WindowDoesNotAcceptFocus);
    setWindowFlag(Qt::WindowTransparentForInput);

    this->customId = rand() % 100000;
    this->targetX = 0;
    this->targetY = 0;
    this->targetWidth = 1;
    this->targetHeight = 1;
    this->targetOpacity = 0;

    this->setFixedSize(10, 10);

    // testLabel = new QLabel("Example Text", this);
    // testLabel->setStyleSheet("QLabel { color: white; font-size: 24px; }");
    // testLabel->show();
}

#ifdef WITH_WINE
void CustomWindow::setTexture(ID3D11Resource* resource) {
    if (this->qtImage != nullptr) {
        delete this->qtImage;
    }
    this->qtImage = nullptr;
    this->stagingTexture = nullptr;
    this->resource = resource;
    this->texture = nullptr;
    resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);
    texture->GetDesc(&desc);

    this->qtImage = new QImage(desc.Width, desc.Height, QImage::Format_ARGB32_Premultiplied);
    
    stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;
    
    HRESULT returnCode;
    
    returnCode = app->device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (returnCode != 0) {
        std::cerr << "CreateTexture2D ERROR: " << std::hex << returnCode << std::dec << std::endl;
    }

    this->copyTexture();
}

void CustomWindow::copyTexture() {
    if (!isVisible) return;
    ID3D11DeviceContext* ctx = NULL;
    app->device->GetImmediateContext(&ctx);
    ctx->CopyResource(stagingTexture, texture);
    HRESULT returnCode = ctx->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (returnCode != 0) {
        std::cerr << "Map ERROR: " << std::hex << returnCode << std::dec << std::endl;
    }

    int bytesPerLine = qtImage->bytesPerLine();
    int height = desc.Height;
    uchar* startingBits = qtImage->bits();
    uchar* source = (uchar*)mapped.pData;

    for (int i = 0; i < height; i++) {
        int invertedY = (height - i - 1);
        memcpy(startingBits + i * bytesPerLine, source + invertedY * bytesPerLine, bytesPerLine);
    }

    ctx->Unmap(stagingTexture, 0);
    ctx->Release();
}

#else

void CustomWindow::setTexture(GLuint textureId) {
    this->glTextureId = textureId;
}

void CustomWindow::setTextureSize(int w, int h) {
    this->qtImage = new QImage(w, h, QImage::Format_RGBA8888_Premultiplied);
    SAFE_FREE(this->tempTexture);
    tempTexture = malloc(w * h * 4);
}

void CustomWindow::copyTexture() {
    glBindTexture(GL_TEXTURE_2D, this->glTextureId);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempTexture);
    
    int bytesPerLine = qtImage->bytesPerLine();
    int height = qtImage->height();
    uchar* startingBits = qtImage->bits();
    uchar* source = (uchar*)tempTexture;
    for (int i = 0; i < height; i++) {
        int invertedY = (height - i - 1);
        memcpy(startingBits + i * bytesPerLine, source + invertedY * bytesPerLine, bytesPerLine);
    }
}

#endif

void CustomWindow::_setX11Decorations(bool hasDecorations) {
    MotifWmHints hints = {
        .flags = 2,
        .decorations = hasDecorations
    };

    xcb_atom_t atom = getAtom(globalXcbConnection, "_MOTIF_WM_HINTS");

    xcb_change_property(
        globalXcbConnection,
        XCB_PROP_MODE_REPLACE,
        window()->winId(),
        atom,
        atom,
        32,
        5,
        &hints
    );

    xcb_flush(globalXcbConnection);
}

void CustomWindow::setTargetMove(int x, int y) {
    this->targetX = x;
    this->targetY = y;
}

void CustomWindow::setTargetSize(int w, int h) {
    this->targetWidth = w;
    this->targetHeight = h;
}

void CustomWindow::updateThings() {
    auto primaryScreen = app->primaryScreen();
    qreal scaling = this->devicePixelRatio();
    auto primaryScreenGeometry = screenGeometries[primaryScreen];
    auto unitedScreenGeometry = screenGeometries.first();
    int smallestHeight = 999999; // KDE window constraints are buggy. Other monitors also have invisible taskbar limits.
    int finalX = this->targetX / scaling;
    int finalY = this->targetY / scaling;
    int finalWidth = this->targetWidth / scaling;
    int finalHeight = this->targetHeight / scaling;
    float finalOpacity = this->targetOpacity;
    bool finalDecorations = targetDecorations;

    if (!useWayland) {
        for (auto g : screenGeometries) {
            unitedScreenGeometry = unitedScreenGeometry.united(g);
            int h = g.height() + primaryScreenGeometry.y();
            if (h < smallestHeight) {
                smallestHeight = h;
            }
        }
    }

    if (finalWidth > 5000) {
        finalWidth = 5000;
    }
    if (finalHeight > 5000) {
        finalHeight = 5000;
    }

#ifdef WITH_WINE
finalX += primaryScreen->geometry().x();
#endif

    if (!useWayland) {
        if (finalX < 0 - finalWidth) {
            finalOpacity = 0;
        }
        if (finalX > unitedScreenGeometry.width()) {
            finalOpacity = 0;
        }
        if (finalY < 0 - finalHeight) {
            finalOpacity = 0;
        }
        if (finalY > smallestHeight) {
            finalOpacity = 0;
        }

        cutoffX = 0;
        cutoffY = 0;

        int titleBarHeight = 30;

        // Offscreen top/left
        if (finalX < 0) {
            finalWidth += finalX;
            cutoffX = finalX;
            finalX = 0;
        }
        if (finalY < primaryScreenGeometry.y() + titleBarHeight) {
            finalDecorations = false;
        }
        if (finalY < primaryScreenGeometry.y()) {
            finalHeight += finalY - primaryScreenGeometry.y();
            cutoffY = finalY - primaryScreenGeometry.y();
            finalY = primaryScreenGeometry.y();
        }

        // Offscreen bottom/right
        int rightEdge = unitedScreenGeometry.width() - finalWidth;
        int bottomEdge = smallestHeight - finalHeight;
        if (finalX > rightEdge) {
            int difference = finalX - rightEdge;
            finalWidth -= difference;
        }
        if (finalY > bottomEdge) {
            int difference = finalY - bottomEdge;
            finalHeight -= difference;
        }

        if (finalWidth < 5) {
            finalOpacity = 0;
            finalWidth = 5;
        }
        if (finalHeight < 5) {
            finalOpacity = 0;
            finalHeight = 5;
        }
    }

    isVisible = finalOpacity > 0;

    if (useWayland && !isVisible) {
        finalY = -5000; // Just put the window far away in Wayland, do not bother with actual opacity.
    }

    // testLabel->setText(QString("Position: %1, %2\nSize: %3 x %4").arg(QString::number(targetX), QString::number(targetY), QString::number(targetWidth), QString::number(targetHeight)));
    // testLabel->setGeometry(0, 0, finalWidth, finalHeight);

    if (useWayland) { // Wayland doesn't support actual window movement. We have to "smuggle" data in the title to the JS plugin.
        std::string encoded = "";
        std::vector<int> smuggledInfo = {finalX, finalY, finalWidth, finalHeight, finalDecorations ? 1 : 0, this->customId, (int)waylandWindowOrder.size()};
        for (auto winId : waylandWindowOrder) {
            smuggledInfo.push_back(winId);
        }
        int i = 0;
        for (auto info : smuggledInfo) {
            std::string infoEncoded = std::bitset<23>(abs(info)).to_string();
            encoded += info >= 0 ? "\u200B" : "\u200C";
            for (auto character : infoEncoded) {
                encoded += character == '0' ? "\u200B" : "\u200C";
            }
            i++;
        }
        this->setWindowTitle(targetTitle + QString::fromStdString(encoded));
    } else {
        this->setFixedSize(finalWidth, finalHeight);
        this->setGeometry(finalX, finalY, finalWidth, finalHeight);
        this->setWindowTitle(targetTitle);
        this->setWindowOpacity(finalOpacity);

        if (finalDecorations != _lastDecorations) {
            this->_lastDecorations = finalDecorations;
            this->_setX11Decorations(finalDecorations);
        }
    }
}

void CustomWindow::paintEvent(QPaintEvent* paintEvent) {
    QPainter painter(this);
    if (!isVisible) return;
    if (this->qtImage == nullptr) {
        return;
    }

    qreal scaling = this->devicePixelRatio();

    painter.drawImage(QRect(
        this->cutoffX,
        this->cutoffY,
        this->targetWidth / scaling,
        this->targetHeight / scaling
    ), *this->qtImage, this->qtImage->rect());
}

void CustomWindow::setIcon(QImage* image) {
    SAFE_DELETE(iconIcon);

    if (image == nullptr) return;

    iconPixmap = QPixmap::fromImage(*image);
    iconIcon = new QIcon(iconPixmap);

    this->setWindowIcon(*iconIcon);
}

void CustomWindow::closeEvent(QCloseEvent* closeEvent) {
    if (!isClosing) {
        closeEvent->ignore();
    }
}

CustomWindow::~CustomWindow() {
#ifdef WITH_WINE
    SAFE_DELETE(this->qtImage);
#else
    SAFE_FREE(this->tempTexture);
#endif

    setIcon(nullptr);

    // For some reason this segfaults often:
    // SAFE_RELEASE(this->texture);
    // SAFE_RELEASE(this->stagingTexture);
    // delete this->testLabel;
}

// ---- End of CustomWindow ---- 

// ---- Start of ScreenSizeWindow ---- 
void ScreenSizeWindow::doTheStuff(QScreen* screen) {
    this->actualScreen = screen;
    this->setWindowTitle("IGNORE THIS WINDOW");
    this->setWindowFlag(Qt::WindowStaysOnBottomHint);
    this->setWindowOpacity(0);
    this->setGeometry(screen->geometry());
    this->show();
    QTimer::singleShot(500, [this] {
        delete this;
    });
}

void ScreenSizeWindow::resizeEvent(QResizeEvent* event) {
    resizeCount++;
    if (resizeCount != 2) return;
    QTimer::singleShot(10, [this] {
        screenGeometries[this->actualScreen] = this->frameGeometry();
        qInfo() << "Screen size is" << screenGeometries[this->actualScreen] << "now" << screenGeometries.size() << "screens";
        this->close();
    });
}
// ---- End of ScreenSizeWindow ---- 


int MAIN_WINDOW_GEOMETRY_SKIP = 0xFEAB12;
void setMainWindowGeometry(int x, int y, int w, int h) {
    if (x != MAIN_WINDOW_GEOMETRY_SKIP) main_window_x = x;
    if (y != MAIN_WINDOW_GEOMETRY_SKIP) main_window_y = y;
    if (w != MAIN_WINDOW_GEOMETRY_SKIP) main_window_width = w;
    if (h != MAIN_WINDOW_GEOMETRY_SKIP) main_window_height = h;

    bool invisible = x < -1500 || y < -1500;

#ifdef WITH_WINE
    if (invisible) {
        SetWindowLongPtr(main_window_handle, GWL_EXSTYLE, GetWindowLongPtr(main_window_handle, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(main_window_handle, 0, 0, LWA_ALPHA);
        return;
    }

    SetWindowLongPtr(main_window_handle, GWL_EXSTYLE, GetWindowLongPtr(main_window_handle, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    SetWindowPos(main_window_handle, NULL, main_window_x, main_window_y, main_window_width, main_window_height, 0);
#else
    uint32_t opacity = invisible ? 0x00000000 : 0xFFFFFFFF;

    xcb_atom_t atom = getAtom(globalXcbConnection, "_NET_WM_WINDOW_OPACITY");

    xcb_change_property(
        globalXcbConnection,
        XCB_PROP_MODE_REPLACE,
        main_window_handle,
        atom,
        XCB_ATOM_CARDINAL,
        32,
        1,
        &opacity
    );

    xcb_flush(globalXcbConnection);
#endif
}

extern "C" WINAPI HANDLE get_main_window() {
    std::cerr << "get_main_window()" << std::endl;
    return MAIN_WINDOW;
}

extern "C" WINAPI const char* refresh_main_window_ptr() {
    std::cerr << "refresh_main_window_ptr()" << std::endl;
    return createString("");
}

extern "C" WINAPI const char* set_window_title(HANDLE window, const char* title) {
    std::cerr << "set_window_title(" << std::hex << window << std::dec << ", " << title << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return createString("");
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(customWindow, [customWindow, title = QString(title)]() {
        customWindow->targetTitle = title;
        customWindow->updateThings();
    }, Qt::QueuedConnection);

    return createString("");
}

extern "C" WINAPI HANDLE __win32_get_hwnd(HANDLE window) {
    // std::cerr << "__win32_get_hwnd(" << std::hex << window << std::dec << ")" << std::endl;
    return window;
}

struct Size {
    int width;
    int height;
};

extern "C" WINAPI Size get_window_size(HANDLE window) {
    // std::cerr << "get_window_size(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        Size size;
        #ifdef WITH_WINE
        size.width = main_window_width;
        size.height = main_window_height;
        #else
        auto cookie = xcb_get_geometry(globalXcbConnection, main_window_handle);
        auto reply = xcb_get_geometry_reply(globalXcbConnection, cookie, NULL);
        size.width = reply->width;
        size.height = reply->height;
        free(reply);
        #endif
        return size;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    Size size;
    size.width = customWindow->targetWidth;
    size.height = customWindow->targetHeight;
    return size;
}

extern "C" WINAPI Size get_view_size(HANDLE window) {
    // std::cerr << "get_view_size(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        Size size;
        #ifdef WITH_WINE
        size.width = main_window_width;
        size.height = main_window_height;
        #else
        auto cookie = xcb_get_geometry(globalXcbConnection, main_window_handle);
        auto reply = xcb_get_geometry_reply(globalXcbConnection, cookie, NULL);
        size.width = reply->width;
        size.height = reply->height;
        free(reply);
        #endif
        return size;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    Size size;
    size.width = customWindow->targetWidth;
    size.height = customWindow->targetHeight;
    return size;
}

extern "C" WINAPI Size get_window_position(HANDLE window) {
    // std::cerr << "get_window_position(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        Size size;
        #ifdef WITH_WINE
        size.width = main_window_x;
        size.height = main_window_y;
        #else
        auto cookie = xcb_get_geometry(globalXcbConnection, main_window_handle);
        auto reply = xcb_get_geometry_reply(globalXcbConnection, cookie, NULL);
        size.width = reply->x;
        size.height = reply->y;
        free(reply);
        #endif
        return size;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    Size size;
    size.width = customWindow->targetX;
    size.height = customWindow->targetY;
    return size;
}

extern "C" WINAPI void set_window_position(HANDLE window, int x, int y) {
    std::cerr << "set_window_position(" << std::hex << window << std::dec << ", " << x << ", " << y << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        setMainWindowGeometry(x, y, MAIN_WINDOW_GEOMETRY_SKIP, MAIN_WINDOW_GEOMETRY_SKIP);
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;

    customWindow->setTargetMove(x, y);
    QMetaObject::invokeMethod(customWindow, [customWindow]() {
        customWindow->updateThings();
    }, Qt::QueuedConnection);
}

extern "C" WINAPI const char* move_window(HANDLE window, int x, int y, int w, int h) {
    if (window == MAIN_WINDOW) {
        std::cerr << "move_window(" << std::hex << window << std::dec << ", " << x << ", " << y << ", " << w << ", " << h << ")" << std::endl;
        setMainWindowGeometry(x, y, w, h);
        return createString("");
    }
    
    CustomWindow* customWindow = (CustomWindow*)window;
    // if (customWindow->targetX != x || customWindow->targetY != y || customWindow->targetWidth != w || customWindow->targetHeight != h) {
    //     std::cerr << "move_window(" << std::hex << window << std::dec << ", " << x << ", " << y << ", " << w << ", " << h << ")" << std::endl;
    // }
    customWindow->setTargetMove(x, y);
    customWindow->setTargetSize(w, h);
    QMetaObject::invokeMethod(customWindow, [customWindow]() {
        customWindow->updateThings();
    }, Qt::QueuedConnection);
    return createString("");
}

extern "C" WINAPI const char* move_window_to_top(HANDLE window) {
    std::cerr << "move_window_to_top(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return createString("");
    }

    return createString("");
}

extern "C" WINAPI void set_window_size(HANDLE window, int w, int h) {
    std::cerr << "set_window_size(" << std::hex << window << std::dec << ", " << w << ", " << h << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        setMainWindowGeometry(MAIN_WINDOW_GEOMETRY_SKIP, MAIN_WINDOW_GEOMETRY_SKIP, w, h);
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->setTargetSize(w, h);
    QMetaObject::invokeMethod(app, [customWindow]() {
        customWindow->updateThings();
    }, Qt::QueuedConnection);
}

struct FFIResult {
    uint8_t status;
    HANDLE data;
    char* error;
};

extern "C" WINAPI FFIResult new_window(
    LPSTR title,
    int x, int y, int w, int h, bool frameless, bool opaque, bool allowFullscreen) {
    std::cerr << "new_window(title: "
        << title << ", x: "
        << x << ", y: "
        << y << ", w: "
        << w << ", h: "
        << h << ", frameless: "
        << boolToStr(frameless) << ", opaque: "
        << boolToStr(opaque) << ", allowFullscreen: "
        << boolToStr(allowFullscreen) << ")"
        << std::endl;

    CustomWindow* customWindow = nullptr;
    QMetaObject::invokeMethod(app, [&customWindow, title, x, y, w, h, frameless]() {
        customWindow = new CustomWindow();
        customWindow->targetDecorations = !frameless;
        customWindow->targetTitle = title;
        customWindow->setTargetMove(x, y);
        customWindow->setTargetSize(w, h);
        customWindow->updateThings();
        allCustomWindows.push_back(customWindow);
        customWindow->show();
    }, Qt::BlockingQueuedConnection);

    FFIResult result;
    result.status = 1;
    result.data = (void*)customWindow;
    result.error = (char*)"Not an error, I promise";
    return result;
}

extern "C" WINAPI const char* focus_window(HWND window) {
    std::cerr << "focus_window(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        #ifdef WITH_WINE
        SetForegroundWindow(main_window_handle);
        SetActiveWindow(main_window_handle);
        #endif
    }
    return createString("");
}

extern "C" WINAPI bool is_window_focused(HWND window) {
    // std::cerr << "is_window_focused(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
#ifdef WITH_WINE
        return GetActiveWindow() == main_window_handle;
#else
        return true;
#endif
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    return customWindow->isActiveWindow();
}

extern "C" char* set_window_texture(
    HWND window,
    #ifdef WITH_WINE
    HWND texturePtr
    #else
    GLuint texturePtr
    #endif
) {
    std::cerr << "set_window_texture(" << std::hex << window << std::dec << ", " << std::hex << texturePtr << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return createString("");
    }

    
    CustomWindow* customWindow = (CustomWindow*)window;
    #ifdef WITH_WINE
    customWindow->setTexture((ID3D11Resource*)texturePtr);
    #else
    customWindow->setTexture(texturePtr);
    #endif
    return createString("");
}

#ifndef WITH_WINE
extern "C" void set_window_texture_size(
    HWND window,
    int w,
    int h
) {
    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->setTextureSize(w, h);
}
#endif

extern "C" WINAPI FFIResult create_icon(void* buffer, int width) {
    std::cerr << "create_icon(" << width << " width)" << std::endl;
    auto image = new QImage(width, width, QImage::Format_ARGB32);
    int bytesPerLine = image->bytesPerLine();
    uchar* startingBits = image->bits();

    for (int i = 0; i < width; i++) {
        int invertedY = (width - i - 1);
        memcpy(startingBits + i * bytesPerLine, (char*)buffer + invertedY * bytesPerLine, bytesPerLine);
    }

    FFIResult result;
    result.status = 1;
    result.data = (void*)image;
    result.error = (char*)"Not an error, I promise";
    return result;
}

extern "C" WINAPI void destroy_icon(HWND icon) {
    std::cerr << "destroy_icon(" << std::hex << icon << std::dec << ")" << std::endl;
    QImage* image = (QImage*)icon;
    delete image;
}

extern "C" WINAPI void set_window_icon(HWND window, HWND icon) {
    std::cerr << "set_window_icon(" << std::hex << window << std::dec << ", " << std::hex << icon << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->setIcon((QImage*)icon);
}

extern "C" WINAPI const char* enable_input(HWND window) {
    std::cerr << "enable_input(" << std::hex << window << std::dec << ")" << std::endl;
    return createString("");
}

extern "C" WINAPI const char* disable_inptut(HWND window) {
    std::cerr << "disable_inptut(" << std::hex << window << std::dec << ")" << std::endl;
    return createString("");
}

struct SamplerConfig {
    uint8_t windowFilter;
};

extern "C" WINAPI const char* set_window_sampler_config(HWND window, SamplerConfig config) {
    std::cerr << "set_window_sampler_config(" << std::hex << window << std::dec << ")" << std::endl;
    return createString("");
}

struct KeyDownData {
    int key;
    bool repeat;
};

struct NativeWindowEvent {
    uint16_t type;
    void* data;
};

extern "C" WINAPI bool pop_event(HWND window, NativeWindowEvent* event) {
    // std::cerr << "pop_event(" << std::hex << window << std::dec << ")" << std::endl;
    event->type = 0;
    event->data = 0;
    return false;
}

extern "C" WINAPI void destroy_window(HWND window) {
    std::cerr << "destroy_window(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return;
    }
    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(app, [customWindow]() {
        customWindow->isClosing = true;
        if (!useWayland) {
            customWindow->setWindowOpacity(0);
        }
        customWindow->close();
        allCustomWindows.erase(std::find(allCustomWindows.begin(), allCustomWindows.end(), customWindow));
        delete customWindow;
    }, Qt::QueuedConnection);
}


extern "C" WINAPI void present_window(HWND window) {
    // std::cerr << "present_window(" << std::hex << window << std::dec << ")" << std::endl;
}



extern "C" WINAPI const char* show_window(HWND window) {
    std::cerr << "show_window(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return createString("");
    }
    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(app, [customWindow]() {
        customWindow->targetOpacity = 1;
        customWindow->updateThings();
    }, Qt::QueuedConnection);
    return createString("");
}

extern "C" WINAPI const char* hide_window(HWND window) {
    std::cerr << "hide_window(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return createString("");
    }
    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(app, [customWindow]() {
        customWindow->targetOpacity = 0;
        customWindow->updateThings();
    }, Qt::QueuedConnection);
    return createString("");
}

void arrangeWindowsX11(HWND* windows, int count) {
    std::vector<CustomWindow*> windowList;
    for (int i = 0; i < count; i++) {
        if (windows[i] == MAIN_WINDOW) continue;
        windowList.push_back((CustomWindow*)windows[i]);
    }

    for (int i = 0; i < windowList.size() - 1; i++) {
        WId configOptions[] = { windowList[i + 1]->window()->winId(), XCB_STACK_MODE_BELOW };
        xcb_configure_window(
            globalXcbConnection,
            windowList[i]->window()->winId(),
            XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
            configOptions
        );
    }

    xcb_flush(globalXcbConnection);
}

void arrangeWindowsWayland(HWND* windows, int count) {
    std::vector<CustomWindow*> windowList;
    for (int i = 0; i < count; i++) {
        if (windows[i] == MAIN_WINDOW) continue;
        windowList.push_back((CustomWindow*)windows[i]);
    }

    waylandWindowOrder.resize(windowList.size());
    for (int i = 0; i < windowList.size(); i++) {
        waylandWindowOrder[i] = windowList[windowList.size() - i - 1]->customId;
    }
}

extern "C" WINAPI void arrange_windows(HWND* windows, int count) {
    if (useWayland) {
        arrangeWindowsWayland(windows, count);
    } else {
        arrangeWindowsX11(windows, count);
    }
}

static void UNITY_INTERFACE_API render(int eventID) {
    // std::cerr << "render(" << eventID << ")" << std::endl;
    for (auto customWindow : allCustomWindows) {
        customWindow->copyTexture();
        QMetaObject::invokeMethod(customWindow, qOverload<>(&QWidget::repaint), Qt::QueuedConnection);
    }
}

extern "C" WINAPI void* get_render_event_func() {
    // std::cerr << "get_render_event_func()" << std::endl;
    return (void*)render;
}

extern "C" WINAPI bool get_window_fullscreen(HWND window) {
    std::cerr << "get_window_fullscreen(" << std::hex << window << std::dec << ")" << std::endl;
    return false;
}

extern "C" WINAPI bool set_window_fullscreen(HWND window, bool mode) {
    std::cerr << "set_window_fullscreen(" << std::hex << window << std::dec << ", " << boolToStr(mode) << ")" << std::endl;
    return false; // Unused?
}

extern "C" WINAPI void set_window_frame_visible(HWND window, bool visible) {
    std::cerr << "set_window_frame_visible(" << std::hex << window << std::dec << ", " << boolToStr(visible) << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->targetDecorations = visible;
    QMetaObject::invokeMethod(app, [customWindow]() {
        customWindow->updateThings();
    }, Qt::QueuedConnection);
}

#ifndef WITH_WINE
struct NativeMonitor {
    int X;
    int Y;
    int Width;
    int Height;
    int Scale;
};

struct NativeMonitors {
    NativeMonitor* monitors;
    int count;
};

extern "C" NativeMonitors get_monitors() {
    NativeMonitors monitors;
    auto screens = app->screens();
    monitors.count = screens.size();
    monitors.monitors = (NativeMonitor*)malloc(sizeof(NativeMonitor) * monitors.count);
    for (int i = 0; i < monitors.count; i++) {
        QScreen* screen = screens[i];
        QRect geometry = screen->geometry();
        monitors.monitors[i] = {
            geometry.x(),
            geometry.y(),
            geometry.width(),
            geometry.height(),
            100
        };
    }
    return monitors;
}
#endif

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_RendererType = kUnityGfxRendererNull;

static void UNITY_INTERFACE_API
    OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
    switch (eventType)
    {
        case kUnityGfxDeviceEventInitialize:
        {
            s_RendererType = s_Graphics->GetRenderer();
#ifdef WITH_WINE
            if (s_RendererType != kUnityGfxRendererD3D11) {
                blockWithError("Only D3D11 is supported.");
                return;
            }
            IUnityGraphicsD3D11* d3d = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
            if (d3d == nullptr) return;
            app->device = d3d->GetDevice();
#else
            if (s_RendererType != kUnityGfxRendererOpenGLCore) {
                blockWithError("Only OpenGL Core is supported.");
                return;
            }
#endif
            break;
        }
        case kUnityGfxDeviceEventShutdown:
        {
            s_RendererType = kUnityGfxRendererNull;
            break;
        }
        case kUnityGfxDeviceEventBeforeReset:
        {
            break;
        }
        case kUnityGfxDeviceEventAfterReset:
        {
            break;
        }
    };
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    findMainWindowHandle();
    createApplication();
    while (!appReady) usleep(100);

    s_UnityInterfaces = unityInterfaces;
    s_Graphics = unityInterfaces->Get<IUnityGraphics>();
    
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
    
    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    // to not miss the event in case the graphics device is already initialized
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

#ifdef WITH_WINE
bool WINAPI CustomGetWindowRect(
    HWND   hWnd,
    LPRECT lpRect
) {
    // std::cerr << "CustomGetWindowRect " << std::hex << hWnd << std::dec << std::endl;
    if (hWnd == (HWND)0x987) {
        lpRect->left = 123;
        lpRect->top = 456;
        lpRect->right = 789;
        lpRect->bottom = 987;
        return true;
    }

    if (hWnd == MAIN_WINDOW) {
        lpRect->left = main_window_x;
        lpRect->top = main_window_y;
        lpRect->right = main_window_x + main_window_width;
        lpRect->bottom = main_window_y + main_window_height;
        return true;
    }

    CustomWindow* customWindow = (CustomWindow*)hWnd;
    if (std::find(allCustomWindows.begin(), allCustomWindows.end(), customWindow) == allCustomWindows.end()) {
        // Not found. Not our window.
        WINDOWINFO windowInfo;
        bool response = GetWindowInfo(hWnd, &windowInfo);
        *lpRect = windowInfo.rcWindow;
        return response;
    }

    lpRect->left = customWindow->targetX;
    lpRect->top = customWindow->targetY;
    lpRect->right = customWindow->targetX + customWindow->targetWidth;
    lpRect->bottom = customWindow->targetY + customWindow->targetHeight;
    return true;
}
#endif
