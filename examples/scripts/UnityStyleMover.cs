using Archura;

namespace Archura
{
    // Attach this class name to an entity's Script component: UnityStyleMover
    public class UnityStyleMover : MonoBehaviour
    {
        public float Speed = 4.0f;
        public float BobHeight = 0.35f;

        private Vector3 _start;
        private float _time;

        public override void OnStart()
        {
            _start = Transform.Position;
            Log($"Script attached to {Name} at {_start}");
        }

        public override void Update()
        {
            _time += DeltaTime;

            Vector3 p = _start;
            p.X += Speed * DeltaTime;
            p.Y += (float)System.Math.Sin(_time * 3.0f) * BobHeight;
            Transform.Position = p;

            if (HasComponent<MeshRenderer>())
            {
                GetComponent<MeshRenderer>().Color = new Vector3(0.35f, 0.65f, 1.0f);
            }
        }
    }
}
