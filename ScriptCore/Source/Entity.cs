using System;

namespace Archura
{
    public class Entity
    {
        public ulong ID { get; set; }

        protected Entity() { ID = 0; } // ID will be set by runtime

        public Vector3 Translation
        {
            get { InternalCalls.Transform_GetPosition(ID, out Vector3 result); return result; }
            set { InternalCalls.Transform_SetPosition(ID, ref value); }
        }

        public Vector3 Rotation
        {
            get { InternalCalls.Transform_GetRotation(ID, out Vector3 result); return result; }
            set { InternalCalls.Transform_SetRotation(ID, ref value); }
        }

        public Vector3 Scale
        {
            get { InternalCalls.Transform_GetScale(ID, out Vector3 result); return result; }
            set { InternalCalls.Transform_SetScale(ID, ref value); }
        }

        // --- Lifecycle ---
        public static Entity Instantiate(string name = "New Entity")
        {
            ulong newID = InternalCalls.Entity_Create(name);
            Entity newEntity = new Entity();
            newEntity.ID = newID;
            return newEntity;
        }

        public void Destroy()
        {
            InternalCalls.Entity_Destroy(ID);
            ID = 0; // Invalidate
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
