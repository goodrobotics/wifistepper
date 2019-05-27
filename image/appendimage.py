from sys import argv
import md5
from struct import pack
from os.path import basename

print "Appending image " + basename(argv[1])
fin = open(argv[1], 'rb')
data = fin.read()
fin.close()

hash = md5.new(data)
size = len(data)

fout = open(argv[2], 'ab')
fout.write(pack('<B33sI', 0x2, hash.hexdigest(), size))
fout.write(data)
fout.flush()
fout.close()

