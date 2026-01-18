#include "CharacterMovement.h"
#include <cmath>

using namespace DirectX;

CharacterMovement::CharacterMovement() {}

void CharacterMovement::SetMoveInput(float x, float z)
{
    velocity.x = x * moveSpeed;
    velocity.z = z * moveSpeed;

    // Update rotation only if there is significant input
    if (fabs(x) > 0.01f || fabs(z) > 0.01f)
    {
        // Calculate target angle based on input vector
        rotation.y = atan2f(x, z);
    }
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
    velocity.y += gravity * elapsedTime;

    // Integrate Position (Euler)
    position.x += velocity.x * elapsedTime;
    position.y += velocity.y * elapsedTime;
    position.z += velocity.z * elapsedTime;

    // Basic Ground Collision (Y = 0 plane)
    if (position.y < 0.0f)
    {
        position.y = 0.0f;
        velocity.y = 0.0f;
        isGrounded = true;
    }
    else
    {
        isGrounded = false;
    }
}

bool CharacterMovement::IsMoving() const
{
    return (fabs(velocity.x) > 0.01f || fabs(velocity.z) > 0.01f);
}