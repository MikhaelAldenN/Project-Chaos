#pragma once

#include <DirectXMath.h> 
#include <memory>
#include <vector>
#include "System/Graphics.h"

class Enemy;
class ShapeRenderer;

enum class EnemyType
{
    Paddle,
    Ball,
    Pentagon // [BARU] Tambahkan ini
};

enum class AttackType
{
    None,
    Static,
    Tracking,
    TrackingHorizontal,
    TrackingRandom,
    RadialBurst // [BARU] Serangan menyebar ke segala arah
};

enum class MoveDir
{
    None,
    Left,
    Right
};

struct EnemySpawnConfig
{
    DirectX::XMFLOAT3 Position; 
    DirectX::XMFLOAT3 Rotation; 
    DirectX::XMFLOAT4 Color;    
    EnemyType Type = EnemyType::Paddle;
    AttackType AttackBehavior = AttackType::None;
    MoveDir Direction = MoveDir::None;
    float MinX = 0.0f;
    float MaxX = 0.0f;
    float MinZ = 0.0f; 
    float MaxZ = 0.0f; 

    DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };
    float BaseSpeed = 2.0f;         // Game Beyond Enemy Speed
};

namespace EnemyLevelData
{
    // ==========================================
    // COLOR PRESETS
    // ==========================================
    static const DirectX::XMFLOAT4 Blue         = { 0.0f, 0.0f, 0.8f, 1.0f };
    static const DirectX::XMFLOAT4 PaleYellow   = { 0.76f, 0.74f, 0.56f, 1.0f };

    // ==========================================
    // ROTATION PRESETS
    // ==========================================
    namespace Rot
    {
        static const DirectX::XMFLOAT3 Backward = { 0.0f, 0.0f, 0.0f };
        static const DirectX::XMFLOAT3 Forward  = { 0.0f, 180.0f, 0.0f };
        static const DirectX::XMFLOAT3 Left     = { 0.0f, 90.0f, 0.0f };
        static const DirectX::XMFLOAT3 Right    = { 0.0f, -90.0f, 0.0f };
    }

    // ==========================================
    // MASTER SPAWN LIST
    // ==========================================
    static const std::vector<EnemySpawnConfig> Spawns =
    {
        // ------------------------------------------
        // PADDLE LIST
        // ------------------------------------------
        // Paddle 1
        { { 4.6f, 0.0f, -40.3f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 2
        { { -4.7f, 0.0f, -52.9f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 3
        { { -2.9f, 0.0f, -157.2f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Tracking, MoveDir::None, 0, 0, 0, 0, {1,1,1}, 0.0f },
        // Paddle 4
        { { 1.3f, 0.0f, -192.7f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingHorizontal, MoveDir::Left, -4.4f, 4.3f },
        // Paddle 5
        { { 28.1f, 0.0f, -226.9f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 6
        { { 28.1f, 0.0f, -230.0f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 7
        { { 28.1f, 0.0f, -233.2f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 8
        { { 28.1f, 0.0f, -236.3f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 9
        { { 28.1f, 0.0f, -239.4f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 10
        { { 28.1f, 0.0f, -242.5f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 11
        { { 28.1f, 0.0f, -245.7f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 12
        { { 28.1f, 0.0f, -249.1f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 13
        { { 7.2f, 0.0f, -341.5f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Tracking, MoveDir::None, 0, 0, 0, 0, {1,1,1}, 0.0f },
        // Paddle 14
        { { 4.0f, 0.0f, -354.2f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingHorizontal, MoveDir::Left, -2.0f, 2.0f },
        // Paddle 15
        { { 10.0f, 0.0f, -354.2f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingHorizontal, MoveDir::Right, -2.0f, 2.0f },
        // Paddle 16
        { { 8.0f, 0.0f, -377.7f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Tracking, MoveDir::None, 0, 0, 0, 0, {1,1,1}, 0.0f },
        // Paddle 17
        { { 15.4f, 0.0f, -381.7f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingRandom, MoveDir::None, -5.0f, 5.0f, -10.0f, 10.0f },
        // Paddle 18
        { { 0.4f, 0.0f, -381.7f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingRandom, MoveDir::None, -5.0f, 5.0f, -10.0f, 10.0f },
        // Paddle 19
        { { 0.4f, 0.0f, -397.5f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingRandom, MoveDir::None, -5.0f, 5.0f, -10.0f, 10.0f },
        // Paddle 20
        { { 15.4f, 0.0f, -397.5f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingRandom, MoveDir::None, -5.0f, 5.0f, -10.0f, 10.0f },
        // Paddle 21
        { { 9.1f, 0.0f, -427.6f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 22
        { { 6.3f, 0.0f, -426.6f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 23
        { { 3.5f, 0.0f, -425.4f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 24
        { { 1.0f, 0.0f, -424.1f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 25
        { { 13.9f, 0.0f, -430.5f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 26
        { { 11.1f, 0.0f, -432.2f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 27
        { { 7.8f, 0.0f, -433.6f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 28
        { { 4.6f, 0.0f, -435.6f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 29
        { { 1.1f, 0.0f, -411.7f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingHorizontal, MoveDir::Right, -5.0f, 5.0f },
        // Paddle 30
        { { 14.7f, 0.0f, -411.7f }, Rot::Backward, Blue, EnemyType::Paddle, AttackType::TrackingHorizontal, MoveDir::Left, -5.0f, 5.0f },
        // Paddle 31
        { { 10.7f, 0.0f, -545.1f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 32
        { { -24.1f, 0.0f, -559.9f }, Rot::Left, Blue, EnemyType::Paddle, AttackType::Static },
        // Paddle 33
        { { 4.8f, 0.0f, -568.1f }, Rot::Right, Blue, EnemyType::Paddle, AttackType::Static },

        // ------------------------------------------
        // BALL LIST
        // ------------------------------------------
        // Ball 1
        { { 14.1f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 2
        { { 13.1f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 3
        { { 12.0f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 4
        { { 10.9f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 5
        { { 0.3f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 6
        { { 1.1f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 7
        { { 2.0f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 8
        { { 2.9f, 0.0f, -303.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 9
        { { 14.1f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 10
        { { 13.1f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 11
        { { 12.0f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 12
        { { 10.9f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 13
        { { 0.2f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 14
        { { 1.1f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 15
        { { 2.0f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 16
        { { 2.9f, 0.0f, -304.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 17
        { { 14.1f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 18
        { { 13.1f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 19
        { { 12.0f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 20
        { { 10.9f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 21
        { { 0.2f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 22
        { { 1.2f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 23
        { { 2.1f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 24
        { { 3.0f, 0.0f, -305.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 25
        { { 8.0f, 0.0f, -379.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 26
        { { 8.0f, 0.0f, -380.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 27
        { { 8.0f, 0.0f, -381.3f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 28
        { { 8.0f, 0.0f, -382.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 29
        { { 8.0f, 0.0f, -383.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 30
        { { 8.0f, 0.0f, -384.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 31
        { { 8.0f, 0.0f, -385.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 32
        { { 8.0f, 0.0f, -393.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 33
        { { 8.0f, 0.0f, -394.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 34
        { { 8.0f, 0.0f, -396.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 35
        { { 8.0f, 0.0f, -397.3f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 36
        { { 8.0f, 0.0f, -398.5f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 37
        { { 8.0f, 0.0f, -399.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 38
        { { 8.0f, 0.0f, -401.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 39
        { { -0.7f, 0.0f, -412.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 40
        { { 16.4f, 0.0f, -412.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 41
        { { -0.7f, 0.0f, -414.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 42
        { { -0.7f, 0.0f, -416.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 43
        { { -0.7f, 0.0f, -418.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 44
        { { -0.7f, 0.0f, -420.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 45
        { { -0.7f, 0.0f, -421.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 46
        { { -0.7f, 0.0f, -423.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 47
        { { -0.7f, 0.0f, -425.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 48
        { { -0.7f, 0.0f, -427.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 49
        { { -0.7f, 0.0f, -428.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 50
        { { -0.7f, 0.0f, -430.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 51
        { { -0.7f, 0.0f, -432.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 52
        { { -0.7f, 0.0f, -434.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 53
        { { -0.7f, 0.0f, -436.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 54
        { { -0.7f, 0.0f, -438.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 55
        { { -0.7f, 0.0f, -440.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 56
        { { -0.7f, 0.0f, -442.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 57
        { { -0.7f, 0.0f, -444.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 58
        { { -0.7f, 0.0f, -447.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 59
        { { -0.7f, 0.0f, -449.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 60
        { { -0.7f, 0.0f, -451.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 61
        { { 16.4f, 0.0f, -414.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 62
        { { 16.4f, 0.0f, -416.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 63
        { { 16.4f, 0.0f, -418.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 64
        { { 16.4f, 0.0f, -420.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 65
        { { 16.4f, 0.0f, -421.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 66
        { { 16.4f, 0.0f, -423.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 67
        { { 16.4f, 0.0f, -425.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 68
        { { 16.4f, 0.0f, -427.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 69
        { { 16.4f, 0.0f, -428.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 70
        { { 16.4f, 0.0f, -430.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 71
        { { 16.4f, 0.0f, -432.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 72
        { { 16.4f, 0.0f, -434.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 73
        { { 16.4f, 0.0f, -436.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 74
        { { 16.4f, 0.0f, -438.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 75
        { { 16.4f, 0.0f, -440.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 76
        { { 16.4f, 0.0f, -442.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 77
        { { 16.4f, 0.0f, -444.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 78
        { { 16.4f, 0.0f, -447.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 79
        { { 16.4f, 0.0f, -449.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 80
        { { 16.4f, 0.0f, -451.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 81
        { { -0.1f, 0.0f, -477.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 82
        { { -1.2f, 0.0f, -480.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 83
        { { -2.3f, 0.0f, -484.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 84
        { { -3.5f, 0.0f, -487.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 85
        { { -4.6f, 0.0f, -491.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 86
        { { 4.3f, 0.0f, -477.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 87
        { { 8.8f, 0.0f, -477.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 88
        { { 12.6f, 0.0f, -477.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 89
        { { 16.8f, 0.0f, -477.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 90
        { { 2.5f, 0.0f, -480.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 91
        { { 7.2f, 0.0f, -480.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 92
        { { 11.4f, 0.0f, -480.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 93
        { { 15.2f, 0.0f, -480.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 94
        { { 12.2f, 0.0f, -486.5f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 95
        { { 10.8f, 0.0f, -486.5f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 96
        { { 9.6f, 0.0f, -486.5f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 97
        { { 13.7f, 0.0f, -486.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 98
        { { 8.4f, 0.0f, -483.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 99
        { { 6.5f, 0.0f, -484.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 100
        { { 4.7f, 0.0f, -484.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 101
        { { 3.1f, 0.0f, -486.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 102
        { { 2.9f, 0.0f, -489.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 103
        { { 4.8f, 0.0f, -491.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 104
        { { 6.8f, 0.0f, -492.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 105
        { { 8.2f, 0.0f, -492.2f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 106
        { { 9.8f, 0.0f, -489.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 107
        { { 11.1f, 0.0f, -489.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 108
        { { 12.5f, 0.0f, -489.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 109
        { { 13.7f, 0.0f, -489.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 110
        { { 4.5f, 0.0f, -506.3f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 111
        { { 4.5f, 0.0f, -510.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 112
        { { -3.4f, 0.0f, -510.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 113
        { { -3.3f, 0.0f, -506.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 114
        { { -5.3f, 0.0f, -508.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 115
        { { -7.3f, 0.0f, -506.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 116
        { { -8.9f, 0.0f, -508.9f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 117
        { { -10.9f, 0.0f, -507.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 118
        { { -10.7f, 0.0f, -511.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 119
        { { -6.9f, 0.0f, -511.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 120
        { { 7.2f, 0.0f, -508.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 121
        { { 8.9f, 0.0f, -506.4f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 122
        { { 11.5f, 0.0f, -508.5f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 123
        { { 13.5f, 0.0f, -506.3f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 124
        { { 14.1f, 0.0f, -510.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 125
        { { 10.0f, 0.0f, -510.6f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 126
        { { 16.3f, 0.0f, -523.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 127
        { { 14.7f, 0.0f, -523.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 128
        { { 13.1f, 0.0f, -523.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 129
        { { 11.4f, 0.0f, -523.7f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 130
        { { 11.4f, 0.0f, -527.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 131
        { { 13.2f, 0.0f, -527.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 132
        { { 14.9f, 0.0f, -527.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 133
        { { 16.6f, 0.0f, -527.1f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 134
        { { -12.3f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 135
        { { -10.8f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 136
        { { -9.2f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 137
        { { -7.3f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 138
        { { -5.4f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 139
        { { -3.4f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 140
        { { -1.3f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 141
        { { 0.9f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 142
        { { 3.4f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 143
        { { 5.8f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 144
        { { 8.1f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 145
        { { 10.4f, 0.0f, -536.8f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 146
        { { -15.0f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 147
        { { -13.4f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 148
        { { -11.8f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 149
        { { -10.1f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 150
        { { -8.4f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 151
        { { -6.8f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 152
        { { -4.9f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 153
        { { -3.1f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 154
        { { -1.1f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 155
        { { 0.7f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 156
        { { 2.6f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 157
        { { 4.4f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
        // Ball 158
        { { 6.2f, 0.0f, -551.0f }, Rot::Backward, PaleYellow, EnemyType::Ball, AttackType::None },
    };
}

class EnemyManager
{
public:
    EnemyManager();
    ~EnemyManager();

    void Initialize(ID3D11Device* device);
    void Update(float elapsedTime, Camera* camera, const DirectX::XMFLOAT3& playerPos, bool allowAttack = true);
    void Render(ModelRenderer* renderer, Camera* camera = nullptr);
    void RenderDebug(ShapeRenderer* renderer);
    void RespawnEnemyAs(size_t index, AttackType attack, MoveDir dir = MoveDir::None, float minX = 0, float maxX = 0, float minZ = 0, float maxZ = 0);
    void SpawnEnemy(const EnemySpawnConfig& config);

    std::vector<std::unique_ptr<Enemy>>& GetEnemies() { return m_enemies; }

private:
    std::vector<std::unique_ptr<Enemy>> m_enemies;
};