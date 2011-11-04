/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";
Cu.import("resource://gre/modules/NetUtil.jsm", this);

function DebuggerTransport(aInput, aOutput)
{
  this._input = aInput;
  this._output = aOutput;
  this._outgoing = "";
  this._incoming = "";
}

DebuggerTransport.prototype = {
  _hooks: null,
  get hooks() { return this._hooks; },
  set hooks(aHooks) { this._hooks = aHooks; },

  send: function DS_send(aPacket) {
    let data = JSON.stringify(aPacket);
    data = data.length + ':' + data;
    this._outgoing += data;
    this._flushOutgoing();
  },

  close: function DS_close() {
    this._input.close();
    this._output.close();
  },

  _flushOutgoing: function DS_flushOutgoing() {
    if (this._outgoing.length > 0) {
      var threadManager = Cc["@mozilla.org/thread-manager;1"].getService();
      this._output.asyncWait(this, 0, 0, threadManager.currentThread);
    }
  },

  onOutputStreamReady: function DS_ready(aStream) {
    let written = aStream.write(this._outgoing, this._outgoing.length);
    this._outgoing = this._outgoing.slice(written);
    this._flushOutgoing();
  },

  ready: function DS_ready() {
    let pump = Cc["@mozilla.org/network/input-stream-pump;1"]
      .createInstance(Ci.nsIInputStreamPump);
    pump.init(this._input, -1, -1, 0, 0, false);
    pump.asyncRead(this, null);
  },

  // nsIStreamListener
  onStartRequest: function DS_onStartRequest(aRequest, aContext) {},

  onStopRequest: function DS_onStopRequest(aRequest, aContext, aStatus) {
    this.close();
    this.hooks.onClosed(aStatus);
  },

  onDataAvailable: function DS_onDataAvailable(aRequest, aContext,
                                                aStream, aOffset, aCount) {
    try {
      this._incoming += NetUtil.readInputStreamToString(aStream,
                                                        aStream.available());
      while (this.processIncoming()) {};
    } catch(e) {
      dumpn("Unexpected error reading from debugging connection: " + e + " - " + e.stack);
      this.close();
      return;
    }
  },

  processIncoming: function DS_processIncoming() {
    // Well this is ugly.
    let sep = this._incoming.indexOf(':');
    if (sep < 0) {
      return false;
    }

    let count = parseInt(this._incoming.substring(0, sep));
    if (this._incoming.length - (sep + 1) < count) {
      // Don't have a complete request yet.
      return false;
    }

    // We have a complete request, pluck it out of the data and parse it.
    this._incoming = this._incoming.substring(sep + 1);
    let packet = this._incoming.substring(0, count);
    this._incoming = this._incoming.substring(count);

    try {
      var parsed = JSON.parse(packet);
    } catch(e) {
      dumpn("Error parsing incoming packet: " + packet + " (" + e + " - " + e.stack + ")");
      return true;
    }

    try {
      dumpn("Got: " + packet);
      let thr = Cc["@mozilla.org/thread-manager;1"].getService().currentThread;
      let self = this;
      thr.dispatch({run: function() {
        self.hooks.onPacket(parsed);
      }}, 0);
    } catch(e) {
      dumpn("Error handling incoming packet: " + e + " - " + e.stack);
      dumpn("Packet was: " + packet);
    }

    return true;
  }
}
