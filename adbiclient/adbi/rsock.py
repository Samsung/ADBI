import socket

class ReliableSocket(object):

    def __init__(self, address):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.socket.connect(address)
        except socket.error as e:
            raise IOError

    def send(self, data):
        total = 0
        while total < len(data):
            sent = self.socket.send(data[total:])
            if not sent:
                raise IOError('Connection error.')
            total += sent

    def recv(self, length):
        result = ''
        while len(result) < length:
            chunk = self.socket.recv(length - len(result))
            if not chunk:
                raise IOError('Connection error.')
            result += chunk
        return result

    def close(self):
        self.socket.close()