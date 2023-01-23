package sun.awt.screencast;

import sun.awt.util.ThreadGroupUtils;
import sun.security.action.GetPropertyAction;

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.concurrent.CountDownLatch;

@SuppressWarnings("removal")
public class ScreencastHelper {

    private static final boolean screencastDebug;

    static {
        screencastDebug = Boolean.parseBoolean(
                AccessController.doPrivileged(
                        new GetPropertyAction("screencastDebug", "false")
                ));
    }

    static volatile boolean initComplete = false;

    static volatile CountDownLatch initCountDown;

    private static final Runnable runnable = () -> {
        initScreencast(screencastDebug);
        initCountDown.countDown();
        initComplete = true;
        start();
    };

    @SuppressWarnings("removal")
    public static void initAndStart() {
        initCountDown = new CountDownLatch(1);
        initComplete = false;
        Thread pipewireThread = AccessController.doPrivileged((PrivilegedAction<Thread>) () -> {
            String name = "AWT-PIPEWIRE";
            Thread thread = new Thread(
                    ThreadGroupUtils.getRootThreadGroup(), runnable, name,
                    0, false);
            thread.setContextClassLoader(null);
            thread.setDaemon(true);
            return thread;
        });
        pipewireThread.start();
    }

    public static void waitForInit() {
        if (!initComplete) {
            try {
                initCountDown.await();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    private static native void initScreencast(boolean screencastDebug);
    private static native void start();
    private static native void stop();

    public static synchronized native void getRGBPixelsImpl(
            int x, int y, int width, int height, int[] pixelArray
    );

    public static void shutDown() {
        stop();
    }
}
