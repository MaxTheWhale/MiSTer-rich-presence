# Discord Rich Presence for [MiSTer](https://github.com/MiSTer-devel/Main_MiSTer/wiki)

# Building
To build the server, an ARM linux compiler is required. On x86, [Linaro GCC](https://releases.linaro.org/components/toolchain/binaries/latest-7/arm-linux-gnueabihf/) is recommended. The `BASE` and `CC` variables in the Makefile should be changed to point to the compiler binary. The server should then be able to be built by running `make` in the `server` folder.

To build the client, the [Discord Game SDK](https://discord.com/developers/docs/game-sdk/sdk-starter-guide) needs to be copied into the project, following the instructions in the SDK starter guide. Once the project has been built, the `discord_game_sdk.dll` file needs to be copied to the directory containing the executable.

# Installing on MiSTer
To install the server on your MiSTer, copy the binary `mister_status` to the `/sbin` directory. This can then be run manually using:

`> nohup mister_status > /dev/null &`

To make the status server start automatically on boot, copy the file `S92status` to the `/etc/init.d` directory.
