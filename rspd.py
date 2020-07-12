#!/usr/bin/env python3

# SPDX-License-Identifier: GPL-3.0-or-later

import ctypes
lib=ctypes.cdll.LoadLibrary("./libxtem.so")
lib.xtem_rsp_init.restype = ctypes.c_void_p
lib.xtem_rsp_s.argtypes = (ctypes.c_void_p,)
lib.xtem_rsp_c.argtypes = (ctypes.c_void_p,)
lib.xtem_rsp_g.argtypes = (ctypes.c_void_p,ctypes.c_char_p)
lib.xtem_rsp_m.argtypes = (ctypes.c_void_p,ctypes.c_char_p,ctypes.c_int,ctypes.c_int)
rsp=lib.xtem_rsp_init()
print("rsp=%x" % rsp)

import socket
ss=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
ss.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
port=1235;ss.bind(("",port));ss.listen(1);print("accepting on port %d.."%port)
s,addr=ss.accept();print("receiving from %s.."%str(addr))
while True:
	a=s.recv(1).decode()
	if a=='+':
		continue
	if a=='$':			# begin sync cmd, terminated by #xy
		c=""
		while True:
			c+=s.recv(1024).decode()
			d=c.find('#')
			if d!=-1:
				if len(c)<d+2:
					c+=s.recv(2).decode()
				break
		#print("received sync cmd %s"%c)
		s.send('+'.encode())		# ack
		r=""		# default reply is "$", N/A
		csum=0
		if c[0]=='?':			# get program status
			#s.sendall("$O4142430D0A#66")
			r+="S05"
#			r+="T05thread:01;"
			#r+="O4142430D0A"
			#r+="OABCDEFGHIJK"
		elif c[0]=='k':			# kill
			break
		elif c[0]=='s':			# step instruction
			res=lib.xtem_rsp_s(rsp)
			#print("res=%d" % res)
			r+="S05"
#			r+="T05thread:01;"
		elif c[0]=='c':			# continue execution
			res=lib.xtem_rsp_c(rsp)
			#print("res=%d" % res)
			r+="S05"
			#r+="O414243440D0A"
		elif c[0]=='g':			# get registers
			data="0"*(8*16+560)
			data = data.encode()
			#print("before=%s" % data)
			res=lib.xtem_rsp_g(rsp, data)
			data = data.decode()
			#print("after=%s" % data)
			r+=data
		elif c[0]=='m':			# read memory
			#print("m command : [%s]" % c)
			a,l=c.split('m')[1].split("#")[0].split(',')
			a=int('0x' + a,0)
			l=int('0x' + l,0)
			#print("a=%x l=%x" % (a, l))
			data="0"*2*l
			data = data.encode()
			#print("before=%s" % data)
			res=lib.xtem_rsp_m(rsp, data, ctypes.c_int(a), ctypes.c_int(l))
			data = data.decode()
			#print("after=%s" % data)
			r+=data
		for c in r:
			csum+=ord(c)
		csum&=0xff
		#print("reply=%s csum=%02x"%(r,csum))
		r="$"+r+"#%02x"%csum
		s.sendall(r.encode())
	elif a=="":
		print("client left")
		break
	else:
		print("received unknown async cmd %s (%x)"%(a,ord(a)))
		break
