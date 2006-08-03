/*
 * Rjb - Ruby <-> Java Bridge
 * Copyright(c) 2004 arton
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * $Id: RBridge.java 2 2006-04-11 19:04:40Z arton $
 * $Log: RBridge.java,v $
 * Revision 1.2  2004/06/19 09:05:00  arton
 * delete debug lines
 *
 * Revision 1.1  2004/06/19 09:00:19  arton
 * Initial revision
 *
 */
package jp.co.infoseek.hp.arton.rjb;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public class RBridge implements InvocationHandler {
    public Object register(Class itf) {
	return Proxy.newProxyInstance(itf.getClassLoader(),
		      new Class[] { itf }, this);
    }
    public Object invoke(Object proxy, Method method, Object[] args)
	throws Throwable {
	return call(method.getName(), proxy, args);
    }
    private native Object call(String methodName, Object target, Object[] args);
}
