#include <windef.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#undef _WIN32
#undef WIN32
#undef __WIN32__
#undef WIN64
#undef _WIN64
#undef __WIN64__
#undef WINAPI_FAMILY
#undef __NT__
#include <QtWidgets>
#include <unistd.h>
#include <thread>
#include <xcb/xcb.h>

void* MAIN_WINDOW = (void*)0x12345;
bool createdApplication = false;

void updateAll();

class CustomApplication : public QApplication {
public:
    CustomApplication(int &argc, char** argv) : QApplication(argc, argv) {
        this->setQuitOnLastWindowClosed(false);
    }

    void updateCustom() {
        updateAll();
    }

    void startRunning() {
        // QTimer* updateTimer = new QTimer();
        // connect(updateTimer, &QTimer::timeout, this, QOverload<>::of(&CustomApplication::updateCustom));
        // updateTimer->start(10);

        this->exec();
    }
};

CustomApplication* app;

void createApplication() {
    if (createdApplication) return;
    createdApplication = true;
    qputenv("QT_QPA_PLATFORM", "xcb");

    std::thread([] {
        int argc = 0;
        app = new CustomApplication(argc, {});
        app->startRunning();
    }).detach();
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved) {
    // fprintf(stderr, "[multiwindow_unity] DllMain();\n");
    if (reason == DLL_PROCESS_ATTACH) {
        createApplication();
    }

    return TRUE;
}


    
class CustomWindow : public QWidget {
public:
    int targetX;
    int targetY;
    int targetWidth;
    int targetHeight;
    float targetOpacity;
    QLabel* testLabel;

    CustomWindow() {
        this->targetX = 0;
        this->targetY = 0;
        this->targetWidth = 1;
        this->targetHeight = 1;
        this->targetOpacity = 1;

        testLabel = new QLabel("Example Text", this);
        testLabel->setStyleSheet("QLabel { color: white; font-size: 24px; }");
        testLabel->show();
    }

    void setTargetMove(int x, int y) {
        this->targetX = x;
        this->targetY = y;
    }

    void setTargetSize(int w, int h) {
        this->targetWidth = w;
        this->targetHeight = h;
    }

    void updateThings() {
        int finalX = this->targetX;
        int finalY = this->targetY;
        int finalWidth = this->targetWidth;
        int finalHeight = this->targetHeight;
        float finalOpacity = this->targetOpacity;

        if (finalWidth > 5000) {
            finalWidth = 5000;
        }
        if (finalHeight > 5000) {
            finalHeight = 5000;
        }

        if (finalX < 0 - finalWidth) {
            finalOpacity = 0;
        }
        if (finalX > 50000) { // TODO: Use screen size
            finalOpacity = 0;
        }
        if (finalY < 0 - finalHeight) {
            finalOpacity = 0;
        }
        if (finalY > 50000) { // TODO: Use screen size
            finalOpacity = 0;
        }
        if (finalWidth < 2) {
            finalOpacity = 0;
            finalWidth = 2;
        }
        if (finalHeight < 2) {
            finalOpacity = 0;
            finalHeight = 2;
        }

        testLabel->setText(QString("Position: %1, %2\nSize: %3 x %4").arg(QString::number(finalX), QString::number(finalY), QString::number(finalWidth), QString::number(finalHeight)));
        testLabel->setGeometry(0, 0, finalWidth, finalHeight);

        this->setGeometry(finalX, finalY, finalWidth, finalHeight);
        this->setWindowOpacity(finalOpacity);
    }

    ~CustomWindow() {
        delete this->testLabel;
    }
};

std::vector<CustomWindow*> allCustomWindows;
std::mutex customWindowMutex;

void updateAll() {
    std::cerr << "updateAll" << std::endl;
}

std::string boolToStr(bool value) {
    return value ? "true" : "false";
}

extern "C" WINAPI HANDLE get_main_window() {
    std::cerr << "get_main_window()" << std::endl;
    return MAIN_WINDOW;
}

const char *unused = "Unused";

extern "C" WINAPI const char* refresh_main_window_ptr() {
    std::cerr << "refresh_main_window_ptr()" << std::endl;
    return unused;
}

extern "C" WINAPI const char* set_window_title(HANDLE window, char* title) {
    std::cerr << "set_window_title(" << std::hex << window << std::dec << ", " << title << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return unused;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(customWindow, [customWindow, title]() {
        customWindow->setWindowTitle(title);
    }, Qt::QueuedConnection);

    return unused;
}

extern "C" WINAPI HANDLE __win32_get_hwnd(HANDLE window) {
    // std::cerr << "__win32_get_hwnd(" << std::hex << window << std::dec << ")" << std::endl;
    return (void*)0xDEADBEEF;
}

struct Size {
    int width;
    int height;
};

extern "C" WINAPI Size get_window_size(HANDLE window) {
    // std::cerr << "get_window_size(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        Size size;
        size.width = 700;
        size.height = 400;
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
        size.width = 700;
        size.height = 400;
        return size;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    Size size;
    size.width = customWindow->targetWidth;
    size.height = customWindow->targetHeight;
    return size;
}

extern "C" WINAPI Size get_window_position(HANDLE window) {
    std::cerr << "get_window_position(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        Size size;
        size.width = 150;
        size.height = 150;
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
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;

    QMetaObject::invokeMethod(customWindow, [customWindow, x, y]() {
        customWindow->setTargetMove(x, y);
        customWindow->updateThings();
    }, Qt::QueuedConnection);
}

extern "C" WINAPI const char* move_window(HANDLE window, int x, int y, int w, int h) {
    if (window == MAIN_WINDOW) {
        std::cerr << "move_window(" << std::hex << window << std::dec << ", " << x << ", " << y << ", " << w << ", " << h << ")" << std::endl;
        return "";
    }
    
    CustomWindow* customWindow = (CustomWindow*)window;
    if (customWindow->targetX != x || customWindow->targetY != y || customWindow->targetWidth != w || customWindow->targetHeight != h) {
        std::cerr << "move_window(" << std::hex << window << std::dec << ", " << x << ", " << y << ", " << w << ", " << h << ")" << std::endl;
    }
    QMetaObject::invokeMethod(customWindow, [customWindow, x, y, w, h]() {
        customWindow->setTargetMove(x, y);
        customWindow->setTargetSize(w, h);
        customWindow->updateThings();
    }, Qt::QueuedConnection);
    return "";
}

extern "C" WINAPI const char* move_window_to_top(HANDLE window) {
    std::cerr << "move_window_to_top(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return "";
    }

    return "";
}

extern "C" WINAPI void set_window_size(HANDLE window, int w, int h) {
    std::cerr << "set_window_size(" << std::hex << window << std::dec << ", " << w << ", " << h << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(app, [customWindow, w, h]() {
        customWindow->setTargetSize(w, h);
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
    QMetaObject::invokeMethod(app, [&customWindow, title, x, y, w, h]() {
        customWindow = new CustomWindow();
        customWindow->setWindowTitle(title);
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
    return "";
}

extern "C" WINAPI bool is_window_focused(HWND window) {
    // std::cerr << "is_window_focused(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return true;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    return customWindow->hasFocus(); // TODO: Check if this actually works
}

extern "C" WINAPI const char* set_window_texture(HWND window, HWND texturePtr) {
    std::cerr << "set_window_texture(" << std::hex << window << std::dec << ", " << std::hex << texturePtr << std::dec << ")" << std::endl;
    return "";
}

extern "C" WINAPI FFIResult create_icon(void* buffer, int size) {
    std::cerr << "create_icon(" << size << " bytes)" << std::endl;
    FFIResult result;
    result.status = 1;
    result.data = (void*)0xdd;
    result.error = (char*)"Not an error, I promise";
    return result;
}

extern "C" WINAPI void destroy_icon(HWND icon) {
    std::cerr << "destroy_icon(" << std::hex << icon << std::dec << ")" << std::endl;
}

extern "C" WINAPI void set_window_icon(HWND window, HWND icon) {
    std::cerr << "set_window_icon(" << std::hex << window << std::dec << ", " << std::hex << icon << std::dec << ")" << std::endl;
}

extern "C" WINAPI const char* enable_input(HWND window) {
    std::cerr << "enable_input(" << std::hex << window << std::dec << ")" << std::endl;
    return "";
}

extern "C" WINAPI const char* disable_inptut(HWND window) {
    std::cerr << "disable_inptut(" << std::hex << window << std::dec << ")" << std::endl;
    return "";
}

struct SamplerConfig {
    uint8_t windowFilter;
};

extern "C" WINAPI const char* set_window_sampler_config(HWND window, SamplerConfig config) {
    std::cerr << "set_window_sampler_config(" << std::hex << window << std::dec << ")" << std::endl;
    return "";
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
        customWindow->setWindowOpacity(0);
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
        return "";
    }
    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(app, [customWindow]() {
        customWindow->targetOpacity = 1;
        customWindow->updateThings();
    }, Qt::QueuedConnection);
    return "";
}

extern "C" WINAPI const char* hide_window(HWND window) {
    std::cerr << "hide_window(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return "";
    }
    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(app, [customWindow]() {
        customWindow->targetOpacity = 0;
        customWindow->updateThings();
    }, Qt::QueuedConnection);
    return "";
}

extern "C" WINAPI void arrange_windows(HWND* windows, int count) {
    auto *x11Application = app->nativeInterface<QNativeInterface::QX11Application>();
    auto connection = x11Application->connection();

    std::vector<CustomWindow*> windowList;
    for (int i = 0; i < count; i++) {
        if (windows[i] == MAIN_WINDOW) continue;
        windowList.push_back((CustomWindow*)windows[i]);
    }

    for (int i = 0; i < windowList.size() - 1; i++) {
        xcb_configure_window(
            connection,
            windowList[i]->window()->winId(),
            XCB_CONFIG_WINDOW_SIBLING | XCB_CONFIG_WINDOW_STACK_MODE,
            (WId[]){ windowList[i + 1]->window()->winId(), XCB_STACK_MODE_BELOW });
    }
}

void __stdcall render(int eventID) {
    // std::cerr << "render(" << eventID << ")" << std::endl;
    // for (auto customWindow : allCustomWindows) {
    //     customWindow->repaint();
    // }
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
}
