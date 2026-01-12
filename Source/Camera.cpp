#include "Camera.h"
#include <cmath>

using namespace DirectX;

Camera::Camera() :
    position({ 0, 0, -10 }),
    rotation({ 0, 0, 0 }),
    front({ 0, 0, 1 }),
    right({ 1, 0, 0 }),
    up({ 0, 1, 0 })
{
    XMStoreFloat4x4(&view, XMMatrixIdentity());
    XMStoreFloat4x4(&projection, XMMatrixIdentity());
    UpdateViewMatrix();
}

Camera::~Camera() {}

// --- Transform Operations ---

void Camera::SetPosition(const XMFLOAT3& pos)
{
    this->position = pos;
    UpdateViewMatrix();
}

void Camera::SetPosition(float x, float y, float z)
{
    SetPosition({ x, y, z });
}

void Camera::Translate(const XMFLOAT3& translation)
{
    XMVECTOR vPos = XMLoadFloat3(&position);
    XMVECTOR vFront = XMLoadFloat3(&front);
    XMVECTOR vRight = XMLoadFloat3(&right);
    XMVECTOR vUp = XMLoadFloat3(&up);

    vPos = XMVectorAdd(vPos, XMVectorScale(vRight, translation.x));
    vPos = XMVectorAdd(vPos, XMVectorScale(vUp, translation.y));
    vPos = XMVectorAdd(vPos, XMVectorScale(vFront, translation.z));

    XMStoreFloat3(&position, vPos);
    UpdateViewMatrix();
}

void Camera::Translate(float x, float y, float z)
{
    Translate({ x, y, z });
}

void Camera::SetRotation(const XMFLOAT3& rot)
{
    this->rotation = rot;

    // Clamp Pitch to prevent gimbal lock
    constexpr float limit = XMConvertToRadians(90.0f);
    if (this->rotation.x > limit) this->rotation.x = limit;
    if (this->rotation.x < -limit) this->rotation.x = -limit;

    UpdateViewMatrix();
}

void Camera::SetRotation(float x, float y, float z)
{
    SetRotation({ x, y, z });
}

void Camera::LookAt(const XMFLOAT3& target)
{
    XMVECTOR vPos = XMLoadFloat3(&position);
    XMVECTOR vTarget = XMLoadFloat3(&target);
    XMVECTOR vDir = XMVectorSubtract(vTarget, vPos);
    vDir = XMVector3Normalize(vDir);

    XMFLOAT3 dir;
    XMStoreFloat3(&dir, vDir);

    float pitch = asinf(-dir.y);
    float yaw = atan2f(dir.x, dir.z);

    SetRotation({ pitch, yaw, 0.0f });
}

void Camera::SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up)
{
    this->position = eye;
    LookAt(focus);
}

// --- Projection Settings ---

void Camera::SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
    this->fovY = fovY;
    this->nearZ = nearZ;
    this->farZ = farZ;

    XMMATRIX matProj = XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
    XMStoreFloat4x4(&projection, matProj);
}

void Camera::SetAspectRatio(float aspect)
{
    SetPerspectiveFov(this->fovY, aspect, this->nearZ, this->farZ);
}

void Camera::SetOffCenterProjection(float left, float right, float bottom, float top, float nZ, float fZ)
{
    this->nearZ = nZ;
    this->farZ = fZ;
    XMMATRIX matProj = XMMatrixPerspectiveOffCenterLH(left, right, bottom, top, nZ, fZ);
    XMStoreFloat4x4(&projection, matProj);
}

void Camera::SetOrthographic(float viewWidth, float viewHeight, float nearZ, float farZ)
{
    this->nearZ = nearZ;
    this->farZ = farZ;
    // Membuat matriks proyeksi ortografis (kotak, bukan piramida)
    XMMATRIX matProj = XMMatrixOrthographicLH(viewWidth, viewHeight, nearZ, farZ);
    XMStoreFloat4x4(&projection, matProj);
}

// --- Helpers & Internals ---

DirectX::XMFLOAT3 Camera::GetFocus() const
{
    return { position.x + front.x, position.y + front.y, position.z + front.z };
}

void Camera::UpdateViewMatrix()
{
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);

    XMVECTOR vFront = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), matRot);
    XMVECTOR vRight = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), matRot);
    XMVECTOR vUp = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), matRot);

    XMStoreFloat3(&front, vFront);
    XMStoreFloat3(&right, vRight);
    XMStoreFloat3(&up, vUp);

    XMVECTOR vPos = XMLoadFloat3(&position);
    XMStoreFloat4x4(&view, XMMatrixLookToLH(vPos, vFront, vUp));
}