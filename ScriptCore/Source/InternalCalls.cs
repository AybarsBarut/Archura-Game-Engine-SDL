using System;
using System.Runtime.CompilerServices;

namespace Archura
{
    public static class InternalCalls
    {
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void NativeLog(string text, int parameter);

        #region Transform
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_GetPosition(ulong entityID, out Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Transform_SetPosition(ulong entityID, ref Vector3 position);
        #endregion

        #region Input
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern bool Input_IsKeyDown(int keycode);
        #endregion

        #region Physics
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void RigidBody_ApplyForce(ulong entityID, ref Vector3 force);
        #endregion

        #region Audio
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void AudioSource_Play(ulong entityID);
        #endregion

        #region Camera
        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern ulong Camera_Create(ref Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetActive(ulong cameraPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Camera_GetPosition(ulong cameraPtr, out Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern void Camera_SetPosition(ulong cameraPtr, ref Vector3 position);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetPitch(ulong cameraPtr);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public static extern float Camera_GetYaw(ulong cameraPtr);
        #endregion
    }


}
