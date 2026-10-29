# Dear ImGui dependency

Archura uses upstream **v1.89.9-docking**, commit
`1d8e48c161370c37628c4f37f3f87cb19fbcb723` from
https://github.com/ocornut/imgui/tree/v1.89.9-docking.

Core source/headers and the SDL2/OpenGL3 backends are kept together at that
revision. `imconfig.h`, the Windows include-order wrappers under `src/`, and
the existing MIT license are retained. This enables panel docking without a
major-version API migration. External platform windows are disabled; floating
panels remain inside the SDL window. Replacing the previously formatted widget
source with upstream source accounts for part of the vendor diff.
