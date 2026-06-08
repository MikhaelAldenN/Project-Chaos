#pragma once

class SceneGame;

class GameBreakerGUI
{
public:
    static void Draw(SceneGame* scene);

private:
    // Fungsi-fungsi internal per Tab
    static void DrawCameraTab(SceneGame* scene);
    static void DrawPostProcessTab(SceneGame* scene);
    static void DrawObjectColorTab(SceneGame* scene);
    static void DrawObjectTransformTab(SceneGame* scene);
};