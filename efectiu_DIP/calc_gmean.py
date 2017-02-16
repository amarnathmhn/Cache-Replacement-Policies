# get the data into lines array
with open('results.txt') as f:
	data = f.read()
	lines = data.split('\n')

# calculate product of lru_ipc/crc_ipc
prod = 1.0
size = len(lines) - 1
for bmk in range(0, size ):
	#print 'bmk = ',bmk,'lines[bmk] = ', lines[bmk]
	lru_ipc = float(lines[bmk].split()[0])
	crc_ipc = float(lines[bmk].split()[1])
	prod = prod*crc_ipc/lru_ipc

# calculate gmean
gmean = prod**(1.0/size)
print 'gmean = ', gmean
