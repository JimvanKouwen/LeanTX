# LeanTX RF support

LeanTX supports CRSF modules only: ExpressLRS and TBS Crossfire. Internal
radios with legacy RF hardware default to RF off; use a compatible CRSF module.
Removed or unrecognized module types in model files load as RF off.

Trainer and buddy-box support (including PPM, SBUS, Bluetooth, and CRSF
trainer input) has been removed. CRSF telemetry, Lua configuration, and ELRS
firmware flashing remain supported. RF binding and failsafe settings belong
to the CRSF module/receiver; the radio also offers the ELRS bind shortcut when
supported by the module.

Build the representative matrix with `./build-targets.sh lite`, or all current
firmware presets plus the simulator with `./build-targets.sh full`.

[![GitHub release (latest by date)](https://img.shields.io/github/v/release/Edgetx/edgetx)](https://github.com/EdgeTX/edgetx/releases/latest)
[![GitHub all releases](https://img.shields.io/github/downloads/EdgeTX/edgetx/total)](https://github.com/EdgeTX/edgetx/releases)
[![GitHub license](https://img.shields.io/github/license/Edgetx/edgetx)](https://github.com/EdgeTX/edgetx/blob/main/LICENSE)
[![Commit Tests](https://github.com/EdgeTX/edgetx/actions/workflows/build_fw.yml/badge.svg)](https://github.com/EdgeTX/edgetx/actions/workflows/build_fw.yml)
[![GitHub CodesSpaces ready-to-code](https://img.shields.io/badge/GitHub%20CodesSpaces-ready--to--code-blue?logo=github)](https://codespaces.new/EdgeTX/edgetx)
[![Conventional Commits](https://img.shields.io/badge/Conventional%20Commits-1.0.0-%23FE5196?logo=conventionalcommits&logoColor=white)](https://conventionalcommits.org)
[![Discord](https://img.shields.io/discord/839849772864503828.svg?label=&logo=discord&logoColor=ffffff&color=7389D8&labelColor=6A7EC2)](https://discord.gg/wF9wUKnZ6H)
[![Support us on OpenCollective](https://img.shields.io/opencollective/all/edgetx)](https://opencollective.com/edgetx)


<p align="center">
<a href="https://raw.githubusercontent.com/EdgeTX/edgetx.github.io/master/docs/assets/logo.png"><img src="https://raw.githubusercontent.com/EdgeTX/edgetx.github.io/master/docs/assets/logo.png" align="center" height="150" width="150" ></a>

# Welcome to EdgeTX!
**The cutting edge open-source firmware for your R/C radio!**


### About EdgeTX
EdgeTX is the cutting edge of OpenTX. It is the place where innovative ideas and cutting-edge features are developed and field-tested by the enthusiasts of our hobby. EdgeTX is a community project – ideas from the community, developed by the community, and enjoyed by the community! The community will always have a say in what EdgeTX is and what EdgeTX will be in the future. Without community feedback and involvement EdgeTX cannot exist.

### Community
- [Discord](https://discord.gg/wF9wUKnZ6H)   

- [Facebook](https://www.facebook.com/groups/edgetx)

- [Github Discussions](https://github.com/EdgeTX/edgetx/discussions)
  
### Navigation Links

- [Community Guidelines](https://github.com/EdgeTX/edgetx.github.io/wiki/Community-Guidlines)

- [Installation Guide](https://manual.edgetx.org/installing-and-updating-edgetx/update-from-opentx-to-edgetx)

- [Installation Video](https://www.youtube.com/watch?v=Y9OvW9XCjOs)

- [Reporting Issues / Requesting features](https://github.com/EdgeTX/edgetx/issues/new/choose)

- [Lua Documentation Site](https://luadoc.edgetx.org/)
  
- Buddy: [Info](https://github.com/EdgeTX/buddy) - [Downloads](https://github.com/EdgeTX/buddy/releases) 

- SD Card: [Info](https://github.com/EdgeTX/edgetx-sdcard) - [Downloads](https://github.com/EdgeTX/edgetx-sdcard/releases)

- Sound Packs:  [Info](https://github.com/EdgeTX/edgetx-sdcard-sounds) - [Downloads](https://github.com/EdgeTX/edgetx-sdcard-sounds/releases)

- [Developer Documentation](https://edgetx.org/edgetx/latest/) - [Docker Build Environment](https://github.com/EdgeTX/build-edgetx)


## Acknowledgements
Some icon assets provided by [ICONS8](https://icons8.com).</br>
Lua Documentation site powered with the kind support of [GitBook](https://www.gitbook.com).

Traditional trim values are removed: physical trim buttons never offset stick
inputs. Each direction remains a momentary switch (`T1-`, `T1+`, and so on),
independent of stick mapping and flight mode, and can be selected wherever a
switch condition is accepted. GPIO scanning, button diagnostics, simulator
buttons, and bootloader button combinations remain supported. No new button
actions are assigned by default.

Old trim values, inheritance, and trim-specific actions are ignored when loading
models. Their packed storage positions and numeric source/action IDs are reserved
to avoid shifting unrelated settings; reserved fields are not written to YAML.
Ordinary output offsets and audio-recording trim tools are unaffected.
