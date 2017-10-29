#!/usr/bin/python
# -*- coding: utf-8 -*-

__author__  = "Jörg Liebig"
__date__    = "$Date: 2009-02-20 09:37:25 +0100 (Fri, 20 Feb 2009) $"
__version__ = "$Rev: 541 $"

# modules from the std-library
import csv, os, re, subprocess, sys

# external libs
# python-lxml module
try:
	from lxml import etree
except ImportError:
	print("python-lxml module not found!")
	print("see http://codespeak.net/lxml/")
	print("programm terminating ...!")
	sys.exit(-1)

# statistics module
try:
	import pstat
except ImportError:
	print("pstat module not found!")
	print("see http://www.nmr.mgh.harvard.edu/Neural_Systems_Group/gary/python.html")
	print("programm terminating ...!")
	sys.exit(-1)

# enum module
try:
	from enum import Enum
except ImportError:
	print("enum module not found!")
	print("see http://pypi.python.org/pypi/enum/")
	print("programm terminating ...!")
	sys.exit(-1)


##################################################
# config:
__outputfile = "cppstats.csv"
##################################################


##################################################
# constants:
# namespace-constant for src2srcml
__cppnscpp = 'http://www.sdml.info/srcML/cpp'
__cppnsdef = 'http://www.sdml.info/srcML/src'
__cpprens = re.compile('{(.+)}(.+)')

# conditionals - necessary for parsing the right tags
__conditionals = ['if', 'ifdef', 'ifndef']
__conditionals_elif = ['elif']
__conditionals_else = ['else']
__conditionals_endif = ['endif']
__conditionals_all = __conditionals + __conditionals_elif + __conditionals_else

# collected statistics
__statsorder = Enum(
	'filename',			# name of the file
	'loc',				# lines of code
	'nof',				# number of features
	'lof',				# number of feature-code lines
	'minlof',			# minimum number of lof (single feature)
	'maxlof',			# maximum number of lof (single feature)
	'meanlof',			# average number of lof (single feature)
	'stdevlof',			# standard-deviation of lof (single feature)
	'nifdefs',			# number of nested ifdefs
	'scode',			# shared code "||"
	'deriv',			# derivative
	'desc',				# combination of shared code and derivative
)
##################################################


def _collapseSubElementsToList(node):
	"""This function collapses all subelements of the given element
	into a list used for getting the signature out of an #ifdef-node."""
	# get all descendants - recursive - children, children of children ...
	itdesc = node.iterdescendants()

	# iterate over the elemtents and add them to a list
	return [ item.text for item in itdesc ]


def _getMacroSignature(ifdefnode):
	"""This function gets the signature of an ifdef or corresponding macro
	out of the xml-element and its descendants. Since the macros are held
	inside the xml-representation in an own namespace, all descendants
	and their text corresponds to the macro-signature.
	"""
	# get either way the expr-tag for if and elif
	# or the name-tag for ifdef and ifndef,
	# which are both the starting point for signature
	# see the srcml.dtd for more information
	nexpr = []
	res = ''
	ns, tag = __cpprens.match(ifdefnode.tag).groups()

	# get either the expr or the name tag, which is always the second descendant
	if (tag in ['if', 'elif', 'ifdef', 'ifndef']):
		nexpr = [itex for itex in ifdefnode.iterdescendants()]
		if (len(nexpr) == 1):
			res = nexpr[0].tail
		else:
			nexpr = nexpr[1]
			res = ''.join([token for token in nexpr.itertext()])
	return res


def _prologCSV(folder):
	"""prolog of the CSV-output file
	no corresponding _epilogCSV."""
	fd = open(os.path.join(folder, __outputfile), 'w')
	fdcsv = csv.writer(fd, delimiter=',')
	fdcsv.writerow(list(__statsorder._keys))

	return (fd, fdcsv)


def _countNestedIfdefs(root):
	"""This function counts the number of nested ifdefs (conditionals)
	within the source-file."""
	cncur = 0
	cnmax = cncur

	elements = [it for it in root.iterdescendants()]

	for elem in elements:
		ns, tag = __cpprens.match(elem.tag).groups()

		if ((tag in __conditionals_endif)
				and (ns == __cppnscpp)):
			cncur -= 1
		if ((tag in __conditionals)
				and (ns == __cppnscpp)):		# if, ifdef, ifndef; else and elif doesn't count
			cncur += 1

		cnmax = max(cnmax, cncur)

	return cnmax


def _getFeatureSignature(macrohistory, elem, tag):
	"""This method updates the macro-history according to certain rules.
	For more information see below."""

	if (tag not in __conditionals_else):
		sig = _getMacroSignature(elem)

	# hitting a else-macro
	# invert all macros before
	# !( <macros before> )
	if (tag in __conditionals_else):
		macrohistory.insert(0, '!(')
		macrohistory.append(')')

	# hitting a conditional macro
	# means and-connection
	# (( <macros before> ) && )
	if (tag in __conditionals):
		if macrohistory:
			macrohistory.insert(0, '((')
			macrohistory.append(') && (%s))' % sig)
		else:
			macrohistory.append(sig)

	# hitting a elif-macro
	# means combination of and and not
	#  (!( <macros before> ) && )
	if (tag in __conditionals_elif):
		macrohistory.insert(0, '(!(')
		macrohistory.append(') && (%s))' % sig)

	return macrohistory


def _getFeatures(fname):
	"""This function returns all features in the source-file.
	A feature is defined as an enframement of soure-code. The frame
	consists of an ifdef (conditional) and an endif-macro. The function
	returns a tuple with the following format:
	({'<feature signature>: [<feature elements>]},
	{'<feature signature>: [<feature tags-enclosed>]

	feature elements: Every feature element reflects one part of a
	feature withing the whole source-code, that is framed by contional
	and endif-macros.
	
	feature tags-enclosed: All tags from the feature elements (see above).

	feature tags-surround: All tags from the elements arround the feature.
	"""
	features = {}			# see above; return value
	featurestags = {}		# see above; return value
	flist = []				# holds the features in order, that are currently in stock
							# list empty -> no features to parse; list used as a stack
							# last element = top of stack; and the element we currently
							# collecting source-code lines for
	fcode = []				# holds the code of the features in order like featurelist
	ftags = []				# holds the tags of the features in order like featurelist
	macrohistory = []		# history of the already visited macros for creating the
							# feature-signature
	parcon = False			# parse-conditional-flag
	parend = False			# parse-endif-flag
	elelifdepth = 0			# else and elif depth

	# iterate over all tags separately <start>- and <end>-tag
	for event, elem in etree.iterparse(fname, events=("start", "end")):
		ns, tag = __cpprens.match(elem.tag).groups()

		# handling conditionals
		# hitting on conditional-macro
		if ((tag in __conditionals_all)
				and (event == "start")
				and (ns == __cppnscpp)):	# check the cpp:namespace
			parcon = True

		# hitting end-tag conditional-macro
		if ((tag in __conditionals_all)
				and (event == "end")
				and (ns == __cppnscpp)):	# check the cpp:namespace
			parcon = False
			item = _getFeatureSignature(macrohistory, elem, tag)
			item = reduce(lambda m,n: m+n, item)
			#print('feature-signature :', item)
			flist.append(item)
			fcode.append('')
			ftags.append([])

			if ((tag in __conditionals_elif) \
					or (tag in __conditionals_else)):
				elelifdepth += 1

		# iterateting in subtree of conditional-node
		if parcon:
			continue

		# handling endif-macro
		# hitting an endif-macro start-tag
		if ((tag in __conditionals_endif)
				and (event == "start")
				and (ns == __cppnscpp)):	# check the cpp:namespace
			parend = True

		# hitting the endif-macro end-tag
		if ((tag in __conditionals_endif)
				and (event == "end")
				and (ns == __cppnscpp)):	# check the cpp:namespace
			parend = False

			# wrap up the feature
			itsig = flist[-1]				# feature signature
			flist = flist[:-1]

			itcode = fcode[-1][1:]			# feature code
			itcode = itcode.replace('\n\n', '\n')
			fcode = fcode[:-1]

			ittags = ftags[-1]				# feature enclosed tags
			ftags = ftags[:-1]

			# TODO elelifdepth
			macrohistory = macrohistory[1:-1]	# delete first and last element

			if (features.has_key(itsig)):
				features[itsig].append(itcode)
			else:
				features[itsig] = [itcode]

		# iterating the endif-node subtree
		if parend:
			continue

		# collect the source-code of the feature
		if (len(flist)):
			if ((event == "start") and (elem.text)):
				fcode[-1] += elem.text
			if ((event == "end") and (elem.tail)):
				fcode[-1] += elem.tail

			ftags[-1].append(tag)

	return (features, featurestags)


def _getDNF(signature):
	"""This function returns a DNF-representation of a given signature.
	It uses the the tool booldnf (http://perso.b2b2c.ca/sarrazip/dev/boolstuff.html)
	for this task, which returns a DNF-form for an expression."""

	def _prepareSignature(signature):
		"""This function prepares the signature for the DNF-creation. In order
		to do this, it replaces the && and || with & and | respective. In addition
		to that <, >, <=, >=, == and != are replaced by the __COMPARE_OPERATOR__.
		COMPARE_OPERATORS are in:
		<  => LT;  >  => GT;  <= => LE;  >= => GE;  == => EQ;  != => NE."""
		signature = signature.replace('&&', '&')
		signature = signature.replace('||', '|')
		return signature

	sig = _prepareSignature(signature)
	cmd = ['booldnf']
	dnftrans = subprocess.Popen(args = cmd, \
			stdin = subprocess.PIPE, stderr = subprocess.PIPE, stdout = subprocess.PIPE)
	retout, reterr = dnftrans.communicate(sig)

	# checking for error occurance
	if retout.startswith('?'):
		print('ERROR: could not transform signature (%s) into DNF!' % sig)


def _checkSignatureEquivalence(sig1, sig2):
	"""This method checks the the equivalence of two signatures
	by comparing their expressions
	ssig1 = set(sig1.split('|'))
	ssig2 = set(sig2.split('|'))
	return (ssig1==ssig2)


def _getFeatureStats(features):
	"""This function determines and returns the following statistics about
	the features:
		- nof			# number of features
		- lof			# lines of feature code (sum)
		- minlof		# minimum line of feature-code
		- maxlof		# maximum number of feature-code lines
		- meanlof		# mean of feature-code lines
		- stdevlof		# std-deviation of featurec-code lines
	"""
	nof = 0
	lof = 0
	minlof = -1			# necessary a feature can also not contain any code
	maxlof = 0
	floflist = []		# feature-line of code list; necessary for computing mean, stdev
	meanlof = 0
	stdevlof = 0

	nof = len(features.keys())
	tmp = [item[0] for item in features.itervalues()]
	floflist = map(lambda n: n.count('\n'), tmp)

	if (len(floflist)):
		minlof = min(floflist)
		maxlof = max(floflist)
		lof = reduce(lambda m,n: m+n, floflist)
		meanlof = pstat.stats.lmean(floflist)

	if (len(floflist) > 1):
		stdevlof = pstat.stats.lstdev(floflist)
	else:
		stdevlof = 0		# stdev from one value zero?

	return (nof, lof, minlof, maxlof, meanlof, stdevlof)


def _distinguishFeatures(features):
	"""This function returns a tuple with dicts, each holding
	one type of feature. The determination is according to the
	given macro-signatures. Following differentiation:
	1. "||" -> shared code
	2. "&&" -> derivative
	3. "||" & "&&" -> ??
	"""
	# TODO - how about the ==, !=, <=, >=, < and >
	scode = {}
	deriv = {}
	desc = {}

	for key, item in features.iteritems():
		# shared code
		if ((key.__contains__("||")) \
				and (not key.__contains__("&&"))):
			scode[key] = item

		# derivative
		if ((key.__contains__("&&")) \
				and (not key.__contains__("||"))):
			deriv[key] = item

		# combination shared code and derivative
		if ((key.__contains__("||")) \
				and (key.__contains__("&&"))):
			desc[key] = item

	return (scode, deriv, desc)


def apply(folder):
	"""This function applies the analysis to all xml-files in that
	directory and take the results and joins them together. Results
	are getting written into the fdcsv-file."""
	# overall status variables
	aloc = 0				# lines of code
	afeatures = {}			# hash of identified features; {<signature>: <implementation>}
	afeatures_gr = {}		# hash of granularity of identified features; {<signature>: <granularity>}
	rt = lambda m,n: m+n

	def _mergeFeatures(ffeatures):
		"""This function merges the, with the parameter given dictionary (ffeatures)
		to the afeatures (overall-features)."""
		for key, item in ffeatures.iteritems():
			countnlbef = [0]
			countnlelm = map(lambda n: n.count('\n'), item)
			if afeatures.has_key(key):
				countnlbef = map(lambda n: n.count('\n'), afeatures[key])
				afeatures[key] += item
			else:
				afeatures[key] = item
			countnlaft = map(lambda n: n.count('\n'), afeatures[key])
			nlbef = reduce(rt, countnlbef)
			nlelm = reduce(rt, countnlelm)
			nlaft = reduce(rt, countnlaft)
			print('before: %3s    elem: %3s    after: %3s ===> %s %s' % \
					(nlbef, nlelm, nlaft, countnlaft, item))
			if ((nlbef+nlelm) != nlaft):
				print('ERROR: this is not going to work!')

	# outputfile
	fd, fdcsv = _prologCSV(folder)

	files = os.listdir(folder)
	files = filter(lambda n: os.path.splitext(n)[1] == '.xml', files)
	fstats = [None]*len(__statsorder._keys)

	# get statistics for all files; write results into csv and merge the features
	for file in files:
		try:
			tree = etree.parse(file)
		except etree.XMLSyntaxError:
			print("ERROR: cannot parse (%s). Skipping this file." % file)
			continue

		#print('INFO: parsing (%s).' % file)
		root = tree.getroot()
		features, featurestags = _getFeatures(file)

		# general stats
		fstats[__statsorder.filename.index] = file
		fstats[__statsorder.nifdefs.index] = _countNestedIfdefs(root)
		tmp = [it for it in root.iterdescendants()]

		if (len(tmp)):
			floc = tmp[-1].sourceline
		else:
			floc = 0

		aloc += floc
		fstats[__statsorder.loc.index] = floc

		# feature-amount
		(nof, lof, minlof, maxlof, meanlof, stdevlof) = _getFeatureStats(features)
		fstats[__statsorder.nof.index] = nof
		fstats[__statsorder.lof.index] = lof

		if minlof == -1:
			minlof = 0

		fstats[__statsorder.minlof.index] = minlof
		fstats[__statsorder.maxlof.index] = maxlof
		fstats[__statsorder.meanlof.index] = meanlof
		fstats[__statsorder.stdevlof.index] = stdevlof

		# feature differentiation
		scode, deriv, desc = _distinguishFeatures(features)
		fstats[__statsorder.scode.index] = len(scode.keys())
		fstats[__statsorder.deriv.index] = len(deriv.keys())
		fstats[__statsorder.desc.index] = len(desc.keys())

		fdcsv.writerow(fstats)
		_mergeFeatures(features)

	# writing convinience functions
	fnum = len(files)
	excelcols = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	excelfunc = [None]*len(__statsorder._keys)
	excelfunc[__statsorder.filename.index] = "FUNCTIONS"
	excelfunc[__statsorder.loc.index] = "=SUM(%s2:%s%s)" % \
			(excelcols[__statsorder.loc.index], excelcols[__statsorder.loc.index], fnum)
	excelfunc[__statsorder.nof.index] = "=SUM(%s2:%s%s)" % \
			(excelcols[__statsorder.nof.index], excelcols[__statsorder.nof.index], fnum)
	excelfunc[__statsorder.lof.index] = "=SUM(%s2:%s%s)" % \
			(excelcols[__statsorder.lof.index], excelcols[__statsorder.lof.index], fnum)
	excelfunc[__statsorder.minlof.index] = "=MIN(%s2:%s%s)" % \
			(excelcols[__statsorder.minlof.index], excelcols[__statsorder.minlof.index], fnum)
	excelfunc[__statsorder.maxlof.index] = "=MAX(%s2:%s%s)" % \
			(excelcols[__statsorder.maxlof.index], excelcols[__statsorder.maxlof.index], fnum)
	excelfunc[__statsorder.nifdefs.index] = "=MAX(%s2:%s%s)" % \
			(excelcols[__statsorder.nifdefs.index], excelcols[__statsorder.nifdefs.index], fnum)
	excelfunc[__statsorder.scode.index] = "=MAX(%s2:%s%s)" % \
			(excelcols[__statsorder.scode.index], excelcols[__statsorder.scode.index], fnum)
	excelfunc[__statsorder.deriv.index] = "=MAX(%s2:%s%s)" % \
			(excelcols[__statsorder.deriv.index], excelcols[__statsorder.deriv.index], fnum)
	excelfunc[__statsorder.desc.index] = "=MAX(%s2:%s%s)" % \
			(excelcols[__statsorder.desc.index], excelcols[__statsorder.desc.index], fnum)
	fdcsv.writerow(excelfunc)

	# overall - stats
	astats = [None]*len(__statsorder._keys)
	(nof, lof, minlof, maxlof, meanlof, stdevlof) = _getFeatureStats(afeatures)

	astats[__statsorder.filename.index] = "ALL - MERGED"
	astats[__statsorder.nof.index] = nof
	astats[__statsorder.lof.index] = lof
	astats[__statsorder.minlof.index] = minlof
	astats[__statsorder.maxlof.index] = maxlof
	astats[__statsorder.meanlof.index] = meanlof
	astats[__statsorder.stdevlof.index] = stdevlof
	scode, deriv, desc = _distinguishFeatures(afeatures)
	astats[__statsorder.scode.index] = len(scode.keys())
	astats[__statsorder.deriv.index] = len(deriv.keys())
	astats[__statsorder.desc.index] = len(desc.keys())
	fdcsv.writerow(astats)

	fd.close()


def usage():
	"""This function prints usage-informations to stdout."""
	print('usage:')
	print('  ' + sys.argv[0] + ' <folder>')


##################################################
if __name__ == '__main__':

	if (len(sys.argv) < 2):
		usage()
		sys.exit(-1)

	folder = os.path.abspath(sys.argv[1])
	if (os.path.isdir(folder)):
		apply(folder)
	else:
		usage()
		sys.exit(-1)
