import glob
import numpy as np
import matplotlib.pyplot as plt

def sumzip(*items):
    return [sum(values) for values in zip(*items)]

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
FileCOUNT = []

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
	FileCOUNTT = 0
	VP = []
	
	for line in lines:
		if line.count(',') > 5 and line.split(',')[0][0] == '/':
			FileCOUNTT = FileCOUNTT + 1
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
	FileCOUNT.append(FileCOUNTT)
	
print(FileCOUNT)

PLOF = np.divide(LOF, LOC)

VP = map(sum, zip(GRANGL, GRANFL, GRANBL, GRANSL, GRANEL, GRANML, GRANERR))
LOFperVP = np.divide(LOF, VP)

SD_FILE = np.divide(np.multiply(SDEGMEAN, NOFC), FileCOUNT)
TD_FILE = np.divide(np.multiply(TDEGMEAN, NOFC), FileCOUNT)


versions = ['2.4.25','2.4.26','2.4.27','2.4.28','2.4.29']
numberVersions = range(len(versions))
widthBar = 0.35

# Global Metrics

fig, ax1 = plt.subplots()
ax1.plot(LOC[1:], 'r-', label='LOC')
ax1.set_ylabel('LOC', color='r')
ax2 = ax1.twinx()
ax2.plot(LOF[1:], 'b-', label='LOF')
ax2.set_ylabel('LOF', color='b')
fig.tight_layout()
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(PLOF[1:])
plt.title('PLOF')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(NOFC[1:])
plt.title('NOFC')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(VP[1:])
plt.title('VP (number of #ifdefs out of GRAN)')
plt.xticks(numberVersions, versions)
plt.show()


plt.plot(LOFperVP[1:])
plt.title('LOF per VP')
plt.xticks(numberVersions, versions)
plt.show()


# SD, TD Metric

plt.errorbar(numberVersions, SDEGMEAN[1:], SDEGSTD[1:], linestyle='None', marker='s')
x1,x2,y1,y2 = plt.axis()
plt.axis((x1,x2,-10,20))
plt.title('SD Mean + SD')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(SDEGMEAN[1:])
plt.title('SDEGMEAN')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(SDEGSTD[1:])
plt.title('SDEGSTD')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(SD_FILE[1:])
plt.title('SD per File')
plt.xticks(numberVersions, versions)
plt.show()

plt.errorbar(numberVersions, TDEGMEAN[1:], TDEGSTD[1:], linestyle='None', marker='s')
x1,x2,y1,y2 = plt.axis()
plt.axis((x1,x2,-4,4))
plt.title('TD Mean + SD')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(TDEGMEAN[1:])
plt.title('TDEGMEAN')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(TDEGSTD[1:])
plt.title('TDEGSTD')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(TD_FILE[1:])
plt.title('TD per File')
plt.xticks(numberVersions, versions)
plt.show()

# TYPE

p1 = plt.bar(numberVersions, HOM[1:], widthBar, color='#d62728')
p2 = plt.bar(numberVersions, HOHE[1:], widthBar, bottom=HOM[1:])
p3 = plt.bar(numberVersions, HET[1:], widthBar, bottom=list(map(sum, zip(HOM[1:], HOHE[1:]))))

plt.legend((p1[0], p2[0], p3[0]), ('HOM', 'HOHE', 'HET'))
plt.xticks(numberVersions, versions)
plt.show()

# GRAN

p1 = plt.bar(numberVersions, GRANGL[1:], widthBar, color='#d62728')
p2 = plt.bar(numberVersions, GRANFL[1:], widthBar, bottom=GRANGL[1:])
p3 = plt.bar(numberVersions, GRANBL[1:], widthBar, bottom=list(map(sum, zip(GRANGL[1:], GRANFL[1:]))))
p4 = plt.bar(numberVersions, GRANSL[1:], widthBar, bottom=list(map(sum, zip(GRANGL[1:], GRANFL[1:], GRANBL[1:]))))
p5 = plt.bar(numberVersions, GRANEL[1:], widthBar, bottom=list(map(sum, zip(GRANGL[1:], GRANFL[1:], GRANBL[1:], GRANSL[1:]))))
p6 = plt.bar(numberVersions, GRANML[1:], widthBar, bottom=list(map(sum, zip(GRANGL[1:], GRANFL[1:], GRANBL[1:], GRANSL[1:], GRANEL[1:]))))
p7 = plt.bar(numberVersions, GRANERR[1:], widthBar, bottom=list(map(sum, zip(GRANGL[1:], GRANFL[1:], GRANBL[1:], GRANSL[1:], GRANEL[1:], GRANML[1:]))))

plt.legend((p1[0], p2[0], p3[0], p4[0], p5[0], p6[0], p7[0]), ('GL', 'FL', 'BL', 'SL', 'EL', 'ML', 'ERR'))
plt.xticks(numberVersions, versions)
plt.show()

# AND

plt.errorbar(numberVersions, ANDAVG[1:], ANDSTDEV[1:], linestyle='None', marker='s')
plt.title('AND Mean + SD')
x1,x2,y1,y2 = plt.axis()
plt.axis((x1,x2,0,4))
plt.xticks(numberVersions, versions)
plt.show()


plt.plot(ANDAVG[1:])
plt.title('ANDAVG')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(ANDSTDEV[1:])
plt.title('ANDSTDEV')
plt.xticks(numberVersions, versions)
plt.show()

plt.plot(NDMAX[1:])
plt.title('NDMAX')
plt.xticks(numberVersions, versions)
plt.show()

