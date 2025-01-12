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
        -v "$PWD:/usr/src/tdesktop" \
        ghcr.io/telegramdesktop/tdesktop/centos_env:latest \
        /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
        -D TDESKTOP_API_ID=2040 \
        -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

Or, to create a debug build, run

    docker run --rm -it \
        -v "$PWD:/usr/src/tdesktop" \
        -e CONFIG=Debug \
        ghcr.io/telegramdesktop/tdesktop/centos_env:latest \
        /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
        -D TDESKTOP_API_ID=2040 \
        -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627

**P. S. If docker image build takes too long, cherry-pick [this](https://github.com/TDesktop-x64/tdesktop/commit/b99c084862053f441caa6525837a7e193cc671f7) commit.**

The built files will be in the `out` directory.

You can use `strip` command to reduce binary size.
