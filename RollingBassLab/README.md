# Rolling Bass Lab

Tech house bass synth + pattern generator (VST3, Windows). Built with JUCE 8.

## Build
Push this folder to GitHub. The workflow in `.github/workflows/build.yml` builds on
`windows-2025-vs2026` with the Visual Studio 18 2026 generator. When it finishes, open the
run in the **Actions** tab and download **RollingBassLab-VST3-Windows**.

## Install
Unzip and copy the `Rolling Bass Lab.vst3` folder to:

    C:\Program Files\Common Files\VST3\

Rescan plugins in Ableton. It shows up under Plug-ins as an instrument.

## Use
- **Pattern On** + press play in Ableton: the bass plays the 16-step pattern in time with your project.
- **Pattern On off**: play it like a normal synth from a MIDI clip.
- **Preview**: hear the pattern with Ableton stopped.
- **Generate**: new groove in the chosen Style and Key. Density and Swing shape it.
- **Step grid**: click = on/off, Shift-click = accent, Alt-click = slide, mouse wheel = pitch, right-click = menu.
- **Drag MIDI to Ableton**: drag the box onto a MIDI track to get a 4-bar clip. **Save .mid** if dragging doesn't work.
- **Kick Duck**: built-in sidechain pump on every beat (4-on-the-floor). Turn Depth to 0 if you sidechain yourself.
- **Quiet Check**: drops the output 18 dB. If the groove still hits this quiet, it's working.
