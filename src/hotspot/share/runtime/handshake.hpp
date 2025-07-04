/*
 * Copyright (c) 2017, 2025, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#ifndef SHARE_RUNTIME_HANDSHAKE_HPP
#define SHARE_RUNTIME_HANDSHAKE_HPP

#include "memory/allStatic.hpp"
#include "memory/iterator.hpp"
#include "runtime/flags/flagSetting.hpp"
#include "runtime/mutex.hpp"
#include "runtime/orderAccess.hpp"
#include "utilities/filterQueue.hpp"

class HandshakeOperation;
class AsyncHandshakeOperation;
class JavaThread;
class UnsafeAccessErrorHandshakeClosure;
class ThreadsListHandle;

// A handshake closure is a callback that is executed for a JavaThread
// while it is in a safepoint/handshake-safe state. Depending on the
// nature of the closure, the callback may be executed by the initiating
// thread, the target thread, or the VMThread. If the callback is not executed
// by the target thread it will remain in a blocked state until the callback completes.
class HandshakeClosure : public ThreadClosure, public CHeapObj<mtThread> {
  const char* const _name;
 public:
  HandshakeClosure(const char* name) : _name(name) {}
  virtual ~HandshakeClosure()                      {}
  const char* name() const                         { return _name; }
  virtual bool is_async()                          { return false; }
  virtual bool is_suspend()                        { return false; }
  virtual bool is_async_exception()                { return false; }
  virtual void do_thread(Thread* thread) = 0;
};

class AsyncHandshakeClosure : public HandshakeClosure {
 public:
   AsyncHandshakeClosure(const char* name) : HandshakeClosure(name) {}
   virtual ~AsyncHandshakeClosure() {}
   virtual bool is_async()          { return true; }
};

class Handshake : public AllStatic {
 public:
  // Execution of handshake operation
  static void execute(HandshakeClosure*       hs_cl);
  // This version of execute() relies on a ThreadListHandle somewhere in
  // the caller's context to protect target (and we sanity check for that).
  static void execute(HandshakeClosure*       hs_cl, JavaThread* target);
  // This version of execute() is used when you have a ThreadListHandle in
  // hand and are using it to protect target. If tlh == nullptr, then we
  // sanity check for a ThreadListHandle somewhere in the caller's context
  // to verify that target is protected.
  static void execute(HandshakeClosure*       hs_cl, ThreadsListHandle* tlh, JavaThread* target);
  // This version of execute() relies on a ThreadListHandle somewhere in
  // the caller's context to protect target (and we sanity check for that).
  static void execute(AsyncHandshakeClosure*  hs_cl, JavaThread* target);
};

class JvmtiRawMonitor;

// The HandshakeState keeps track of an ongoing handshake for this JavaThread.
// VMThread/Handshaker and JavaThread are serialized with _lock making sure the
// operation is only done by either VMThread/Handshaker on behalf of the
// JavaThread or by the target JavaThread itself.
class HandshakeState {
  friend UnsafeAccessErrorHandshakeClosure;
  friend JavaThread;
  // This a back reference to the JavaThread,
  // the target for all operation in the queue.
  JavaThread* _handshakee;
  // The queue containing handshake operations to be performed on _handshakee.
  FilterQueue<HandshakeOperation*> _queue;
  // Provides mutual exclusion to this state and queue. Also used for
  // JavaThread suspend/resume operations performed by SuspendResumeManager.
  Monitor _lock;
  // Set to the thread executing the handshake operation.
  Thread* volatile _active_handshaker;

  bool claim_handshake();
  bool possibly_can_process_handshake();
  bool can_process_handshake();

  bool have_non_self_executable_operation();
  HandshakeOperation* get_op_for_self(bool allow_suspend, bool check_async_exception);
  HandshakeOperation* get_op();
  void remove_op(HandshakeOperation* op);

  void set_active_handshaker(Thread* thread) { Atomic::store(&_active_handshaker, thread); }

  class MatchOp {
    HandshakeOperation* _op;
   public:
    MatchOp(HandshakeOperation* op) : _op(op) {}
    bool operator()(HandshakeOperation* op) {
      return op == _op;
    }
  };

 public:
  HandshakeState(JavaThread* thread);
  ~HandshakeState();

  void add_operation(HandshakeOperation* op);

  bool has_operation() { return !_queue.is_empty(); }
  bool has_operation(bool allow_suspend, bool check_async_exception);
  bool has_async_exception_operation();
  void clean_async_exception_operation();

  bool operation_pending(HandshakeOperation* op);

  // If the method returns true we need to check for a possible safepoint.
  // This is due to a suspension handshake which put the JavaThread in blocked
  // state so a safepoint may be in-progress.
  bool process_by_self(bool allow_suspend, bool check_async_exception);

  enum ProcessResult {
    _no_operation = 0,
    _not_safe,
    _claim_failed,
    _processed,
    _succeeded,
    _number_states
  };
  ProcessResult try_process(HandshakeOperation* match_op);

  Thread* active_handshaker() const { return Atomic::load(&_active_handshaker); }

  // Support for asynchronous exceptions
 private:
  bool _async_exceptions_blocked;

  bool async_exceptions_blocked() { return _async_exceptions_blocked; }
  void set_async_exceptions_blocked(bool b) { _async_exceptions_blocked = b; }
  void handle_unsafe_access_error();
};
#endif // SHARE_RUNTIME_HANDSHAKE_HPP
