#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-3.0-or-later

import socket
s1=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
port1=1234;s1.connect(("127.0.0.1",port1));print("connected to port %d.."%port1)
s2=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
port2=1235;s2.connect(("127.0.0.1",port2));print("connected to port %d.."%port2)

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

def send_recv_packet2(s1, s2, packet):
	packet1 = send_recv_packet(s1, packet)
	packet2 = send_recv_packet(s2, packet)
#	print("received2 %d %s %s" % (len(packet2), type(packet2), packet2))
	if packet2[:96] != packet1[:96]:
		print("ERROR : packet2 is different from packet1 !\nreceived2 %d %s" % (len(packet2), packet2[:96]))
#		print("ERROR : packet2 is different from packet1 !")
#		for c in packet2:
#			print("%x" % ord(c),end="")
#		print("")
#		for c in packet1:
#			print("%x" % ord(c),end="")
#		print("")
#		print("received1 %d %s %s" % (len(packet1), type(packet1), packet1))
		print("received1 %d %s" % (len(packet1), packet1[:96]))
		return None
#	print("received1 %d %s %s" % (len(packet1), type(packet1), packet1))
	return packet1

while True:
	a = send_recv_packet2(s1, s2, "+$?#3f")
	while True:
		a = send_recv_packet2(s1, s2, "+$g#67")
		if a==None:
			break
		a = send_recv_packet2(s1, s2, "+$s#73")
		if a==None:
			break
	a = send_recv_packet2(s1, s2, "+$k#6b")
	break;
