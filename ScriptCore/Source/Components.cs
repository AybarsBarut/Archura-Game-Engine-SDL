namespace Archura
{
    public abstract class Component
    {
        public Entity Entity { get; internal set; }
    }

    public class RigidBody : Component
    {
        public void ApplyForce(Vector3 force)
        {
            InternalCalls.RigidBody_ApplyForce(Entity.ID, ref force);
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
