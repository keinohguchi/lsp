# SPDX-License-Identifier: GPL-2.0
FROM archlinux/base
RUN pacman -Sy --noconfirm gcc make binutils systemd-libs
WORKDIR /home/build
CMD ["make"]