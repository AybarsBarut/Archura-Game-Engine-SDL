using System;
using System.Runtime.CompilerServices;

namespace Archura
{
    public static class InternalCalls
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void NativeLog(string text, int parameter);

        #region Entity Lifecycle
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern ulong Entity_Create(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Entity_Destroy(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern bool Entity_Exists(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern ulong Entity_FindByName(string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern string Entity_GetName(ulong entityID);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Entity_SetName(ulong entityID, string name);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern bool Entity_HasComponent(ulong entityID, string componentName);
        #endregion

        #region Transform
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_GetPosition(ulong entityID, out Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_SetPosition(ulong entityID, ref Vector3 position);
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_GetRotation(ulong entityID, out Vector3 rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_SetRotation(ulong entityID, ref Vector3 rotation);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_GetScale(ulong entityID, out Vector3 scale);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_SetScale(ulong entityID, ref Vector3 scale);
        #endregion

        #region Input
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern bool Input_IsKeyDown(int keycode);
        #endregion

        #region Physics (RigidBody)
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_ApplyForce(ulong entityID, ref Vector3 force);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_GetVelocity(ulong entityID, out Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetVelocity(ulong entityID, ref Vector3 velocity);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_SetGravityEnabled(ulong entityID, bool enabled);
        #endregion

        #region MeshRenderer
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void MeshRenderer_GetColor(ulong entityID, out Vector3 color);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void MeshRenderer_SetColor(ulong entityID, ref Vector3 color);
        #endregion

        #region Audio
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_Play(ulong entityID);
        #endregion

        #region Camera
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern ulong Camera_Create(ref Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetActive(ulong cameraHandle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Camera_GetPosition(ulong cameraHandle, out Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetPosition(ulong cameraHandle, ref Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetPitch(ulong cameraHandle);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetYaw(ulong cameraHandle);
        #endregion
    }


}
