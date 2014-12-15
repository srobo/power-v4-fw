import usb1
import struct

class Power:
	CMD_WRITE_output0 = 0
	CMD_WRITE_output1 = 1
	CMD_WRITE_output2 = 2
	CMD_WRITE_output3 = 3
	CMD_WRITE_output4 = 4
	CMD_WRITE_output5 = 5
	CMD_WRITE_runled = 6
	CMD_WRITE_errorled = 7
	CMD_WRITE_piezo = 8
	CMD_READ_output0 = 0
	CMD_READ_output1 = 1
	CMD_READ_output2 = 2
	CMD_READ_output3 = 3
	CMD_READ_output4 = 4
	CMD_READ_output5 = 5
	CMD_READ_5vrail = 6
	CMD_READ_batt = 7
	CMD_READ_button = 8
	def __init__(self):
		ctx = usb1.USBContext()
		self.usb_dev = ctx.getByVendorIDAndProductID(0x1bda, 0x0010)
		self.handle = self.usb_dev.open()
		if (self.usb_dev == None):
			raise Exception("Could not find attached power board")

	def set_output_rail(self, rail, status):
		if rail > 5 or rail < 0:
			raise Exception("Setting out-of-range rail address")

		if status:
			val = True
		else:
			val = False

		cmd = Power.CMD_WRITE_output0 + rail
		self.handle.controlWrite(0, 64, val, cmd, 0)

	def set_run_led(self, status):
		if status:
			val = True
		else:
			val = False

		self.handle.controlWrite(0, 64, val, Power.CMD_WRITE_runled, 0)

	def set_error_led(self, status):
		if status:
			val = True
		else:
			val = False

		self.handle.controlWrite(0, 64, val, Power.CMD_WRITE_errorled, 0)

    def send_piezo(self, freq, duration):
        data = struct.pack("HH", freq, duration)
		self.handle.controlWrite(0, 64, 0, Power.CMD_WRITE_piezo, data)

	def read_batt_status(self):
		""" Measured in mA and mV"""
		result = self.handle.controlRead(0x80, 64, 0, Power.CMD_READ_batt, 8)
		current, voltage = struct.unpack("ii", result)
		return current, voltage

	def read_button(self):
		result = self.handle.controlRead(0x80, 64, 0, Power.CMD_READ_button, 4)
		status, = struct.unpack("i", result)
		if status == 0:
			return False
		else:
			return True
