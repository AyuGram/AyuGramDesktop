## Build instructions for Linux using Docker

### Prepare folder

Choose a folder for the future build, for example **/home/user/TBuild**. It will be named ***BuildPath*** in the rest of this document. All commands will be launched from Terminal.

### Clone source code and prepare libraries

Install [poetry](https://python-poetry.org), go to ***BuildPath*** and run

    git clone --recursive https://github.com/AyuGram/AyuGramDesktop.git tdesktop
    ./tdesktop/Telegram/build/prepare/linux.sh

### Building the project

Go to ***BuildPath*/tdesktop** and run

    docker run --rm -it \
        -v $PWD:/usr/src/tdesktop \
        tdesktop:centos_env \
        /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
        -D TDESKTOP_API_ID=2040 \
        -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 \
        -D CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

Or, to create a debug build, run

    docker run --rm -it \
        -v $PWD:/usr/src/tdesktop \
        -e DEBUG=1 \
        tdesktop:centos_env \
        /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
        -D TDESKTOP_API_ID=2040 \
        -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 \
        -D CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

If you need a backward compatible binary (running on older OS like the official one), you should build the binary with LTO.  
To do this, add `-D CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` option.

The built files will be in the `out` directory.

You can use `strip` command to reduce binary size.
