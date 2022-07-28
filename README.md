# elink

![elink](media/elink.jpeg)

elink is a daisy chain of [electronic shelf labels](https://en.wikipedia.org/wiki/Electronic_shelf_label) (ESL). They are linked together via [UART](https://en.wikipedia.org/wiki/Universal_asynchronous_receiver-transmitter), in such a way that the `TX` line of a node is connected to the `RX` line of the next. The first node is special, as it can be connected to via Bluetooth Low Energy (BLE).

The ESLs are Hanshow Stellar-MN2, an e-paper, BLE capable device that uses the [Telink TLSR8359](http://wiki.telink-semi.cn/wiki/chip-series/TLSR835x-Series/) SoC under the hood.

## Repository organization

| Directory                                    | Description                                                                                                                                                |
| -------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [`code/`](./code)                            | Firmware for the displays - based on [`atc1441/ATC_TLSR_Paper`](https://github.com/atc1441/ATC_TLSR_Paper)                                                 |
| [`clients/python-cli`](./clients/python-cli) | A complete Python command line client for elink                                                                                                            |
| [`clients/web/`](./clients/web)              | A simple web client for elink. Supports only drawing text via BLE. It is additionally deployed on [rbaron.github.io/elink](https://rbaron.github.io/elink) |
| [`case/`](./case)                            | Fabrication files for the case - SVGs templates for laser cutting and 3D-printable supports                                                                |

## Building the code

The [Dockerfile](./code/Dockerfile) sets up a Linux Alpine container with all the dependencies and toolchain.

```
# Build the Docker image.
$ cd code/
$ docker build -t elink-docker .
# Mount the code/ directory inside the container and run `make`
$ docker run -it --rm -v "${PWD}":/app elink-docker
# make
```

The firmware will be created in `./code/elink.bin`.

### Developing

The [Dockerfile.devcontainer](code/.devcontainer/Dockerfile.devcontainer) defines an image for [VSCode's devcontainer](https://code.visualstudio.com/docs/remote/containers), and [.c_cpp_properties.json](code/.vscode/c_cpp_properties.json) sets up the include path inside the container. Once set up, opening `code/` in VSCode should allow you to edit the code with auto-complete and build it with `make` inside VSCode's built-in terminal, that runs inside the Docker container. The toolchain, SDK and custom linker script should be set up automatically.

## License

The code is released under the MIT license, and fabrication files (STL, SVGs) under [CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/).

## Demo

Here's drawing "Hello, world" over BLE with the [web client](https://rbaron.github.io/elink):

https://user-images.githubusercontent.com/1573409/181444598-2fc59157-b527-484d-a00a-630492e747f3.mp4


