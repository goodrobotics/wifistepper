import goodrobotics as gr

s = gr.WifiStepper(host='wsx100.local')
s.connect()


s.run(True, 100)
s.waitms(5000)
s.goto(-1000)
s.waitbusy()
s.stop()


import goodrobotics as gr
s = gr.WifiStepper(proto=gr.ComCrypto, host='wsx100.local', key="newpass1")
s.connect()


s.run(True, 100, queue=2)
s.waitms(5000, queue=2)
s.goto(-1000, queue=2)
s.waitbusy(queue=2)
s.stop(queue=2)
s.waitms(1000, queue=2)
s.runqueue(2, queue=2)


import goodrobotics as gr
import time

s = gr.WifiStepper(host='wsx100.local')
s.connect()

def p():
	start = time.time()
	s.ping()
	end = time.time()
	time_taken = end - start
	print('Time: ',time_taken)

while True:
	time.sleep(0.1)
	p()
