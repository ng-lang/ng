FROM debian:bookworm-slim

RUN apt-get update &&\
    apt-get install -y ca-certificates build-essential procps curl file git

RUN useradd -ms /bin/bash linuxbrew
RUN usermod -a -G sudo linuxbrew
USER linuxbrew

WORKDIR /home/linuxbrew

ENV NONINTERACTIVE=1

ENV HOMEBREW_INSTALL_FROM_API=1
RUN /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

RUN eval "$(/home/linuxbrew/.linuxbrew/bin/brew shellenv)" && echo "eval \"\$($(brew --prefix)/bin/brew shellenv)\"" >> ~/.bashrc
RUN /home/linuxbrew/.linuxbrew/bin/brew install llvm gcc
