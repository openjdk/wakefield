/*
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2022, JetBrains s.r.o.. All rights reserved.
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

package sun.awt.wl;

import java.awt.*;
import java.awt.geom.AffineTransform;
import java.awt.image.ColorModel;
import java.awt.image.DirectColorModel;
import java.awt.image.WritableRaster;

import sun.awt.image.OffScreenImage;
import sun.java2d.SurfaceData;
import sun.java2d.loops.SurfaceType;
import sun.java2d.wl.WLSurfaceData;

public class WLGraphicsConfig extends GraphicsConfiguration {
    private final WLGraphicsDevice device;

    private final Object dataLock = new Object();

    private final Rectangle bounds = new Rectangle(800, 600);

    // This is Wayland scale for the use with wl_buffers only.
    // From wayland.xml, wl_surface.set_buffer_scale request:
    // "It is intended that you pick the same buffer scale as the scale of the
    //	output that the surface is displayed on."
    private int wlBufferScale = 1;

    public WLGraphicsConfig(WLGraphicsDevice device) {
        this.device = device;
    }

    @Override
    public WLGraphicsDevice getDevice() {
        return device;
    }

    @Override
    public ColorModel getColorModel() {
        return new DirectColorModel(24, 0xff0000, 0xff00, 0xff);
    }

    @Override
    public ColorModel getColorModel(int transparency) {
        return switch (transparency) {
            case Transparency.OPAQUE -> getColorModel();
            case Transparency.BITMASK -> new DirectColorModel(25, 0xff0000, 0xff00, 0xff, 0x1000000);
            case Transparency.TRANSLUCENT -> new DirectColorModel(32, 0xff0000, 0xff00, 0xff, 0xff000000);
            default -> null;
        };
    }

    public Image createAcceleratedImage(Component target,
                                        int width, int height)
    {
        ColorModel model = getColorModel(Transparency.OPAQUE);
        WritableRaster raster = model.createCompatibleWritableRaster(width, height);
        return new OffScreenImage(target, model, raster, model.isAlphaPremultiplied());
    }

    @Override
    public AffineTransform getDefaultTransform() {
        double scale = getScale();
        return AffineTransform.getScaleInstance(scale, scale);
    }

    @Override
    public AffineTransform getNormalizingTransform() {
        // TODO: may not be able to implement this fully, but we can try
        // obtaining physical width/height from wl_output.geometry event.
        // Those may be 0, of course.
        return getDefaultTransform();
    }

    @Override
    public Rectangle getBounds() {
        synchronized (dataLock) {
            return bounds;
        }
    }

    public SurfaceType getSurfaceType() {
        return SurfaceType.IntArgb;
    }

    public SurfaceData createSurfaceData(WLComponentPeer peer) {
        return WLSurfaceData.createData(peer);
    }

    int getScale() {
        synchronized (dataLock) {
            return wlBufferScale;
        }
    }

    void update(int width, int height, int scale) {
        synchronized (dataLock) {
            this.wlBufferScale = scale;
            bounds.setSize(width, height);
        }
    }
}
