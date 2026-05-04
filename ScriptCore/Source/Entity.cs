using System;

namespace Archura
{
    public class Entity
    {
        public ulong ID { get; set; }

        protected Entity() { ID = 0; } // ID will be set by runtime

        public bool IsValid => ID != 0 && InternalCalls.Entity_Exists(ID);

        public string Name
        {
            get { return InternalCalls.Entity_GetName(ID); }
            set { InternalCalls.Entity_SetName(ID, value); }
        }

        public TransformComponent Transform => GetComponent<TransformComponent>();

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

        public static Entity Find(string name)
        {
            ulong id = InternalCalls.Entity_FindByName(name);
            return id == 0 ? null : new Entity { ID = id };
        }

        // Called when the entity is created
        public virtual void OnCreate() {}

        // Unity-style lifecycle alias. Prefer this in new scripts.
        public virtual void OnStart() {}

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

        public bool HasComponent<T>() where T : Component
        {
            string componentName = typeof(T).Name;
            return InternalCalls.Entity_HasComponent(ID, componentName);
        }
    }

    public class MonoBehaviour : Entity
    {
        public float DeltaTime { get; private set; }

        public sealed override void OnUpdate(float ts)
        {
            DeltaTime = ts;
            Update();
        }

        public virtual void Update() {}
    }
}
