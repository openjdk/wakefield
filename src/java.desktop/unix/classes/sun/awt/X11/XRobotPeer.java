/*
 * Copyright (c) 2003, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.awt.X11;

import java.awt.Rectangle;
import java.awt.Toolkit;
import java.awt.peer.RobotPeer;
import java.security.AccessController;

import sun.awt.AWTAccessor;
import sun.awt.SunToolkit;
import sun.awt.UNIXToolkit;
import sun.awt.X11GraphicsConfig;
import sun.awt.X11GraphicsDevice;
import sun.security.action.GetPropertyAction;

import javax.imageio.ImageIO;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;

@SuppressWarnings("removal")
final class XRobotPeer implements RobotPeer {

    private static final boolean tryGtk;
    static {
        loadNativeLibraries();
        tryGtk = Boolean.parseBoolean(
                            AccessController.doPrivileged(
                                    new GetPropertyAction("awt.robot.gtk", "true")
                            ));
    }
    private static volatile boolean useGtk;
    private final X11GraphicsConfig  xgc;
    private final boolean useDbusApi;

    XRobotPeer(X11GraphicsDevice gd) {
        xgc = (X11GraphicsConfig) gd.getDefaultConfiguration();
        SunToolkit tk = (SunToolkit)Toolkit.getDefaultToolkit();
        setup(tk.getNumberOfButtons(),
                AWTAccessor.getInputEventAccessor().getButtonDownMasks());

        boolean isGtkSupported = false;
        if (tryGtk) {
            if (tk instanceof UNIXToolkit && ((UNIXToolkit) tk).loadGTK()) {
                isGtkSupported = true;
            }
        }

        useGtk = (tryGtk && isGtkSupported);

        boolean isWayland = System.getenv("WAYLAND_DISPLAY") != null; //TODO improve wayland detection?

        String property = System.getProperty("awt.robot.useDbusApi");
        if (property != null) {
            useDbusApi = useGtk && Boolean.parseBoolean(property);
        } else {
            useDbusApi = useGtk && isWayland;
        }

        System.out.printf("""
                        Use DBUS API for screen capture: %b
                        Property awt.robot.useDbusApi  : %s
                        
                        WAYLAND_DISPLAY                : %s
                        DISPLAY                        : %s
                        
                        DBUS_SESSION_BUS_ADDRESS       : %s
                        XDG_CURRENT_DESKTOP            : %s
                        XDG_SESSION_TYPE               : %s
                        
                        """,
                useDbusApi, property,
                System.getenv("WAYLAND_DISPLAY"),
                System.getenv("DISPLAY"),
                System.getenv("DBUS_SESSION_BUS_ADDRESS"),
                System.getenv("XDG_CURRENT_DESKTOP"),
                System.getenv("XDG_SESSION_TYPE")
        );
    }

    @Override
    public void mouseMove(int x, int y) {
        mouseMoveImpl(xgc, xgc.scaleUp(x), xgc.scaleUp(y));
    }

    @Override
    public void mousePress(int buttons) {
        mousePressImpl(buttons);
    }

    @Override
    public void mouseRelease(int buttons) {
        mouseReleaseImpl(buttons);
    }

    @Override
    public void mouseWheel(int wheelAmt) {
        mouseWheelImpl(wheelAmt);
    }

    @Override
    public void keyPress(int keycode) {
        keyPressImpl(keycode);
    }

    @Override
    public void keyRelease(int keycode) {
        keyReleaseImpl(keycode);
    }

    @Override
    public int getRGBPixel(int x, int y) {
        int[] pixelArray = new int[1];
        if (useDbusApi) {
            wlGetRGBPixelsImpl(x, y, 1, 1, pixelArray);
        } else  {
            getRGBPixelsImpl(xgc, x, y, 1, 1, pixelArray, useGtk);
        }
        return pixelArray[0];
    }

    @Override
    public int [] getRGBPixels(Rectangle bounds) {
        int[] pixelArray = new int[bounds.width*bounds.height];
        if (useDbusApi) {
            wlGetRGBPixelsImpl(bounds.x, bounds.y, bounds.width, bounds.height, pixelArray);
        } else {
            getRGBPixelsImpl(xgc, bounds.x, bounds.y, bounds.width, bounds.height,
                    pixelArray, useGtk);
        }
        return pixelArray;
    }

    private static synchronized native void setup(int numberOfButtons, int[] buttonDownMasks);
    private static native void loadNativeLibraries();

    private static synchronized native void mouseMoveImpl(X11GraphicsConfig xgc, int x, int y);
    private static synchronized native void mousePressImpl(int buttons);
    private static synchronized native void mouseReleaseImpl(int buttons);
    private static synchronized native void mouseWheelImpl(int wheelAmt);

    private static synchronized native void keyPressImpl(int keycode);
    private static synchronized native void keyReleaseImpl(int keycode);

    private static synchronized native void getRGBPixelsImpl(X11GraphicsConfig xgc,
                                                             int x, int y,
                                                             int width, int height,
                                                             int[] pixelArray,
                                                             boolean isGtkSupported);

    private static native String _wlGetRGBPixelsImpl(int x, int y, int width, int height); // , int[] pixelArray

    private static synchronized void wlGetRGBPixelsImpl(int x, int y, int width, int height, int[] pixelArray) {
        var path = _wlGetRGBPixelsImpl(x, y, width, height);
        if (path != null) {
           try {
               File file = new File(path);
               BufferedImage bImage = ImageIO.read(file);
               bImage.getRGB(0, 0, width, height, pixelArray, 0, width);
           } catch (IOException e) {
               e.printStackTrace();
           }
        } else {
           System.err.println("wlGetRGBPixelsImpl: Failed to get screenshot path.");
        }
    }
}
