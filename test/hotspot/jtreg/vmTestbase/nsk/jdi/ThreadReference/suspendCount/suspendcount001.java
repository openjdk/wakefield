/*
 * Copyright (c) 2001, 2025, Oracle and/or its affiliates. All rights reserved.
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

package nsk.jdi.ThreadReference.suspendCount;

import nsk.share.*;
import nsk.share.jpda.*;
import nsk.share.jdi.*;

import com.sun.jdi.*;
import java.util.*;
import java.io.*;

import com.sun.jdi.event.*;
import com.sun.jdi.request.*;

/**
 * The test for the implementation of an object of the type     <BR>
 * ThreadReference.                                             <BR>
 *                                                              <BR>
 * The test checks up that results of the method                <BR>
 * <code>com.sun.jdi.ThreadReference.suspendCount()</code>      <BR>
 * complies with its spec.                                      <BR>
 * <BR>
 * The cases for testing are as follows.                                <BR>
 * After being started up,                                              <BR>
 * a debuggee creates a 'lockingObject' for synchronizing threads,      <BR>
 * enters a synchronized block in which it creates new thread, thread2, <BR>
 * informs a debugger of the thread2 creation, and is waiting for reply.<BR>
 * Since the thread2 uses the same locking object in its 'run' method   <BR>
 * it is locked up until the debuggee leaves the synchronized block.    <BR>
 * Upon the receiption a message from the debuggee, the debugger        <BR>
 * calls methods ThreadReference.suspend() and .resume(), and           <BR>
 * VirualMachine.suspend() and .resume() in a number of combinations,   <BR>
 * and checks up that each combination results in the expected value    <BR>
 * returned by the method suspendCount().                               <BR>
 * At the end the debugger instructs the debuggee to unlock the thread2.<BR>
 * <BR>
 */

public class suspendcount001 {

    //----------------------------------------------------- templete section
    static final int PASSED = 0;
    static final int FAILED = 2;
    static final int PASS_BASE = 95;

    //----------------------------------------------------- templete parameters
    static final String
    sHeader1 = "\n==> nsk/jdi/ThreadReference/suspendCount/suspendcount001  ",
    sHeader2 = "--> debugger: ",
    sHeader3 = "##> debugger: ";

    //----------------------------------------------------- main method

    public static void main (String argv[]) {
        int result = run(argv, System.out);
        if (result != 0) {
            throw new RuntimeException("TEST FAILED with result " + result);
        }
    }

    public static int run (String argv[], PrintStream out) {
        return new suspendcount001().runThis(argv, out);
    }

    //--------------------------------------------------   log procedures

    private static Log  logHandler;

    private static void log1(String message) {
        logHandler.display(sHeader1 + message);
    }
    private static void log2(String message) {
        logHandler.display(sHeader2 + message);
    }
    private static void log3(String message) {
        logHandler.complain(sHeader3 + message);
    }

    //  ************************************************    test parameters

    private String debuggeeName =
        "nsk.jdi.ThreadReference.suspendCount.suspendcount001a";

    private String testedClassName =
        "nsk.jdi.ThreadReference.suspendCount.Threadsuspendcount001a";

    //String mName = "nsk.jdi.ThreadReference.suspendCount";

    //====================================================== test program
    //------------------------------------------------------ common section

    static ArgumentHandler      argsHandler;

    static int waitTime;

    static VirtualMachine      vm            = null;
    static EventRequestManager eventRManager = null;
    static EventQueue          eventQueue    = null;
    static EventSet            eventSet      = null;

    ReferenceType     testedclass  = null;
    ThreadReference   thread2      = null;
    ThreadReference   mainThread   = null;

    static int  testExitCode = PASSED;

    static final int returnCode0 = 0;
    static final int returnCode1 = 1;
    static final int returnCode2 = 2;
    static final int returnCode3 = 3;
    static final int returnCode4 = 4;

    //------------------------------------------------------ methods

    private int runThis (String argv[], PrintStream out) {

        Debugee debuggee;

        argsHandler     = new ArgumentHandler(argv);
        logHandler      = new Log(out, argsHandler);
        Binder binder   = new Binder(argsHandler, logHandler);

        if (argsHandler.verbose()) {
            debuggee = binder.bindToDebugee(debuggeeName + " -vbs");
        } else {
            debuggee = binder.bindToDebugee(debuggeeName);
        }

        waitTime = argsHandler.getWaitTime();


        IOPipe pipe     = new IOPipe(debuggee);

        debuggee.redirectStderr(out);
        log2(debuggeeName + " debuggee launched");
        debuggee.resume();

        String line = pipe.readln();
        if ((line == null) || !line.equals("ready")) {
            log3("signal received is not 'ready' but: " + line);
            return FAILED;
        } else {
            log2("'ready' recieved");
        }

        vm = debuggee.VM();
        ReferenceType debuggeeClass = debuggee.classByName(debuggeeName);

    //------------------------------------------------------  testing section
        log1("      TESTING BEGINS");

        for (int i = 0; ; i++) {

            pipe.println("newcheck");
            line = pipe.readln();

            if (line.equals("checkend")) {
                log2("     : returned string is 'checkend'");
                break ;
            } else if (!line.equals("checkready")) {
                log3("ERROR: returned string is not 'checkready'");
                testExitCode = FAILED;
                break ;
            }

            log1("new checkready: #" + i);

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ variable part

            int expresult = returnCode0;


            eventRManager = vm.eventRequestManager();
            eventQueue    = vm.eventQueue();

            String threadName = "testedThread";

            //String breakpointMethod1 = "runt1";
            //String breakpointMethod2 = "runt2";

            //String bpLine1 = "breakpointLineNumber1";
            //String bpLine2 = "breakpointLineNumber2";
            //String bpLine3 = "breakpointLineNumber3";

            List            classes      = null;

            //BreakpointRequest breakpRequest1 = null;
            //BreakpointRequest breakpRequest2 = null;
            //BreakpointRequest breakpRequest3 = null;


            label0: {

                log2("getting ThreadReference objects and setting up breakponts");
                try {
                    classes     = vm.classesByName(testedClassName);
                    testedclass = (ReferenceType) classes.get(0);
                } catch ( Exception e) {
                    log3("ERROR: Exception at very beginning !? : " + e);
                    expresult = returnCode1;
                    break label0;
                }

                thread2 = debuggee.threadByFieldNameOrThrow(debuggeeClass, "test_thread", threadName);
            }

            label1: {
                if (expresult != returnCode0)
                    break label1;


                int suspCount;

                log2("         check #1: the beginning: suspCount == 0 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 0) {
                    log3("ERROR: 1: suspCount != 0 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("        suspending thread2 with: thread2.suspend(); ");
                thread2.suspend();

                log2("         check #2:  suspCount == 1 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 1) {
                    log3("ERROR: 2: suspCount != 1 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("        suspending thread2 with: thread2.suspend(); ");
                thread2.suspend();

                log2("         check #3:  suspCount == 2 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 2) {
                    log3("ERROR: 3: suspCount != 2 : " + suspCount);
                    expresult = returnCode1;;
                }


                log2("        resuming thread2 with: thread2.resume(); ");
                thread2.resume();

                log2("         check #4:  suspCount == 1 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 1) {
                    log3("ERROR: 4: suspCount != 1 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("        resuming thread2 with: thread2.resume(); ");
                thread2.resume();

                log2("         check #5:  suspCount == 0 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 0) {
                    log3("ERROR: 5: suspCount != 0 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("        resuming NON-SUSPENDED thread2 with: thread2.resume(); ");
                thread2.resume();

                log2("         check #6:  suspCount == 0 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 0) {
                    log3("ERROR: 6: suspCount != 0 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("        suspending thread2 with: thread2.suspend(); ");
                thread2.suspend();

                log2("         check #7:  suspCount == 1 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 1) {
                    log3("ERROR: 7: suspCount != 1 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("       suspending VirtualMachine with vm.suspend(); ");
                vm.suspend();

                log2("         check #8:  suspCount == 2 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 2) {
                    log3("ERROR: 8: suspCount != 2 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("       9: resuming only thread2 with double thread2.resume(); ");
                thread2.resume();
                thread2.resume();

                log2("         check #9:  suspCount == 0 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 0) {
                    log3("ERROR: 9: suspCount != 0 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("       10: suspending VirtualMachine with double vm.suspend(); ");
                vm.suspend();
                vm.suspend();

                log2("         check #10:  suspCount == 2 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 2) {
                    log3("ERROR: 10: suspCount != 2 : " + suspCount);
                    expresult = returnCode1;
                }


                log2("       11: resuming with thread2.resume(); and vm.resume(); ");
                thread2.resume();
                vm.resume();

                log2("         check #11:  suspCount == 0 ");
                suspCount = thread2.suspendCount();
                if (suspCount != 0) {
                    log3("ERROR: 11: suspCount != 0 : " + suspCount);
                    expresult = returnCode1;
                }

                vm.resume();
                vm.resume();


                log2("       forcing the main thread to leave synchronized block");
                pipe.println("continue");
                line = pipe.readln();
                if (!line.equals("docontinue")) {
                    log3("ERROR: returned string is not 'docontinue'");
                    expresult = returnCode4;
                }

            }

            //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            log2("     the end of testing");
            if (expresult != returnCode0)
                testExitCode = FAILED;
        }
        log1("      TESTING ENDS");

    //--------------------------------------------------   test summary section
    //-------------------------------------------------    standard end section

        pipe.println("quit");
        log2("waiting for the debuggee to finish ...");
        debuggee.waitFor();

        int status = debuggee.getStatus();
        if (status != PASSED + PASS_BASE) {
            log3("debuggee returned UNEXPECTED exit status: " +
                    status + " != PASS_BASE");
            testExitCode = FAILED;
        } else {
            log2("debuggee returned expected exit status: " +
                    status + " == PASS_BASE");
        }

        if (testExitCode != PASSED) {
            logHandler.complain("TEST FAILED");
        }
        return testExitCode;
    }


   /*
    * private BreakpointRequest settingBreakpoint(String, String, String)
    *
    * It sets up a breakpoint within a given method at given line number
    * for the thread2 only.
    * Third parameter is required for any case in future debugging, as if.
    *
    * Return codes:
    *  = BreakpointRequest object  in case of success
    *  = null   in case of an Exception thrown within the method
    */
/*
    private BreakpointRequest settingBreakpoint ( String methodName,
                                                  String bpLine,
                                                  String property) {

        log2("setting up a breakpoint: method: '" + methodName + "' line: " + bpLine );

        List              alllineLocations = null;
        Location          lineLocation     = null;
        BreakpointRequest breakpRequest    = null;

        try {
            Method  method  = (Method) testedclass.methodsByName(methodName).get(0);

            alllineLocations = method.allLineLocations();

            int n =
                ( (IntegerValue) testedclass.getValue(testedclass.fieldByName(bpLine) ) ).value();
            if (n > alllineLocations.size()) {
                log3("ERROR:  TEST_ERROR_IN_settingBreakpoint(): number is out of bound of method's lines");
            } else {
                lineLocation = (Location) alllineLocations.get(n);
                try {
                    breakpRequest = eventRManager.createBreakpointRequest(lineLocation);
                    breakpRequest.putProperty("number", property);
                    breakpRequest.addThreadFilter(thread2);
                    breakpRequest.setSuspendPolicy( EventRequest.SUSPEND_EVENT_THREAD);
                } catch ( Exception e1 ) {
                    log3("ERROR: inner Exception within settingBreakpoint() : " + e1);
                    breakpRequest    = null;
                }
            }
        } catch ( Exception e2 ) {
            log3("ERROR: ATTENTION:  outer Exception within settingBreakpoint() : " + e2);
            breakpRequest    = null;
        }

        if (breakpRequest == null)
            log2("      A BREAKPOINT HAS NOT BEEN SET UP");
        else
            log2("      a breakpoint has been set up");

        return breakpRequest;
    }
*/

    /*
     * private int breakpoint ()
     *
     * It removes events from EventQueue until gets first BreakpointEvent.
     * To get next EventSet value, it uses the method
     *    EventQueue.remove(int timeout)
     * The timeout argument passed to the method, is "waitTime*60000".
     * Note: the value of waitTime is set up with
     *       the method ArgumentHandler.getWaitTime() at the beginning of the test.
     *
     * Return codes:
     *  = returnCode0 - success;
     *  = returnCode2 - Exception when "eventSet = eventQueue.remove()" is executed
     *  = returnCode3 - default case when loop of processing an event, that is,
     *                  an unspecified event was taken from the EventQueue
     */
/*
    private int breakpoint () {

        int returnCode = returnCode0;

        log2("       waiting for BreakpointEvent");

        labelBP:
            for (;;) {

                log2("       new:  eventSet = eventQueue.remove();");
                try {
                    eventSet = eventQueue.remove(waitTime*60000);
                    if (eventSet == null) {
                        log3("ERROR:  timeout for waiting for a BreakpintEvent");
                        returnCode = returnCode3;
                        break labelBP;
                    }
                } catch ( Exception e ) {
                    log3("ERROR: Exception for  eventSet = eventQueue.remove(); : " + e);
                    returnCode = 1;
                    break labelBP;
                }

                if (eventSet != null) {

                    log2("     :  eventSet != null;  size == " + eventSet.size());

                    EventIterator eIter = eventSet.eventIterator();
                    Event         ev    = null;

                    for (; eIter.hasNext(); ) {

                        if (returnCode != returnCode0)
                            break;

                        ev = eIter.nextEvent();

                    ll: for (int ifor =0;  ; ifor++) {

                        try {
                          switch (ifor) {

                          case 0:  AccessWatchpointEvent awe = (AccessWatchpointEvent) ev;
                                   log2("      AccessWatchpointEvent removed");
                                   break ll;
                          case 1:  BreakpointEvent be = (BreakpointEvent) ev;
                                   log2("      BreakpointEvent removed");
                                   break labelBP;
                          case 2:  ClassPrepareEvent cpe = (ClassPrepareEvent) ev;
                                   log2("      ClassPreparEvent removed");
                                   break ll;
                          case 3:  ClassUnloadEvent cue = (ClassUnloadEvent) ev;
                                   log2("      ClassUnloadEvent removed");
                                   break ll;
                          case 4:  ExceptionEvent ee = (ExceptionEvent) ev;
                                   log2("      ExceptionEvent removed");
                                   break ll;
                          case 5:  MethodEntryEvent mene = (MethodEntryEvent) ev;
                                   log2("      MethodEntryEvent removed");
                                   break ll;
                          case 6:  MethodExitEvent mexe = (MethodExitEvent) ev;
                                   log2("      MethodExiEvent removed");
                                   break ll;
                          case 7:  ModificationWatchpointEvent mwe = (ModificationWatchpointEvent) ev;
                                   log2("      ModificationWatchpointEvent removed");
                                   break ll;
                          case 8:  StepEvent se = (StepEvent) ev;
                                   log2("      StepEvent removed");
                                   break ll;
                          case 9:  ThreadDeathEvent tde = (ThreadDeathEvent) ev;
                                   log2("      ThreadDeathEvent removed");
                                   break ll;
                          case 10: ThreadStartEvent tse = (ThreadStartEvent) ev;
                                   log2("      ThreadStartEvent removed");
                                   break ll;
                          case 11: VMDeathEvent vmde = (VMDeathEvent) ev;
                                   log2("      VMDeathEvent removed");
                                   break ll;
                          case 12: VMStartEvent vmse = (VMStartEvent) ev;
                                   log2("      VMStartEvent removed");
                                   break ll;
                          case 13: WatchpointEvent we = (WatchpointEvent) ev;
                                   log2("      WatchpointEvent removed");
                                   break ll;

                          default: log3("ERROR:  default case for casting event");
                                   returnCode = returnCode3;
                                   break ll;
                          } // switch
                        } catch ( ClassCastException e ) {
                        }   // try
                    }       // ll: for (int ifor =0;  ; ifor++)
                }           // for (; ev.hasNext(); )
            }
        }
        if (returnCode == returnCode0)
            log2("     :  eventSet == null:  EventQueue is empty");

        return returnCode;
    }
*/
}
