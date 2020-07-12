#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-3.0-or-later

import socket
s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
port=1234;s.connect(("127.0.0.1",port));print("connected to port %d.."%port)

def recv_packet(s):
	packet = ""
	while True:
		a = s.recv(1024).decode()
		if len(a) == 0:
			break
#		print("received %d bytes" % len(a))
		packet += a
		if len(packet) >= 3 and packet[-3] == "#":
			break
	return packet

def send_recv_packet(s, packet):
	s.send(packet.encode())
	packet = ""
	while True:
		a = s.recv(1024).decode()
		if len(a) == 0:
			break
#		print("received %d bytes" % len(a))
		packet += a
		if len(packet) >= 3 and packet[-3] == "#":
			break
	return packet

while True:
	a = send_recv_packet(s, "+$?#3f")
	print("received %s" % a)
	a = send_recv_packet(s, "+$g#67")
	print("received %s" % a)
	a = send_recv_packet(s, "+$s#73")
	print("received %s" % a)
	a = send_recv_packet(s, "+$g#67")
	print("received %s" % a)
	a = send_recv_packet(s, "+$k#6b")
	print("received %s" % a)
	break;
