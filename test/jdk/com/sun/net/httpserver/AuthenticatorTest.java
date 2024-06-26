/*
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates. All rights reserved.
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
 * @bug 8251496
 * @summary Tests for methods in Authenticator
 * @run junit AuthenticatorTest
 */

import com.sun.net.httpserver.Authenticator;
import com.sun.net.httpserver.HttpPrincipal;

import org.junit.jupiter.api.Test;

import static org.junit.jupiter.api.Assertions.assertEquals;


public class AuthenticatorTest {
    @Test
    public void testFailure() {
        var failureResult = new Authenticator.Failure(666);
        assertEquals(666, failureResult.getResponseCode());
    }

    @Test
    public void testRetry() {
        var retryResult = new Authenticator.Retry(333);
        assertEquals(333, retryResult.getResponseCode());
    }

    @Test
    public void testSuccess() {
        var principal = new HttpPrincipal("test", "123");
        var successResult = new Authenticator.Success(principal);
        assertEquals(principal, successResult.getPrincipal());
        assertEquals("test", principal.getUsername());
        assertEquals("123", principal.getRealm());
    }
}
