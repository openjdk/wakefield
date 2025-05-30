/*
 * Copyright (c) 2018, 2025, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 6199320 8347841
 * @summary Properties.store() causes deadlock when concurrently calling TimeZone apis
 * @run main/timeout=20 StoreDeadlock
 * @author Xueming Shen
 */

import java.io.IOException;
import java.util.Properties;
import java.util.TimeZone;

public class StoreDeadlock {
    public StoreDeadlock() {
        Properties sysproperty = System.getProperties();
        Thread1 t1 = new Thread1(sysproperty);
        Thread2 t2 = new Thread2();
        t1.start();
        t2.start();
    }
    public static void main(String[] args) {
        StoreDeadlock deadlock = new StoreDeadlock();
    }
    class Thread1 extends Thread {
        Properties sp;
        public Thread1(Properties p) {
            sp = p;
        }
        public void run() {
            try {
                sp.store(System.out, null);
            } catch (IOException e) {
                System.out.println("IOException : " + e);
            }
        }
    }
    class Thread2 extends Thread {
        public void run() {
            System.out.println("tz=" + TimeZone.getTimeZone("America/Los_Angeles"));
        }
    }
}
