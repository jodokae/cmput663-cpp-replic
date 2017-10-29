Python Source: https://github.com/python/cpython/archive/v2.6.1.tar.gz

CPP Stats: http://www.infosun.fim.uni-passau.de/cl/staff/liebig/cppstats/deploy/cppstats_0.1.zip

CPP Stats 0.7: https://github.com/joliebig/cppstats/zipball/v0.7

Dependencies:
sudo apt-get install python-lxml
sudo apt-get install astyle
sudo apt-get install xsltproc
sudo apt-get install boolstuff


Download compatible version of src2srcml (http://www.sdml.cs.kent.edu/lmcrs/)

Change pxml Version to python2.7
Change cpp_general_checkall.sh Paths to absolute paths

# Run #

1. Go to cppstats0.7
2. Change cppstats_input.txt to your filepath
3. change cpp_general_checkall.sh paths to yours
4. Run ./cppstats_general_prepareall.sh
5. Run ./cppstats_general_checkall.sh
6. See results in cpython.../_cppstats/cppstats.csv


# Introduction
The aim of this project is reproducing the result of *"An analysis of the variability in forty preprocessor-based software product lines" by Liebig et al., ICSE '10*. This performs in two steps:
1. We execute *cppstats* on the excact verison of *apache* in the paper (2.2.11).
2. *cppstats* are employed on fire version of *apache* and the evolution is analyzed after that.

# Installation
1. To run the code, you need to install the following packages first:
```
sudo apt-get install python python-dev python-doc python-setuptools gcc g++ clang python-lxml astyle xsltproc boolstuff git subversion
```
2. After that, the appropriate version of *<srcML>* should be installed by [Download page](http://www.srcml.org/#download).
3. Clone the latest version of 'cppstates' by 
```
https://github.com/clhunsen/cppstats
```

# Part 1:
1. Download the proper *Apachi project (2.2.x)* by:
```
svn checkout http://svn.apache.org/repos/asf/httpd/httpd/branches/2.2.x httpd-2.2.x
```
2. Create *./project/httpd/source* directory inside the *cppstats* folder and copy the cloned *httpd-2.2.x* inside *source*. Also, add *./projects/httpd-2.2.x* to *cppstats_input.txt*.
3. run *cppstats* by:
```
cppstats --kind general
```
4. The results are in *projects/httpd-2.2.x/cppstats.csv*

# Part 2:
1. Download 5 diffrent versions of the code by: 
```
svn checkout http://svn.apache.org/repos/asf/httpd/httpd/branches/1.3.x httpd-1.3.x
svn checkout http://svn.apache.org/repos/asf/httpd/httpd/branches/2.0.x httpd-2.0.x
svn checkout http://svn.apache.org/repos/asf/httpd/httpd/branches/2.2.x httpd-2.2.x
svn checkout http://svn.apache.org/repos/asf/httpd/httpd/branches/2.2.x-merge-http-strict/ httpd-2.2.x-merge-http-strict/
svn checkout http://svn.apache.org/repos/asf/httpd/httpd/branches/2.4.x httpd-2.4.x
```
2. Based on the instruction of **Part 1**, generate the cresults for all versions. (copy them into */projects/.../source* and add their path to *cppstats_input.txt*.)