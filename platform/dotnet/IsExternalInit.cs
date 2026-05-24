// Polyfill: allows C# 9+ init-only setters to compile targeting netstandard2.1
namespace System.Runtime.CompilerServices {
    internal static class IsExternalInit { }
}
