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
#include <stdint.h>
#include <iostream>
typedef void* HANDLE;
typedef void* HWND;
typedef bool BOOL;
typedef char* LPSTR;
#define WINAPI
#include "unity/IUnityGraphics.h"
#define VK_NO_PROTOTYPES
#include "unity/IUnityGraphicsVulkan.h"
#include <GL/glew.h>
#include <QtCore/QtCore>
#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtGui/QScreen>
#include <QtGui/QCloseEvent>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QWidget>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <bitset>
#include <unistd.h>
#include <thread>
#include <xcb/xcb.h>
#include "multiwindow_unity.hpp"

#define SAFE_RELEASE(a)                                                                                                \
    if (a) {                                                                                                           \
        a->Release();                                                                                                  \
        a = NULL;                                                                                                      \
    }
#define SAFE_FREE(a)                                                                                                   \
    if (a) {                                                                                                           \
        free(a);                                                                                                       \
        a = NULL;                                                                                                      \
    }
#define SAFE_DELETE(a)                                                                                                 \
    if (a) {                                                                                                           \
        delete a;                                                                                                      \
        a = NULL;                                                                                                      \
    }

enum WaylandType { None, KDE, Hyprland };

QString kwinFile = "/tmp/multiwindow_unity_kwin.js";
void* MAIN_WINDOW = (void*)0x12345;
#ifdef WITH_WINE
HWND main_window_handle = NULL;
#else
xcb_window_t main_window_handle = 0;
#endif
int main_window_x = 0;
int main_window_y = 0;
int main_window_width = 800;
int main_window_height = 600;
bool createdApplication = false;
bool appReady = false;
WaylandType waylandType = WaylandType::None;
bool needsX11Cutoff = true;
#ifdef WITH_WINE
bool usingWine = true;
#else
bool usingWine = false;
#endif
bool unsupportedDE = false;
bool forceDisabledWayland = false;
bool hyprlandError = false;
// Need to use this hack instead of using actual "availableGeometry".
// Maximizing invisible windows (ScreenSizeWindow) is a more reliable
// method of getting the actual screen size without the taskbar.
QMap<QScreen*, QRect> screenGeometries;
int framesSinceReorder = 0;

std::vector<CustomWindow*> allCustomWindows;
QMutex customWindowMutex;
std::vector<WId> storedWindowOrder;

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;
static UnityGfxRenderer s_RendererType = kUnityGfxRendererNull;
static bool s_DebugLoggingEnabled = false;
#ifndef WITH_WINE
static IUnityGraphicsVulkan* s_GraphicsVulkan = NULL;
static PFN_vkGetPhysicalDeviceMemoryProperties s_vkGetPhysicalDeviceMemoryProperties = nullptr;
static PFN_vkCreateBuffer s_vkCreateBuffer = nullptr;
static PFN_vkDestroyBuffer s_vkDestroyBuffer = nullptr;
static PFN_vkGetBufferMemoryRequirements s_vkGetBufferMemoryRequirements = nullptr;
static PFN_vkAllocateMemory s_vkAllocateMemory = nullptr;
static PFN_vkFreeMemory s_vkFreeMemory = nullptr;
static PFN_vkBindBufferMemory s_vkBindBufferMemory = nullptr;
static PFN_vkMapMemory s_vkMapMemory = nullptr;
static PFN_vkUnmapMemory s_vkUnmapMemory = nullptr;
static PFN_vkInvalidateMappedMemoryRanges s_vkInvalidateMappedMemoryRanges = nullptr;
static PFN_vkCmdCopyImageToBuffer s_vkCmdCopyImageToBuffer = nullptr;
static PFN_vkCmdPipelineBarrier s_vkCmdPipelineBarrier = nullptr;
static constexpr int kRenderEventVulkanCopy = 1;
#endif

xcb_connection_t* globalXcbConnection;
Hyprctl* hyprctl = nullptr;

std::string boolToStr(bool value) { return value ? "true" : "false"; }

bool isTruthyEnvValue(QByteArray value) {
    value = value.trimmed().toLower();
    return !value.isEmpty() && value != "0" && value != "false" && value != "off" && value != "no";
}

const char* rendererTypeToString(UnityGfxRenderer rendererType) {
    switch (rendererType) {
    case kUnityGfxRendererOpenGLCore:
        return "OpenGLCore";
    case kUnityGfxRendererVulkan:
        return "Vulkan";
    case kUnityGfxRendererD3D11:
        return "D3D11";
    case kUnityGfxRendererNull:
        return "Null";
    default:
        return "Other";
    }
}

QString samplePixelSummary(const unsigned char* pixels, int width, int height) {
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return "pixels=null";
    }

    auto sample = [&](int x, int y) {
        x = std::max(0, std::min(x, width - 1));
        y = std::max(0, std::min(y, height - 1));
        int index = (y * width + x) * 4;
        return QString("(%1,%2,%3,%4)")
            .arg((int)pixels[index + 0])
            .arg((int)pixels[index + 1])
            .arg((int)pixels[index + 2])
            .arg((int)pixels[index + 3]);
    };

    return QString("tl=%1 center=%2 br=%3")
        .arg(sample(0, 0))
        .arg(sample(width / 2, height / 2))
        .arg(sample(width - 1, height - 1));
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

static inline void requestWindowUpdate(CustomWindow* customWindow) {
    if (customWindow == nullptr) {
        return;
    }

    if (customWindow->isClosing || !customWindow->isVisible || customWindow->targetOpacity <= 0 ||
        customWindow->targetWidth <= 0 || customWindow->targetHeight <= 0 || customWindow->qtImage == nullptr) {
        return;
    }

    bool updateWasAlreadyQueued = customWindow->updateQueued.exchange(true, std::memory_order_acq_rel);
    if (updateWasAlreadyQueued) {
        return;
    }

    QMetaObject::invokeMethod(customWindow, qOverload<>(&QWidget::update), Qt::QueuedConnection);
}

#ifndef WITH_WINE
static void loadVulkanFunctions(const UnityVulkanInstance& instance) {
    auto getProc = [&](const char* name) -> PFN_vkVoidFunction {
        if (instance.getInstanceProcAddr == nullptr) {
            return nullptr;
        }
        return instance.getInstanceProcAddr(instance.instance, name);
    };

    s_vkGetPhysicalDeviceMemoryProperties =
        (PFN_vkGetPhysicalDeviceMemoryProperties)getProc("vkGetPhysicalDeviceMemoryProperties");
    s_vkCreateBuffer = (PFN_vkCreateBuffer)getProc("vkCreateBuffer");
    s_vkDestroyBuffer = (PFN_vkDestroyBuffer)getProc("vkDestroyBuffer");
    s_vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)getProc("vkGetBufferMemoryRequirements");
    s_vkAllocateMemory = (PFN_vkAllocateMemory)getProc("vkAllocateMemory");
    s_vkFreeMemory = (PFN_vkFreeMemory)getProc("vkFreeMemory");
    s_vkBindBufferMemory = (PFN_vkBindBufferMemory)getProc("vkBindBufferMemory");
    s_vkMapMemory = (PFN_vkMapMemory)getProc("vkMapMemory");
    s_vkUnmapMemory = (PFN_vkUnmapMemory)getProc("vkUnmapMemory");
    s_vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)getProc("vkInvalidateMappedMemoryRanges");
    s_vkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)getProc("vkCmdCopyImageToBuffer");
    s_vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)getProc("vkCmdPipelineBarrier");
}

static uint32_t findVulkanMemoryType(uint32_t typeBits, VkMemoryPropertyFlags propertyFlags) {
    if (s_GraphicsVulkan == nullptr || s_vkGetPhysicalDeviceMemoryProperties == nullptr) {
        return UINT32_MAX;
    }

    UnityVulkanInstance instance = s_GraphicsVulkan->Instance();
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    s_vkGetPhysicalDeviceMemoryProperties(instance.physicalDevice, &memoryProperties);
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) == 0) {
            continue;
        }
        if ((memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
            return i;
        }
    }
    return UINT32_MAX;
}

static void releaseVulkanReadbackResources(CustomWindow* customWindow) {
    if (customWindow == nullptr) {
        return;
    }

    if (s_GraphicsVulkan != nullptr) {
        UnityVulkanInstance instance = s_GraphicsVulkan->Instance();
        if (instance.device != VK_NULL_HANDLE) {
            VkDeviceMemory memory = (VkDeviceMemory)customWindow->vulkanReadbackMemory;
            VkBuffer buffer = (VkBuffer)customWindow->vulkanReadbackBuffer;
            if (memory != VK_NULL_HANDLE && customWindow->vulkanReadbackMapped != nullptr && s_vkUnmapMemory != nullptr) {
                s_vkUnmapMemory(instance.device, memory);
            }
            if (buffer != VK_NULL_HANDLE && s_vkDestroyBuffer != nullptr) {
                s_vkDestroyBuffer(instance.device, buffer, nullptr);
            }
            if (memory != VK_NULL_HANDLE && s_vkFreeMemory != nullptr) {
                s_vkFreeMemory(instance.device, memory, nullptr);
            }
        }
    }

    customWindow->vulkanReadbackBuffer = nullptr;
    customWindow->vulkanReadbackMemory = nullptr;
    customWindow->vulkanReadbackMapped = nullptr;
    customWindow->vulkanReadbackSize = 0;
    customWindow->vulkanReadbackHostCoherent = false;
    customWindow->vulkanReadbackWidth = 0;
    customWindow->vulkanReadbackHeight = 0;
    customWindow->vulkanReadbackSwapRedBlue = false;
    customWindow->vulkanReadbackSubmittedFrame = 0;
    customWindow->vulkanReadbackPending = false;
}

static bool ensureVulkanReadbackResources(CustomWindow* customWindow, int width, int height) {
    if (customWindow == nullptr || s_GraphicsVulkan == nullptr || customWindow->qtImage == nullptr || width <= 0 || height <= 0 ||
        s_vkCreateBuffer == nullptr || s_vkGetBufferMemoryRequirements == nullptr || s_vkAllocateMemory == nullptr ||
        s_vkBindBufferMemory == nullptr || s_vkMapMemory == nullptr) {
        return false;
    }

    size_t requiredSize = (size_t)width * (size_t)height * 4;
    if (customWindow->vulkanReadbackBuffer != nullptr && customWindow->vulkanReadbackMapped != nullptr &&
        customWindow->vulkanReadbackSize == requiredSize && customWindow->vulkanReadbackWidth == width &&
        customWindow->vulkanReadbackHeight == height) {
        return true;
    }

    releaseVulkanReadbackResources(customWindow);

    UnityVulkanInstance instance = s_GraphicsVulkan->Instance();
    if (instance.device == VK_NULL_HANDLE) {
        return false;
    }

    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = requiredSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer = VK_NULL_HANDLE;
    if (s_vkCreateBuffer(instance.device, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS) {
        qWarning() << "[TextureDebug] Failed to create Vulkan readback buffer for window" << customWindow->customId;
        return false;
    }

    VkMemoryRequirements requirements = {};
    s_vkGetBufferMemoryRequirements(instance.device, buffer, &requirements);

    VkMemoryPropertyFlags preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t memoryTypeIndex = findVulkanMemoryType(requirements.memoryTypeBits, preferredFlags);
    bool hostCoherent = true;
    if (memoryTypeIndex == UINT32_MAX) {
        preferredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        hostCoherent = false;
        memoryTypeIndex = findVulkanMemoryType(requirements.memoryTypeBits, preferredFlags);
    }
    if (memoryTypeIndex == UINT32_MAX) {
        qWarning() << "[TextureDebug] Failed to find Vulkan host-visible memory type for window" << customWindow->customId;
        if (s_vkDestroyBuffer != nullptr) {
            s_vkDestroyBuffer(instance.device, buffer, nullptr);
        }
        return false;
    }

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory memory = VK_NULL_HANDLE;
    if (s_vkAllocateMemory(instance.device, &allocateInfo, nullptr, &memory) != VK_SUCCESS) {
        qWarning() << "[TextureDebug] Failed to allocate Vulkan readback memory for window" << customWindow->customId;
        if (s_vkDestroyBuffer != nullptr) {
            s_vkDestroyBuffer(instance.device, buffer, nullptr);
        }
        return false;
    }

    if (s_vkBindBufferMemory(instance.device, buffer, memory, 0) != VK_SUCCESS) {
        qWarning() << "[TextureDebug] Failed to bind Vulkan readback memory for window" << customWindow->customId;
        if (s_vkFreeMemory != nullptr) {
            s_vkFreeMemory(instance.device, memory, nullptr);
        }
        if (s_vkDestroyBuffer != nullptr) {
            s_vkDestroyBuffer(instance.device, buffer, nullptr);
        }
        return false;
    }

    void* mapped = nullptr;
    if (s_vkMapMemory(instance.device, memory, 0, requirements.size, 0, &mapped) != VK_SUCCESS || mapped == nullptr) {
        qWarning() << "[TextureDebug] Failed to map Vulkan readback memory for window" << customWindow->customId;
        if (s_vkFreeMemory != nullptr) {
            s_vkFreeMemory(instance.device, memory, nullptr);
        }
        if (s_vkDestroyBuffer != nullptr) {
            s_vkDestroyBuffer(instance.device, buffer, nullptr);
        }
        return false;
    }

    customWindow->vulkanReadbackBuffer = (void*)buffer;
    customWindow->vulkanReadbackMemory = (void*)memory;
    customWindow->vulkanReadbackMapped = mapped;
    customWindow->vulkanReadbackSize = requiredSize;
    customWindow->vulkanReadbackHostCoherent = hostCoherent;
    customWindow->vulkanReadbackWidth = width;
    customWindow->vulkanReadbackHeight = height;
    customWindow->vulkanReadbackPending = false;
    return true;
}

static bool processCompletedVulkanReadback(CustomWindow* customWindow, unsigned long long safeFrameNumber) {
    if (customWindow == nullptr || !customWindow->textureUsesVulkan || !customWindow->vulkanReadbackPending ||
        customWindow->qtImage == nullptr || customWindow->vulkanReadbackMapped == nullptr ||
        safeFrameNumber < customWindow->vulkanReadbackSubmittedFrame) {
        return false;
    }

    if (!customWindow->vulkanReadbackHostCoherent && s_GraphicsVulkan != nullptr && s_vkInvalidateMappedMemoryRanges != nullptr) {
        UnityVulkanInstance instance = s_GraphicsVulkan->Instance();
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = (VkDeviceMemory)customWindow->vulkanReadbackMemory;
        mappedRange.offset = 0;
        mappedRange.size = customWindow->vulkanReadbackSize;
        s_vkInvalidateMappedMemoryRanges(instance.device, 1, &mappedRange);
    }

    int width = customWindow->qtImage->width();
    int height = customWindow->qtImage->height();
    int destinationBytesPerLine = customWindow->qtImage->bytesPerLine();
    int sourceBytesPerLine = width * 4;
    uchar* destination = customWindow->qtImage->bits();
    const uchar* source = (const uchar*)customWindow->vulkanReadbackMapped;
    bool canMemcpyWithoutSwizzle =
        !customWindow->vulkanReadbackSwapRedBlue || customWindow->qtImage->format() == QImage::Format_ARGB32;

    if (canMemcpyWithoutSwizzle && destinationBytesPerLine == sourceBytesPerLine) {
        memcpy(destination, source, (size_t)destinationBytesPerLine * (size_t)height);
    } else {
        for (int y = 0; y < height; ++y) {
            uchar* destinationRow = destination + (size_t)y * (size_t)destinationBytesPerLine;
            const uchar* sourceRow = source + (size_t)y * (size_t)sourceBytesPerLine;
            if (canMemcpyWithoutSwizzle) {
                memcpy(destinationRow, sourceRow, sourceBytesPerLine);
            } else {
                for (int x = 0; x < width; ++x) {
                    destinationRow[x * 4 + 0] = sourceRow[x * 4 + 2];
                    destinationRow[x * 4 + 1] = sourceRow[x * 4 + 1];
                    destinationRow[x * 4 + 2] = sourceRow[x * 4 + 0];
                    destinationRow[x * 4 + 3] = sourceRow[x * 4 + 3];
                }
            }
        }
    }

    // Unity's Vulkan texture memory reaches us in the same bottom-up orientation
    // expected by the existing Qt paint path used for OpenGL/CPU copies.
    // Keep the orientation flag aligned with those paths so paintEvent performs
    // the vertical flip before presenting into the QWidget.
    customWindow->textureDataIsUpsideDown = true;
    customWindow->hasTexturePixels = true;
    customWindow->vulkanReadbackPending = false;
    customWindow->debugLoggedMissingTexture = false;

    if (s_DebugLoggingEnabled && (customWindow->debugCopyCount < 3 || customWindow->debugCopyCount % 120 == 0)) {
        qInfo() << "[TextureDebug] copyTexture(Vulkan) window" << customWindow->customId << "image" << width << "x"
                << height << samplePixelSummary((const unsigned char*)customWindow->qtImage->constBits(), width, height);
    }
    customWindow->debugCopyCount++;
    return true;
}

static bool submitVulkanReadback(CustomWindow* customWindow, const UnityVulkanRecordingState& recordingState) {
    if (customWindow == nullptr || !customWindow->textureUsesVulkan || customWindow->unityVulkanTexture == nullptr ||
        customWindow->qtImage == nullptr || customWindow->vulkanReadbackPending || s_GraphicsVulkan == nullptr ||
        s_vkCmdCopyImageToBuffer == nullptr || s_vkCmdPipelineBarrier == nullptr) {
        return false;
    }

    UnityVulkanImage unityImage = {};
    if (!s_GraphicsVulkan->AccessTexture(customWindow->unityVulkanTexture, UnityVulkanWholeImage,
                                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_ACCESS_TRANSFER_READ_BIT, kUnityVulkanResourceAccess_PipelineBarrier,
                                         &unityImage)) {
        qWarning() << "[TextureDebug] AccessTexture failed for Vulkan window" << customWindow->customId;
        return false;
    }

    int width = (int)unityImage.extent.width;
    int height = (int)unityImage.extent.height;
    if (width <= 0 || height <= 0) {
        return false;
    }

    customWindow->vulkanReadbackSwapRedBlue =
        unityImage.format == VK_FORMAT_B8G8R8A8_UNORM || unityImage.format == VK_FORMAT_B8G8R8A8_SRGB;

    if (customWindow->qtImage == nullptr || customWindow->qtImage->width() != width || customWindow->qtImage->height() != height) {
        customWindow->setTextureSize(width, height);
    } else if (customWindow->vulkanReadbackSwapRedBlue && customWindow->qtImage->format() != QImage::Format_ARGB32) {
        customWindow->setTextureSize(width, height);
    } else if (!customWindow->vulkanReadbackSwapRedBlue && customWindow->qtImage->format() != QImage::Format_RGBA8888) {
        customWindow->setTextureSize(width, height);
    }

    if (!ensureVulkanReadbackResources(customWindow, width, height)) {
        return false;
    }

    VkBufferImageCopy copyRegion = {};
    copyRegion.imageSubresource.aspectMask = unityImage.aspect != 0 ? unityImage.aspect : VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent.width = (uint32_t)width;
    copyRegion.imageExtent.height = (uint32_t)height;
    copyRegion.imageExtent.depth = 1;

    s_vkCmdCopyImageToBuffer(recordingState.commandBuffer, unityImage.image, unityImage.layout,
                             (VkBuffer)customWindow->vulkanReadbackBuffer, 1, &copyRegion);

    VkBufferMemoryBarrier bufferBarrier = {};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = (VkBuffer)customWindow->vulkanReadbackBuffer;
    bufferBarrier.offset = 0;
    bufferBarrier.size = customWindow->vulkanReadbackSize;

    s_vkCmdPipelineBarrier(recordingState.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                           0, nullptr, 1, &bufferBarrier, 0, nullptr);

    customWindow->vulkanReadbackSubmittedFrame = recordingState.currentFrameNumber;
    customWindow->vulkanReadbackPending = true;

    if (s_DebugLoggingEnabled && (customWindow->debugTextureUpdateCount < 3 || customWindow->debugTextureUpdateCount % 120 == 0)) {
        qInfo() << "[TextureDebug] submitted Vulkan readback window" << customWindow->customId << "frame"
                << recordingState.currentFrameNumber << "size" << width << "x" << height;
    }
    customWindow->debugTextureUpdateCount++;
    return true;
}
#endif

class CustomApplication : public QApplication {
  public:
#ifdef WITH_WINE
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
#endif

    CustomApplication(int& argc, char** argv) : QApplication(argc, argv) { this->setQuitOnLastWindowClosed(false); }

    bool createKWinFile() {
        QFile file(kwinFile);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }

        QTextStream textStream(&file);
        textStream << "const appPid = " << this->applicationPid();
        textStream << R"""(
// puts your hands up in the sky. puts your hands up in the sky. puts your puts your. PUT YOUR HANDS UP HIGH
// https://youtu.be/eBC9r5WMNng

print("KWin Multiwindow Plugin")

const mainWindow = workspace.stackingOrder.find((win) => win.pid == appPid);
let lastOrder = "";
const lastConstraints = [];
const supportsConstraints = Object.getOwnPropertyNames(workspace).includes("constrain");

if (!supportsConstraints) {
    print("[WARNING] Constraints/window ordering not supported. Update KDE Plasma.")
}

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
        if (arrangementCount > 0 && supportsConstraints) {
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

    win.maximizedChanged.connect(() => {
        win.setMaximize(false, false);
    });

    win.activeChanged.connect(() => {
        if (!win.active) return;
        if (win.destroyed) return;
        updateWindow();
    })

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
        if (!createKWinFile()) {
            return;
        }

        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"), QString(), QStringLiteral("loadScript"));

        QList<QVariant> arguments;
        arguments << QVariant(kwinFile);
        message.setArguments(arguments);

        QDBusMessage reply = QDBusConnection::sessionBus().call(message);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qInfo() << "KWin load script error" << reply.errorMessage();
            QMessageBox::critical(
                nullptr, "RD Window Dance",
                "Could not load the KWin script. Please create an issue on GitHub with the logs from the Wine output.");
            removeKWinFile();
            return;
        }

        auto id = reply.arguments().constFirst().toInt();

        message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                 QString("/Scripting/Script") + QString::number(id), QString(),
                                                 QStringLiteral("run"));

        reply = QDBusConnection::sessionBus().call(message);

        if (reply.type() == QDBusMessage::ErrorMessage) {
            qInfo() << "KWin run script error" << reply.errorMessage();
            QMessageBox::critical(
                nullptr, "RD Window Dance",
                "Could not run the KWin script. Please create an issue on GitHub with the logs from the Wine output.");
        }

        removeKWinFile();
    }

    void startRunning() {
        if (waylandType == WaylandType::KDE) {
            addKWinScript();
        }

        appReady = true;

        for (auto screen : this->screens()) {
            screenGeometries[screen] = screen->availableGeometry(); // Temporary until we get actual sizes later (or use
                                                                    // Wayland, there these are not used).

            if (waylandType == WaylandType::None) {
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
    QMetaObject::invokeMethod(
        app, [error]() { QMessageBox::critical(nullptr, "RD Window Dance", error); }, Qt::BlockingQueuedConnection);
}

xcb_atom_t getAtom(xcb_connection_t* connection, const char* name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, strlen(name), name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, NULL);
    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

#ifdef WITH_WINE
bool WINAPI CustomGetWindowRect(HWND hWnd, LPRECT lpRect);

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

void checkForWayland() {
    auto sessionDesktop = qgetenv("XDG_SESSION_DESKTOP").toLower();
    bool useX11 = false;

    if (qgetenv("XDG_SESSION_TYPE").toLower() != "wayland") {
        useX11 = true;
    } else if (qgetenv("RD_DANCE_NO_WAYLAND").toLower() == "1") {
        useX11 = true;
        qInfo() << "Wayland force disabled.";
        forceDisabledWayland = true;
    }

    if (useX11) {
        if (sessionDesktop != "kde" && sessionDesktop != "plasma") {
            qWarning() << "You are using an unsupported DE/WM on X11! Bug reports may be ignored.";
            unsupportedDE = true;
        }
        return;
    }

    if (sessionDesktop == "kde" || sessionDesktop == "plasma") {
        waylandType = WaylandType::KDE;
#ifndef WITH_WINE
    } else if (sessionDesktop == "hyprland") {
        waylandType = WaylandType::Hyprland;
#endif
    } else {
        qWarning() << "You are using an unsupported DE/WM on Wayland! Bug reports may be ignored.";
        unsupportedDE = true;
    }
}

bool doesDesktopNeedCutoff() {
    QString sessionDesktop = qgetenv("XDG_SESSION_DESKTOP").toLower();
    if (sessionDesktop == "i3" || sessionDesktop == "hyprland") {
        return false;
    }
    return true;
}

void createApplication() {
    if (createdApplication) {
        return;
    }
    createdApplication = true;
    s_DebugLoggingEnabled = isTruthyEnvValue(qgetenv("RD_DANCE_DEBUG"));
    qInfo() << "RD_DANCE_DEBUG:" << s_DebugLoggingEnabled;
    needsX11Cutoff = doesDesktopNeedCutoff();
    checkForWayland();
#ifdef WITH_WINE
    hookIntoDLL();
#endif

    qInfo() << "Wayland type:" << waylandType;

    if (waylandType == WaylandType::None) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    } else if (waylandType == WaylandType::Hyprland) {
        hyprctl = new Hyprctl();
    }

    std::thread([] {
        int argc = 0;
        app = new CustomApplication(argc, {});
        app->startRunning();
    }).detach();
}

void connectX11Display() { globalXcbConnection = xcb_connect(NULL, NULL); }

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

bool findMainWindowHandle() {
    EnumThreadWindows(GetCurrentThreadId(), findMainWindowHandleCallback, 0);
    return main_window_handle != NULL;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        // fprintf(stderr, "[multiwindow_unity] DllMain();\n");
    }

    return TRUE;
}
#else
bool findMainWindowHandle() {
    if (main_window_handle != 0) {
        return true;
    }
    int currentPid = getpid();

    xcb_screen_iterator_t screens = xcb_setup_roots_iterator(xcb_get_setup(globalXcbConnection));
    xcb_screen_t* screen = screens.data;
    xcb_window_t root = screen->root;

    xcb_atom_t clientListAtom = getAtom(globalXcbConnection, "_NET_CLIENT_LIST");
    xcb_atom_t pidAtom = getAtom(globalXcbConnection, "_NET_WM_PID");

    xcb_get_property_cookie_t cookie =
        xcb_get_property(globalXcbConnection, 0, root, clientListAtom, XCB_ATOM_WINDOW, 0, 1024);

    xcb_get_property_reply_t* reply = xcb_get_property_reply(globalXcbConnection, cookie, NULL);
    int length = xcb_get_property_value_length(reply) / sizeof(xcb_window_t);

    xcb_window_t* windows = (xcb_window_t*)xcb_get_property_value(reply);
    for (int i = 0; i < length; i++) {
        xcb_window_t window = windows[i];
        xcb_get_property_cookie_t pidCookie =
            xcb_get_property(globalXcbConnection, 0, window, pidAtom, XCB_ATOM_CARDINAL, 0, 1);
        xcb_get_property_reply_t* pidReply = xcb_get_property_reply(globalXcbConnection, pidCookie, NULL);
        if (pidReply == nullptr) {
            continue;
        }

        if (xcb_get_property_value_length(pidReply) != 4) {
            continue;
        }

        int pid = *(int*)xcb_get_property_value(pidReply);
        free(pidReply);
        if (pid == currentPid) {
            main_window_handle = window;
            break;
        }
    }

    free(reply);
    return main_window_handle != 0;
}
#endif

struct MotifWmHints {
    uint32_t flags;
    uint32_t functions;
    uint32_t decorations;
    int32_t input_mode;
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

    if (waylandType != WaylandType::Hyprland) {
        this->setFixedSize(10, 10);
    }

    QPixmap* cursor = new QPixmap(1, 1);
    cursor->fill(QColor(0, 0, 0, 0));
    this->setCursor(QCursor(*cursor));

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

bool CustomWindow::copyTexture() {
    if (!isVisible) {
        return false;
    }
    if (isClosing) {
        qWarning() << "WARNING: Tried to copyTexture() when window is closing.";
        return false;
    }
    if (this->qtImage == nullptr) {
        qWarning() << "WARNING: Tried to copyTexture() when texture is deleted.";
        return false;
    }

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

    if (mapped.RowPitch == (UINT)bytesPerLine) {
        memcpy(startingBits, source, bytesPerLine * height);
    } else {
        for (int i = 0; i < height; i++) {
            memcpy(startingBits + i * bytesPerLine, source + i * mapped.RowPitch, bytesPerLine);
        }
    }
    this->textureDataIsUpsideDown = true;

    ctx->Unmap(stagingTexture, 0);
    ctx->Release();
    return true;
}

#else

void CustomWindow::setTexture(GLuint textureId) {
    this->glTextureId = textureId;
    this->textureUsesOpenGL = true;
    this->textureUsesVulkan = false;
    if (s_DebugLoggingEnabled) {
        qInfo() << "[TextureDebug] setTexture(OpenGL) window" << this->customId << "textureId" << textureId;
    }
}

void CustomWindow::setVulkanTexture(void* texturePtr) {
    this->unityVulkanTexture = texturePtr;
    this->textureUsesOpenGL = false;
    this->textureUsesVulkan = texturePtr != nullptr;
    if (texturePtr == nullptr) {
        this->hasTexturePixels = false;
        this->vulkanReadbackPending = false;
    }
    if (s_DebugLoggingEnabled) {
        qInfo() << "[TextureDebug] setTexture(Vulkan) window" << this->customId << "texturePtr" << texturePtr;
    }
}

void CustomWindow::setTextureSize(int w, int h) {
    if (w <= 0 || h <= 0) {
        qWarning() << "[TextureDebug] setTextureSize ignored invalid size for window" << this->customId << w << h;
        return;
    }

    QImage::Format desiredFormat =
        this->textureUsesVulkan && this->vulkanReadbackSwapRedBlue ? QImage::Format_ARGB32 : QImage::Format_RGBA8888;

    if (this->qtImage != nullptr && this->qtImage->width() == w && this->qtImage->height() == h &&
        this->qtImage->format() == desiredFormat) {
        return;
    }

    SAFE_DELETE(this->qtImage);
    this->qtImage = new QImage(w, h, desiredFormat);
    SAFE_FREE(this->tempTexture);
    releaseVulkanReadbackResources(this);
    this->hasTexturePixels = false;
    this->textureDataIsUpsideDown = false;
    if (s_DebugLoggingEnabled) {
        qInfo() << "[TextureDebug] setTextureSize window" << this->customId << "size" << w << "x" << h << "format"
                << desiredFormat;
    }
}

void CustomWindow::setTexturePixels(const void* pixels, int byteCount, int w, int h) {
    if (w <= 0 || h <= 0) {
        qWarning() << "[TextureDebug] setTexturePixels ignored invalid size for window" << this->customId << w << h;
        return;
    }

    int expectedByteCount = w * h * 4;
    if (byteCount < expectedByteCount || pixels == nullptr) {
        qWarning() << "[TextureDebug] setTexturePixels ignored invalid pixel payload for window" << this->customId
                   << "bytes" << byteCount << "expected" << expectedByteCount << "pixelsNull" << (pixels == nullptr);
        return;
    }

    if (this->qtImage == nullptr || this->qtImage->width() != w || this->qtImage->height() != h ||
        this->tempTexture == nullptr && this->textureUsesOpenGL) {
        this->setTextureSize(w, h);
    }

    if (this->hasTexturePixels && this->updateQueued.load(std::memory_order_acquire)) {
        return;
    }

    int bytesPerLine = qtImage->bytesPerLine();
    int height = qtImage->height();
    int sourceBytesPerLine = w * 4;
    uchar* startingBits = qtImage->bits();
    const uchar* source = (const uchar*)pixels;
    if (bytesPerLine == sourceBytesPerLine) {
        memcpy(startingBits, source, expectedByteCount);
    } else {
        for (int i = 0; i < height; i++) {
            memcpy(startingBits + i * bytesPerLine, source + i * sourceBytesPerLine, sourceBytesPerLine);
        }
    }

    this->textureUsesOpenGL = false;
    this->textureUsesVulkan = false;
    this->hasTexturePixels = true;
    this->textureDataIsUpsideDown = true;
    this->debugLoggedMissingTexture = false;

    if (s_DebugLoggingEnabled && (this->debugTextureUpdateCount < 3 || this->debugTextureUpdateCount % 120 == 0)) {
        qInfo() << "[TextureDebug] setTexturePixels window" << this->customId << "size" << w << "x" << h << "bytes"
                << byteCount << samplePixelSummary((const unsigned char*)pixels, w, h);
    }
    this->debugTextureUpdateCount++;

    requestWindowUpdate(this);
}

bool CustomWindow::copyTexture() {
    if (!isVisible) {
        return false;
    }
    if (isClosing) {
        qWarning() << "WARNING: Tried to copyTexture() when window is closing.";
        return false;
    }
    if (this->qtImage == nullptr) {
        qWarning() << "WARNING: Tried to copyTexture() when texture is deleted.";
        return false;
    }

    if (this->textureUsesOpenGL) {
        if (this->tempTexture == nullptr) {
            this->tempTexture = malloc(this->qtImage->width() * this->qtImage->height() * 4);
            if (this->tempTexture == nullptr) {
                qWarning() << "[TextureDebug] Failed to allocate OpenGL temp texture buffer for window" << this->customId;
                return false;
            }
        }
        glBindTexture(GL_TEXTURE_2D, this->glTextureId);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, tempTexture);
        GLenum glError = glGetError();
        if (glError != GL_NO_ERROR) {
            qWarning() << "[TextureDebug] glGetTexImage error for window" << this->customId << "error" << glError;
        } else if (s_DebugLoggingEnabled && (this->debugCopyCount < 3 || this->debugCopyCount % 120 == 0)) {
            qInfo() << "[TextureDebug] copyTexture(OpenGL) window" << this->customId << "textureId" << this->glTextureId
                    << "size" << this->qtImage->width() << "x" << this->qtImage->height();
        }
    } else if (!this->hasTexturePixels) {
        if (!this->debugLoggedMissingTexture || s_DebugLoggingEnabled) {
            qWarning() << "[TextureDebug] copyTexture waiting for CPU/Vulkan pixels for window" << this->customId;
        }
        this->debugLoggedMissingTexture = true;
        return false;
    } else {
        return true;
    }

    int bytesPerLine = qtImage->bytesPerLine();
    int height = qtImage->height();
    int sourceBytesPerLine = this->qtImage->width() * 4;
    uchar* startingBits = qtImage->bits();
    uchar* source = (uchar*)tempTexture;
    if (bytesPerLine == sourceBytesPerLine) {
        memcpy(startingBits, source, bytesPerLine * height);
    } else {
        for (int i = 0; i < height; i++) {
            memcpy(startingBits + i * bytesPerLine, source + i * sourceBytesPerLine, sourceBytesPerLine);
        }
    }
    this->textureDataIsUpsideDown = true;

    if (s_DebugLoggingEnabled && (this->debugCopyCount < 3 || this->debugCopyCount % 120 == 0)) {
        qInfo() << "[TextureDebug] copyTexture(" << (this->textureUsesOpenGL ? "OpenGL" : "CPU/Vulkan") << ") window"
                << this->customId << "image" << this->qtImage->width() << "x" << this->qtImage->height()
                << samplePixelSummary((const unsigned char*)this->qtImage->constBits(), this->qtImage->width(),
                                      this->qtImage->height());
    }
    this->debugCopyCount++;

    return true;
}

#endif

void CustomWindow::_setX11Decorations(bool hasDecorations) {
    MotifWmHints hints = {.flags = 2, .decorations = hasDecorations};

    xcb_atom_t atom = getAtom(globalXcbConnection, "_MOTIF_WM_HINTS");

    xcb_change_property(globalXcbConnection, XCB_PROP_MODE_REPLACE, window()->winId(), atom, atom, 32, 5, &hints);

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
    qreal scaling = waylandType == WaylandType::None ? this->devicePixelRatio() : 1;
    auto primaryScreenGeometry = screenGeometries[primaryScreen];
    auto unitedScreenGeometry = screenGeometries.first();
    int smallestHeight = 999999; // KDE window constraints are buggy. Other monitors also have invisible taskbar limits.
    int finalX = this->targetX / scaling;
    int finalY = this->targetY / scaling;
    int finalWidth = this->targetWidth / scaling;
    int finalHeight = this->targetHeight / scaling;
    float finalOpacity = this->targetOpacity;
    bool finalDecorations = targetDecorations;

    if (waylandType == WaylandType::None) {
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

    if (waylandType == WaylandType::None && needsX11Cutoff) {
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
    }

    if (finalWidth < 5) {
        finalOpacity = 0;
        finalWidth = 5;
    }
    if (finalHeight < 5) {
        finalOpacity = 0;
        finalHeight = 5;
    }

    isVisible = finalOpacity > 0;

    if ((waylandType == WaylandType::KDE || waylandType == WaylandType::Hyprland) && !isVisible) {
        finalY = -5000; // Just put the window far away in Wayland, do not bother with actual opacity.
    }

    // testLabel->setText(QString("Position: %1, %2\nSize: %3 x %4").arg(QString::number(targetX),
    // QString::number(targetY), QString::number(targetWidth), QString::number(targetHeight)));
    // testLabel->setGeometry(0, 0, finalWidth, finalHeight);

    if (waylandType == WaylandType::KDE) { // Wayland doesn't support actual window movement. We have to "smuggle" data
                                           // in the title to the JS plugin.
        std::string encoded = "";
        std::vector<int> smuggledInfo = {finalX,
                                         finalY,
                                         finalWidth,
                                         finalHeight,
                                         finalDecorations ? 1 : 0,
                                         this->customId,
                                         (int)storedWindowOrder.size()};
        for (auto winId : storedWindowOrder) {
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
    } else if (waylandType == WaylandType::Hyprland) {
        if (!hyprReady) {
            if (!hyprctl->setProp("initialtitle:" + std::to_string(customId), "no_blur", "on")) {
                return;
            }
            hyprReady = true;
            hyprctl->setProp("initialtitle:" + std::to_string(customId), "no_focus", "on");
            hyprctl->setProp("initialtitle:" + std::to_string(customId), "no_anim", "on");
            hyprctl->sendMessage("dispatch setfloating initialtitle:" + std::to_string(customId));
        }
        hyprctl->moveWindow("initialtitle:" + std::to_string(customId), finalX, finalY);
        hyprctl->sendMessage("dispatch resizewindowpixel exact " + std::to_string(finalWidth) + " " +
                             std::to_string(finalHeight) + ",initialtitle:" + std::to_string(customId));
        if (finalDecorations != _lastDecorations) {
            this->_lastDecorations = finalDecorations;
            hyprctl->setProp("initialtitle:" + std::to_string(customId), "decorate", finalDecorations ? "on" : "off");
        }
        this->setWindowTitle(targetTitle);
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
    this->updateQueued.store(false, std::memory_order_release);
    QPainter painter(this);
    if (!isVisible) {
        return;
    }
    if (this->qtImage == nullptr) {
        if (s_DebugLoggingEnabled && !this->debugLoggedNullImage) {
            qWarning() << "[PaintDebug] paintEvent skipped because qtImage is null for window" << this->customId;
        }
        this->debugLoggedNullImage = true;
        return;
    }
    this->debugLoggedNullImage = false;

    qreal scaling = waylandType == WaylandType::None ? this->devicePixelRatio() : 1;
    QRect targetRect(this->cutoffX, this->cutoffY, this->targetWidth / scaling, this->targetHeight / scaling);

    if (s_DebugLoggingEnabled && (this->debugPaintCount < 3 || this->debugPaintCount % 120 == 0)) {
        qInfo() << "[PaintDebug] paintEvent window" << this->customId << "target" << this->targetWidth << "x"
                << this->targetHeight << "image" << this->qtImage->width() << "x" << this->qtImage->height()
                << "opacity" << this->targetOpacity
                << samplePixelSummary((const unsigned char*)this->qtImage->constBits(), this->qtImage->width(),
                                      this->qtImage->height());
    }
    this->debugPaintCount++;

    if (this->textureDataIsUpsideDown) {
        painter.save();
        painter.translate(0, (2 * targetRect.y()) + targetRect.height());
        painter.scale(1, -1);
        painter.drawImage(targetRect, *this->qtImage, this->qtImage->rect());
        painter.restore();
    } else {
        painter.drawImage(targetRect, *this->qtImage, this->qtImage->rect());
    }
}

void CustomWindow::setIcon(QImage* image) {
    SAFE_DELETE(iconIcon);

    if (image == nullptr) {
        return;
    }

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
    SAFE_DELETE(this->qtImage);
#ifndef WITH_WINE
    releaseVulkanReadbackResources(this);
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
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setWindowOpacity(0);
    this->setGeometry(screen->geometry());
    this->show();
    QTimer::singleShot(500, [this] {
        qWarning() << "Could not detect usable screen size";
        delete this;
    });
}

void ScreenSizeWindow::resizeEvent(QResizeEvent* event) {
    resizeCount++;
    if (resizeCount != 2) {
        return;
    }
    QTimer::singleShot(10, [this] {
        screenGeometries[this->actualScreen] = this->frameGeometry();
        qInfo() << "Usable screen size is" << screenGeometries[this->actualScreen] << "now" << screenGeometries.size()
                << "screens";
        this->close();
    });
}
// ---- End of ScreenSizeWindow ----

// ---- Start of Hyprctl ----
Hyprctl::Hyprctl() {
    const char* instance = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (instance == nullptr) {
        instance = "";
    }

    socketPath = "/run/user/" + std::to_string(getuid()) + "/hypr/" + instance + "/.socket.sock";
    if (access(socketPath.c_str(), 0) != 0) {
        qCritical() << "Hyprland socket does not exist at" << socketPath;
        qWarning() << "Falling back to X11";
        waylandType = WaylandType::None;
        qputenv("QT_QPA_PLATFORM", "xcb");
        hyprlandError = true;
    }
}

void Hyprctl::sendMessage(std::string message) {
    std::thread([this, message] { this->sendMessageSync(message); }).detach();
}

bool Hyprctl::sendMessageSync(std::string message) {
#ifndef WITH_WINE // TODO: Find out a way to have unix sockets work with winelib (if there is a way)
    auto sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        qCritical() << "sock -1";
        close(sock);
        return false;
    }
    sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
    int res = connect(sock, (sockaddr*)&addr, sizeof(addr));
    if (res == -1) {
        qCritical() << "connect -1";
        close(sock);
        return false;
    }
    const char* data = message.c_str();
    res = write(sock, data, strlen(data));
    if (res == -1) {
        qCritical() << "write -1";
        close(sock);
        return false;
    }
    constexpr size_t bufferSize = 4096;
    char buffer[bufferSize] = {0};
    int written = read(sock, buffer, bufferSize);
    if (written == -1) {
        qCritical() << "read -1";
        close(sock);
        return false;
    }
    std::string reply = std::string(buffer, written);
    if (reply != "ok") {
        qCritical() << reply;
        close(sock);
        return false;
    }
    close(sock);
#endif
    return true;
}

bool Hyprctl::setProp(std::string window, std::string effect, std::string argument) {
    return sendMessageSync("dispatch setprop " + window + " " + effect + " " + argument);
}

void Hyprctl::moveWindow(std::string window, int x, int y) {
    return sendMessage("dispatch movewindowpixel exact " + std::to_string(x) + " " + std::to_string(y) + "," + window);
}

// ---- End of Hyprctl ----

int MAIN_WINDOW_GEOMETRY_SKIP = 0xFEAB12;
void setMainWindowGeometry(int x, int y, int w, int h) {
    if (x != MAIN_WINDOW_GEOMETRY_SKIP) {
        main_window_x = x;
    }
    if (y != MAIN_WINDOW_GEOMETRY_SKIP) {
        main_window_y = y;
    }
    if (w != MAIN_WINDOW_GEOMETRY_SKIP) {
        main_window_width = w;
    }
    if (h != MAIN_WINDOW_GEOMETRY_SKIP) {
        main_window_height = h;
    }

    bool invisible = x < -1500 || y < -1500;

#ifdef WITH_WINE
    if (main_window_handle == 0) {
        return;
    }

    if (invisible) {
        SetWindowLongPtr(main_window_handle, GWL_EXSTYLE,
                         GetWindowLongPtr(main_window_handle, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(main_window_handle, 0, 0, LWA_ALPHA);
        return;
    }

    SetWindowLongPtr(main_window_handle, GWL_EXSTYLE,
                     GetWindowLongPtr(main_window_handle, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    SetWindowPos(main_window_handle, NULL, main_window_x, main_window_y, main_window_width, main_window_height, 0);
#else
    if (waylandType == WaylandType::Hyprland) {
        if (invisible) {
            hyprctl->setProp("initialtitle:Rhythm.Doctor", "opacity", "0");
            hyprctl->setProp("initialtitle:Rhythm.Doctor", "no_blur", "1");
        } else {
            hyprctl->setProp("initialtitle:Rhythm.Doctor", "opacity", "1");
            hyprctl->moveWindow("initialtitle:Rhythm.Doctor", x, y);
        }
    } else {
        if (main_window_handle == 0) {
            return;
        }

        uint32_t opacity = invisible ? 0x00000000 : 0xFFFFFFFF;

        xcb_atom_t atom = getAtom(globalXcbConnection, "_NET_WM_WINDOW_OPACITY");

        xcb_change_property(globalXcbConnection, XCB_PROP_MODE_REPLACE, main_window_handle, atom, XCB_ATOM_CARDINAL, 32,
                            1, &opacity);

        xcb_flush(globalXcbConnection);
    }
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
    QMetaObject::invokeMethod(
        customWindow,
        [customWindow, title = QString(title)]() {
            customWindow->targetTitle = title;
            customWindow->updateThings();
        },
        Qt::QueuedConnection);

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
        if (usingWine || main_window_handle == 0) {
            size.width = main_window_width;
            size.height = main_window_height;
        } else {
#ifndef WITH_WINE
            auto cookie = xcb_get_geometry(globalXcbConnection, main_window_handle);
            auto reply = xcb_get_geometry_reply(globalXcbConnection, cookie, NULL);
            size.width = reply->width;
            size.height = reply->height;
            free(reply);
#endif
        }
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
        if (usingWine || main_window_handle == 0) {
            size.width = main_window_width;
            size.height = main_window_height;
        } else {
#ifndef WITH_WINE
            auto cookie = xcb_get_geometry(globalXcbConnection, main_window_handle);
            auto reply = xcb_get_geometry_reply(globalXcbConnection, cookie, NULL);
            size.width = reply->width;
            size.height = reply->height;
            free(reply);
#endif
        }
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
        if (usingWine || main_window_handle == 0) {
            size.width = main_window_x;
            size.height = main_window_y;
        } else {
#ifndef WITH_WINE
            auto cookie = xcb_get_geometry(globalXcbConnection, main_window_handle);
            auto reply = xcb_get_geometry_reply(globalXcbConnection, cookie, NULL);
            size.width = reply->x;
            size.height = reply->y;
            free(reply);
#endif
        }
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
    QMetaObject::invokeMethod(customWindow, [customWindow]() { customWindow->updateThings(); }, Qt::QueuedConnection);
}

extern "C" WINAPI const char* move_window(HANDLE window, int x, int y, int w, int h) {
    if (window == MAIN_WINDOW) {
        std::cerr << "move_window(" << std::hex << window << std::dec << ", " << x << ", " << y << ", " << w << ", "
                  << h << ")" << std::endl;
        setMainWindowGeometry(x, y, w, h);
        return createString("");
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    // if (customWindow->targetX != x || customWindow->targetY != y || customWindow->targetWidth != w ||
    // customWindow->targetHeight != h) {
    //     std::cerr << "move_window(" << std::hex << window << std::dec << ", " << x << ", " << y << ", " << w << ", "
    //     << h << ")" << std::endl;
    // }
    customWindow->setTargetMove(x, y);
    customWindow->setTargetSize(w, h);
    QMetaObject::invokeMethod(customWindow, [customWindow]() { customWindow->updateThings(); }, Qt::QueuedConnection);
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
    QMetaObject::invokeMethod(app, [customWindow]() { customWindow->updateThings(); }, Qt::QueuedConnection);
}

struct FFIResult {
    uint8_t status;
    HANDLE data;
    char* error;
};

extern "C" WINAPI FFIResult new_window(LPSTR title, int x, int y, int w, int h, bool frameless, bool opaque,
                                       bool allowFullscreen) {
    std::cerr << "new_window(title: " << title << ", x: " << x << ", y: " << y << ", w: " << w << ", h: " << h
              << ", frameless: " << boolToStr(frameless) << ", opaque: " << boolToStr(opaque)
              << ", allowFullscreen: " << boolToStr(allowFullscreen) << ")" << std::endl;

    CustomWindow* customWindow = nullptr;
    QMetaObject::invokeMethod(
        app,
        [&customWindow, title, x, y, w, h, frameless]() {
            customWindow = new CustomWindow();
            customWindow->targetDecorations = !frameless;
            customWindow->targetTitle = title;
            customWindow->setTargetMove(x, y);
            customWindow->setTargetSize(w, h);
            customWindow->updateThings();
            if (waylandType == WaylandType::Hyprland) {
                customWindow->setWindowTitle(QString::number(customWindow->customId));
            }
            allCustomWindows.push_back(customWindow);
            customWindow->show();
        },
        Qt::BlockingQueuedConnection);

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
        if (main_window_handle != 0) {
            SetForegroundWindow(main_window_handle);
            SetActiveWindow(main_window_handle);
        }
#endif
    }
    return createString("");
}

extern "C" WINAPI bool is_window_focused(HWND window) {
    // std::cerr << "is_window_focused(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        if (main_window_handle == 0) {
            return false;
        }
#ifdef WITH_WINE
        return GetActiveWindow() == main_window_handle;
#else
        return true;
#endif
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    return customWindow->isActiveWindow();
}

extern "C" char* set_window_texture(HWND window,
#ifdef WITH_WINE
                                    HWND texturePtr
#else
                                    uintptr_t texturePtr
#endif
) {
    std::cerr << "set_window_texture(" << std::hex << window << std::dec << ", " << std::hex << texturePtr << std::dec
              << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return createString("");
    }

    CustomWindow* customWindow = (CustomWindow*)window;
#ifdef WITH_WINE
    customWindow->setTexture((ID3D11Resource*)texturePtr);
#else
    if (s_RendererType == kUnityGfxRendererOpenGLCore) {
        customWindow->setTexture((GLuint)texturePtr);
    } else if (s_RendererType == kUnityGfxRendererVulkan) {
        customWindow->setVulkanTexture((void*)texturePtr);
    } else {
        customWindow->textureUsesOpenGL = false;
        qInfo() << "[TextureDebug] set_window_texture called while renderer is" << rendererTypeToString(s_RendererType)
                << "for window" << customWindow->customId << "texturePtr" << texturePtr
                << "- renderer not ready yet.";
    }
#endif
    return createString("");
}

#ifndef WITH_WINE
extern "C" void set_window_texture_size(HWND window, int w, int h) {
    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->setTextureSize(w, h);
}

extern "C" void set_window_texture_pixels(HWND window, const unsigned char* pixels, int byteCount, int w, int h) {
    if (window == MAIN_WINDOW) {
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    customWindowMutex.lock();
    customWindow->setTexturePixels(pixels, byteCount, w, h);
    customWindowMutex.unlock();
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
    std::cerr << "set_window_icon(" << std::hex << window << std::dec << ", " << std::hex << icon << std::dec << ")"
              << std::endl;
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
    customWindowMutex.lock();
    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->isClosing = true;
    QMetaObject::invokeMethod(
        app,
        [customWindow]() {
            if (waylandType == WaylandType::None) {
                customWindow->setWindowOpacity(0);
            }
            customWindow->close();
            allCustomWindows.erase(std::find(allCustomWindows.begin(), allCustomWindows.end(), customWindow));
            delete customWindow;
            customWindowMutex.unlock();
        },
        Qt::QueuedConnection);
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
    QMetaObject::invokeMethod(
        app,
        [customWindow]() {
            customWindow->targetOpacity = 1;
            customWindow->updateThings();
        },
        Qt::QueuedConnection);
    return createString("");
}

extern "C" WINAPI const char* hide_window(HWND window) {
    std::cerr << "hide_window(" << std::hex << window << std::dec << ")" << std::endl;
    if (window == MAIN_WINDOW) {
        return createString("");
    }
    CustomWindow* customWindow = (CustomWindow*)window;
    QMetaObject::invokeMethod(
        app,
        [customWindow]() {
            customWindow->targetOpacity = 0;
            customWindow->updateThings();
        },
        Qt::QueuedConnection);
    return createString("");
}

void arrangeWindowsX11(HWND* windows, int count) {
    std::vector<CustomWindow*> windowList;
    for (int i = 0; i < count; i++) {
        if (windows[i] == MAIN_WINDOW) {
            continue;
        }
        windowList.push_back((CustomWindow*)windows[i]);
    }

    bool hasChanged = false;
    if (storedWindowOrder.size() != windowList.size()) {
        hasChanged = true;
        storedWindowOrder.resize(windowList.size());
    }

    for (size_t i = 0; i < windowList.size(); i++) {
        WId target = windowList[i]->window()->winId();
        if (target != storedWindowOrder[i]) {
            hasChanged = true;
        }
        storedWindowOrder[i] = target;
    }

    if (!hasChanged && framesSinceReorder < 30) {
        return;
    }

    framesSinceReorder = 0;

    for (size_t i = 0; i < windowList.size(); i++) {
        WId configOptions[] = {XCB_STACK_MODE_BELOW};
        xcb_configure_window(globalXcbConnection, windowList[i]->window()->winId(), XCB_CONFIG_WINDOW_STACK_MODE,
                             configOptions);
    }

    xcb_flush(globalXcbConnection);
}

void arrangeWindowsKWinWayland(HWND* windows, int count) {
    std::vector<CustomWindow*> windowList;
    for (int i = 0; i < count; i++) {
        if (windows[i] == MAIN_WINDOW) {
            continue;
        }
        windowList.push_back((CustomWindow*)windows[i]);
    }

    storedWindowOrder.resize(windowList.size());
    for (size_t i = 0; i < windowList.size(); i++) {
        storedWindowOrder[i] = windowList[windowList.size() - i - 1]->customId;
    }
}

void arrangeWindowsHyprland(HWND* windows, int count) {
    std::vector<CustomWindow*> windowList;
    for (int i = 0; i < count; i++) {
        if (windows[i] == MAIN_WINDOW) {
            continue;
        }
        windowList.insert(windowList.begin(), (CustomWindow*)windows[i]);
    }

    bool hasChanged = false;
    if (storedWindowOrder.size() != windowList.size()) {
        hasChanged = true;
    }
    storedWindowOrder.resize(windowList.size());
    for (size_t i = 0; i < windowList.size(); i++) {
        WId target = windowList[i]->customId;
        if (target != storedWindowOrder[i]) {
            hasChanged = true;
        }
        storedWindowOrder[i] = target;
    }

    if (hasChanged) {
        for (auto win : windowList) {
            hyprctl->sendMessageSync("dispatch alterzorder top,initialtitle:" + std::to_string(win->customId));
        }
    }
}

extern "C" WINAPI void arrange_windows(HWND* windows, int count) {
    if (waylandType == WaylandType::KDE) {
        arrangeWindowsKWinWayland(windows, count);
    } else if (waylandType == WaylandType::None) {
        arrangeWindowsX11(windows, count);
    } else if (waylandType == WaylandType::Hyprland) {
        arrangeWindowsHyprland(windows, count);
    }
}

static void UNITY_INTERFACE_API render(int eventID) {
    // std::cerr << "render(" << eventID << ")" << std::endl;
    customWindowMutex.lock();
#ifndef WITH_WINE
    UnityVulkanRecordingState vulkanRecordingState = {};
    bool hasVulkanRecordingState = false;
    if (s_RendererType == kUnityGfxRendererVulkan && eventID == kRenderEventVulkanCopy && s_GraphicsVulkan != nullptr) {
        hasVulkanRecordingState =
            s_GraphicsVulkan->CommandRecordingState(&vulkanRecordingState, kUnityVulkanGraphicsQueueAccess_DontCare);
    }
#endif
    for (auto customWindow : allCustomWindows) {
        if (customWindow->textureUsesOpenGL) {
            if (customWindow->copyTexture()) {
                requestWindowUpdate(customWindow);
            }
#ifndef WITH_WINE
        } else if (customWindow->textureUsesVulkan && hasVulkanRecordingState) {
            if (processCompletedVulkanReadback(customWindow, vulkanRecordingState.safeFrameNumber)) {
                requestWindowUpdate(customWindow);
            }
            submitVulkanReadback(customWindow, vulkanRecordingState);
#endif
        }
    }
    customWindowMutex.unlock();
    framesSinceReorder += 1;
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
    std::cerr << "set_window_fullscreen(" << std::hex << window << std::dec << ", " << boolToStr(mode) << ")"
              << std::endl;
    return false; // Unused?
}

extern "C" WINAPI void set_window_frame_visible(HWND window, bool visible) {
    std::cerr << "set_window_frame_visible(" << std::hex << window << std::dec << ", " << boolToStr(visible) << ")"
              << std::endl;
    if (window == MAIN_WINDOW) {
        return;
    }

    CustomWindow* customWindow = (CustomWindow*)window;
    customWindow->targetDecorations = visible;
    QMetaObject::invokeMethod(app, [customWindow]() { customWindow->updateThings(); }, Qt::QueuedConnection);
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
        monitors.monitors[i] = {geometry.x(), geometry.y(), geometry.width(), geometry.height(), 100};
    }
    return monitors;
}

extern "C" const char* get_info() {
    std::string info = "Changing will require restarting the level/game.";
    info += "\n\n---- Linux Info ----\n";
    if (waylandType == WaylandType::None) {
        info += "Using X11\n";
    } else if (waylandType == WaylandType::KDE) {
        info += "Using Wayland KDE\n";
    } else if (waylandType == WaylandType::Hyprland) {
        info += "Using Wayland Hyprland\n";
    }

    if (unsupportedDE) {
        info += "<color=red>Using unsupported DE/WM.</color>\n";
    } else if (waylandType != WaylandType::None) {
        info += "<color=green>You are using a supported DE/WM</color>\n";
    }

    if (forceDisabledWayland) {
        info += "<color=blue>You have force disabled Wayland.</color>\n";
    }

    if (hyprlandError) {
        info += "<color=red>Hyprland socket not found. Try launching the game outside of Steam.</color>\n";
    }

    return createString(info.c_str());
}
#endif

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
    case kUnityGfxDeviceEventInitialize: {
        s_RendererType = s_Graphics->GetRenderer();
        qInfo() << "Unity graphics renderer:" << rendererTypeToString(s_RendererType);
#ifdef WITH_WINE
        if (s_RendererType != kUnityGfxRendererD3D11) {
            blockWithError("Only D3D11 is supported.");
            return;
        }
        IUnityGraphicsD3D11* d3d = s_UnityInterfaces->Get<IUnityGraphicsD3D11>();
        if (d3d == nullptr) {
            return;
        }
        app->device = d3d->GetDevice();
#else
        if (s_RendererType != kUnityGfxRendererOpenGLCore && s_RendererType != kUnityGfxRendererVulkan) {
            blockWithError("Only OpenGL Core and Vulkan are supported.");
            return;
        }
        if (s_RendererType == kUnityGfxRendererVulkan) {
            s_GraphicsVulkan = s_UnityInterfaces->Get<IUnityGraphicsVulkan>();
            if (s_GraphicsVulkan == nullptr) {
                blockWithError("Unity Vulkan graphics interface is unavailable.");
                return;
            }

            loadVulkanFunctions(s_GraphicsVulkan->Instance());
            UnityVulkanPluginEventConfig eventConfig = {};
            eventConfig.renderPassPrecondition = kUnityVulkanRenderPass_EnsureOutside;
            eventConfig.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare;
            eventConfig.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission |
                                kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
            s_GraphicsVulkan->ConfigureEvent(kRenderEventVulkanCopy, &eventConfig);
            qInfo() << "[TextureDebug] Vulkan renderer detected, using native Vulkan readback path for custom windows.";
        }
#endif
        break;
    }
    case kUnityGfxDeviceEventShutdown: {
#ifndef WITH_WINE
        customWindowMutex.lock();
        for (auto customWindow : allCustomWindows) {
            releaseVulkanReadbackResources(customWindow);
        }
        customWindowMutex.unlock();
        s_GraphicsVulkan = NULL;
        s_vkGetPhysicalDeviceMemoryProperties = nullptr;
        s_vkCreateBuffer = nullptr;
        s_vkDestroyBuffer = nullptr;
        s_vkGetBufferMemoryRequirements = nullptr;
        s_vkAllocateMemory = nullptr;
        s_vkFreeMemory = nullptr;
        s_vkBindBufferMemory = nullptr;
        s_vkMapMemory = nullptr;
        s_vkUnmapMemory = nullptr;
        s_vkInvalidateMappedMemoryRanges = nullptr;
        s_vkCmdCopyImageToBuffer = nullptr;
        s_vkCmdPipelineBarrier = nullptr;
#endif
        s_RendererType = kUnityGfxRendererNull;
        break;
    }
    case kUnityGfxDeviceEventBeforeReset: {
        break;
    }
    case kUnityGfxDeviceEventAfterReset: {
        break;
    }
    };
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces) {
    connectX11Display();
    if (!findMainWindowHandle()) {
        qWarning() << "Main window handle could not be found!";
    }
    createApplication();
    while (!appReady) {
        usleep(100);
    }

    s_UnityInterfaces = unityInterfaces;
    s_Graphics = unityInterfaces->Get<IUnityGraphics>();

    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    // to not miss the event in case the graphics device is already initialized
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

#ifdef WITH_WINE
bool WINAPI CustomGetWindowRect(HWND hWnd, LPRECT lpRect) {
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
