#!/bin/sh

# parameters
# cmd - script-name itself
# indir - input-directory
cmd=${0}


# get the abspath of the input-directory
if [ -z ${1} ]; then
	echo '### no input directory given!'
	exit -1
fi
indir=${1}

D=`dirname "${indir}"`
B=`basename "${indir}"`
indirabs="`cd \"$D\" 2>/dev/null && pwd || echo \"$D\"`/$B"


# change to script directory
if [ `dirname ${cmd}` != '.' ]; then
	cd `dirname ${cmd}` || exit -1
fi


# check the preconditions
bin=${PWD}
echo ${bin}
echo '### preliminaries ...'

which astyle > /dev/null
if [ $? -ne 0 ]; then
	echo '### programm astyle missing!'
	echo '    see: http://astyle.sourceforge.net/'
	exit 1
fi

which xsltproc > /dev/null
if [ $? -ne 0 ]; then
	echo '### programm xsltproc missing!'
	echo '    see: http://www.xmlsoft.org/XSLT/xsltproc2.html'
	exit 1
fi

which booldnf
if [ $? -ne 0 ]; then
	echo '### programm booldnf missing!'
	echo '    see: http://perso.b2b2c.ca/sarrazip/dev/boolstuff.html'
	exit 1
fi


# create the working directory within the sw-project
cd ${indirabs}
invest=${indirabs}/_cppstats

if [ -e ${invest} ]; then
	rm -rf ${invest}
fi
mkdir ${invest}


# copy source-files
echo '### preparing sources ...'
echo '### copying all-files to one folder ...'
echo '### and renaming duplicates (only filenames) to a unique name.'
for i in .h .c;
do
	echo "formating source-file $i"
	find . -path ./_cppstats -prune -o -type f -iname "*${i}" -exec cp --backup=t '{}' ${invest} \;
done

cd ${invest}
for i in `ls *~`;
do
	echo $i
	mv $i `echo $i | sed -r 's/(.+)\.(.+)\.~([0-9]+)~$/\1__\3.\2/g'`
done


# reformat source-files and delete comments and include guards
echo '### reformat source-files'
for i in .h .c;
do
	for j in `ls *${i}`;
	do
		j=${invest}/${j}

		# translate macros that span over multiple lines to one line
		mv ${j} tmp.txt
		${bin}/move_multiple_macros.py tmp.txt ${j}
		rm tmp.txt

		# format source-code
		astyle --style=java ${j}
		if [ -e ${j}.orig ]; then
			rm ${j}.orig
		fi

		# delete leading and trailing whitespaces
		mv ${j} tmp.txt
		${bin}/delete_leadingwhitespaces.sed tmp.txt > ${j}
		rm tmp.txt
		mv ${j} tmp.txt
		${bin}/delete_trailingwhitespaces.sed tmp.txt > ${j}
		rm tmp.txt
		mv ${j} tmp.txt
		${bin}/delete_interwhitespaces.sed tmp.txt > ${j}
		rm tmp.txt

		# delete comments
		${bin}/src2srcml ${j} tmp.xml
		xsltproc ${bin}/delete_comments.xsl tmp.xml > tmp_out.xml
		${bin}/srcml2src tmp_out.xml ${j}
		rm tmp.xml tmp_out.xml

		# delete empty lines
		mv ${j} tmp.txt
		${bin}/delete_emptylines.sed tmp.txt > ${j}
		rm tmp.txt
	done
done

# delete include guards and delete empty lines
#for i in .h;
#do
#	for j in `ls *${i}`;
#	do
#		# delete include guards
#		mv ${j} tmp.txt
#		${bin}/delete_include_guards.py tmp.txt > ${j}
#		rm tmp.txt
#
#		# delete empty lines
#		mv ${j} tmp.txt
#		${bin}/delete_emptylines.sed tmp.txt > ${j}
#		rm tmp.txt
#	done
#done

# create xml-representation of the source-code
echo '### create xml-representation of the source-code files'
for i in .h .c;
do
	for j in `ls *${i}`;
	do
		echo "create representation for ${j}"
		# || rm ${j}.xml is a workaround - since for longer files src2srcml does not work
		src2srcml --cpp_markup_if0 --language=C ${j} ${j}.xml || rm ${j}.xml
	done
done

${bin}/pxml.py ${invest}
