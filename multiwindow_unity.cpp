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

void* MAIN_WINDOW = (void*)0x12345;
bool createdApplication = false;

void createApplication() {
    if (createdApplication) return;
    createdApplication = true;
    qputenv("QT_QPA_PLATFORM", "xcb");

    std::cerr << "Create new application" << std::endl;
    int argc = 0;
    QApplication* app = new QApplication(argc, {});
    app->setQuitOnLastWindowClosed(false);
    std::thread([&app] {
        std::cerr << "Executing application" << std::endl;
        QEventLoop eventLoop;
        eventLoop.exec(QEventLoop::ApplicationExec);
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

    CustomWindow() {
        this->targetX = 0;
        this->targetY = 0;
        this->targetWidth = 1;
        this->targetHeight = 1;
        this->targetOpacity = 1;
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
        int finalOpacity = this->targetOpacity;

        if (finalWidth < 2) {
            finalOpacity = 0;
            finalWidth = 2;
        }
        if (finalHeight < 2) {
            finalOpacity = 0;
            finalHeight = 2;
        }

        this->setGeometry(finalX, finalY, finalWidth, finalHeight);
        this->setWindowOpacity(finalOpacity);
    }
};

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
    customWindow->setWindowTitle(title);

    return unused;
}

extern "C" WINAPI HANDLE __win32_get_hwnd(HANDLE window) {
    std::cerr << "__win32_get_hwnd(" << std::hex << window << std::dec << ")" << std::endl;
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
        size.width = 150;
        size.height = 150;
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
    Size size;
    size.width = 1280;
    size.height = 720;
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
    customWindow->setTargetMove(x, y);
    customWindow->updateThings();
}

extern "C" WINAPI const char* move_window(HANDLE window, int x, int y, int w, int h) {
    // std::cerr << "move_window(" << std::hex << window << std::dec << ", " << x << ", " << y << ", " << w << ", " << h << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return "";
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->setTargetMove(x, y);
    customWindow->setTargetSize(w, h);
    customWindow->updateThings();
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
    customWindow->setTargetSize(w, h);
    customWindow->updateThings();
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
    
    CustomWindow* customWindow = new CustomWindow();
    customWindow->setWindowTitle(title);
    customWindow->setTargetMove(x, y);
    customWindow->setTargetSize(w, h);
    customWindow->updateThings();
    customWindow->show();

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
    customWindow->setWindowOpacity(0);
    customWindow->close();
    delete customWindow;
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
    customWindow->targetOpacity = 1;
    customWindow->updateThings();
    return "";
}

extern "C" WINAPI const char* hide_window(HWND window) {
    std::cerr << "hide_window(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return "";
    }
    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->targetOpacity = 0;
    customWindow->updateThings();
    return "";
}

extern "C" WINAPI void arrange_windows(HWND* windows, int count) {
    // std::cerr << "arrange_windows(" << count << ")" << std::endl;
}

void __stdcall render(int eventID) {
    // std::cerr << "render(" << eventID << ")" << std::endl;
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
