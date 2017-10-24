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
