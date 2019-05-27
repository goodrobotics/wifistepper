from sys import argv
import md5
from struct import pack

print "Appending data file " + argv[2]
fin = open(argv[1], 'rb')
data = fin.read()
fin.close()

name = argv[2]
hash = md5.new(data)
size = len(data)

fout = open(argv[3], 'ab')
fout.write(pack('<B36s33sI', 0x3, name, hash.hexdigest(), size))
fout.write(data)
fout.flush()
fout.close()

