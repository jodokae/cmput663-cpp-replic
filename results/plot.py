 
import glob
import matplotlib.pyplot as plt

files = glob.glob('*.csv')
NOFC = []
LOF = []
ANDAVG = [] 
ANDSTDEV = []	
SDEGMEAN = 	[]
SDEGSTD = []
TDEGMEAN = []
TDEGSTD = []
HOM	 = []
HET = []
HOHE = []
NOFPFCMEAN = []
NOFPFCSTD = []
GRANGL = []
GRANFL = []
GRANBL = []
GRANSL = []
GRANEL = []
GRANML = []
GRANERR = []
NDMAX = []
for file in files:

	lines = open(file, 'rt').readlines()
	l2  = lines[-1].split(',')
	data2 = l2[2:13] + l2[21:23]
	data2 = [d.rstrip() for d in data2]
	NOFC.append(float(data2[0]))
	LOF.append(float(data2[1]))
	ANDAVG.append(float(data2[2]))
	ANDSTDEV.append(float(data2[3]))
	SDEGMEAN.append(float(data2[4]))
	SDEGSTD.append(float(data2[5]))
	TDEGMEAN.append(float(data2[6]))
	TDEGSTD.append(float(data2[7]))
	HOM.append(float(data2[8]))
	HET.append(float(data2[9]))
	HOHE.append(float(data2[10]))
	NOFPFCMEAN.append(float(data2[11]))
	NOFPFCSTD.append(float(data2[12]))
	GRANGLT = []
	GRANFLT = []
	GRANBLT = []
	GRANSLT = []
	GRANELT = []
	GRANMLT = []
	GRANERRT = []
	NDMAXT = []
	for line in lines:
		if line.count(',') > 5 and line.split(',')[0][0] == '/':
			GRANGLT.append(float(line.split(',')[13]))
			GRANFLT.append(float(line.split(',')[14]))
			GRANBLT.append(float(line.split(',')[15]))
			GRANSLT.append(float(line.split(',')[16]))
			GRANELT.append(float(line.split(',')[17]))
			GRANMLT.append(float(line.split(',')[18]))
			GRANERRT.append(float(line.split(',')[19]))
			NDMAXT.append(float(line.split(',')[20]))
	GRANGL.append(sum(GRANGLT))
	GRANFL.append(sum(GRANFLT))
	GRANBL.append(sum(GRANBLT))
	GRANSL.append(sum(GRANSLT))
	GRANEL.append(sum(GRANELT))
	GRANML.append(sum(GRANMLT))
	GRANERR.append(sum(GRANERRT))
	NDMAX.append(max(NDMAXT))
print(NDMAX)


plt.plot(NOFC)
plt.title('NOFC')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(LOF)
plt.title('LOF')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(ANDAVG)
plt.title('ANDAVG')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(ANDSTDEV)
plt.title('ANDSTDEV')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(SDEGMEAN)
plt.title('SDEGMEAN')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(SDEGSTD)
plt.title('SDEGSTD')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(TDEGMEAN)
plt.title('TDEGMEAN')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(TDEGSTD)
plt.title('TDEGSTD')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()




plt.plot(HOM)
plt.title('HOM')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(HET)
plt.title('HET')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(HOHE)
plt.title('HOHE')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(NOFPFCMEAN)
plt.title('NOFPFCMEAN')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(NOFPFCSTD)
plt.title('NOFPFCSTD')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(GRANGL)
plt.title('GRANGL')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANFL)
plt.title('GRANFL')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(GRANBL)
plt.title('GRANBL')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(GRANSL)
plt.title('GRANSL')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(GRANEL)
plt.title('GRANEL')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()



plt.plot(GRANML)
plt.title('GRANML')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(GRANERR)
plt.title('GRANERR')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()


plt.plot(NDMAX)
plt.title('NDMAX')
plt.xlabel(('2.2.11','2.2.33','2.2.34','2.4.27','2.4.28','2.4.29'))
plt.show()

