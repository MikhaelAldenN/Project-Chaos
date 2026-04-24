#include "CharacterMovement.h"
#include <cmath>

using namespace DirectX;

CharacterMovement::CharacterMovement() {}

void CharacterMovement::SetMoveInput(float x, float z)
{
    // Get current rotation (Yaw)
    float yaw = rotation.y;
    float sinY = sinf(yaw);
    float cosY = cosf(yaw);

    // Rotate Input Vector (Local Space -> World Space)
    // Formula to transform inputs based on character facing direction:
    float worldX = (x * cosY) + (z * sinY);
    float worldZ = (x * -sinY) + (z * cosY);

    velocity.x = worldX * moveSpeed;
    velocity.z = worldZ * moveSpeed;
}

void CharacterMovement::Jump()
{
    if (isGrounded)
    {
        velocity.y = jumpForce;
        isGrounded = false;
    }
}

void CharacterMovement::Update(float elapsedTime)
{
    // Apply Gravity
    if (useGravity)
    {
        velocity.y += gravity * elapsedTime;
    }

    // Integrate Position (Euler)
    position.x += velocity.x * elapsedTime;
    position.y += velocity.y * elapsedTime;
    position.z += velocity.z * elapsedTime;

    // Basic Ground Collision (Y = 0 plane)
    //if (position.y < 0.0f)
    //{
    //    position.y = 0.0f;
    //    velocity.y = 0.0f;
    //    isGrounded = true;
    //}
    //else
    //{
    //    isGrounded = false;
    //}
}

bool CharacterMovement::IsMoving() const
{
    return (fabs(velocity.x) > 0.01f || fabs(velocity.z) > 0.01f);
}