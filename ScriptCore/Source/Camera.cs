using System;

namespace Archura
{
    public class Camera
    {
        private ulong m_NativeHandle;

        public Camera(Vector3 position)
        {
            m_NativeHandle = InternalCalls.Camera_Create(ref position);
        }

        public void SetActive()
        {
            InternalCalls.Camera_SetActive(m_NativeHandle);
        }

        public Vector3 Position
        {
            get
            {
                InternalCalls.Camera_GetPosition(m_NativeHandle, out Vector3 result);
                return result;
            }
            set
            {
                InternalCalls.Camera_SetPosition(m_NativeHandle, ref value);
            }
        }

        public float Pitch => InternalCalls.Camera_GetPitch(m_NativeHandle);
        public float Yaw => InternalCalls.Camera_GetYaw(m_NativeHandle);
    }
}
