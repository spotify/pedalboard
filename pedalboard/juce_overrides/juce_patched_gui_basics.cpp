/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#ifdef JUCE_GUI_BASICS_H_INCLUDED
/* When you add this cpp file to your project, you mustn't include it in a file
   where you've already included any other headers - just put it inside a file
   on its own, possibly with your config flags preceding it, but don't include
   anything else. That also includes avoiding any automatic prefix header files
   that the compiler may be using.
*/
#error "Incorrect use of JUCE cpp file"
#endif

#define NS_FORMAT_FUNCTION(F, A) // To avoid spurious warnings from GCC

#define JUCE_CORE_INCLUDE_OBJC_HELPERS 1
#define JUCE_CORE_INCLUDE_COM_SMART_PTR 1
#define JUCE_CORE_INCLUDE_JNI_HELPERS 1
#define JUCE_CORE_INCLUDE_NATIVE_HEADERS 1
#define JUCE_EVENTS_INCLUDE_WIN32_MESSAGE_WINDOW 1
#define JUCE_GRAPHICS_INCLUDE_COREGRAPHICS_HELPERS 1
#define JUCE_GUI_BASICS_INCLUDE_XHEADERS 1
#define JUCE_GUI_BASICS_INCLUDE_SCOPED_THREAD_DPI_AWARENESS_SETTER 1

// Avoid error: ‘exchange’ is not a member of ‘std’:
#include <utility>

#include "../JUCE/modules/juce_gui_basics/juce_gui_basics.h"

//==============================================================================
#if JUCE_MAC
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <WebKit/WebKit.h>

#if JUCE_SUPPORT_CARBON
#import <Carbon/Carbon.h> // still needed for SetSystemUIMode()
#endif

#elif JUCE_IOS
#if JUCE_PUSH_NOTIFICATIONS && defined(__IPHONE_10_0) &&                       \
    __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0
#import <UserNotifications/UserNotifications.h>
#endif

#import <UIKit/UIActivityViewController.h>

//==============================================================================
#elif JUCE_WINDOWS
#include <commctrl.h>
#include <commdlg.h>
#include <vfw.h>
#include <windowsx.h>

#if !JUCE_MINGW
#include <UIAutomation.h>
#include <sapi.h>
#endif

#if JUCE_WEB_BROWSER
#include <exdisp.h>
#include <exdispid.h>
#endif

#if JUCE_MINGW
#include <imm.h>
#elif !JUCE_DONT_AUTOLINK_TO_WIN32_LIBRARIES
#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "comctl32.lib")

#if JUCE_OPENGL
#pragma comment(lib, "OpenGL32.Lib")
#pragma comment(lib, "GlU32.Lib")
#endif

#if JUCE_DIRECT2D
#pragma comment(lib, "Dwrite.lib")
#pragma comment(lib, "D2d1.lib")
#endif
#endif
#endif

#include <set>

//==============================================================================
#define JUCE_ASSERT_MESSAGE_MANAGER_IS_LOCKED_OR_OFFSCREEN                     \
  jassert((MessageManager::getInstanceWithoutCreating() != nullptr &&          \
           MessageManager::getInstanceWithoutCreating()                        \
               ->currentThreadHasLockedMessageManager()) ||                    \
          getPeer() == nullptr);

namespace juce {
bool juce_areThereAnyAlwaysOnTopWindows();

bool isEmbeddedInForegroundProcess(Component *c);

#if !JUCE_WINDOWS
bool isEmbeddedInForegroundProcess(Component *) { return false; }
#endif

/*  Returns true if this process is in the foreground, or if the viewComponent
    is embedded into a window owned by the foreground process.
*/
static bool isForegroundOrEmbeddedProcess(Component *viewComponent) {
  return Process::isForegroundProcess() ||
         isEmbeddedInForegroundProcess(viewComponent);
}

bool isWindowOnCurrentVirtualDesktop(void *);

struct CustomMouseCursorInfo {
  ScaledImage image;
  Point<int> hotspot;
};
} // namespace juce

#include "../JUCE/modules/juce_gui_basics/accessibility/juce_AccessibilityHandler.cpp"

#include "../JUCE/modules/juce_gui_basics/components/juce_Component.cpp"

#include "../JUCE/modules/juce_gui_basics/components/juce_ComponentListener.cpp"

#include "../JUCE/modules/juce_gui_basics/components/juce_FocusTraverser.cpp"

#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseInputSource.cpp"

#include "../JUCE/modules/juce_gui_basics/desktop/juce_Displays.cpp"

#include "../JUCE/modules/juce_gui_basics/desktop/juce_Desktop.cpp"

#include "../JUCE/modules/juce_gui_basics/components/juce_ModalComponentManager.cpp"

#include "../JUCE/modules/juce_gui_basics/mouse/juce_ComponentDragger.cpp"

#include "../JUCE/modules/juce_gui_basics/mouse/juce_DragAndDropContainer.cpp"

#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseEvent.cpp"

#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseInactivityDetector.cpp"

#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseListener.cpp"

#include "../JUCE/modules/juce_gui_basics/keyboard/juce_CaretComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/keyboard/juce_KeyboardFocusTraverser.cpp"

#include "../JUCE/modules/juce_gui_basics/keyboard/juce_KeyListener.cpp"

#include "../JUCE/modules/juce_gui_basics/keyboard/juce_KeyPress.cpp"

#include "../JUCE/modules/juce_gui_basics/keyboard/juce_ModifierKeys.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_ArrowButton.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_Button.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_DrawableButton.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_HyperlinkButton.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_ImageButton.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_ShapeButton.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_TextButton.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_ToggleButton.cpp"

#include "../JUCE/modules/juce_gui_basics/buttons/juce_ToolbarButton.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_Drawable.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableComposite.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableImage.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawablePath.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableRectangle.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableShape.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableText.cpp"

#include "../JUCE/modules/juce_gui_basics/drawables/juce_SVGParser.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_DirectoryContentsDisplayComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_DirectoryContentsList.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileBrowserComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooser.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooserDialogBox.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileListComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FilenameComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileSearchPathListComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileTreeComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_ImagePreviewComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_ContentSharer.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentAnimator.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentBoundsConstrainer.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentBuilder.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentMovementWatcher.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ConcertinaPanel.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_GroupComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_MultiDocumentPanel.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ResizableBorderComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ResizableCornerComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ResizableEdgeComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_ScrollBar.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_SidePanel.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_StretchableLayoutManager.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_StretchableLayoutResizerBar.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_StretchableObjectResizer.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_TabbedButtonBar.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_TabbedComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_Viewport.cpp"

#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel.cpp"

#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V2.cpp"

#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V1.cpp"

#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V3.cpp"

#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V4.cpp"

#include "../JUCE/modules/juce_gui_basics/menus/juce_MenuBarComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/menus/juce_BurgerMenuComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/menus/juce_MenuBarModel.cpp"

#include "../JUCE/modules/juce_gui_basics/menus/juce_PopupMenu.cpp"

#include "../JUCE/modules/juce_gui_basics/positioning/juce_MarkerList.cpp"

#include "../JUCE/modules/juce_gui_basics/positioning/juce_RelativeCoordinate.cpp"

#include "../JUCE/modules/juce_gui_basics/positioning/juce_RelativeCoordinatePositioner.cpp"

#include "../JUCE/modules/juce_gui_basics/positioning/juce_RelativeParallelogram.cpp"

#include "../JUCE/modules/juce_gui_basics/positioning/juce_RelativePoint.cpp"

#include "../JUCE/modules/juce_gui_basics/positioning/juce_RelativePointPath.cpp"

#include "../JUCE/modules/juce_gui_basics/positioning/juce_RelativeRectangle.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_BooleanPropertyComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_ButtonPropertyComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_ChoicePropertyComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_PropertyComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_PropertyPanel.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_SliderPropertyComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_TextPropertyComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/properties/juce_MultiChoicePropertyComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_ComboBox.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_ImageComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_Label.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_ListBox.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_ProgressBar.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_Slider.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_TableHeaderComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_TableListBox.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_TextEditor.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_ToolbarItemComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_Toolbar.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_ToolbarItemPalette.cpp"

#include "../JUCE/modules/juce_gui_basics/widgets/juce_TreeView.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_AlertWindow.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_CallOutBox.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_ComponentPeer.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_DialogWindow.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_DocumentWindow.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_ResizableWindow.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_ThreadWithProgressWindow.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_TooltipWindow.cpp"

#include "../JUCE/modules/juce_gui_basics/windows/juce_TopLevelWindow.cpp"

#include "../JUCE/modules/juce_gui_basics/commands/juce_ApplicationCommandInfo.cpp"

#include "../JUCE/modules/juce_gui_basics/commands/juce_ApplicationCommandManager.cpp"

#include "../JUCE/modules/juce_gui_basics/commands/juce_ApplicationCommandTarget.cpp"

#include "../JUCE/modules/juce_gui_basics/commands/juce_KeyPressMappingSet.cpp"

#include "../JUCE/modules/juce_gui_basics/application/juce_Application.cpp"

#include "../JUCE/modules/juce_gui_basics/misc/juce_BubbleComponent.cpp"

#include "../JUCE/modules/juce_gui_basics/misc/juce_DropShadower.cpp"

#include "../JUCE/modules/juce_gui_basics/misc/juce_JUCESplashScreen.cpp"

#include "../JUCE/modules/juce_gui_basics/layout/juce_FlexBox.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_Grid.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_GridItem.cpp"

#if JUCE_IOS || JUCE_WINDOWS
#include "../JUCE/modules/juce_gui_basics/native/juce_MultiTouchMapper.h"
#endif

#if JUCE_ANDROID || JUCE_WINDOWS
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_AccessibilityTextHelpers.h"
#endif

namespace juce {

static const juce::Identifier disableAsyncLayerBackedViewIdentifier{
    "disableAsyncLayerBackedView"};

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wmissing-prototypes")

/** Used by the macOS and iOS peers. */
void setComponentAsyncLayerBackedViewDisabled(
    juce::Component &comp, bool shouldDisableAsyncLayerBackedView) {
  comp.getProperties().set(disableAsyncLayerBackedViewIdentifier,
                           shouldDisableAsyncLayerBackedView);
}

/** Used by the macOS and iOS peers. */
bool getComponentAsyncLayerBackedViewDisabled(juce::Component &comp) {
  return comp.getProperties()[disableAsyncLayerBackedViewIdentifier];
}

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

} // namespace juce

#if JUCE_MAC || JUCE_IOS
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_mac_AccessibilitySharedCode.mm"

#if JUCE_IOS
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_ios_Accessibility.mm"

#include "../JUCE/modules/juce_gui_basics/native/juce_ios_UIViewComponentPeer.mm"

#include "../JUCE/modules/juce_gui_basics/native/juce_ios_Windowing.mm"

#include "../JUCE/modules/juce_gui_basics/native/juce_ios_FileChooser.mm"

#if JUCE_CONTENT_SHARING
#include "../JUCE/modules/juce_gui_basics/native/juce_ios_ContentSharer.cpp"
#endif

#else
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_mac_Accessibility.mm"

#include "../JUCE/modules/juce_gui_basics/native/juce_mac_NSViewComponentPeer.mm"

#include "juce_mac_PatchedWindowing.mm"

#include "../JUCE/modules/juce_gui_basics/native/juce_mac_MainMenu.mm"

#include "../JUCE/modules/juce_gui_basics/native/juce_mac_FileChooser.mm"

#endif

#include "../JUCE/modules/juce_gui_basics/native/juce_mac_MouseCursor.mm"

#elif JUCE_WINDOWS

#if !JUCE_MINGW
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_win32_WindowsUIAWrapper.h"

#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_win32_AccessibilityElement.h"

#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_win32_UIAHelpers.h"

#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_win32_UIAProviders.h"

#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_win32_AccessibilityElement.cpp"

#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_win32_Accessibility.cpp"

#else
namespace juce {
namespace WindowsAccessibility {
long getUiaRootObjectId() { return -1; }
bool handleWmGetObject(AccessibilityHandler *, WPARAM, LPARAM, LRESULT *) {
  return false;
}
void revokeUIAMapEntriesForWindow(HWND) {}
} // namespace WindowsAccessibility
} // namespace juce
#endif

#include "../JUCE/modules/juce_gui_basics/native/juce_win32_Windowing.cpp"

#include "../JUCE/modules/juce_gui_basics/native/juce_win32_DragAndDrop.cpp"

#include "../JUCE/modules/juce_gui_basics/native/juce_win32_FileChooser.cpp"

#elif JUCE_LINUX || JUCE_BSD
#include "../JUCE/modules/juce_gui_basics/native/x11/juce_linux_X11_DragAndDrop.cpp"

#include "../JUCE/modules/juce_gui_basics/native/x11/juce_linux_X11_Symbols.cpp"

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wzero-as-null-pointer-constant")

#include "../JUCE/modules/juce_gui_basics/native/juce_linux_Windowing.cpp"

#include "../JUCE/modules/juce_gui_basics/native/x11/juce_linux_XWindowSystem.cpp"

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

#include "../JUCE/modules/juce_gui_basics/native/juce_linux_FileChooser.cpp"

#elif JUCE_ANDROID
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_android_Accessibility.cpp"

#include "../JUCE/modules/juce_gui_basics/native/juce_android_Windowing.cpp"

#include "../JUCE/modules/juce_gui_basics/native/juce_common_MimeTypes.cpp"

#include "../JUCE/modules/juce_gui_basics/native/juce_android_FileChooser.cpp"

#if JUCE_CONTENT_SHARING
#include "../JUCE/modules/juce_gui_basics/native/juce_android_ContentSharer.cpp"
#endif

#endif

namespace juce {
#if !JUCE_NATIVE_ACCESSIBILITY_INCLUDED
class AccessibilityHandler::AccessibilityNativeImpl {
public:
  AccessibilityNativeImpl(AccessibilityHandler &) {}
};
void AccessibilityHandler::notifyAccessibilityEvent(AccessibilityEvent) const {}
void AccessibilityHandler::postAnnouncement(const String &,
                                            AnnouncementPriority) {}
AccessibilityNativeHandle *
AccessibilityHandler::getNativeImplementation() const {
  return nullptr;
}
void notifyAccessibilityEventInternal(const AccessibilityHandler &,
                                      InternalAccessibilityEvent) {}
std::unique_ptr<AccessibilityHandler::AccessibilityNativeImpl>
AccessibilityHandler::createNativeImpl(AccessibilityHandler &) {
  return nullptr;
}
#else
std::unique_ptr<AccessibilityHandler::AccessibilityNativeImpl>
AccessibilityHandler::createNativeImpl(AccessibilityHandler &handler) {
  return std::make_unique<AccessibilityNativeImpl>(handler);
}
#endif
} // namespace juce

//==============================================================================
#if JUCE_WINDOWS
bool juce::isWindowOnCurrentVirtualDesktop(void *x) {
  if (x == nullptr)
    return false;

  static auto *desktopManager = [] {
    // IVirtualDesktopManager Copied from ShObjdl_core.h, because it may not be
    // defined
    MIDL_INTERFACE("a5cd92ff-29be-454c-8d04-d82879fb3f1b")
  juce_IVirtualDesktopManager:
  public
    IUnknown {
    public:
      virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(
          __RPC__in HWND topLevelWindow,
          __RPC__out BOOL * onCurrentDesktop) = 0;

      virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(
          __RPC__in HWND topLevelWindow, __RPC__out GUID * desktopId) = 0;

      virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(
          __RPC__in HWND topLevelWindow, __RPC__in REFGUID desktopId) = 0;
    };

    juce_IVirtualDesktopManager *result = nullptr;

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wlanguage-extension-token")

    class DECLSPEC_UUID("aa509086-5ca9-4c25-8f95-589d3c07b48a")
        juce_VirtualDesktopManager;

    if (SUCCEEDED(CoCreateInstance(__uuidof(juce_VirtualDesktopManager),
                                   nullptr, CLSCTX_ALL, IID_PPV_ARGS(&result))))
      return result;

    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    return static_cast<juce_IVirtualDesktopManager *>(nullptr);
  }();

  BOOL current = false;

  if (auto *dm = desktopManager)
    if (SUCCEEDED(dm->IsWindowOnCurrentVirtualDesktop(static_cast<HWND>(x),
                                                      &current)))
      return current != false;

  return true;
}
#else
bool juce::isWindowOnCurrentVirtualDesktop(void *) { return true; }
juce::ScopedDPIAwarenessDisabler::ScopedDPIAwarenessDisabler() {
  ignoreUnused(previousContext);
}
juce::ScopedDPIAwarenessDisabler::~ScopedDPIAwarenessDisabler() {}
#endif

// Depends on types defined in platform-specific windowing files
#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseCursor.cpp"
