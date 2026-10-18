# Project and scene persistence

Both public entry points use one scene codec: `GameSaveManager::WriteEntities` /
`LoadEntities`. The `.project` wrapper retains project metadata; `.scene` retains
project-name/timestamp metadata. New files declare `format_version: 1`; unversioned
legacy files remain readable. Future format versions are rejected.

The codec supports names, local transforms, hierarchy, mesh source recipes
(cube, plane, sphere, capsule, stairs, ramp, OBJ/FBX), texture paths/color,
colliders, lights, rigid bodies, health, script class names, spawn points, skybox
face paths, both existing audio source component configurations and surface type.
Runtime script instance IDs and audio playback handles/state are reset. Asset
paths retain existing working-directory-relative semantics; packaging/relocation
is not implemented. Texture cache keys use the canonical complete path.

File IDs establish hierarchy references and are remapped to fresh runtime entity
handles on load. They are not persistent UUIDs across loads. Hierarchies deeper
than 127 links, missing parents, duplicate IDs and cycles are rejected.

Entities default to authored ownership. Application bootstrap explicitly marks
its Player, Skybox, Sun, AmbientLight, Floor and boundary walls as engine-owned.
`.scene` omits and preserves these by ownership, never by display name; authored
entities named Player/Floor and renamed engine services therefore behave correctly.
Project files include every entity and persist optional boolean `engine_owned`
metadata. Legacy records without it migrate as authored, including system-like
names. `.scene` input cannot promote content to retained engine ownership.
A `.scene` save rejects hierarchy edges crossing the ownership boundary instead
of writing an unreadable file. Data already omitted by older saves cannot be
recovered by this migration.

Unknown serialized component names and unhandled live component types are
rejected. This includes Weapon/Projectile/Particle/Lifetime and custom component
types until dedicated codecs exist. Edited/custom geometry, assigned shaders and
textures without source paths also fail saving explicitly. Rejection leaves the
previous destination intact. Materials beyond the existing color/texture fields
are not supported.

JSON parsing validates the complete document, Unicode escapes/UTF-8, duplicate
keys, numeric grammar and finite values. Files are capped at 32 MiB, entities at
100,000, nesting at 64 and JSON values at 2,000,000. Field schemas and resource
paths are validated before load mutation or save replacement.

Loads construct all new components/resources and hierarchy while retaining the
old entities. Any reconstruction failure removes only the new entities. Commit
then destroys the old replaceable entities. Failed attempts can consume runtime
entity IDs, but preserve existing handles and project metadata.

Saves write a sibling temporary file, flush and close it, copy the preceding
destination to `.bak`, then atomically replace the destination (Windows
`MoveFileExW`, POSIX rename). This is process-failure protection, not a complete
power-loss durability guarantee. A backup-copy failure cancels replacement.
Recovery is explicit: load the `.bak` file through the same loader; malformed
primary files are never silently replaced by a backup load.

Regression coverage lives in ProjectSerializerTests and GameSaveManagerTests;
headless mesh reconstruction checks CPU-side identity and data, not GPU rendering
or the full editor close/reopen acceptance scenario.
