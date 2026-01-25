#pragma once

// Forward Declaration agar tidak terjadi circular dependency
class SceneGameBreaker;

class GameBreakerGUI
{
public:
    // Fungsi utama yang dipanggil di SceneGameBreaker::DrawGUI()
    static void Draw(SceneGameBreaker* scene);

private:
    // Fungsi-fungsi internal per Tab
    static void DrawCameraTab(SceneGameBreaker* scene);
    static void DrawUI_LayoutTab(SceneGameBreaker* scene); // Tab baru untuk Tutorial UI
    static void DrawPostProcessTab(SceneGameBreaker* scene);
    static void DrawObjectColorTab(SceneGameBreaker* scene); // Tab baru untuk Color
	static void DrawObjectTransformTab(SceneGameBreaker* scene); // Tab baru untuk Transform
    static void DrawSpriteTab(SceneGameBreaker* scene);

    // Helper untuk edit layout UI
    static void ImGuiEditPanel(const char* label, float& x, float& y, float& scale, float& spacing, float* color);
};