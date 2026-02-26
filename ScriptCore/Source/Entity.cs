using System;

namespace Archura
{
    public class Entity
    {
        public ulong ID { get; set; }

        protected Entity() { ID = 0; } // ID will be set by runtime

        public Vector3 Translation
        {
            get
            {
                InternalCalls.Transform_GetPosition(ID, out Vector3 result);
                return result;
            }
            set
            {
                InternalCalls.Transform_SetPosition(ID, ref value);
            }
        }

        // Called when the entity is created
        public virtual void OnCreate() {}

        // Called every frame
        public virtual void OnUpdate(float ts) {}

        protected void Log(string text)
        {
            InternalCalls.NativeLog(text, 0);
        }

        public T GetComponent<T>() where T : Component, new()
        {
            T component = new T();
            component.Entity = this;
            return component;
        }
    }
}
