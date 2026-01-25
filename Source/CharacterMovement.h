#pragma once
#include <DirectXMath.h>

class CharacterMovement
{
public:
    CharacterMovement();
    ~CharacterMovement() = default;

    // Physics update loop
    void Update(float elapsedTime);

    // Input handling (Range: -1.0 to 1.0)
    void SetMoveInput(float x, float z);
    void Jump();

    // Getters & Setters
    const DirectX::XMFLOAT3& GetPosition() const { return position; }
    void SetPosition(const DirectX::XMFLOAT3& pos) { position = pos; }

    const DirectX::XMFLOAT3& GetRotation() const { return rotation; }
    void SetRotation(const DirectX::XMFLOAT3& rot) { rotation = rot; }

    float GetRotationY() const { return rotation.y; }
    void SetRotationY(float rad) { rotation.y = rad; }
    void SetVelocity(const DirectX::XMFLOAT3& vel) { velocity = vel; }
    void SetVelocityY(float y) { velocity.y = y; }
    void SetVelocityX(float x) { velocity.x = x; }
    void SetVelocityZ(float z) { velocity.z = z; }
    void SetGravityEnabled(bool enable) { useGravity = enable; }

    const DirectX::XMFLOAT3& GetVelocity() const { return velocity; }

    // Status checks
    bool IsGravityEnabled() const { return useGravity; }
    bool IsGrounded() const { return isGrounded; }
    bool IsMoving() const;

private:
    // Physics State
    DirectX::XMFLOAT3 position = { 0, 0, 0 };
    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
    DirectX::XMFLOAT3 rotation = { 0, 0, 0 };

    // Parameters
    float moveSpeed = 5.0f;
    float jumpForce = 8.0f;
    float gravity = -20.0f;
    bool isGrounded = false;
    bool useGravity = true;
};