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
    static void DrawPostProcessTab(SceneGameBreaker* scene);
    static void DrawObjectColorTab(SceneGameBreaker* scene);
    static void DrawObjectTransformTab(SceneGameBreaker* scene);
};