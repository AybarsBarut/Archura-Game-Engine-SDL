namespace Archura
{
    public abstract class Component
    {
        public Entity Entity { get; internal set; }
    }

    public class TransformComponent : Component
    {
        public Vector3 Position
        {
            get { InternalCalls.Transform_GetPosition(Entity.ID, out Vector3 result); return result; }
            set { InternalCalls.Transform_SetPosition(Entity.ID, ref value); }
        }

        public Vector3 Rotation
        {
            get { InternalCalls.Transform_GetRotation(Entity.ID, out Vector3 result); return result; }
            set { InternalCalls.Transform_SetRotation(Entity.ID, ref value); }
        }

        public Vector3 Scale
        {
            get { InternalCalls.Transform_GetScale(Entity.ID, out Vector3 result); return result; }
            set { InternalCalls.Transform_SetScale(Entity.ID, ref value); }
        }
    }

    public class RigidBody : Component
    {
        public Vector3 Velocity
        {
            get { InternalCalls.RigidBody_GetVelocity(Entity.ID, out Vector3 result); return result; }
            set { InternalCalls.RigidBody_SetVelocity(Entity.ID, ref value); }
        }

        public bool GravityEnabled
        {
            set { InternalCalls.RigidBody_SetGravityEnabled(Entity.ID, value); }
        }

        public void ApplyForce(Vector3 force)
        {
            InternalCalls.RigidBody_ApplyForce(Entity.ID, ref force);
        }
    }

    public class MeshRenderer : Component
    {
        public Vector3 Color
        {
            get { InternalCalls.MeshRenderer_GetColor(Entity.ID, out Vector3 result); return result; }
            set { InternalCalls.MeshRenderer_SetColor(Entity.ID, ref value); }
        }
    }

    public class AudioSource : Component
    {
        public void Play()
        {
            InternalCalls.AudioSource_Play(Entity.ID);
        }
    }
}
