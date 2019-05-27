import threading
import time
import socket
import sys

image_test = 'mfgtest.wifistepper.image.bin'
image_final = 'wifistepper.image.1.bin'

sock_print = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock_print.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock_print.bind(('0.0.0.0', 2000))
sock_print.listen(5)

sock_update_test = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock_update_test.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock_update_test.bind(('0.0.0.0', 2001))
sock_update_test.listen(5)

sock_update_final = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock_update_final.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock_update_final.bind(('0.0.0.0', 2002))
sock_update_final.listen(5)

def _print(conn, addr):
	try:
		while True:
			line = conn.recv(1024)
			sys.stdout.write(addr[0] + ': ' + line)
	except:
		print "* [print] connection closed from addr"

def _print_listen():
	while True:
		conn, addr = sock_print.accept()
		print "* New [print] connection from", addr[0]
		thread_print = threading.Thread(target=_print, args=(conn, addr))
		thread_print.setDaemon(True)
		thread_print.start()


thread_print_listen = threading.Thread(target=_print_listen)
thread_print_listen.setDaemon(True)
thread_print_listen.start()

def _update(conn, addr, filename):
	print "* Updating client with image", filename
	fin = open(filename, 'rb')
	data = fin.read()
	fin.close()
	conn.send(data)
	conn.close()

def _update_listen(sock, filename):
	while True:
		conn, addr = sock.accept()
		print "* New [update] connection from", addr[0]
		thread_update = threading.Thread(target=_update, args=(conn, addr, filename))
		thread_update.setDaemon(True)
		thread_update.start()

thread_update_test_listen = threading.Thread(target=_update_listen, args=(sock_update_test, image_test))
thread_update_test_listen.setDaemon(True)
thread_update_test_listen.start()

thread_update_final_listen = threading.Thread(target=_update_listen, args=(sock_update_final, image_final))
thread_update_final_listen.setDaemon(True)
thread_update_final_listen.start()

while True:
	time.sleep(10)
