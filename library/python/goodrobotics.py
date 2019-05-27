import threading
import time
import struct
import socket
import json
import hashlib
import hmac
import binascii

import Queue as waitqueue

class Nack(Exception):
    def __init__(self, message): self.message = message
    def __str__(self): return str(self.message)

class Closed(Exception):
    def __init__(self): pass
    def __str__(self): return "Connection closed or not connected."

class RPM:
    def __init__(self, steps_per_revolution=200): self.coeff = float(steps_per_revolution) / 60.0
    def __call__(self, rpm): return float(rpm) * self.coeff
    def fromStepss(self, stepss): return float(stepss) / self.coeff

class Angle:
    def __init__(self, stepsize, steps_per_revolution=200): self.coeff = float(steps_per_revolution) * float(stepsize) / 360.0
    def __call__(self, angle): return float(angle) * self.coeff
    def fromPos(self, pos): return float(pos) / self.coeff


def _ascii_encode_dict(data):
    ascii_encode = lambda x: x.encode('ascii') if isinstance(x, unicode) else x 
    return dict(map(ascii_encode, pair) for pair in data.items())

class _ComCommon:
    _L_MAGIC_1 = (0xAE)
    _L_MAGIC_2 = (0x7B11)

    _TYPE_ERROR = (0x00)
    _TYPE_HELLO = (0x01)
    _TYPE_GOODBYE = (0x02)
    _TYPE_PING = (0x03)
    _TYPE_STD = (0x04)
    _TYPE_CRYPTO = (0x05)

    _OPCODE_ESTOP = (0x00)
    _OPCODE_PING = (0x01)
    _OPCODE_CLEARERROR = (0x02)
    _OPCODE_LASTWILL = (0x03)
    _OPCODE_SETHTTP = (0x04)
    _OPCODE_RUNQUEUE = (0x05)
    _OPCODE_SETCONFIG = (0x06)
    _OPCODE_GETCONFIG = (0x07)
    _OPCODE_GETSTATE = (0x08)

    _OPCODE_STOP = (0x11)
    _OPCODE_RUN = (0x12)
    _OPCODE_STEPCLOCK = (0x13)
    _OPCODE_MOVE = (0x14)
    _OPCODE_GOTO = (0x15)
    _OPCODE_GOUNTIL = (0x16)
    _OPCODE_RELEASESW = (0x17)
    _OPCODE_GOHOME = (0x18)
    _OPCODE_GOMARK = (0x19)
    _OPCODE_RESETPOS = (0x1A)
    _OPCODE_SETPOS = (0x1B)
    _OPCODE_SETMARK = (0x1C)

    _OPCODE_WAITBUSY = (0x21)
    _OPCODE_WAITRUNNING = (0x22)
    _OPCODE_WAITMS = (0x23)
    _OPCODE_WAITSWITCH = (0x24)

    _OPCODE_EMPTYQUEUE = (0x31)
    _OPCODE_SAVEQUEUE = (0x32)
    _OPCODE_LOADQUEUE = (0x33)
    _OPCODE_ADDQUEUE = (0x34)
    _OPCODE_COPYQUEUE = (0x35)
    
    _OPCODE_GETQUEUE = (0x37)

    _SUBCODE_NACK = (0x00)
    _SUBCODE_ACK = (0x01)
    _SUBCODE_CMD = (0x02)
    _SUBCODE_REPLY = (0x03)

    _LTO_PING = (3000)
    _LTO_ACK = (2000)

    _PACK_HELLO = '<36s36s36sH24sIBBI'
    _PACK_STD = '<BBBBHH'
    _PACK_ERRORSTATE = '<BLBIib'

    class Response:
        def __init__(self, t, d):
            self.type = t
            self.data = d

    def _nextid(self):
        self.last_id += 1
        return self.last_id

    def _preamble(self, type):
        return struct.pack('<BHB', self._L_MAGIC_1, self._L_MAGIC_2, type)

    def _header(self, opcode, subcode, target, queue, packetid, length):
        return struct.pack('<BBBBHH', opcode, subcode, target, queue, packetid, length)

    def _checkconnected(self):
        if not self.connected: raise Closed()

    def _waitreply(self, packetid, subcode):
        q = waitqueue.Queue(1)
        self.wait_dict[packetid] = q
        try:
            r = q.get(True, self._LTO_ACK / 1000.0)
            if r.type == subcode: return r.data
            elif r.type == self._SUBCODE_NACK: raise Nack(r.data)
            else: return None
        except waitqueue.Empty: return None
        finally: del self.wait_dict[packetid]

    def _rxworker(self):
        while self.connected:
            try:
                (p_magic1,) = struct.unpack('<B', self.sock.recv(1))
                if p_magic1 != self._L_MAGIC_1: continue
            except: continue

            try:
                (p_magic2, p_type) = struct.unpack('<HB', self.sock.recv(3))
                if p_magic2 != self._L_MAGIC_2: continue
            except: continue

            if p_type == self._TYPE_ERROR:
                (d_errored, d_when, d_subsystem, d_id, d_type, d_arg) = struct.unpack(self._PACK_ERRORSTATE, self.sock.recv(struct.calcsize(self._PACK_ERRORSTATE)))
                self.error_last = {'id':d_id, 'subsystem':d_subsystem, 'type':d_type, 'arg':d_arg, 'when':d_when} if d_errored else None
                if self.error_callback is not None: self.error_callback(id=d_id, subsystem=d_subsystem, type=d_type, arg=d_arg, when=d_when)

            elif p_type == self._TYPE_HELLO:
                (d_product, d_model, d_branch, d_version, d_hostname, d_chipid, self.enabled_std, self.enabled_crypto, self.nonce) = struct.unpack(self._PACK_HELLO, self.sock.recv(struct.calcsize(self._PACK_HELLO)))
                self.meta.update({'product':d_product, 'model':d_model, 'branch':d_branch, 'version':d_version, 'hostname':d_hostname, 'chipid':d_chipid})
                self.last_id = 0
                self._recv_hello(self.enabled_std, self.enabled_crypto, self.nonce, self.meta)

            elif p_type == self._TYPE_GOODBYE:
                self.close()

            elif p_type == self._TYPE_PING:
                self.last_ping = time.time()

            elif p_type == self._TYPE_STD:
                (h_opcode, h_subcode, h_target, h_queue, h_packetid, h_length) = struct.unpack(self._PACK_STD, self.sock.recv(struct.calcsize(self._PACK_STD)))
                (d_remaining, d_data) = (h_length, '')
                while d_remaining > 0:
                    d_read = self.sock.recv(d_remaining)
                    d_data += d_read
                    d_remaining -= len(d_read)
                self._recv_std(h_opcode, h_subcode, h_target, h_queue, h_packetid, d_data)

            elif p_type == self._TYPE_CRYPTO:
                mac = self.sock.recv(32)
                (h_opcode, h_subcode, h_target, h_queue, h_packetid, h_length) = struct.unpack(self._PACK_STD, self.sock.recv(struct.calcsize(self._PACK_STD)))
                (d_remaining, d_data) = (h_length, '')
                while d_remaining > 0:
                    d_read = self.sock.recv(d_remaining)
                    d_data += d_read
                    d_remaining -= len(d_read)
                self._recv_crypto(mac, h_opcode, h_subcode, h_target, h_queue, h_packetid, d_data)

    def _txworker(self):
        while self.connected:
            self.sock.send(self._preamble(self._TYPE_PING))
            time.sleep(self._LTO_PING / (1000.0 * 4.0))

    def __init__(self, host, port, error_callback):
        self.connected = False
        self.meta = dict()
        self.wait_dict = dict()
        self.enabled_std = False
        self.enabled_crypto = False
        self.nonce = 0
        self.last_id = 0
        self.last_ping = 0

        self.error_callback = error_callback
        self.error_last = None

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.host = host
        self.port = port
        self.rx = threading.Thread(name='lowcom rxworker', target=self._rxworker)
        self.rx.setDaemon(True)
        self.tx = threading.Thread(name='lowcom txworker', target=self._txworker)
        self.tx.setDaemon(True)

    def connect(self):
        if self.connected: return

        self.sock.connect((self.host, self.port))
        self.connected = True
        self.rx.start()
        self.tx.start()

        # Send hello
        self.sock.send(self._preamble(self._TYPE_HELLO))

        # Send ping command
        return self.cmd_ping(0, 0)

    def close(self):
        self.connected = False
        self.sock.close()

    def error(self):
        return self.error_last

    def cmd_estop(self, target, hiz, soft):
        self._checkconnected()
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        return self._waitreply(self._send(self._OPCODE_ESTOP, self._SUBCODE_CMD, target, 0, struct.pack('<BB', b_hiz, b_soft)), self._SUBCODE_ACK)

    def cmd_ping(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_PING, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_clearerror(self, target):
        self._checkconnected()
        self.error_last = None
        return self._waitreply(self._send(self._OPCODE_CLEARERROR, self._SUBCODE_CMD, target, 0), self._SUBCODE_ACK)

    def cmd_lastwill(self, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_LASTWILL, self._SUBCODE_CMD, 0, queue), self._SUBCODE_ACK)

    def cmd_sethttp(self, active):
        self._checkconnected()
        b_active = 0x01 if active else 0x00
        return self._waitreply(self._send(self._OPCODE_SETHTTP, self._SUBCODE_CMD, 0, 0, struct.pack('<B', b_active)), self._SUBCODE_ACK)

    def cmd_runqueue(self, target, queue, targetqueue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_RUNQUEUE, self._SUBCODE_CMD, target, queue, struct.pack('<B', targetqueue)), self._SUBCODE_ACK)

    def cmd_setconfig(self, target, queue, config):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_SETCONFIG, self._SUBCODE_CMD, target, queue, config + struct.pack('x')), self._SUBCODE_ACK)

    def cmd_getconfig(self, target):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_GETCONFIG, self._SUBCODE_CMD, target, 0), self._SUBCODE_REPLY).rstrip('\x00')

    def cmd_getstate(self, target):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_GETSTATE, self._SUBCODE_CMD, target, 0), self._SUBCODE_REPLY).rstrip('\x00')

    def cmd_stop(self, target, queue, hiz, soft):
        self._checkconnected()
        b_hiz = 0x01 if hiz else 0x00
        b_soft = 0x01 if soft else 0x00
        return self._waitreply(self._send(self._OPCODE_STOP, self._SUBCODE_CMD, target, queue, struct.pack('<BB', b_hiz, b_soft)), self._SUBCODE_ACK)

    def cmd_run(self, target, queue, dir, stepss):
        self._checkconnected()
        b_dir = 0x01 if dir else 0x00
        return self._waitreply(self._send(self._OPCODE_RUN, self._SUBCODE_CMD, target, queue, struct.pack('<Bf', b_dir, stepss)), self._SUBCODE_ACK)

    def cmd_stepclock(self, target, queue, dir):
        self._checkconnected()
        b_dir = 0x01 if dir else 0x00
        return self._waitreply(self._send(self._OPCODE_STEPCLOCK, self._SUBCODE_CMD, target, queue, struct.pack('<B', b_dir)), self._SUBCODE_ACK)

    def cmd_move(self, target, queue, dir, microsteps):
        self._checkconnected()
        b_dir = 0x01 if dir else 0x00
        return self._waitreply(self._send(self._OPCODE_MOVE, self._SUBCODE_CMD, target, queue, struct.pack('<BI', b_dir, microsteps)), self._SUBCODE_ACK)

    def cmd_goto(self, target, queue, hasdir, dir, pos):
        self._checkconnected()
        b_hasdir = 0x01 if hasdir else 0x00
        b_dir = 0x01 if dir else 0x00
        return self._waitreply(self._send(self._OPCODE_GOTO, self._SUBCODE_CMD, target, queue, struct.pack('<BBi', b_hasdir, b_dir, pos)), self._SUBCODE_ACK)

    def cmd_gountil(self, target, queue, action, dir, stepss):
        self._checkconnected()
        b_action = 0x01 if action else 0x00
        b_dir = 0x01 if dir else 0x00
        return self._waitreply(self._send(self._OPCODE_GOUNTIL, self._SUBCODE_CMD, target, queue, struct.pack('<BBf', b_action, b_dir, stepss)), self._SUBCODE_ACK)
    
    def cmd_releasesw(self, target, queue, action, dir):
        self._checkconnected()
        b_action = 0x01 if action else 0x00
        b_dir = 0x01 if dir else 0x00
        return self._waitreply(self._send(self._OPCODE_RELEASESW, self._SUBCODE_CMD, target, queue, struct.pack('<BB', b_action, b_dir)), self._SUBCODE_ACK)

    def cmd_gohome(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_GOHOME, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_gomark(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_GOMARK, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_resetpos(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_RESETPOS, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_setpos(self, target, queue, pos):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_SETPOS, self._SUBCODE_CMD, target, queue, struct.pack('<i', pos)), self._SUBCODE_ACK)

    def cmd_setmark(self, target, queue, mark):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_SETMARK, self._SUBCODE_CMD, target, queue, struct.pack('<i', pos)), self._SUBCODE_ACK)

    def cmd_waitbusy(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_WAITBUSY, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_waitrunning(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_WAITRUNNING, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_waitms(self, target, queue, ms):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_WAITMS, self._SUBCODE_CMD, target, queue, struct.pack('<I', ms)), self._SUBCODE_ACK)

    def cmd_waitswitch(self, target, queue, state):
        self._checkconnected()
        b_state = 0x01 if state else 0x00
        return self._waitreply(self._send(self._OPCODE_WAITSWITCH, self._SUBCODE_CMD, target, queue, struct.pack('<B', b_state)), self._SUBCODE_ACK)

    def cmd_emptyqueue(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_EMPTYQUEUE, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_savequeue(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_SAVEQUEUE, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_loadqueue(self, target, queue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_LOADQUEUE, self._SUBCODE_CMD, target, queue), self._SUBCODE_ACK)

    def cmd_copyqueue(self, target, queue, sourcequeue):
        self._checkconnected()
        return self._waitreply(self._send(self._OPCODE_COPYQUEUE, self._SUBCODE_CMD, target, queue, struct.pack('<B', sourcequeue)), self._SUBCODE_ACK)


class ComStandard(_ComCommon):
    def __init__(self, host, port, error_callback, **kwargs):
        _ComCommon.__init__(self, host, port, error_callback)

    def _send(self, opcode, subcode, target, queue, data = ''):
        packetid = self._nextid()
        self.sock.send(self._preamble(self._TYPE_STD) + self._header(opcode, subcode, target, queue, packetid, len(data)) + data)
        return packetid

    def _recv_hello(self, enabled_std, enabled_crypto, nonce, meta):
        pass

    def _recv_std(self, opcode, subcode, target, queue, packetid, data):
        if subcode in [self._SUBCODE_ACK, self._SUBCODE_NACK, self._SUBCODE_REPLY]:
            if subcode == self._SUBCODE_ACK: (data,) = struct.unpack('<I', data)
            if subcode == self._SUBCODE_NACK: data = data.rstrip('\x00')
            if packetid in self.wait_dict:
                try: self.wait_dict[packetid].put(self.Response(subcode, data), False)
                except waitqueue.Full: pass

    def _recv_crypto(self, mac, opcode, subcode, target, queue, packetid, data):
        pass


class ComCrypto(_ComCommon):
    def __init__(self, host, port, key, error_callback, **kwargs):
        _ComCommon.__init__(self, host, port, error_callback)
        self.__key = hashlib.sha256(key).digest()

    def _send(self, opcode, subcode, target, queue, data = ''):
        packetid = self._nextid()
        payload = self._header(opcode, subcode, target, queue, packetid, len(data)) + data
        calcmac = hmac.new(self.__key, struct.pack('<I', self.nonce) + payload, hashlib.sha256).digest()
        self.sock.send(self._preamble(self._TYPE_CRYPTO) + calcmac + payload)
        return packetid

    def _recv_hello(self, enabled_std, enabled_crypto, nonce, meta):
        pass

    def _recv_std(self, opcode, subcode, target, queue, packetid, data):
        if subcode == self._SUBCODE_NACK:
            data = data.rstrip('\x00')
            if packetid in self.wait_dict:
                try: self.wait_dict[packetid].put(self.Response(subcode, data), False)
                except waitqueue.Full: pass

    def _recv_crypto(self, mac, opcode, subcode, target, queue, packetid, data):
        calcmac = hmac.new(self.__key, struct.pack('<I', self.nonce) + self._header(opcode, subcode, target, queue, packetid, len(data)) + data, hashlib.sha256).digest()
        verified = mac == calcmac

        if subcode in [self._SUBCODE_ACK, self._SUBCODE_NACK, self._SUBCODE_REPLY]:
            if subcode == self._SUBCODE_ACK: (data,) = struct.unpack('<I', data)
            if subcode == self._SUBCODE_NACK: data = data.rstrip('\x00')
            if packetid in self.wait_dict:
                resp = self.Response(subcode, data)
                if not verified: resp = self.Response(self._SUBCODE_NACK, "Bad hmac signature in response (Key error)")
                try: self.wait_dict[packetid].put(resp, False)
                except waitqueue.Full: pass

class ComClosed:
    def __init__(self, **kwargs):
        pass

    def close(self):
        pass

    def __getattr__(self, attr):
        raise Closed()


class WifiStepper:
    __comm = None
    __target = 0

    FORWARD = True
    REVERSE = False

    RESET = False
    COPYMARK = True

    CLOSED = True
    OPEN = False

    def _target(self, t):
        return t if t is not None else self.__target

    def __init__(self, proto=ComStandard, host=None, port=1000, target=0, key=None, comm=None, error_callback=None):
        self.__comm = comm if comm is not None else proto(**{'host': host, 'port': port, 'key': key, 'error_callback': error_callback})
        self.__target = target

    def connect(self):
        return self.__comm.connect()

    def close(self):
        self.__comm.close()
        self.__comm = ComClosed()

    def target(self, target):
        return WifiStepper(target=target, comm=self.__comm)

    def estop(self, hiz = True, soft = True, target = None):
        return self.__comm.cmd_estop(self._target(target), hiz, soft)

    def ping(self, target = None, queue = 0):
        return self.__comm.cmd_ping(self._target(target), queue)

    def geterror(self):
        return self.__comm.error()

    def clearerror(self, target = None):
        return self.__comm.cmd_clearerror(self._target(target))

    def lastwill(self, queue):
        return self.__comm.cmd_lastwill(queue)

    def sethttp(self, active):
        return self.__comm.cmd_sethttp(active)

    def runqueue(self, targetqueue, target = None, queue = 0):
        return self.__comm.cmd_runqueue(self._target(target), queue, targetqueue)

    def setconfig(self, config, target = None, queue = 0):
        return self.__comm.cmd_setconfig(self._target(target), queue, json.dumps(config, separators=(',',':')))

    def getconfig(self, target = None):
        return json.loads(self.__comm.cmd_getconfig(self._target(target)), object_hook=_ascii_encode_dict)

    def getstate(self, target = None):
        return json.loads(self.__comm.cmd_getstate(self._target(target)), object_hook=_ascii_encode_dict)

    def busy(self, target = None):
        return self.getstate(target).get('busy', None)

    def hiz(self, target = None):
        return self.getstate(target).get('hiz', None)

    def stop(self, hiz = True, soft = True, target = None, queue = 0):
        return self.__comm.cmd_stop(self._target(target), queue, hiz, soft)

    def run(self, dir, stepss, target = None, queue = 0):
        return self.__comm.cmd_run(self._target(target), queue, dir, stepss)

    def stepclock(self, dir, target = None, queue = 0):
        return self.__comm.cmd_stepclock(self._target(target), queue, dir)

    def move(self, dir, microsteps, target = None, queue = 0):
        return self.__comm.cmd_move(self._target(target), queue, dir, microsteps)

    def goto(self, pos, dir = None, target = None, queue = 0):
        return self.__comm.cmd_goto(self._target(target), queue, dir != None, dir, pos)

    def gountil(self, action, dir, stepss, target = None, queue = 0):
        return self.__comm.cmd_gountil(self._target(target), queue, action, dir, stepss)

    def releasesw(self, action, dir, target = None, queue = 0):
        return self.__comm.cmd_releasesw(self._target(target), queue, action, dir)

    def gohome(self, target = None, queue = 0):
        return self.__comm.cmd_gohome(self._target(target), queue)

    def gomark(self, target = None, queue = 0):
        return self.__comm.cmd_gomark(self._target(target), queue)

    def resetpos(self, target = None, queue = 0):
        return self.__comm.cmd_resetpos(self._target(target), queue)

    def setpos(self, pos, target = None, queue = 0):
        return self.__comm.cmd_setpos(self._target(target), queue, pos)

    def setmark(self, pos, target = None, queue = 0):
        return self.__comm.cmd_setmark(self._target(target), queue, pos)

    def waitbusy(self, target = None, queue = 0):
        return self.__comm.cmd_waitbusy(self._target(target), queue)

    def waitrunning(self, target = None, queue = 0):
        return self.__comm.cmd_waitrunning(self._target(target), queue)

    def waitms(self, ms, target = None, queue = 0):
        return self.__comm.cmd_waitms(self._target(target), queue, ms)

    def waitswitch(self, state, target = None, queue = 0):
        return self.__comm.cmd_waitswitch(self._target(target), queue, state)

    def emptyqueue(self, queue, target = None):
        return self.__comm.cmd_emptyqueue(self._target(target), queue)

    def savequeue(self, queue, target = None):
        return self.__comm.cmd_savequeue(self._target(target), queue)

    def loadqueue(self, queue, target = None):
        return self.__comm.cmd_loadqueue(self._target(target), queue)

    def copyqueue(self, queue, sourcequeue, target = None):
        return self.__comm.cmd_copyqueue(self._target(target), queue, sourcequeue)
