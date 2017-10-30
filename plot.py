 
import glob
import matplotlib.pyplot as plt

files = sorted(glob.glob('results/*.csv'))
LOC = []
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

# Skip 2.2.11 Version
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
	LOCT = []
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
			LOCT.append(float(line.split(',')[1]))
			GRANGLT.append(float(line.split(',')[13]))
			GRANFLT.append(float(line.split(',')[14]))
			GRANBLT.append(float(line.split(',')[15]))
			GRANSLT.append(float(line.split(',')[16]))
			GRANELT.append(float(line.split(',')[17]))
			GRANMLT.append(float(line.split(',')[18]))
			GRANERRT.append(float(line.split(',')[19]))
			NDMAXT.append(float(line.split(',')[20]))
	LOC.append(sum(LOCT))
	GRANGL.append(sum(GRANGLT))
	GRANFL.append(sum(GRANFLT))
	GRANBL.append(sum(GRANBLT))
	GRANSL.append(sum(GRANSLT))
	GRANEL.append(sum(GRANELT))
	GRANML.append(sum(GRANMLT))
	GRANERR.append(sum(GRANERRT))
	NDMAX.append(max(NDMAXT))


plt.plot(LOC[1:])
plt.title('LOC')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(LOF[1:])
plt.title('LOF')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(ANDAVG[1:])
plt.title('ANDAVG')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(ANDSTDEV[1:])
plt.title('ANDSTDEV')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(SDEGMEAN[1:])
plt.title('SDEGMEAN')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(SDEGSTD[1:])
plt.title('SDEGSTD')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(TDEGMEAN[1:])
plt.title('TDEGMEAN')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(TDEGSTD[1:])
plt.title('TDEGSTD')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(HOM[1:])
plt.title('HOM')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(HET[1:])
plt.title('HET')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(HOHE[1:])
plt.title('HOHE')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(NOFPFCMEAN[1:])
plt.title('NOFPFCMEAN')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(NOFPFCSTD[1:])
plt.title('NOFPFCSTD')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANGL[1:])
plt.title('GRANGL')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANFL[1:])
plt.title('GRANFL')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANBL[1:])
plt.title('GRANBL')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANSL[1:])
plt.title('GRANSL')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANEL[1:])
plt.title('GRANEL')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANML[1:])
plt.title('GRANML')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(GRANERR[1:])
plt.title('GRANERR')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()

plt.plot(NDMAX[1:])
plt.title('NDMAX')
plt.xlabel(('2.4.25','2.4.26','2.4.27','2.4.28','2.4.29'))
plt.show()
