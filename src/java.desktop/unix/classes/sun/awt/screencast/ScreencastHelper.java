package sun.awt.screencast;

import sun.security.action.GetPropertyAction;

import java.security.AccessController;

@SuppressWarnings("removal")
public class ScreencastHelper {

    private static final boolean screencastDebug;

    static {
        screencastDebug = Boolean.parseBoolean(
                AccessController.doPrivileged(
                        new GetPropertyAction("screencastDebug", "false")
                ));
    }

    public static synchronized native void getRGBPixelsImpl(
            int x, int y, int width, int height, int[] pixelArray, boolean screencastDebug
    );

    public static void getRGBPixelsImpl(
            int x, int y, int width, int height, int[] pixelArray
    ) {
        //            System.err.println("\u26A0\u26A0\u26A0 getRGBPixels getRGBPixels");
        getRGBPixelsImpl(x, y, width, height, pixelArray, screencastDebug);
    }
}
