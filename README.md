# Introduction
The aim of this project is reproducing the result of *"An analysis of the variability in forty preprocessor-based software product lines" by Liebig et al., ICSE '10*. This performs in two steps:
1. We execute *cppstats* on the exact version of *apache* in the paper (2.2.11).
2. *cppstats* are employed on the five latest versions of *apache* and the evolution is analysed after that.

This analysis was done for the CMPUT 663 course at the University of Alberta in the Fall 2017 term by Johannes Kästle and Moein Owhadi Kareshk.

Original authors website and GitHub: 
```
http://fosd.net/cppstats
https://github.com/clhunsen/cppstats
```

# Technical details

Since it was not quite clear on which version of their tool *cppstats* the authors of the named paper ran the analysis, we tried different ones (as can be partly seen in the commit history of this repository). Still, it was not possible for us to reproduce exactly the results that were published. We tried this on cppstats 0.7, 0.93 and tried on 0.1 but did not get it running. We tried to analyse python, subversion and apache. 

Because we were unable to reproduce the results (all metrics are different as can be seen in *plots/original-vs-ours.txt*, we concluded that we would use the latest version of cppstats. 

# Results

The results of the extraction can be found under *results/*. The plots produced with the results can be found under *plots/*

We compared the following metrics over the project history:
* LOC - Lines of Code
* LOF - Lines of Feature Code
* PLOF - Relative fraction of Feature code (LOF / LOC)
* NOFC - Number of Feature Constants
* VP - Number of #ifdefs (extracted from the original GRAN metrics)
* LOF per VP - Lines of Feature code per #ifdef
* SD, TD - Scattering and Tangling Degree, Mean & Standard Deviation
* SD, TD / File - The mean of SD and TD per File
* TYPE - Type of extensions
* GRAN - Granularity of #ifdef
* AND - Average nesting depth, Mean & Standard Deviation
* NDMAX - Maxing nesting depth

# Usage

## Automized

### Installing & Extracting

Tested under Ubuntu 16.04 LTS x64
Tested with: cppstats v0.9.3, python3.5.2, srcML v0.9.5. If not stated otherwise, the newest version (30.10.2017) was used.

In order to install all necessary tools and extract all metrics out of the six tested apache versions, just run the following script.

Please consider that the script is downloading SrcML for Ubuntu with a version greater than 14.04 in the x64 version. If you run this script under any other OS it may not work, since srcML is OS dependant! If doing so change the script to download the appropriate version for your OS or remove these lines and install manually.

```
./replicate.sh
```

### Creating plots

Run now the python script to get the results. The following python packages are needed: glob, numpy, matplotlib, prettytable.

```
python plot.py
```

## Manual

### Installation
1. To run the code, you need to install the following packages first:
```
sudo apt-get install python python-dev python-doc python-setuptools gcc g++ clang python-lxml astyle xsltproc boolstuff git
```
2. After that, the appropriate version of *<srcML>* should be installed by [Download page](http://www.srcml.org/#download).
3. Clone the latest version of 'cppstats' by 
```
https://github.com/clhunsen/cppstats
```

4. Install cppstats 
```
sudo python setup.py install
```

### Extraction
1. Download the proper *Apache project (2.2.11)* by:
```
wget https://github.com/apache/httpd/archive/2.2.11.tar.gz
```
2. Create *./project/httpd/source* directory inside the *cppstats* folder and copy the cloned *httpd-2.2.11* inside *source*. Also, add *./projects/httpd-2.2.11* to *cppstats_input.txt*.
3. run *cppstats* by:
```
cppstats --kind general
```
4. The results are in *projects/httpd-2.2.11/cppstats.csv*

# Version comparison:
1. Download 5 different versions of the code by: 
```
wget https://github.com/apache/httpd/archive/2.4.25.tar.gz
wget https://github.com/apache/httpd/archive/2.4.26.tar.gz
wget https://github.com/apache/httpd/archive/2.4.27.tar.gz
wget https://github.com/apache/httpd/archive/2.4.28.tar.gz
wget https://github.com/apache/httpd/archive/2.4.29.tar.gz
```

2. Based on the instruction of **Part 1**, generate the results for all versions. (copy them into */projects/.../source* and add their path to *cppstats_input.txt*.)
3. copy all csv files in project's folders to *results* folder and run *plot.py*.
