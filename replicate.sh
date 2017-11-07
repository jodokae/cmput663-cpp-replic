#!/bin/bash

# Download function 
AddVersion() {
    cd projects
    
    mkdir -p "httpd-$1"
    cd "httpd-$1"
    mkdir -p "source"
    
    wget "https://github.com/apache/httpd/archive/$1.tar.gz"
    tar -xzf "$1.tar.gz"
    
    mv "httpd-$1"/* "source/"
    
    rm "$1.tar.gz"
    rm -r "httpd-$1"
    
    cd ../..
    echo "./projects/httpd-$1" >> cppstats_input.txt
}

SaveResults() {
    cp "cppstats/projects/httpd-$1/cppstats.csv" "results/httpd-$1.csv"
}

# Install necessary dependencies
sudo apt-get install python python-dev python-doc python-setuptools gcc g++ clang python-lxml astyle xsltproc boolstuff git subversion wget dpkg

# Install srcML
wget http://131.123.42.38/lmcrs/beta/srcML-Ubuntu14.04-64.deb
sudo dpkg -i srcML-Ubuntu14.04-64.deb

rm srcML-Ubuntu14.04-64.deb

# Install cppstats
git clone https://github.com/clhunsen/cppstats
cd cppstats
sudo python setup.py install

echo "" > cppstats_input.txt

# Get Apache Projects
mkdir -p projects
AddVersion "2.2.11"
AddVersion "2.4.25"
AddVersion "2.4.26"
AddVersion "2.4.27"
AddVersion "2.4.28"
AddVersion "2.4.29"

# Run analysis
cppstats --kind general

# Copy Results
cd ..
mkdir -p results

SaveResults "2.2.11"
SaveResults "2.4.25"
SaveResults "2.4.26"
SaveResults "2.4.27"
SaveResults "2.4.28"
SaveResults "2.4.29"
