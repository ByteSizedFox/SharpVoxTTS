# SharpTalk WebUI

A WebAssembly-based platform for SharpTalk, built with Blazor WebAssembly.

## How to Run

1. Ensure you have the .NET SDK installed (net8.0 or later).
2. Navigate to this directory:
   ```bash
   cd platform/webui
   ```
3. Run the project:
   ```bash
   dotnet watch
   ```
   or
   ```bash
   dotnet run
   ```
4. Open your browser to the URL displayed in the terminal (usually `http://localhost:5000` or `https://localhost:5001`).

## Features

- **Text-to-Speech**: Type text and hear it spoken in real-time.
- **Voice Controls**: Adjust the speech rate and pitch.
- **Voice Selection**: Choose between Baseline and Whisper voices.
- **Pure WASM**: The entire engine runs in your browser; no server-side synthesis required.
