#
# Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.  Oracle designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#

################################################################################
# Setup pipewire
################################################################################
AC_DEFUN_ONCE([LIB_SETUP_PIPEWIRE],
[
  if test "x$NEEDS_LIB_PIPEWIRE" = xfalse; then
    PIPEWIRE_CFLAGS=
    PIPEWIRE_LIBS=
  else
    PIPEWIRE_FOUND=no

    # Do not try pkg-config if we have a sysroot set.
    if test "x$SYSROOT" = x; then
      if test "x$PIPEWIRE_FOUND" = xno; then
        PKG_CHECK_MODULES([PIPEWIRE], [libpipewire-0.3], [PIPEWIRE_FOUND=yes], [PIPEWIRE_FOUND=no]) #
      fi
    fi


    if test "x$PIPEWIRE_FOUND" = xno; then
      AC_CHECK_HEADERS([pipewire-0.3/pipewire/pipewire.h],
          [
            PIPEWIRE_FOUND=yes
            PIPEWIRE_CFLAGS=-Iignoreme
            PIPEWIRE_LIBS=-lpipewire-0.3
            DEFAULT_PIPEWIRE=yes
          ],
          [PIPEWIRE_FOUND=no]
      )
    fi

    if test "x$PIPEWIRE_FOUND" = xno; then
      HELP_MSG_MISSING_DEPENDENCY([pipewire])
      AC_MSG_ERROR([Could not find pipewire! $HELP_MSG])
    fi
  fi

  AC_SUBST(PIPEWIRE_CFLAGS)
  AC_SUBST(PIPEWIRE_LIBS)
])
