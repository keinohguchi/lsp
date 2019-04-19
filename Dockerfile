# SPDX-License-Identifier: GPL-2.0
FROM archlinux/base
RUN pacman -Sy --noconfirm gcc make binutils systemd-libs
COPY . /home/build
WORKDIR /home/build
CMD ["make"]
