/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 7 End-User License
   Agreement and JUCE Privacy Policy.

   End User License Agreement: www.juce.com/juce-7-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

// clang-format off
// (JUCE's include order below is significant; don't let clang-format sort it.)
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

// Avoid error: 'exchange' is not a member of 'std':
#include <utility>

#include "../JUCE/modules/juce_gui_basics/juce_gui_basics.h"

#include <cctype>

//==============================================================================
#if JUCE_MAC
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <MetalKit/MetalKit.h>

#elif JUCE_IOS
#if JUCE_PUSH_NOTIFICATIONS
#import <UserNotifications/UserNotifications.h>
#endif

#import <MetalKit/MetalKit.h>
#import <UIKit/UIActivityViewController.h>

//==============================================================================
#elif JUCE_WINDOWS
#include <windowsx.h>
#include <vfw.h>
#include <commdlg.h>
#include <commctrl.h>
#include <sapi.h>
#include <dxgi.h>

#if JUCE_MINGW
// Some MinGW headers use 'new' as a parameter name
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wkeyword-macro")
#define new new_
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
#endif

#include <uiautomation.h>

#undef new

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
#pragma comment(lib, "dxgi.lib")

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

//==============================================================================
#include "../JUCE/modules/juce_gui_basics/detail/juce_AccessibilityHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ButtonAccessibilityHandler.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ScalingHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ComponentHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_FocusHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_FocusRestorer.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ViewportHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_LookAndFeelHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_PointerState.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_CustomMouseCursorInfo.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_MouseInputSourceImpl.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_MouseInputSourceList.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ToolbarItemDragAndDropOverlayComponent.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ScopedMessageBoxInterface.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ScopedMessageBoxImpl.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ScopedContentSharerInterface.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_ScopedContentSharerImpl.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_WindowingHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_AlertWindowHelpers.h"
#include "../JUCE/modules/juce_gui_basics/detail/juce_TopLevelWindowManager.h"

//==============================================================================
#if JUCE_IOS || JUCE_WINDOWS
#include "../JUCE/modules/juce_gui_basics/native/juce_MultiTouchMapper.h"
#endif

#if JUCE_ANDROID || JUCE_WINDOWS || JUCE_IOS || JUCE_UNIT_TESTS
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_AccessibilityTextHelpers.h"
#endif

#if JUCE_MAC || JUCE_IOS
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_AccessibilitySharedCode_mac.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_CGMetalLayerRenderer_mac.h"

#if JUCE_IOS
#include "../JUCE/modules/juce_gui_basics/native/juce_UIViewComponentPeer_ios.mm"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_Accessibility_ios.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_WindowUtils_ios.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_Windowing_ios.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_NativeMessageBox_ios.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_NativeModalWrapperComponent_ios.h"
#include "../JUCE/modules/juce_gui_basics/native/juce_FileChooser_ios.mm"

#if JUCE_CONTENT_SHARING
#include "../JUCE/modules/juce_gui_basics/native/juce_ContentSharer_ios.cpp"
#endif

#else
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_Accessibility_mac.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_PerScreenDisplayLinks_mac.h"
#include "../JUCE/modules/juce_gui_basics/native/juce_NSViewComponentPeer_mac.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_WindowUtils_mac.mm"
#include "juce_mac_PatchedWindowing.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_NativeMessageBox_mac.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_MainMenu_mac.mm"
#include "../JUCE/modules/juce_gui_basics/native/juce_FileChooser_mac.mm"
#endif

#include "../JUCE/modules/juce_gui_basics/native/juce_MouseCursor_mac.mm"

#elif JUCE_WINDOWS
#if JUCE_MODULE_AVAILABLE_juce_audio_plugin_client
#include <juce_audio_plugin_client/AAX/juce_AAX_Modifier_Injector.h>
#endif
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_ComInterfaces_windows.h"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_WindowsUIAWrapper_windows.h"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_AccessibilityElement_windows.h"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_UIAHelpers_windows.h"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_UIAProviders_windows.h"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_AccessibilityElement_windows.cpp"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_Accessibility_windows.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_WindowsHooks_windows.h"
#include "../JUCE/modules/juce_gui_basics/native/juce_WindowUtils_windows.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_Windowing_windows.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_WindowsHooks_windows.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_NativeMessageBox_windows.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_DragAndDrop_windows.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_FileChooser_windows.cpp"

#elif JUCE_LINUX || JUCE_BSD
#include "../JUCE/modules/juce_gui_basics/native/juce_XSymbols_linux.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_DragAndDrop_linux.cpp"

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE("-Wzero-as-null-pointer-constant")

#include "../JUCE/modules/juce_gui_basics/native/juce_ScopedWindowAssociation_linux.h"
#include "../JUCE/modules/juce_gui_basics/native/juce_WindowUtils_linux.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_Windowing_linux.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_NativeMessageBox_linux.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_XWindowSystem_linux.cpp"

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

#include "../JUCE/modules/juce_gui_basics/native/juce_FileChooser_linux.cpp"

#elif JUCE_ANDROID

#include "../JUCE/modules/juce_gui_basics/juce_core/files/juce_common_MimeTypes.h"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_Accessibility_android.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_WindowUtils_android.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_Windowing_android.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_NativeMessageBox_android.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_FileChooser_android.cpp"

#if JUCE_CONTENT_SHARING
#include "../JUCE/modules/juce_gui_basics/native/juce_ContentSharer_android.cpp"
#endif

#endif

//==============================================================================
// Depends on types defined in platform-specific windowing files
#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseCursor.cpp"

#if JUCE_UNIT_TESTS
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_AccessibilityTextHelpers_test.cpp"
#endif

//==============================================================================
#include "../JUCE/modules/juce_gui_basics/accessibility/juce_AccessibilityHandler.cpp"
#include "../JUCE/modules/juce_gui_basics/application/juce_Application.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_ArrowButton.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_Button.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_DrawableButton.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_HyperlinkButton.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_ImageButton.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_ShapeButton.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_TextButton.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_ToggleButton.cpp"
#include "../JUCE/modules/juce_gui_basics/buttons/juce_ToolbarButton.cpp"
#include "../JUCE/modules/juce_gui_basics/commands/juce_ApplicationCommandInfo.cpp"
#include "../JUCE/modules/juce_gui_basics/commands/juce_ApplicationCommandManager.cpp"
#include "../JUCE/modules/juce_gui_basics/commands/juce_ApplicationCommandTarget.cpp"
#include "../JUCE/modules/juce_gui_basics/commands/juce_KeyPressMappingSet.cpp"
#include "../JUCE/modules/juce_gui_basics/components/juce_Component.cpp"
#include "../JUCE/modules/juce_gui_basics/components/juce_ComponentListener.cpp"
#include "../JUCE/modules/juce_gui_basics/components/juce_FocusTraverser.cpp"
#include "../JUCE/modules/juce_gui_basics/components/juce_ModalComponentManager.cpp"
#include "../JUCE/modules/juce_gui_basics/desktop/juce_Desktop.cpp"
#include "../JUCE/modules/juce_gui_basics/desktop/juce_Displays.cpp"
#include "../JUCE/modules/juce_gui_basics/detail/juce_AccessibilityHelpers.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_Drawable.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableComposite.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableImage.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawablePath.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableRectangle.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableShape.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_DrawableText.cpp"
#include "../JUCE/modules/juce_gui_basics/drawables/juce_SVGParser.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_ContentSharer.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_DirectoryContentsDisplayComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_DirectoryContentsList.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileBrowserComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooser.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileChooserDialogBox.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileListComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileSearchPathListComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FileTreeComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_FilenameComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/filebrowser/juce_ImagePreviewComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/keyboard/juce_CaretComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/keyboard/juce_KeyListener.cpp"
#include "../JUCE/modules/juce_gui_basics/keyboard/juce_KeyPress.cpp"
#include "../JUCE/modules/juce_gui_basics/keyboard/juce_KeyboardFocusTraverser.cpp"
#include "../JUCE/modules/juce_gui_basics/keyboard/juce_ModifierKeys.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentAnimator.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentBoundsConstrainer.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_BorderedComponentBoundsConstrainer.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentBuilder.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_ComponentMovementWatcher.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_ConcertinaPanel.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_FlexBox.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_Grid.cpp"
#include "../JUCE/modules/juce_gui_basics/layout/juce_GridItem.cpp"
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
#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V1.cpp"
#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V2.cpp"
#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V3.cpp"
#include "../JUCE/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V4.cpp"
#include "../JUCE/modules/juce_gui_basics/menus/juce_BurgerMenuComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/menus/juce_MenuBarComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/menus/juce_MenuBarModel.cpp"
#include "../JUCE/modules/juce_gui_basics/menus/juce_PopupMenu.cpp"
#include "../JUCE/modules/juce_gui_basics/misc/juce_BubbleComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/misc/juce_DropShadower.cpp"
#include "../JUCE/modules/juce_gui_basics/misc/juce_FocusOutline.cpp"
#include "../JUCE/modules/juce_gui_basics/misc/juce_JUCESplashScreen.cpp"
#include "../JUCE/modules/juce_gui_basics/mouse/juce_ComponentDragger.cpp"
#include "../JUCE/modules/juce_gui_basics/mouse/juce_DragAndDropContainer.cpp"
#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseEvent.cpp"
#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseInactivityDetector.cpp"
#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseInputSource.cpp"
#include "../JUCE/modules/juce_gui_basics/mouse/juce_MouseListener.cpp"
#include "../JUCE/modules/juce_gui_basics/native/accessibility/juce_Accessibility.cpp"
#include "../JUCE/modules/juce_gui_basics/native/juce_ScopedDPIAwarenessDisabler.cpp"
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
#include "../JUCE/modules/juce_gui_basics/properties/juce_MultiChoicePropertyComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/properties/juce_PropertyComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/properties/juce_PropertyPanel.cpp"
#include "../JUCE/modules/juce_gui_basics/properties/juce_SliderPropertyComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/properties/juce_TextPropertyComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_ComboBox.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_ImageComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_Label.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_ListBox.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_ProgressBar.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_Slider.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_TableHeaderComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_TableListBox.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_TextEditor.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_Toolbar.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_ToolbarItemComponent.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_ToolbarItemPalette.cpp"
#include "../JUCE/modules/juce_gui_basics/widgets/juce_TreeView.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_NativeMessageBox.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_AlertWindow.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_CallOutBox.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_ComponentPeer.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_DialogWindow.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_DocumentWindow.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_MessageBoxOptions.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_ResizableWindow.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_ScopedMessageBox.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_ThreadWithProgressWindow.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_TooltipWindow.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_TopLevelWindow.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_VBlankAttachment.cpp"
#include "../JUCE/modules/juce_gui_basics/windows/juce_NativeScaleFactorNotifier.cpp"
// clang-format on
