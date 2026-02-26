using System;

namespace Archura
{
    public class Camera
    {
        private ulong m_NativePtr;

        public Camera(Vector3 position)
        {
            m_NativePtr = InternalCalls.Camera_Create(ref position);
        }

        public void SetActive()
        {
            InternalCalls.Camera_SetActive(m_NativePtr);
        }

        public Vector3 Position
        {
            get
            {
                InternalCalls.Camera_GetPosition(m_NativePtr, out Vector3 result);
                return result;
            }
            set
            {
                InternalCalls.Camera_SetPosition(m_NativePtr, ref value);
            }
        }

        public float Pitch => InternalCalls.Camera_GetPitch(m_NativePtr);
        public float Yaw => InternalCalls.Camera_GetYaw(m_NativePtr);
    }
}
