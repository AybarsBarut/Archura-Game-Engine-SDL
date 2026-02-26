namespace Archura
{
    public enum KeyCode
    {
        // Simple mapping, incomplete list
        Space = 44, // SDL Scancode for Space
        W = 26,
        A = 4,
        S = 22,
        D = 7
    }

    public static class Input
    {
        public static bool IsKeyDown(KeyCode key)
        {
            return InternalCalls.Input_IsKeyDown((int)key);
        }
    }
}
