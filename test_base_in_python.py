#!/usr/bin/python
from ctypes import *
import time
import os
import signal

def waitForThread(param1,param2):
	print 'kill me'
	#//call c++ safe quit()
	time.sleep(1)
	os.kill(os.getpid(),signal.SIGKILL)
def OnReceive(sockid, strmsg):
	#print strmsg
	lib.sendMessage(sockid, strmsg)	
	#time.sleep(1)

def OnConnect(sockid, strmsg):
	print strmsg
def OnDisConnect(sockid, strmsg):
	print strmsg

CCReceiveFUNC = CFUNCTYPE(None, c_int32, c_char_p) 
CCConnectFUNC = CFUNCTYPE(None, c_int32, c_char_p) 
CCDisConnectFUNC = CFUNCTYPE(None, c_int, c_char_p) 
gReceiveFunc = CCReceiveFUNC(OnReceive)
gConnectFunc = CCConnectFUNC(OnConnect)
gDisConnectFunc = CCDisConnectFUNC(OnDisConnect)
mydll = cdll.LoadLibrary
lib = mydll("./base.so")
lib.setCallbackFunc(gReceiveFunc,gConnectFunc,gDisConnectFunc)
lib.init("192.168.1.159",8887)

if __name__ == '__main__':
	global lib
	i = lib.test(0,"test")
	signal.signal(signal.SIGINT,waitForThread)
	print i
	while True:
		time.sleep(1)
	lib.destory()
