from sys import argv
import md5
from struct import pack

fin = open(argv[1], 'rb')
data = fin.read()
fin.close()

hash = md5.new(data)
hash.update(pack('<B33x', 0x4))
print "Appending md5 " + hash.hexdigest()

fout = open(argv[1], 'ab')
fout.write(pack('<B33s', 0x4, hash.hexdigest()))
fout.flush()
fout.close()

