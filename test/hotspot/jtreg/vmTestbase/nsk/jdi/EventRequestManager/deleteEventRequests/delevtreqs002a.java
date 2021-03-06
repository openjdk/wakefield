/*
 * Copyright (c) 2001, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

package nsk.jdi.EventRequestManager.deleteEventRequests;

import nsk.share.*;
import nsk.share.jdi.*;

/**
 * This class is used as debuggee application for the delevtreqs002 JDI test.
 */

public class delevtreqs002a {

    //----------------------------------------------------- templete section

    static final int PASSED = 0;
    static final int FAILED = 2;
    static final int PASS_BASE = 95;

    static ArgumentHandler argHandler;
    static Log log;

    //--------------------------------------------------   log procedures

    public static void log1(String message) {
        log.display("**> debuggee: " + message);
    }

    private static void logErr(String message) {
        log.complain("**> debuggee: " + message);
    }

    //====================================================== test program

    static int                   testField1   = 0;
    static NullPointerException  testField2   = new NullPointerException("test");
    static Thread testField3[] = new Thread[10];

    //------------------------------------------------------ common section

    static int exitCode = PASSED;

    static int instruction = 1;
    static int end         = 0;
                                   //    static int quit        = 0;
                                   //    static int continue    = 2;
    static int maxInstr    = 1;    // 2;

    static int lineForComm = 2;

    private static void methodForCommunication() {
        int i1 = instruction;
        int i2 = i1;
        int i3 = i2;
    }
    //----------------------------------------------------   main method

    public static void main (String argv[]) {

        argHandler = new ArgumentHandler(argv);
        log = argHandler.createDebugeeLog();

        log1("debuggee started!");

        label0:
            for (int i = 0; ; i++) {

                if (instruction > maxInstr) {
                    logErr("ERROR: unexpected instruction: " + instruction);
                    exitCode = FAILED;
                    break ;
                }

                switch (i) {

    //------------------------------------------------------  section tested

                    case 0:
                            synchronized (lockObj1) {
                                for (int ii = 0; ii < 10; ii++) {
                                    testField3[ii] = JDIThreadFactory.newThread(new Thread1delevtreqs002a("thread" + ii));
                                    threadStart(testField3[ii]);
                                }
                                methodForCommunication();
                            }
                            break ;

    //-------------------------------------------------    standard end section

                    default:
                                instruction = end;
                                methodForCommunication();
                                break label0;
                }
            }

        log1("debuggee exits");
        System.exit(exitCode + PASS_BASE);
    }
    static Object lockObj1      = new Object();
    static Object waitnotifyObj = new Object();

    static int threadStart(Thread t) {
        synchronized (waitnotifyObj) {
            t.start();
            try {
                waitnotifyObj.wait();
            } catch ( Exception e) {
                exitCode = FAILED;
                logErr("       Exception : " + e );
                return FAILED;
            }
        }
        return PASSED;
    }

}

class Thread1delevtreqs002a extends NamedTask {

    public Thread1delevtreqs002a(String threadName) {
        super(threadName);
    }

    public void run() {
        delevtreqs002a.log1("  'run': enter  :: threadName == " + getName());
        synchronized(delevtreqs002a.waitnotifyObj) {
            delevtreqs002a.waitnotifyObj.notify();
        }
        synchronized(delevtreqs002a.lockObj1) {
            delevtreqs002a.log1("  'run': exit   :: threadName == " + getName());
        }
        return;
    }
}
